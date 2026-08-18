/* ======== SourceHook ========
* Copyright (C) 2004-2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

// Counterpart to sourcehook_metamod_override.h, for a plugin that wants to
// JOIN an existing private SourceHook engine another plugin already owns,
// instead of creating its own separate one.
//
// sourcehook_metamod_override.h's own "Caveat" spells out exactly why this
// exists: two independent private engines each patch a shared target
// address independently -- SH_CALL (and SH_GET_INLINEHOOK_ORIGINAL) from
// one engine can never see, let alone bypass, a Pre/Post handler chain
// registered through a *different* engine, even if both happen to be
// hooking the exact same function. If plugin A's hook needs to be
// reliably bypassable (e.g. an admin command that must always take
// effect, ignoring every other plugin's own veto logic on that same
// function) by a call originating in plugin B, A and B need to actually
// be the same engine instance, not just the same SourceHook version.
//
//   #include <ISmmPlugin.h>
//   #include "sourcehook_metamod_shared.h"
//
//   bool MyPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
//   {
//       PLUGIN_SAVEVARS();
//       SH_METAMOD_SHARED_DECLARE(id);   // <-- right after, same spot as PLUGIN_SAVEVARS()
//       ...
//   }
//
//   // Later, once you've actually obtained the owning plugin's
//   // SourceHook::ISourceHook* (however that plugin exposes it -- e.g. a
//   // CoreAPI accessor, looked up via g_SMAPI->MetaFactory once that
//   // plugin has actually loaded):
//   SH_METAMOD_SHARED_BIND(theOwningPluginsPointer);
//
//   // SH_ADD_HOOK/SH_ADD_MANUALHOOK/SH_ADD_INLINEHOOK/SH_CALL/
//   // SH_GET_INLINEHOOK_ORIGINAL are only safe to use AFTER this --
//   // SH_GLOB_SHPTR is null until SH_METAMOD_SHARED_BIND actually runs.
//
// The owning plugin itself keeps using sourcehook_metamod_override.h
// completely unchanged -- it never needs to know anyone else is joining
// it, and nothing here requires the owner to load before or after any
// particular joiner (SH_METAMOD_SHARED_BIND just needs to run at some
// point before this plugin's own first SH_ADD_*HOOK call; how a joiner
// discovers "the owner has loaded, here's its pointer" -- e.g. an
// immediate lookup attempt plus an IMetamodListener::OnPluginLoad retry,
// to work regardless of which of the two plugins metamod happens to load
// first -- is entirely up to the joiner, this header doesn't care).

#ifndef __SOURCEHOOK_METAMOD_SHARED_H__
#define __SOURCEHOOK_METAMOD_SHARED_H__

// Bare, not "sourcehook/sourcehook_impl.h" -- see
// sourcehook_metamod_override.h's own identical comment on this include for
// why (this file lives in the same include/sourcehook/ directory, so a bare
// include resolves to our own sibling file with no ambiguity against
// metamod-source's own ISmmPlugin.h-adjacent copy).
#include "sourcehook_impl.h"

inline SourceHook::ISourceHook *g_pSharedSourceHook = nullptr;
inline SourceHook::Plugin g_iSharedSourceHookPluginId = 0;

#undef SH_GLOB_SHPTR
#define SH_GLOB_SHPTR g_pSharedSourceHook
#undef SH_GLOB_PLUGPTR
#define SH_GLOB_PLUGPTR g_iSharedSourceHookPluginId

// Call right after PLUGIN_SAVEVARS() in Load(), same spirit/placement as
// SH_METAMOD_OVERRIDE_SAVEVARS(id) -- see the usage example above. Only sets
// this plugin's own id (mirrors what PLUGIN_SAVEVARS() does for g_PLID);
// SH_GLOB_SHPTR itself stays null until SH_METAMOD_SHARED_BIND actually
// runs.
#define SH_METAMOD_SHARED_DECLARE(id) \
	g_iSharedSourceHookPluginId = static_cast<SourceHook::Plugin>(id)

// ptr: the owning plugin's SourceHook::ISourceHook*. Safe to call more than
// once (e.g. a retry loop) -- always just overwrites with the latest value.
#define SH_METAMOD_SHARED_BIND(ptr) \
	(g_pSharedSourceHook = (ptr))

#endif //__SOURCEHOOK_METAMOD_SHARED_H__
