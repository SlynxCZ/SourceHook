/* ======== SourceHook ========
* Copyright (C) 2004-2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

// Inline-hook dispatch: the "third" hook category alongside typed (SH_DECL_HOOK)
// and manual (SH_DECL_MANUALHOOK) vtable hooks. Where those two patch a vtable
// slot, an inline hook detours a raw, non-virtual function's own prologue
// (found by e.g. a byte signature scan) via safetyhook::InlineHook.
//
// The one thing every inline-hook consumer needs that a single per-plugin
// safetyhook::InlineHook doesn't give them for free: if two *different*
// plugins independently scan for the same function and both land on the same
// address, they must not each install their own detour on it -- the second
// safetyhook::InlineHook::create() would either fail outright or silently
// corrupt the first plugin's trampoline chain. CInlineHookAddressGuard is the
// (tiny, type-erased) piece that prevents that: it's a global address ->
// "who owns this, with what declared signature" map that every
// SourceHook::Impl::CInlineDispatcher<Ret,Args...> (sourcehook_inline.h)
// consults before creating its own per-signature dispatcher for an address,
// so N plugins hooking the same address with the *same* declared signature
// share one CInlineDispatcher (and therefore one real safetyhook::InlineHook),
// while a second plugin that declared an *incompatible* signature for the
// same address is rejected instead of silently corrupting the first one.

#ifndef __SOURCEHOOK_IMPL_INLINE_H__
#define __SOURCEHOOK_IMPL_INLINE_H__

#include <cstddef>
#include <mutex>
#include <typeinfo>
#include <unordered_map>

namespace SourceHook
{
	namespace Impl
	{
		class CInlineHookAddressGuard
		{
		public:
			static CInlineHookAddressGuard &Get();

			// Returns true and claims `addr` for `sig` if the address is either
			// unclaimed, or already claimed by an *equal* type_info (i.e. some
			// plugin already registered the exact same Ret/Args... shape here --
			// this is the "N plugins, 1 signature, 1 dispatcher" case).
			// Returns false (and claims nothing) if `addr` is already claimed by
			// a *different* signature -- a genuine usage error the caller should
			// surface loudly rather than silently double-detour.
			bool Claim(void *addr, const std::type_info &sig);

			// Called once a signature's dispatcher for `addr` has no more
			// registered handlers and has torn down its safetyhook::InlineHook.
			void Release(void *addr);

			// Test/debug helper: how many distinct addresses are currently
			// inline-hooked at all (regardless of signature or handler count).
			std::size_t TargetCount() const;

		private:
			struct Entry
			{
				const std::type_info *sig;
				int refcount;
			};

			mutable std::mutex m_Mutex;
			std::unordered_map<void *, Entry> m_Claims;
		};
	}
}

#endif //__SOURCEHOOK_IMPL_INLINE_H__
