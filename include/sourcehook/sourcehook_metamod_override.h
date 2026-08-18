/* ======== SourceHook ========
* Copyright (C) 2004-2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

// A metamod:source plugin normally shares ONE SourceHook engine instance
// across the whole server: PLUGIN_SAVEVARS() (ISmmPlugin.h) points the
// default SH_GLOB_SHPTR/SH_GLOB_PLUGPTR (g_SHPtr/g_PLID) at whatever
// ISourceHook metamod.so itself was built with, via
// ismm->MetaFactory(MMIFACE_SOURCEHOOK, ...). Every plugin's SH_DECL_HOOK*/
// SH_ADD_*HOOK calls go through that one shared instance.
//
// Including this header lets a plugin opt out of that and bring its own,
// private SourceHook engine instead -- e.g. to use a newer SourceHook (this
// repo, with SH_DECL_INLINEHOOK support) than whatever version metamod.so
// on the target server actually ships, without waiting on/depending on that
// server's metamod build at all.
//
//   #include <ISmmPlugin.h>        // (or anything that pulls in sourcehook.h)
//   #include "sourcehook_metamod_override.h"
//
//   SH_DECL_HOOK1_void(SomeClass, SomeMethod, SH_NOATTRIB, 0, int);
//
//   bool MyPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
//   {
//       PLUGIN_SAVEVARS();
//       SH_METAMOD_OVERRIDE_SAVEVARS(id);   // <-- right after, same as PLUGIN_SAVEVARS()
//       ...
//   }
//
//   // SH_ADD_HOOK/SH_ADD_MANUALHOOK/SH_ADD_INLINEHOOK from here on all go
//   // through g_pSourceHook, not metamod's g_SHPtr.
//
// Must be included AFTER sourcehook.h (directly or transitively, e.g. via
// ISmmPlugin.h) has already defined the default SH_GLOB_SHPTR/
// SH_GLOB_PLUGPTR at least once -- the #undef below needs something to
// undefine -- and BEFORE any SH_DECL_HOOK*/SH_ADD_*HOOK/SH_DECL_INLINEHOOK*
// call you want routed through the private instance, since these macros
// resolve SH_GLOB_SHPTR/SH_GLOB_PLUGPTR at the point they're expanded.
//
// SH_METAMOD_OVERRIDE_SAVEVARS(id) just gives g_iSourceHookPluginId the
// plugin's real metamod-assigned id (mirroring what PLUGIN_SAVEVARS() does
// for g_PLID) instead of leaving it at its static-init default of 0 --
// cosmetic for a solo-owned instance (nothing else ever registers against
// it), but keeps it traceable/consistent with the rest of the plugin ABI.
//
// Caveat: g_pSourceHook is NOT the same engine instance as metamod's
// g_SHPtr (or any other plugin using it). Hooks installed through
// g_pSourceHook are not coordinated with hooks on the same vtable
// slot/address installed through a different ISourceHook instance -- each
// side patches independently. This only matters if some other plugin hooks
// the exact same thing through metamod's shared instance; harmless
// otherwise. g_SHPtr/g_PLID themselves are untouched (PLUGIN_SAVEVARS()
// still needs them for the rest of the metamod API), this only redirects
// where the SH_* hook macros look.

#ifndef __SOURCEHOOK_METAMOD_OVERRIDE_H__
#define __SOURCEHOOK_METAMOD_OVERRIDE_H__

// Bare, not "sourcehook/sourcehook_impl.h" -- this file already lives
// inside include/sourcehook/ itself, so a bare include resolves via the
// "search the includer's own directory first" rule, straight to our own
// sibling file, with no include-search-path involved at all. The qualified
// spelling is genuinely ambiguous for a real metamod:source plugin: its
// build also has metamod-source's own core/ on the include path (for
// <ISmmPlugin.h> etc.), which contains its own core/sourcehook/ -- so
// "sourcehook/sourcehook_impl.h" can resolve to *metamod's* copy instead of
// this one, depending on -I ordering, silently pulling in metamod's own
// (older) CSourceHookImpl instead of this fork's.
#include "sourcehook_impl.h"

// hidden, deliberately: these are `inline` (header-defined, COMDAT/weak)
// globals at file scope with no namespace -- without an explicit hidden-
// visibility attribute, a build compiled with -fvisibility=default (this
// repo's own makefiles/linux.base.cmake sets exactly that, project-wide, via
// CMAKE_CXX_FLAGS) exports them into each consuming .so's dynamic symbol
// table under their bare, unmangled-by-namespace names. Any OTHER
// independently-built .so that also happens to vendor this exact file (a
// pinned-commit fork of this same repo, for instance) defines the identical
// symbol names -- and metamod loads plugins via dlopen(..., RTLD_GLOBAL), so
// the dynamic linker can/will interpose the second .so's "private" globals
// onto whichever one loaded first, silently merging two supposedly-isolated
// SourceHook engine instances into one. That's a real, observed bug: a
// third-party plugin's own "private" g_SourceHookImpl ended up aliasing
// this suite's own Core.so instance (shared, by design, with every other
// FUNPLAY plugin via sourcehook_metamod_shared.h) purely because both sides
// exported the same bare symbol name -- corrupting the third-party plugin's
// own hook-manager bookkeeping (it saw an already-populated plugin/hook-id
// space instead of a fresh one) and crashing on a subsequent, unrelated
// vtable lookup. __attribute__((visibility("hidden"))) forces GCC/Clang to
// keep the symbol local to whichever .so actually instantiates it,
// regardless of the translation unit's own -fvisibility flag -- restoring
// the "not shared with any other plugin" guarantee this header already
// documents (see the file header comment above) but didn't actually enforce.
#if defined(__GNUC__) || defined(__clang__)
#define SH_METAMOD_OVERRIDE_HIDDEN __attribute__((visibility("hidden")))
#else
#define SH_METAMOD_OVERRIDE_HIDDEN
#endif

SH_METAMOD_OVERRIDE_HIDDEN inline SourceHook::Impl::CSourceHookImpl g_SourceHookImpl;
SH_METAMOD_OVERRIDE_HIDDEN inline SourceHook::ISourceHook *g_pSourceHook = &g_SourceHookImpl;
SH_METAMOD_OVERRIDE_HIDDEN inline SourceHook::Plugin g_iSourceHookPluginId = 0;

#undef SH_METAMOD_OVERRIDE_HIDDEN

#undef SH_GLOB_SHPTR
#define SH_GLOB_SHPTR g_pSourceHook
#undef SH_GLOB_PLUGPTR
#define SH_GLOB_PLUGPTR g_iSourceHookPluginId

// Call right after PLUGIN_SAVEVARS() in Load(), same spirit/placement --
// see the usage example above.
#define SH_METAMOD_OVERRIDE_SAVEVARS(id) \
	g_iSourceHookPluginId = static_cast<SourceHook::Plugin>(id)

#endif //__SOURCEHOOK_METAMOD_OVERRIDE_H__
