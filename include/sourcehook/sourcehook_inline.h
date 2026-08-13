/* ======== SourceHook ========
* Copyright (C) 2004-2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

// Inline hooks: SourceHook's third hook category, for a raw, non-virtual
// function found at runtime (e.g. by a byte-signature scan) instead of a
// vtable slot. Declared/added/removed the same way typed and manual hooks
// already are, and handlers look exactly like a typed/manual hook handler
// -- a real function with the hooked function's own signature:
//
//   // free function, no `this` (thisclass = void):
//   SH_DECL_INLINEHOOK1(MyHook, void, int, CBaseEntity *);
//
//   int MyDetour(CBaseEntity *pEnt)
//   {
//       RETURN_SH_VALUE(MRES_SUPERCEDE, 1);
//   }
//
//   // non-virtual member function (thisclass = CBaseEntity): `this` isn't
//   // part of the declared parameter list, same as SH_DECL_HOOK/
//   // SH_DECL_MANUALHOOK -- get it via SH_IFACEPTR, same macro typed/manual
//   // (vtable) hooks already use:
//   SH_DECL_INLINEHOOK1(MyOtherHook, CBaseEntity, void, int);
//
//   void MyOtherDetour(int x)
//   {
//       CBaseEntity *pThis = SH_IFACEPTR(CBaseEntity);
//       RETURN_SH(MRES_IGNORED);
//   }
//
//   SH_ADD_INLINEHOOK(MyHook, targetAddr, SH_STATIC(MyDetour), false);
//   SH_ADD_INLINEHOOK(MyHook, targetAddr, SH_MEMBER(this, &MyClass::MyDetour), true);
//   SH_REMOVE_INLINEHOOK(MyHook, targetAddr, SH_STATIC(MyDetour), false);
//
// SH_IFACEPTR/RETURN_SH/RETURN_SH_VALUE/SET_SH_RESULT/SH_RESULT_ORIG_RET are
// the *same* macros typed/manual (vtable) hooks already use (sourcehook.h) --
// including this header makes them auto-detect whether an inline-hook frame
// is currently active and route accordingly, so a file mixing both hook
// styles (the common case -- see InventoryManager_mm_es) only ever writes
// one set of macros in a handler body. Only SH_DECL_*/SH_ADD_*/SH_REMOVE_*
// still differ per hook style, since that's inherent to how each one is
// installed/removed. Inline hooks are backed by their own, separate
// thread-local "current call" frame stack (below) rather than
// ISourceHook's internal hook-loop context -- that's what the detection
// above is actually switching on.
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
// templates: each declared (ThisClass, Ret, Args...) shape gets a small,
// fixed pool of distinct compiled trampoline functions
// (CInlineDispatcher::SlotTable(), below); one is claimed per hooked
// address and installed via safetyhook::InlineHook, so the calling
// convention is whatever the compiler already produces for that exact C++
// signature -- no hand-written register/stack marshaling. This was a
// deliberate scope decision: metamod-source's own x86_64 vtable hookmangen
// only implements its GCC/Linux register-marshaling paths as
// `static_assert(false, ...)` stubs today (MSVC-only), so hand-extending it
// for a second hook category on Linux would mean writing untested SysV-ABI
// JIT code from scratch. See test/testinlinehook.cpp for the behavior this
// is verified against.
//
// `this`, on the SysV x86_64 ABI this is scoped to, is just an implicit
// first argument at the machine-code level -- calling a non-virtual member
// function is byte-identical to calling a free function whose first
// parameter is the class pointer. `thisclass` in SH_DECL_INLINEHOOKn (void
// for none) is what tells the dispatcher to include that leading parameter
// in the real trampoline signature it hands to safetyhook; SH_IFACEPTR just
// reads it back out of the current call frame instead of exposing `this`
// as a fake extra entry in Args....
// There's no thisptroffs/adjustor support (same limitation InventoryManager
// already has today hooking member functions directly through raw
// safetyhook, not a regression).

#ifndef __SOURCEHOOK_INLINE_H__
#define __SOURCEHOOK_INLINE_H__

// SHINT_INLINE_CALLCONV: the calling convention every inline-hook trampoline/original-call
// is invoked with. Free functions and non-virtual member functions share the
// same ABI on the SysV x86_64 target this is scoped to (see file header), so
// plain cdecl is correct there; MSVC targets are not yet supported and would
// need this to become __fastcall/thiscall.
#ifndef SHINT_INLINE_CALLCONV
# if SH_XP == SH_XP_WINAPI
#  define SHINT_INLINE_CALLCONV __cdecl
# else
#  define SHINT_INLINE_CALLCONV
# endif
#endif

#include <cstdio>
#include <array>
#include <cstddef>
#include <mutex>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sourcehook/sourcehook.h"
#include "sourcehook/FastDelegate.h"
#include "sourcehook/sourcehook_impl_inline.h"
#include "safetyhook.hpp"

namespace SourceHook
{
	namespace Impl
	{
		// The "current inline hook call" frame: what SHINT_INLINE_IFACEPTR/SHINT_RETURN_INLINE/
		// SHINT_RETURN_INLINE_VALUE/SHINT_INLINE_ORIG_RET read and write. Type-erased (void*)
		// deliberately -- exactly like ISourceHook's own hook-loop context is
		// type-erased for typed/manual hooks -- so these macros don't need to
		// know or carry the declared (ThisClass, Ret, Args...) shape; the
		// handler function itself already knows its own real types and casts
		// accordingly, same as META_IFACEPTR(SomeClass) does today.
		struct CInlineCallFrame
		{
			void *this_ = nullptr;         // nullptr if this hook's thisclass is void
			void *override_ret = nullptr;  // -> a Ret-sized slot (nullptr if Ret == void)
			const void *orig_ret = nullptr; // -> a Ret-sized slot, valid only once the
			                                // original has actually been called (Post,
			                                // or a later Pre handler after an earlier
			                                // one already forced the call) -- nullptr
			                                // otherwise, same "don't call this before
			                                // it's meaningful" contract META_RESULT-style
			                                // orig-ret accessors already have.
			META_RES status = MRES_IGNORED;
			bool has_override = false;
		};

		// One stack per thread: same-thread reentrancy (a hook's own handler,
		// directly or indirectly, ends up re-entering the *same* hooked
		// function) just pushes a new frame, exactly like ISourceHook's own
		// HookLoopInfo stack handles reentrancy for vtable hooks.
		inline std::vector<CInlineCallFrame *> &InlineCallStack()
		{
			static thread_local std::vector<CInlineCallFrame *> s_Stack;
			return s_Stack;
		}

		inline CInlineCallFrame *CurrentInlineFrame()
		{
			auto &stack = InlineCallStack();
			SH_ASSERT(!stack.empty(), ("SHINT_INLINE_IFACEPTR/SHINT_RETURN_INLINE/SHINT_RETURN_INLINE_VALUE/SHINT_INLINE_ORIG_RET used outside of an inline hook handler"));
			return stack.empty() ? nullptr : stack.back();
		}

		struct CInlineFrameGuard
		{
			explicit CInlineFrameGuard(CInlineCallFrame &frame) { InlineCallStack().push_back(&frame); }
			~CInlineFrameGuard() { InlineCallStack().pop_back(); }
		};

		// Fixed pool of distinct, individually-addressable trampoline
		// instantiations per (ThisClass, Ret, Args...) shape. Each hooked
		// address claims exactly one slot for the lifetime of its dispatcher;
		// safetyhook needs a real, no-capture function pointer to detour to,
		// and this is what stands in for hand-JIT'ing one per address.
		template <typename ThisClass, typename Ret, typename... Args>
		class CInlineDispatcher;

		// Real ABI signature safetyhook actually detours: a leading
		// `ThisClass*` only when ThisClass isn't void.
		template <typename ThisClass, typename Ret, typename... Args>
		struct InlineRawFn
		{
			using Type = Ret(SHINT_INLINE_CALLCONV *)(ThisClass *, Args...);
		};

		template <typename Ret, typename... Args>
		struct InlineRawFn<void, Ret, Args...>
		{
			using Type = Ret(SHINT_INLINE_CALLCONV *)(Args...);
		};

		// Primary: has a `this` (thisclass != void).
		template <std::size_t N, typename ThisClass, typename Ret, typename... Args>
		struct InlineTrampolineSlot
		{
			static inline CInlineDispatcher<ThisClass, Ret, Args...> *s_Owner = nullptr;

			static Ret SHINT_INLINE_CALLCONV Fn(ThisClass *pThis, Args... args)
			{
				return s_Owner->RunChain(pThis, args...);
			}
		};

		// thisclass == void: no leading parameter at all.
		template <std::size_t N, typename Ret, typename... Args>
		struct InlineTrampolineSlot<N, void, Ret, Args...>
		{
			static inline CInlineDispatcher<void, Ret, Args...> *s_Owner = nullptr;

			static Ret SHINT_INLINE_CALLCONV Fn(Args... args)
			{
				return s_Owner->RunChain(nullptr, args...);
			}
		};

		template <typename ThisClass, typename Ret, typename... Args>
		class CInlineDispatcher
		{
		public:
			// Handlers have the hooked function's own real signature -- no
			// `this` in here regardless of ThisClass, same as a typed/manual
			// hook's generated Func() override; get `this` via SHINT_INLINE_IFACEPTR.
			using Handler = fastdelegate::FastDelegate<Ret, Args...>;

			static constexpr std::size_t kMaxConcurrentTargets = 64;
			using RawFn = typename InlineRawFn<ThisClass, Ret, Args...>::Type;

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
					return std::array<RawFn, sizeof...(Is)>{
						&InlineTrampolineSlot<Is, ThisClass, Ret, Args...>::Fn...
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
				(void)((result = fn(InlineTrampolineSlot<Is, ThisClass, Ret, Args...>::s_Owner, static_cast<int>(Is)), result != -2) || ...);
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

			// Calls `handler(args...)`. The handler communicates its META_RES
			// (via SHINT_RETURN_INLINE/SHINT_RETURN_INLINE_VALUE, or just SHINT_SET_INLINE_RESULT + a plain
			// return) and, for non-void Ret, its override value as side
			// effects on `frame` before returning -- the actual C++ return
			// value of a Ret-returning handler is *also* honored as the
			// override when the handler didn't call SHINT_RETURN_INLINE_VALUE itself
			// (see below), so a handler that just does `return x;` without
			// bothering with the macro still works, matching how permissive
			// typed-hook handlers already are today.
			void CallHandler(CInlineCallFrame &frame, Handler &handler, Args &...args)
			{
				if constexpr (std::is_void_v<Ret>)
				{
					handler(args...);
				}
				else
				{
					Ret r = handler(args...);
					if (!frame.has_override && frame.override_ret)
						*static_cast<Ret *>(frame.override_ret) = std::move(r);
				}
			}

			Ret RunChain(ThisClass *pThis, Args... args)
			{
				std::vector<Entry> preCopy, postCopy;
				{
					std::lock_guard<std::mutex> lock(m_Mutex);
					preCopy = m_Pre;
					postCopy = m_Post;
				}

				CInlineCallFrame frame;
				frame.this_ = pThis;

				if constexpr (std::is_void_v<Ret>)
				{
					CInlineFrameGuard guard(frame);

					for (auto &e : preCopy)
						CallHandler(frame, e.handler, args...);

					if (frame.status < MRES_SUPERCEDE)
						CallOriginal(pThis, args...);

					for (auto &e : postCopy)
						CallHandler(frame, e.handler, args...);
				}
				else
				{
					Ret overrideVal{};
					Ret origVal{};
					frame.override_ret = &overrideVal;

					CInlineFrameGuard guard(frame);

					for (auto &e : preCopy)
					{
						CallHandler(frame, e.handler, args...);
						if (frame.status >= MRES_SUPERCEDE)
							break;
					}

					if (frame.status < MRES_SUPERCEDE)
					{
						origVal = CallOriginal(pThis, args...);
						frame.orig_ret = &origVal;
					}

					for (auto &e : postCopy)
						CallHandler(frame, e.handler, args...);

					return (frame.status >= MRES_OVERRIDE) ? overrideVal : origVal;
				}
			}

			Ret CallOriginal(ThisClass *pThis, Args... args)
			{
				if constexpr (std::is_void_v<ThisClass>)
					return m_Hook.template original<Ret(SHINT_INLINE_CALLCONV *)(Args...)>()(args...);
				else
					return m_Hook.template original<Ret(SHINT_INLINE_CALLCONV *)(ThisClass *, Args...)>()(pThis, args...);
			}

			template <std::size_t N, typename T2, typename R2, typename... A2>
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

// SHINT_INLINE_IFACEPTR(type): `this` of the function currently being inline-hooked,
// cast to `type*`. Only meaningful if the hook was declared with a non-void
// thisclass; nullptr otherwise. Mirrors META_IFACEPTR's ergonomics for
// typed/manual (vtable) hooks.
#define SHINT_INLINE_IFACEPTR(type) (reinterpret_cast<type *>(::SourceHook::Impl::CurrentInlineFrame()->this_))

// SHINT_INLINE_ORIG_RET(type): the original function's return value, cast to `type`.
// Only valid once the original has actually run -- i.e. in a Post handler,
// or in a Pre handler that runs after an earlier Pre handler already forced
// the call via SHINT_RETURN_INLINE(MRES_HANDLED) or lower. Guard with
// SH_INLINE_ORIG_CALLED if a Post handler might run after an *earlier*
// handler already SUPERCEDEd the call (in which case the original never ran
// and there's nothing to read).
#define SHINT_INLINE_ORIG_RET(type) (*reinterpret_cast<const type *>(::SourceHook::Impl::CurrentInlineFrame()->orig_ret))

// SH_INLINE_ORIG_CALLED: true once the original function has actually been
// invoked for the current call. False in a Post handler when some earlier
// Pre handler already SUPERCEDEd the call -- SHINT_INLINE_ORIG_RET is not safe
// to read in that case.
#define SH_INLINE_ORIG_CALLED() (::SourceHook::Impl::CurrentInlineFrame()->orig_ret != nullptr)

// SHINT_INLINE_RESULT: the highest META_RES set so far for the current call
// (by this handler or any earlier one). Mirrors META_RESULT_STATUS.
#define SHINT_INLINE_RESULT() (::SourceHook::Impl::CurrentInlineFrame()->status)

#define SHINT_SET_INLINE_RESULT(result) \
	do { \
		auto *__shFrame = ::SourceHook::Impl::CurrentInlineFrame(); \
		if ((result) > __shFrame->status) \
			__shFrame->status = (result); \
	} while (0)

// SHINT_RETURN_INLINE(result): for void-returning inline hook handlers.
#define SHINT_RETURN_INLINE(result) \
	do { SHINT_SET_INLINE_RESULT(result); return; } while (0)

// SHINT_RETURN_INLINE_VALUE(result, value): for non-void inline hook handlers. `value`
// becomes the override return value whenever `result` is MRES_OVERRIDE or
// MRES_SUPERCEDE (same threshold as typed/manual hooks).
#define SHINT_RETURN_INLINE_VALUE(result, value) \
	do { \
		auto *__shFrame = ::SourceHook::Impl::CurrentInlineFrame(); \
		SHINT_SET_INLINE_RESULT(result); \
		if ((result) >= MRES_OVERRIDE && __shFrame->override_ret) \
		{ \
			using __ShRetT = std::decay_t<decltype(value)>; \
			*static_cast<__ShRetT *>(__shFrame->override_ret) = (value); \
			__shFrame->has_override = true; \
		} \
		return (value); \
	} while (0)

// SH_DECL_INLINEHOOK0..N (the "declare" half) already came in transitively
// via the "sourcehook.h" include above -- it lives there now, alongside
// SH_DECL_HOOK/SH_DECL_MANUALHOOK, so opening sourcehook.h shows all three
// hook styles' declare macros in one place. This file only needs to supply
// the real CInlineDispatcher implementation (above) and the "add"/"remove"
// halves below, which do need safetyhook.hpp.

// One macro surface for the handler body, regardless of hook style. Once
// this header is included, SH_IFACEPTR/RETURN_SH/RETURN_SH_VALUE/
// SET_SH_RESULT/SH_RESULT_ORIG_RET each check whether an inline-hook frame
// is currently active (SourceHook::Impl::InlineCallStack() non-empty) and
// route to the inline implementation above if so, or fall back to their
// original sourcehook.h behavior (SH_GLOB_SHPTR's own hook-loop context)
// otherwise -- so a file with both typed/manual *and* inline hooks (like
// most real plugins) only ever writes one set of macros; the only place
// hook style is still spelled out is SH_DECL_*/SH_ADD_*/SH_REMOVE_*.
#undef SH_IFACEPTR
#define SH_IFACEPTR(type) \
	(::SourceHook::Impl::InlineCallStack().empty() \
		? reinterpret_cast<type *>(SH_GLOB_SHPTR->GetIfacePtr()) \
		: SHINT_INLINE_IFACEPTR(type))

#undef SET_SH_RESULT
#define SET_SH_RESULT(result) \
	do { \
		if (!::SourceHook::Impl::InlineCallStack().empty()) \
			SHINT_SET_INLINE_RESULT(result); \
		else \
			SH_GLOB_SHPTR->SetRes(result); \
	} while (0)

#undef RETURN_SH
#define RETURN_SH(result) \
	do { \
		if (!::SourceHook::Impl::InlineCallStack().empty()) \
			SHINT_RETURN_INLINE(result); \
		SET_SH_RESULT(result); \
		return; \
	} while (0)

#undef RETURN_SH_VALUE
#define RETURN_SH_VALUE(result, value) \
	do { \
		if (!::SourceHook::Impl::InlineCallStack().empty()) \
			SHINT_RETURN_INLINE_VALUE(result, value); \
		SET_SH_RESULT(result); \
		return (value); \
	} while (0)

#undef SH_RESULT_ORIG_RET
#define SH_RESULT_ORIG_RET(type) \
	(::SourceHook::Impl::InlineCallStack().empty() \
		? *SourceHook::MacroRefHelpers<type>::GetOrigRet(SH_GLOB_SHPTR) \
		: SHINT_INLINE_ORIG_RET(type))

#define SH_ADD_INLINEHOOK(hookname, targetAddr, handler, post) \
	::SourceHook::Impl::AddInlineHook<SourceHookInlineDecl::hookname::Dispatcher>((targetAddr), (handler), (post))

#define SH_REMOVE_INLINEHOOK(hookname, targetAddr, handler, post) \
	::SourceHook::Impl::RemoveInlineHook<SourceHookInlineDecl::hookname::Dispatcher>((targetAddr), (handler), (post))

#endif //__SOURCEHOOK_INLINE_H__
