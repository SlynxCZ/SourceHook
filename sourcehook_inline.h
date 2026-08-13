/* ======== SourceHook ========
* Copyright (C) 2026 Michal Přikryl (Slynx) / (˙·٠● S l y n x ●٠·˙) -- inline hook dispatch
* No warranties of any kind
*
* License: zlib/libpng
* ============================
*/

// Inline hooks: SourceHook's third hook category, for a raw, non-virtual
// function found at runtime (e.g. by a byte-signature scan) instead of a
// vtable slot. Declared/added/removed the same way typed and manual hooks
// already are:
//
//   SH_DECL_INLINEHOOK1(MyHook, int, CBaseEntity *);
//
//   META_RES MyDetour(SourceHook::InlineHookContext<int, CBaseEntity *> &ctx)
//   {
//       CBaseEntity *pEnt = ctx.Arg<0>();
//       ctx.SetOverrideRet(1);
//       return MRES_SUPERCEDE;
//   }
//
//   SH_ADD_INLINEHOOK(MyHook, targetAddr, SH_STATIC(MyDetour), false);
//   SH_ADD_INLINEHOOK(MyHook, targetAddr, SH_MEMBER(this, &MyClass::MyDetour), true);
//   SH_REMOVE_INLINEHOOK(MyHook, targetAddr, SH_STATIC(MyDetour), false);
//
// If two different SH_ADD_INLINEHOOK calls (from the same plugin or two
// different ones) resolve to the *same* targetAddr with the *same* declared
// signature, they share one real safetyhook::InlineHook and one dispatcher --
// only the first caller for a given address actually detours anything; every
// later caller just joins that dispatcher's handler chain. A second caller
// that resolved the same address but declared an incompatible signature is
// rejected (SH_ADD_INLINEHOOK returns 0) instead of silently double-detouring
// the same bytes.
//
// Implementation note: unlike the typed/manual vtable hookmangen (see
// sourcehook_hookmangen_x86_64.cpp), which hand-generates machine code for
// each hooked prototype, the inline dispatcher below is a set of C++
// templates: each declared (Ret, Args...) shape gets a small, fixed pool of
// distinct compiled trampoline functions (CInlineDispatcher::SlotTable(),
// below); one is claimed per hooked address and installed via
// safetyhook::InlineHook, so the calling convention is whatever the compiler
// already produces for that exact C++ signature -- no hand-written
// register/stack marshaling.
// This was a deliberate scope decision: metamod-source's own x86_64 vtable
// hookmangen only implements its GCC/Linux register-marshaling paths as
// `static_assert(false, ...)` stubs today (MSVC-only), so hand-extending it
// for a second hook category on Linux would mean writing untested SysV-ABI
// JIT code from scratch. See test/testinlinehook.cpp for the behavior this
// is verified against.

#ifndef __SOURCEHOOK_INLINE_H__
#define __SOURCEHOOK_INLINE_H__

// SHINT_INLINE_CALLCONV: the calling convention every inline-hook trampoline/original-call
// is invoked with. Free functions and non-virtual member functions share the
// same ABI on the SysV x86_64 target this is scoped to (see file header), so
// plain cdecl is correct there; MSVC targets are not yet supported (see
// makefiles/README notes) and would need this to become __fastcall/thiscall.
#ifndef SHINT_INLINE_CALLCONV
# if SH_XP == SH_XP_WINAPI
#  define SHINT_INLINE_CALLCONV __cdecl
# else
#  define SHINT_INLINE_CALLCONV
# endif
#endif

#include <array>
#include <cstddef>
#include <mutex>
#include <tuple>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sourcehook.h"
#include "FastDelegate.h"
#include "sourcehook_impl_inline.h"
#include "safetyhook.hpp"

namespace SourceHook
{
	// Per-call context handed to every registered inline-hook handler. Bundles
	// the hooked function's real arguments (by reference into the caller's own
	// stack -- handlers may freely read/modify them before the original runs)
	// plus the override/orig-return machinery every SH hook already has.
	template <typename Ret, typename... Args>
	class InlineHookContext
	{
	public:
		explicit InlineHookContext(Args &...args) : m_Args(args...) {}

		template <std::size_t I>
		decltype(auto) Arg() { return std::get<I>(m_Args); }

		void SetOverrideRet(Ret value) { m_OverrideRet = std::move(value); m_HasOverride = true; }
		bool HasOverrideRet() const { return m_HasOverride; }
		const Ret &GetOverrideRet() const { return m_OverrideRet; }

		void SetOrigRet(Ret value) { m_OrigRet = std::move(value); }
		const Ret &GetOrigRet() const { return m_OrigRet; }

	private:
		std::tuple<Args &...> m_Args;
		Ret m_OverrideRet{};
		Ret m_OrigRet{};
		bool m_HasOverride = false;
	};

	template <typename... Args>
	class InlineHookContext<void, Args...>
	{
	public:
		explicit InlineHookContext(Args &...args) : m_Args(args...) {}

		template <std::size_t I>
		decltype(auto) Arg() { return std::get<I>(m_Args); }

	private:
		std::tuple<Args &...> m_Args;
	};

	namespace Impl
	{
		// Fixed pool of distinct, individually-addressable trampoline
		// instantiations per (Ret, Args...) shape. Each hooked address claims
		// exactly one slot for the lifetime of its dispatcher; safetyhook
		// needs a real, no-capture function pointer to detour to, and this is
		// what stands in for hand-JIT'ing one per address.
		template <typename Ret, typename... Args>
		class CInlineDispatcher;

		template <std::size_t N, typename Ret, typename... Args>
		struct InlineTrampolineSlot
		{
			static inline CInlineDispatcher<Ret, Args...> *s_Owner = nullptr;

			static Ret SHINT_INLINE_CALLCONV Fn(Args... args)
			{
				return s_Owner->RunChain(args...);
			}
		};

		template <typename Ret, typename... Args>
		class CInlineDispatcher
		{
		public:
			using Context = ::SourceHook::InlineHookContext<Ret, Args...>;
			using Handler = fastdelegate::FastDelegate<META_RES, Context &>;

			static constexpr std::size_t kMaxConcurrentTargets = 64;

			// Returns the shared dispatcher for `addr`, creating (and
			// installing the real safetyhook detour for) one if this is the
			// first caller for this address. Returns nullptr if `addr` is
			// already owned by an incompatible declared signature, the slot
			// pool for this signature is exhausted, or safetyhook failed to
			// create the detour.
			static CInlineDispatcher *GetOrCreate(void *addr)
			{
				std::lock_guard<std::mutex> lock(Map_Mutex());

				auto &map = Map();
				auto it = map.find(addr);
				if (it != map.end())
					return it->second;

				if (!CInlineHookAddressGuard::Get().Claim(addr, typeid(CInlineDispatcher)))
					return nullptr;

				auto *disp = new CInlineDispatcher();
				if (!disp->Install(addr))
				{
					CInlineHookAddressGuard::Get().Release(addr);
					delete disp;
					return nullptr;
				}

				map.emplace(addr, disp);
				return disp;
			}

			int AddHook(Handler handler, bool post)
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				int id = m_NextId++;
				(post ? m_Post : m_Pre).push_back(Entry{ id, handler });
				return id;
			}

			// Removes the first handler matching `handler`/`post`. Returns
			// true if a handler was removed. If this was the last handler for
			// this address, the dispatcher (and its safetyhook detour) is torn
			// down; do not use the dispatcher pointer again after this call
			// returns true and HandlerCount() was 1 beforehand.
			static bool RemoveHook(void *addr, Handler handler, bool post)
			{
				std::lock_guard<std::mutex> lock(Map_Mutex());
				auto &map = Map();
				auto it = map.find(addr);
				if (it == map.end())
					return false;

				CInlineDispatcher *disp = it->second;
				bool removed = disp->RemoveHookLocked(handler, post);
				if (removed && disp->EmptyLocked())
				{
					map.erase(it);
					disp->Uninstall();
					CInlineHookAddressGuard::Get().Release(addr);
					delete disp;
				}
				return removed;
			}

			std::size_t HandlerCount() const
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				return m_Pre.size() + m_Post.size();
			}

			void *GetTargetAddr() const { return m_TargetAddr; }

		private:
			struct Entry
			{
				int id;
				Handler handler;
			};

			CInlineDispatcher() = default;

			static std::unordered_map<void *, CInlineDispatcher *> &Map()
			{
				static std::unordered_map<void *, CInlineDispatcher *> s_Map;
				return s_Map;
			}

			static std::mutex &Map_Mutex()
			{
				static std::mutex s_Mutex;
				return s_Mutex;
			}

			static auto &SlotTable()
			{
				static auto s_Table = []<std::size_t... Is>(std::index_sequence<Is...>) {
					return std::array<Ret(SHINT_INLINE_CALLCONV *)(Args...), sizeof...(Is)>{
						&InlineTrampolineSlot<Is, Ret, Args...>::Fn...
					};
				}(std::make_index_sequence<kMaxConcurrentTargets>{});
				return s_Table;
			}

			bool Install(void *addr)
			{
				int slot = ClaimSlot();
				if (slot < 0)
					return false;

				auto result = safetyhook::InlineHook::create(addr, SlotTable()[static_cast<std::size_t>(slot)]);
				if (!result)
				{
					FreeSlot(slot);
					return false;
				}

				m_Hook = std::move(result.value());
				m_TargetAddr = addr;
				m_Slot = slot;
				return true;
			}

			void Uninstall()
			{
				m_Hook = {};
				if (m_Slot >= 0)
					FreeSlot(m_Slot);
			}

			int ClaimSlot()
			{
				return ForEachSlot([this](auto &owner, int i) -> int {
					if (owner == nullptr)
					{
						owner = this;
						return i;
					}
					return -2; // keep looking
				});
			}

			void FreeSlot(int slot)
			{
				ForEachSlot([slot](auto &owner, int i) -> int {
					if (i == slot)
						owner = nullptr;
					return -2;
				});
			}

			// Runs `fn(ownerRef, index)` for every slot until it returns
			// something other than -2, and returns that value (or -1 if every
			// slot was visited without a match).
			template <typename Fn>
			static int ForEachSlot(Fn &&fn)
			{
				return ForEachSlotImpl(std::forward<Fn>(fn), std::make_index_sequence<kMaxConcurrentTargets>{});
			}

			template <typename Fn, std::size_t... Is>
			static int ForEachSlotImpl(Fn &&fn, std::index_sequence<Is...>)
			{
				int result = -1;
				(void)((result = fn(InlineTrampolineSlot<Is, Ret, Args...>::s_Owner, static_cast<int>(Is)), result != -2) || ...);
				return result == -2 ? -1 : result;
			}

			bool RemoveHookLocked(Handler handler, bool post)
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				auto &list = post ? m_Post : m_Pre;
				for (auto it = list.begin(); it != list.end(); ++it)
				{
					if (it->handler == handler)
					{
						list.erase(it);
						return true;
					}
				}
				return false;
			}

			bool EmptyLocked() const
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				return m_Pre.empty() && m_Post.empty();
			}

			Ret RunChain(Args... args)
			{
				Context ctx(args...);

				std::vector<Entry> preCopy, postCopy;
				{
					std::lock_guard<std::mutex> lock(m_Mutex);
					preCopy = m_Pre;
					postCopy = m_Post;
				}

				META_RES status = MRES_IGNORED;
				for (auto &e : preCopy)
				{
					META_RES r = e.handler(ctx);
					if (r > status) status = r;
				}

				if constexpr (std::is_void_v<Ret>)
				{
					if (status < MRES_SUPERCEDE)
						CallOriginal(args...);

					for (auto &e : postCopy)
					{
						META_RES r = e.handler(ctx);
						if (r > status) status = r;
					}
					return;
				}
				else
				{
					if (status < MRES_SUPERCEDE)
						ctx.SetOrigRet(CallOriginal(args...));

					for (auto &e : postCopy)
					{
						META_RES r = e.handler(ctx);
						if (r > status) status = r;
					}

					return (status >= MRES_OVERRIDE) ? ctx.GetOverrideRet() : ctx.GetOrigRet();
				}
			}

			Ret CallOriginal(Args... args)
			{
				return m_Hook.template original<Ret(SHINT_INLINE_CALLCONV *)(Args...)>()(args...);
			}

			template <std::size_t N, typename R2, typename... A2>
			friend struct InlineTrampolineSlot;

			void *m_TargetAddr = nullptr;
			safetyhook::InlineHook m_Hook;
			int m_Slot = -1;
			int m_NextId = 1;
			std::vector<Entry> m_Pre;
			std::vector<Entry> m_Post;
			mutable std::mutex m_Mutex;
		};

		template <typename Dispatcher>
		int AddInlineHook(void *addr, typename Dispatcher::Handler handler, bool post)
		{
			Dispatcher *disp = Dispatcher::GetOrCreate(addr);
			if (!disp)
				return 0;
			return disp->AddHook(handler, post);
		}

		template <typename Dispatcher>
		bool RemoveInlineHook(void *addr, typename Dispatcher::Handler handler, bool post)
		{
			return Dispatcher::RemoveHook(addr, handler, post);
		}
	}
}

#include "generate/sourcehook_inline_decl.h"

#define SH_ADD_INLINEHOOK(hookname, targetAddr, handler, post) \
	::SourceHook::Impl::AddInlineHook<SourceHookInlineDecl::hookname::Dispatcher>((targetAddr), (handler), (post))

#define SH_REMOVE_INLINEHOOK(hookname, targetAddr, handler, post) \
	::SourceHook::Impl::RemoveInlineHook<SourceHookInlineDecl::hookname::Dispatcher>((targetAddr), (handler), (post))

#endif //__SOURCEHOOK_INLINE_H__
