#!/usr/bin/env python3
# vim: set sts=2 ts=8 sw=2 tw=99 et:
# Copyright (C) 2026 Metamod:Source Development Team
# Author(s): Michal "Slynx" Přikryl
"""Generates sourcehook_inline_decl.h: the SH_DECL_INLINEHOOK0..MAX_ARITY family
of macros, one per parameter count, following the exact textual shape of
upstream's own SH_DECL_MANUALHOOK0..20 macros (see sourcehook.h) so inline
hooks declare the same way every other SourceHook hook category does.

This stands in for reviving metamod-source's old C++ `shworker` codegen tool
(core/sourcehook/generate/, kept in this repo under generate/upstream_codegen/
for provenance) -- a plain generator script is easier to read, run, and keep
in sync than a compiled templating tool for a macro family this size.

Run `python3 generate/gen_inline_hooks.py` from the repo root after changing
MAX_ARITY or the macro body below; it overwrites generate/sourcehook_inline_decl.h.
"""
import os

MAX_ARITY = 20

HEADER = """/* ======== SourceHook ========
* Copyright (C) 2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
*
* GENERATED FILE -- do not edit by hand.
* Regenerate with: python3 generate/gen_inline_hooks.py
*
* Declares SH_DECL_INLINEHOOK0..{max_arity} and SH_DECL_INLINEHOOK0..{max_arity}_void,
* the inline-hook counterpart of upstream's SH_DECL_MANUALHOOK0..20 (and its
* own _void family) -- see sourcehook.h. The _void form just drops `rettype`
* (hardcoded to void), same relationship SH_DECL_HOOKn_void/
* SH_DECL_MANUALHOOKn_void already have to their non-_void counterparts.
* Each macro declares
* a SourceHookInlineDecl::<hookname>_Tag type carrying the
* SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1..N>
* specialization that SH_ADD_INLINEHOOK/SH_REMOVE_INLINEHOOK
* (sourcehook_inline.h) dispatch through for that hook name -- plus a
* constexpr *value* actually named <hookname> (of that Tag type), so
* SH_CALL(hookname, targetAddr[, thisptr]) can pass it as a real argument
* and let ordinary C++ overload resolution pick it over vtable hooks'
* SH_CALL(ptr, mfp) (see sourcehook_inline.h's SH_CALL2 overload).
*
* `thisclass` is `void` for a hooked function with no `this` (a real free
* function), or the class type for a non-virtual member function -- see
* sourcehook_inline.h's file header for why that's a separate template
* parameter instead of Args...[0], and SH_INLINE_IFACEPTR for how to read it
* back out inside a handler.
* ============================
*/

#ifndef __SOURCEHOOK_INLINE_DECL_H__
#define __SOURCEHOOK_INLINE_DECL_H__

""".format(max_arity=MAX_ARITY)

FOOTER = "\n#endif //__SOURCEHOOK_INLINE_DECL_H__\n"


def emit_macro(n, void_variant=False):
  params = [f'param{i}' for i in range(1, n + 1)]
  macro_name = f'SH_DECL_INLINEHOOK{n}' + ('_void' if void_variant else '')
  # The _void variant drops `rettype` from the argument list entirely and
  # hardcodes it to void in the template args below -- same relationship
  # SH_DECL_HOOKn_void/SH_DECL_MANUALHOOKn_void already have to their
  # rettype-taking counterparts (see sourcehook.h): a void-returning hook is
  # common enough that not having to spell out "void" is worth a second
  # macro, rather than making every caller write it explicitly.
  arg_list = ', '.join(['hookname', 'thisclass'] + ([] if void_variant else ['rettype']) + params)
  template_args = ', '.join(['thisclass', 'void' if void_variant else 'rettype'] + params)
  lines = []
  lines.append(f'#define {macro_name}({arg_list}) \\')
  lines.append('\tnamespace SourceHookInlineDecl { \\')
  lines.append('\t\tstruct hookname##_Tag \\')
  lines.append('\t\t{ \\')
  lines.append(f'\t\t\tusing Dispatcher = ::SourceHook::Impl::CInlineDispatcher<{template_args}>; \\')
  lines.append('\t\t\tusing ThisClassT = thisclass; \\')
  lines.append(f'\t\t\tusing Callable = ::SourceHook::Impl::InlineExecutable<{template_args}>; \\')
  # Backing store for SH_GET_INLINEHOOK_ORIGINAL(hookname, targetAddr) (see
  # sourcehook_inline.h): a plain, capture-free C function pointer -- of
  # exactly this hook's own real signature -- that always calls through to
  # whatever the *current* original is for targetAddr, for the rare external
  # caller that needs a real function pointer to store (e.g. in another
  # plugin's own extern fn-ptr global) rather than going through SH_CALL/a
  # SourceHook handler chain. Keyed by hookname##_Tag (not bare
  # <thisclass, rettype, param1..N>) so two different hooknames that happen
  # to share an identical signature don't collide on the same trampoline's
  # recorded address.
  lines.append(f'\t\t\tusing OriginalTrampoline = ::SourceHook::Impl::InlineOriginalTrampoline<hookname##_Tag, {template_args}>; \\')
  lines.append('\t\t}; \\')
  # A *value* named `hookname` (not just a type) -- SH_CALL(hookname, addr)
  # needs to pass it as an argument for C++ overload resolution to pick the
  # inline-hook SH_CALL2 overload over the vtable one; a bare type name
  # can't be used as an expression. SH_ADD_INLINEHOOK/SH_REMOVE_INLINEHOOK
  # reach the Dispatcher type back out via decltype(hookname)::Dispatcher.
  lines.append('\t\tinline constexpr hookname##_Tag hookname{}; \\')
  lines.append('\t} \\')
  # SH_CALL(hookname, ...) needs plain, unqualified `hookname` to already
  # name a value at the point of the call (its shared 2-argument macro path
  # can't textually prefix "SourceHookInlineDecl::" itself -- that same path
  # is also what vtable hooks' SH_CALL(ptr, mfp) expands through, and `ptr`
  # there is an arbitrary expression, not something namespace-qualifiable).
  # This brings just this one name out of SourceHookInlineDecl:: into
  # whatever scope SH_DECL_INLINEHOOK* was invoked in, same spirit as a
  # type alias but for the value itself.
  lines.append('\tusing SourceHookInlineDecl::hookname;')
  return '\n'.join(lines) + '\n\n'


def main():
  out_path = os.path.join(os.path.dirname(__file__), 'sourcehook_inline_decl.h')
  with open(out_path, 'w') as f:
    f.write(HEADER)
    for n in range(0, MAX_ARITY + 1):
      f.write(emit_macro(n))
      f.write(emit_macro(n, void_variant=True))
    f.write(FOOTER)
  print(f'Wrote {out_path}')


if __name__ == '__main__':
  main()
