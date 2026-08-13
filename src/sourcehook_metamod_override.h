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
//   // ... SH_ADD_HOOK/SH_ADD_MANUALHOOK/SH_ADD_INLINEHOOK from here on all
//   // go through g_pSourceHook, not metamod's g_SHPtr.
//
// Must be included AFTER sourcehook.h (directly or transitively, e.g. via
// ISmmPlugin.h) has already defined the default SH_GLOB_SHPTR/
// SH_GLOB_PLUGPTR at least once -- the #undef below needs something to
// undefine -- and BEFORE any SH_DECL_HOOK*/SH_ADD_*HOOK/SH_DECL_INLINEHOOK*
// call you want routed through the private instance, since these macros
// resolve SH_GLOB_SHPTR/SH_GLOB_PLUGPTR at the point they're expanded.
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

#include "sourcehook_impl.h"

inline SourceHook::Impl::CSourceHookImpl g_SourceHookImpl;
inline SourceHook::ISourceHook *g_pSourceHook = &g_SourceHookImpl;
inline SourceHook::Plugin g_iSourceHookPluginId = 0;

#undef SH_GLOB_SHPTR
#define SH_GLOB_SHPTR g_pSourceHook
#undef SH_GLOB_PLUGPTR
#define SH_GLOB_PLUGPTR g_iSourceHookPluginId

#endif //__SOURCEHOOK_METAMOD_OVERRIDE_H__
