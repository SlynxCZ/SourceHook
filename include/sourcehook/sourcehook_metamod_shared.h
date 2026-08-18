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
// This is a purely RUNTIME join -- no CMake link dependency between the two
// plugins needed. g_pSourceHook starts out null; SH_ADD_HOOK/SH_ADD_INLINEHOOK/
// SH_CALL/etc. are unsafe until SH_METAMOD_SHARED_BIND(...) actually runs
// (typically as soon as the owning plugin's own ICoreInterface -- or
// whatever the joining plugin uses to reach it -- becomes queryable via
// g_SMAPI->MetaFactory; see JailBreak.cpp's TryBindSharedSourceHook() for a
// worked example, including why it has to be retried both from Load() and
// from IMetamodListener::OnPluginLoad(), since metamod doesn't guarantee any
// particular plugin load order).
//
// For VIRTUAL/vtable hooks (SH_DECL_HOOK*), binding g_pSourceHook this way
// is already the whole story -- SH_ADD_HOOK/SH_CALL go through
// SH_GLOB_SHPTR's own ISourceHook interface directly, so an ordinary virtual
// call correctly reaches the owning plugin's own compiled engine code the
// moment the pointer is bound, regardless of which plugin's .so made the
// call. For SH_DECL_INLINEHOOK*-style hooks, that alone used to NOT be
// enough -- CInlineDispatcher<ThisClass,Ret,Args...> (sourcehook_inline.h)
// used to keep its own hook table as `inline static` DATA MEMBERS, private
// to whichever .so instantiated that exact template, entirely bypassing
// SH_GLOB_SHPTR. That's fixed now: CInlineDispatcher's storage (and
// CInlineHookAddressGuard's) is fetched via
// ISourceHook::GetOrCreateInlineDispatcherStorage -- an ordinary virtual
// call through this SAME g_pSourceHook -- so binding it here is sufficient
// for both hook styles today. See ISourceHook::GetOrCreateInlineDispatcherStorage's
// own comment (sourcehook.h) for the full reasoning, and e.g.
// JailBreak/src/hooks/Hooks.h's own comment on TerminateRoundHook for the
// one remaining caveat that mechanism has (a shared hook's declared
// signature must be built-in-typed/type-erased, not reference any
// per-plugin-namespaced schema type, or the two plugins' own mangled type
// names -- what the registry is keyed by -- won't actually match).
//
//   #include <ISmmPlugin.h>
//   #include "sourcehook_metamod_shared.h"
//
//   bool MyPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
//   {
//       PLUGIN_SAVEVARS();
//       SH_METAMOD_SHARED_DECLARE(id);   // <-- right after, same spot as PLUGIN_SAVEVARS()
//       ...
//       TryBindSharedEngine();           // does SH_METAMOD_SHARED_BIND(...) once reachable
//   }
//
// The owning plugin itself keeps using sourcehook_metamod_override.h
// completely unchanged -- it doesn't need to know anyone is joining it.

#ifndef __SOURCEHOOK_METAMOD_SHARED_H__
#define __SOURCEHOOK_METAMOD_SHARED_H__

// Bare, not "sourcehook/sourcehook.h" -- see sourcehook_metamod_override.h's
// own identical comment on this include for why (this file lives in the
// same include/sourcehook/ directory, so a bare include resolves to our own
// sibling file with no ambiguity against metamod-source's own
// ISmmPlugin.h-adjacent copy).
#include "sourcehook.h"

inline SourceHook::ISourceHook *g_pSourceHook = nullptr;
inline SourceHook::Plugin g_iSharedSourceHookPluginId = 0;

#undef SH_GLOB_SHPTR
#define SH_GLOB_SHPTR g_pSourceHook
#undef SH_GLOB_PLUGPTR
#define SH_GLOB_PLUGPTR g_iSharedSourceHookPluginId

// Call right after PLUGIN_SAVEVARS() in Load(), same spirit/placement as
// SH_METAMOD_OVERRIDE_SAVEVARS(id) -- see the usage example above.
#define SH_METAMOD_SHARED_DECLARE(id) \
	g_iSharedSourceHookPluginId = static_cast<SourceHook::Plugin>(id)

// Call once the owning plugin's ISourceHook* is actually in hand (e.g. via
// its own ICoreInterface accessor). Every SH_ADD_HOOK/SH_ADD_INLINEHOOK/
// SH_CALL/etc. before this has run is unsafe (SH_GLOB_SHPTR is still null).
#define SH_METAMOD_SHARED_BIND(ptr) \
	g_pSourceHook = (ptr)

#endif //__SOURCEHOOK_METAMOD_SHARED_H__
