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
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sourcehook.h"
#include "FastDelegate.h"
#include "sourcehook_impl_cinline.h"
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
			void *target_addr = nullptr;   // the hooked address itself -- what
			                                // SHINT_RETURN_INLINE_(VALUE_)NEWPARAMS
			                                // (below) needs to reach back into
			                                // SH_CALL's own machinery without the
			                                // caller having to repeat targetAddr.
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

			// Read-only lookup for SH_CALL: unlike GetOrCreate(), never
			// installs anything -- returns nullptr if nothing is currently
			// hooked at `addr` for this declared signature. That's the
			// correct answer for SH_CALL too: if nothing's hooked there, the
			// bytes at `addr` already *are* the untouched original.
			static CInlineDispatcher *Find(void *addr)
			{
				std::lock_guard<std::mutex> lock(Map_Mutex());
				auto &map = Map();
				auto it = map.find(addr);
				return it != map.end() ? it->second : nullptr;
			}

			int AddHook(Handler handler, bool post)
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				int id = m_NextId++;
				(post ? m_Post : m_Pre).push_back(Entry{ id, handler });
				return id;
			}

			// Removes the first handler matching `handler`/`post`. Returns
			// true if a handler was removed.
			//
			// Deliberately does NOT tear down the safetyhook detour (or free
			// the dispatcher) when the handler list becomes empty -- same
			// choice KHook itself makes (see DetourCapsule::RemoveHook: the
			// capsule and its JIT dispatcher live for the process' lifetime
			// once created; an empty callback list just makes it a transparent
			// passthrough straight to the original, which RunChain() already
			// does for free when preCopy/postCopy are both empty). A *real*
			// unpatch-on-last-remove is fundamentally at odds with safe
			// concurrent dispatch: some other thread (or, reentrantly, this
			// same thread from inside its own handler) could be mid-RunChain
			// -- inside CallOriginal(), reading m_Hook -- at the exact moment
			// this call decides to destroy m_Hook/free the trampoline/delete
			// the dispatcher. Closing that race properly would need either
			// full refcounted/RCU-style deferred destruction, or an
			// exclusive/shared lock held for the *entire* call duration (which
			// self-deadlocks the instant a handler unhooks its own last
			// registration on its own address, since std::shared_mutex has no
			// safe shared->exclusive upgrade). Never physically tearing down
			// sidesteps the whole problem: nothing is ever destroyed while
			// something might still be using it, so no lock is even needed
			// for this axis at all. The one-time cost is a permanent, empty
			// passthrough hop + a small heap object per distinct address ever
			// inline-hooked, for the life of the process -- bounded by
			// distinct hooked addresses, not by hook add/remove churn.
			static bool RemoveHook(void *addr, Handler handler, bool post)
			{
				std::lock_guard<std::mutex> lock(Map_Mutex());
				auto &map = Map();
				auto it = map.find(addr);
				if (it == map.end())
					return false;

				CInlineDispatcher *disp = it->second;
				return disp->RemoveHookLocked(handler, post);
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

				// Snapshot exactly the bytes we ourselves just patched in, so
				// a later HardUninstall() (see below) can tell "is this still
				// my patch" from "someone else (a different, independent
				// hooking engine -- e.g. KHook -- re-detoured on top of me)"
				// before blindly restoring the pre-hook original over
				// whatever's live there now.
				m_InstalledBytes.assign(
					reinterpret_cast<const std::uint8_t *>(m_TargetAddr),
					reinterpret_cast<const std::uint8_t *>(m_TargetAddr) + m_Hook.original_bytes().size());

				return true;
			}

			// Not called automatically (see RemoveHook() above for why) --
			// kept as an explicit, best-effort escape hatch for a genuine
			// full-engine-shutdown path, should one ever need it. Guarded
			// against the exact hazard uncovered while investigating
			// cross-engine coexistence with KHook: safetyhook::InlineHook's
			// own disable()/destroy() unconditionally memcpy's the bytes it
			// captured at install time back over the live target, with no
			// check of what's actually there *now* -- if some other,
			// independent hooking engine detoured the same address on top of
			// us in the meantime, that blind restore would silently eject
			// (or, worse, if that other engine already tore its own trampoline
			// down too, physically corrupt) live, executing machine code.
			// So: only actually revert if the target's bytes are still
			// byte-for-byte what we ourselves last installed there; otherwise
			// leave it alone and report the mismatch instead of touching it.
			bool HardUninstall()
			{
				if (m_TargetAddr && !m_InstalledBytes.empty())
				{
					if (std::memcmp(m_TargetAddr, m_InstalledBytes.data(), m_InstalledBytes.size()) != 0)
					{
						std::fprintf(stderr,
							"[SourceHook] CInlineDispatcher::HardUninstall: target %p no longer holds the "
							"bytes we installed -- some other hooking engine patched on top of us. Leaving "
							"the target untouched instead of blindly restoring over it (which would corrupt "
							"live code or silently eject the other engine's hook).\n",
							m_TargetAddr);
						return false;
					}
				}

				m_Hook = {};
				m_InstalledBytes.clear();
				if (m_Slot >= 0)
				{
					FreeSlot(m_Slot);
					m_Slot = -1;
				}
				return true;
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

			// True once no handler is registered any more (the dispatcher
			// keeps running as a transparent passthrough at that point --
			// see RemoveHook() above). Exposed for introspection/tests, not
			// used as a teardown trigger.
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
				frame.target_addr = m_TargetAddr;

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

			// SH_CALL's InlineExecutable::operator() calls CallOriginal()
			// directly (see below) to bypass every registered handler for
			// one call, same reason InlineTrampolineSlot needs friendship.
			template <typename T2, typename R2, typename... A2>
			friend class InlineExecutable;

			void *m_TargetAddr = nullptr;
			safetyhook::InlineHook m_Hook;
			std::vector<std::uint8_t> m_InstalledBytes; // see HardUninstall()
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

		// What SH_CALL(hookname, targetAddr[, thisptr]) returns for an
		// inline-hooked target -- the same "callable you invoke with the
		// real args, bypassing every registered handler for this one call"
		// idea as ExecutableClassN is for vtable hooks' SH_CALL(ptr, mfp),
		// just reached through the declared hookname's Dispatcher instead of
		// a member-function pointer.
		template <typename ThisClass, typename Ret, typename... Args>
		class InlineExecutable
		{
		public:
			InlineExecutable(void *addr, ThisClass *pThis) : m_Addr(addr), m_This(pThis) { }

			Ret operator()(Args... args) const
			{
				using Dispatcher = CInlineDispatcher<ThisClass, Ret, Args...>;
				if (Dispatcher *disp = Dispatcher::Find(m_Addr))
					return disp->CallOriginal(m_This, args...);

				// Nobody's hooked this address (for this signature) at all
				// right now -- the bytes there already *are* the untouched
				// original, so just call through them directly.
				using RawFn = typename InlineRawFn<ThisClass, Ret, Args...>::Type;
				if constexpr (std::is_void_v<ThisClass>)
					return reinterpret_cast<RawFn>(m_Addr)(args...);
				else
					return reinterpret_cast<RawFn>(m_Addr)(m_This, args...);
			}

		private:
			void *m_Addr;
			ThisClass *m_This;
		};

		// The un-templated, thisclass-erased form MakeInlineCallable() below
		// needs: SH_DECL_INLINEHOOK* generates a `HookTag` value (see
		// generate/sourcehook_inline_decl.h) exposing exactly these three
		// nested names, so SH_CALL doesn't need the caller to spell out
		// ThisClass/Ret/Args... by hand.
		template <typename HookTag, typename ThisClass>
		typename HookTag::Callable MakeInlineCallable(HookTag, void *addr, ThisClass *pThis)
		{
			static_assert(std::is_same_v<ThisClass, typename HookTag::ThisClassT>,
				"SH_CALL(hookname, targetAddr, thisptr): thisptr's type doesn't match "
				"the thisclass this hook was declared with.");
			return typename HookTag::Callable(addr, pThis);
		}

		// thisclass == void overload: SH_CALL(hookname, targetAddr), no this
		// to pass -- same 2-argument shape as SH_CALL(ptr, mfp) for vtable
		// hooks (see the SH_CALL2 overloads in sourcehook.h). Which overload
		// set actually gets picked is resolved by the real C++ compiler on
		// the *type* of the second macro argument (a HookTag value vs. a
		// pointer-to-member-function) -- SH_CALL itself doesn't need to know
		// which hook style it's looking at, same spirit as SH_IFACEPTR/
		// RETURN_SH auto-detecting via the inline call-frame stack.
		template <typename HookTag>
		typename HookTag::Callable MakeInlineCallable(HookTag, void *addr)
		{
			static_assert(std::is_void_v<typename HookTag::ThisClassT>,
				"SH_CALL(hookname, targetAddr) with only two arguments is for a hook "
				"declared with thisclass = void; pass the this pointer as a third "
				"argument for a non-void thisclass: SH_CALL(hookname, targetAddr, thisptr).");
			return typename HookTag::Callable(addr, nullptr);
		}
	}
}

// SH_CALL2 overload for inline-hook targets: lets the *same* SH_CALL(a, b)
// two-argument macro invocation (see sourcehook.h) resolve to either the
// vtable path (ExecutableClassN, when `b` is a pointer-to-member-function)
// or this one (when `b` is a value of a SH_DECL_INLINEHOOK*-generated
// HookTag type) -- picked by ordinary C++ overload resolution on the actual
// argument types, not by anything the macro itself has to decide.
template <typename HookTag>
typename HookTag::Callable SH_CALL2(HookTag tag, void *addr, void *, SourceHook::ISourceHook *)
{
	return SourceHook::Impl::MakeInlineCallable(tag, addr);
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
// (by this handler or any earlier one). Doubles as both META_RESULT_STATUS
// and META_RESULT_PREVIOUS for inline hooks (see SH_RESULT_STATUS/
// SH_RESULT_PREVIOUS's inline overrides below): unlike the vtable hook loop,
// which tracks "status so far" and "status before this handler" as two
// separate fields, RunChain() only ever has the one running frame.status,
// updated as each handler's own RETURN_SH_INLINE*/SET_SH_RESULT call runs --
// so reading it from *within* a handler, before that handler has set its own
// result, already gives exactly "the highest result any earlier handler
// set," which is what META_RESULT_PREVIOUS means; reading it afterward (rare
// in practice, once a handler already returned) gives "current," which is
// what META_RESULT_STATUS means. Same value, same field, correct for both
// by construction given the call order.
#define SHINT_INLINE_RESULT() (::SourceHook::Impl::CurrentInlineFrame()->status)

// SHINT_INLINE_OVERRIDE_RET(type): the override return value set so far by
// an earlier handler this call (Pre or Post), cast to `type`. Mirrors
// META_RESULT_OVERRIDE_RET. Only meaningful once some earlier handler has
// actually set an override (SHINT_INLINE_RESULT() >= MRES_OVERRIDE) --
// reads whatever's in the (Ret-sized, default-constructed at call start)
// override slot otherwise, same "don't call this before it's meaningful"
// contract as SHINT_INLINE_ORIG_RET/META_RESULT_ORIG_RET.
#define SHINT_INLINE_OVERRIDE_RET(type) (*reinterpret_cast<const type *>(::SourceHook::Impl::CurrentInlineFrame()->override_ret))

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

// SHINT_RETURN_INLINE_NEWPARAMS(result, hookname, newparams) /
// SHINT_RETURN_INLINE_VALUE_NEWPARAMS(result, hookname, newparams): inline-
// hook counterparts of RETURN_META_MNEWPARAMS/RETURN_META_VALUE_MNEWPARAMS
// (sourcehook.h) -- "supersede this call, but instead of my own override
// value, call the real original again with a *different* set of arguments,
// and use whatever it returns". `hookname` is the same SH_DECL_INLINEHOOK*-
// declared name SH_CALL/SH_ADD_INLINEHOOK already use; `newparams` is a
// parenthesized argument list, e.g. (a, b, c) -- same textual convention as
// *MNEWPARAMS's own `newparams` (this expands to `SH_CALL_INLINE3(...)
// newparams`, i.e. "call this callable object with these arguments", same
// as *MNEWPARAMS expands to "call this member-function-pointer with these
// arguments").
//
// Deliberately simpler than the vtable-hook macros they mirror: those
// restart the *entire* Pre/Post loop as a "recall" (SH_GLOB_SHPTR->
// DoRecall()), so every OTHER handler registered on that same hook -- not
// just this one -- also gets a chance to see the new-parameters call, only
// continuing from *this* handler's position in the chain rather than
// starting over from the first one. Inline hooks have no equivalent state
// machine (RunChain() has nothing like SetupHookLoop's State_Recall_*
// continuation, and naively re-invoking RunChain() from here would restart
// the *whole* Pre list from its first handler, not continue from this one --
// wrong semantics, not just a missing feature). What this does instead is
// exactly SH_CALL's own contract: call straight through to the real
// original with the new arguments, bypassing every registered handler
// (including this one, again) for this one call, then supersede with that
// result. Correct and sufficient for the common case -- a single handler
// wanting to adjust an argument and let the real function run with it; a
// plugin that specifically needs *other* plugins' handlers on the same
// address to also observe the modified-parameters call needs the full
// recall machinery this intentionally doesn't implement. Not unified with
// RETURN_META_MNEWPARAMS's own name via the usual inline/vtable
// auto-detection: that macro's body references a symbol
// (__SoureceHook_FHM_GetRecallMFP##hookname) that SH_DECL_MANUALHOOK*
// generates and SH_DECL_INLINEHOOK* does not, so a runtime-selected "both
// branches must compile" version (the trick every other auto-detecting
// macro in this file uses) isn't possible here -- a hookname declared only
// via SH_DECL_INLINEHOOK* would fail to compile the vtable branch. Distinct
// names instead, same spirit as SH_CALL_INLINE3 being its own macro rather
// than trying to overload SH_CALL_MNEWPARAMS.
#define SHINT_RETURN_INLINE_NEWPARAMS(result, hookname, newparams) \
	do { \
		SHINT_SET_INLINE_RESULT(result); \
		auto *__shFrame = ::SourceHook::Impl::CurrentInlineFrame(); \
		SH_CALL_INLINE3(hookname, __shFrame->target_addr, \
			reinterpret_cast<typename decltype(SourceHookInlineDecl::hookname)::ThisClassT *>(__shFrame->this_)) newparams; \
		SHINT_RETURN_INLINE(MRES_SUPERCEDE); \
	} while (0)

#define SHINT_RETURN_INLINE_VALUE_NEWPARAMS(result, hookname, newparams) \
	do { \
		SHINT_SET_INLINE_RESULT(result); \
		auto *__shFrame = ::SourceHook::Impl::CurrentInlineFrame(); \
		SHINT_RETURN_INLINE_VALUE(MRES_SUPERCEDE, \
			SH_CALL_INLINE3(hookname, __shFrame->target_addr, \
				reinterpret_cast<typename decltype(SourceHookInlineDecl::hookname)::ThisClassT *>(__shFrame->this_)) newparams); \
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

// Same auto-detection for the three remaining SH_RESULT_*/META_RESULT_*
// accessors sourcehook.h defines (SH_RESULT_STATUS, SH_RESULT_PREVIOUS,
// SH_RESULT_OVERRIDE_RET) -- closes the last gap where META_RESULT_STATUS/
// META_RESULT_PREVIOUS/META_RESULT_OVERRIDE_RET (plain aliases of these,
// see sourcehook.h) would have silently kept reading the vtable-hook-only
// SH_GLOB_SHPTR-based value even from inside an inline hook handler.
#undef SH_RESULT_STATUS
#define SH_RESULT_STATUS \
	(::SourceHook::Impl::InlineCallStack().empty() \
		? SH_GLOB_SHPTR->GetStatus() \
		: SHINT_INLINE_RESULT())

#undef SH_RESULT_PREVIOUS
#define SH_RESULT_PREVIOUS \
	(::SourceHook::Impl::InlineCallStack().empty() \
		? SH_GLOB_SHPTR->GetPrevRes() \
		: SHINT_INLINE_RESULT())

#undef SH_RESULT_OVERRIDE_RET
#define SH_RESULT_OVERRIDE_RET(type) \
	(::SourceHook::Impl::InlineCallStack().empty() \
		? *SourceHook::MacroRefHelpers<type>::GetOverrideRet(SH_GLOB_SHPTR) \
		: SHINT_INLINE_OVERRIDE_RET(type))

// `hookname` is a value (SH_DECL_INLINEHOOK* declares a constexpr instance,
// not just a type -- see generate/sourcehook_inline_decl.h) so SH_CALL can
// also take it as an argument; decltype() gets back to its Tag type here.
#define SH_ADD_INLINEHOOK(hookname, targetAddr, handler, post) \
	::SourceHook::Impl::AddInlineHook<decltype(SourceHookInlineDecl::hookname)::Dispatcher>((targetAddr), (handler), (post))

#define SH_REMOVE_INLINEHOOK(hookname, targetAddr, handler, post) \
	::SourceHook::Impl::RemoveInlineHook<decltype(SourceHookInlineDecl::hookname)::Dispatcher>((targetAddr), (handler), (post))

// SH_CALL(hookname, targetAddr) / SH_CALL(hookname, targetAddr, thisptr):
// call the real original for an inline-hooked target directly, bypassing
// every Pre/Post handler registered on it for this one call -- the inline-
// hook counterpart of vtable hooks' SH_CALL(ptr, mfp). Two- vs three-
// argument form is picked by SH_CALL itself (arity dispatch, see
// sourcehook.h); which *style* (vtable vs. inline) is picked for the two-
// argument form is resolved by ordinary C++ overload resolution on the
// second argument's type (see the SH_CALL2 overload above) -- same "one
// macro name, auto-detects" spirit as SH_IFACEPTR/RETURN_SH.
#define SH_CALL_INLINE3(hookname, targetAddr, thisptr) \
	::SourceHook::Impl::MakeInlineCallable(SourceHookInlineDecl::hookname, (targetAddr), (thisptr))

#endif //__SOURCEHOOK_INLINE_H__
