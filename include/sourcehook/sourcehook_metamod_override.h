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

inline SourceHook::Impl::CSourceHookImpl g_SourceHookImpl;
inline SourceHook::ISourceHook *g_pSourceHook = &g_SourceHookImpl;
inline SourceHook::Plugin g_iSourceHookPluginId = 0;

#undef SH_GLOB_SHPTR
#define SH_GLOB_SHPTR g_pSourceHook
#undef SH_GLOB_PLUGPTR
#define SH_GLOB_PLUGPTR g_iSourceHookPluginId

// Call right after PLUGIN_SAVEVARS() in Load(), same spirit/placement --
// see the usage example above.
#define SH_METAMOD_OVERRIDE_SAVEVARS(id) \
	g_iSourceHookPluginId = static_cast<SourceHook::Plugin>(id)

#endif //__SOURCEHOOK_METAMOD_OVERRIDE_H__
