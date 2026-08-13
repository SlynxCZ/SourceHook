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
* Declares SH_DECL_INLINEHOOK0..{max_arity}, the inline-hook counterpart of
* upstream's SH_DECL_MANUALHOOK0..20 (see sourcehook.h). Each macro declares
* a SourceHookInlineDecl::<hookname> type carrying the
* SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1..N>
* specialization that SH_ADD_INLINEHOOK/SH_REMOVE_INLINEHOOK
* (sourcehook_inline.h) dispatch through for that hook name.
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


def emit_macro(n):
  params = [f'param{i}' for i in range(1, n + 1)]
  arg_list = ', '.join(['hookname', 'thisclass', 'rettype'] + params)
  template_args = ', '.join(['thisclass', 'rettype'] + params)
  lines = []
  lines.append(f'#define SH_DECL_INLINEHOOK{n}({arg_list}) \\')
  lines.append('\tnamespace SourceHookInlineDecl { \\')
  lines.append('\t\tstruct hookname \\')
  lines.append('\t\t{ \\')
  lines.append(f'\t\t\tusing Dispatcher = ::SourceHook::Impl::CInlineDispatcher<{template_args}>; \\')
  lines.append('\t\t}; \\')
  lines.append('\t}')
  return '\n'.join(lines) + '\n\n'


def main():
  out_path = os.path.join(os.path.dirname(__file__), 'sourcehook_inline_decl.h')
  with open(out_path, 'w') as f:
    f.write(HEADER)
    for n in range(0, MAX_ARITY + 1):
      f.write(emit_macro(n))
    f.write(FOOTER)
  print(f'Wrote {out_path}')


if __name__ == '__main__':
  main()
