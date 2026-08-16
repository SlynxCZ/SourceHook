/* ======== SourceHook ========
* Copyright (C) 2004-2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

// Exercises the inline-hook dispatch added in sourcehook_inline.h:
//  - a free function (no `this`) hooked by two separate registrations
//    against the *same* target address (one static Pre handler registered
//    twice, standing in for "two plugins found the same signature"; one
//    member Post handler, standing in for "static or member callback") --
//    must share exactly one safetyhook::InlineHook/dispatcher, run in
//    Pre-then-original-then-Post order, honor MRES_* override semantics via
//    RETURN_SH_VALUE, and behave exactly like the original, unhooked
//    function once every handler is removed (without actually tearing the
//    detour down -- see RemoveHook()'s comment in sourcehook_inline.h).
//  - a non-virtual member function (has a `this`), read back via
//    SH_IFACEPTR inside the handler.
//  - TestInlineHookConcurrency below: many threads hammering the hooked
//    function while another thread concurrently adds/removes handlers on
//    the *same* address -- the scenario that used to be able to
//    use-after-free once the last handler dropped mid-flight on another
//    thread (fixed by never tearing the dispatcher down on empty).

#include <string>

#include "sourcehook/sourcehook_inline.h"

// SH_IFACEPTR/RETURN_SH/RETURN_SH_VALUE/SET_SH_RESULT/SH_RESULT_ORIG_RET
// (sourcehook_inline.h) always reference SH_GLOB_SHPTR (default: g_SHPtr) in
// the compiled code for their vtable-hook fallback branch, even where that
// branch is never actually *taken* at runtime -- this test only ever uses
// inline hooks, so it's dead code here, but the symbol still has to exist
// for the TU to compile. A real metamod:source plugin already gets this for
// free from PLUGIN_EXPOSE(); a standalone tool/test needs its own stand-in.
SourceHook::ISourceHook *g_SHPtr = nullptr;
SourceHook::Plugin g_PLID = 0;

// -------------------- free function (thisclass = void) --------------------

SH_DECL_INLINEHOOK1(TestInlineAdd, void, int, int);

namespace
{
	int __attribute__((noinline)) TargetAdd(int x)
	{
		// volatile keeps the compiler from folding this down to nothing
		// observable. The nop sled is load-bearing: at -O2 this function's
		// real body alone can compile to as few as ~11 bytes, which isn't
		// always enough room for safetyhook to relocate a prologue into its
		// trampoline safely -- it can end up patching past the end of the
		// function into whatever comes next. Real hooked functions (entire
		// engine methods) are always far larger than this synthetic one, so
		// this is purely a test-fixture concern, not a dispatch concern.
		volatile int y = x + 41;
		asm volatile(
			"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
			"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
			::: "memory");
		return y + 0;
	}

	int g_PreCalls = 0;
	int g_PostCalls = 0;
	int g_LastOrigRet = -1;

	// Real signature -- just like SH_DECL_HOOK's generated Func() override.
	int StaticPre(int x)
	{
		++g_PreCalls;
		(void)x;
		return 0; // ignored: no RETURN_SH_INLINE* call means MRES_IGNORED,
		          // same permissive default typed hooks already have.
	}

	int StaticPreOverride(int)
	{
		RETURN_SH_VALUE(MRES_SUPERCEDE, 1234);
	}

	struct FakePlugin
	{
		int MemberPost(int)
		{
			++g_PostCalls;
			// An earlier Pre handler may have SUPERCEDEd the call (see the
			// override test below) -- SH_RESULT_ORIG_RET is only safe to
			// read once the original actually ran.
			if (SH_INLINE_ORIG_CALLED())
				g_LastOrigRet = SH_RESULT_ORIG_RET(int);
			return 0;
		}
	};
}

// ---------------- non-virtual member function (has `this`) ----------------

namespace
{
	struct Adder
	{
		int base;
		int __attribute__((noinline)) Add(int x)
		{
			volatile int y = base + x;
			asm volatile(
				"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
				"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
				::: "memory");
			return y + 0;
		}
	};

	// volatile is load-bearing here, not decoration: Adder::Add's actual
	// definition is visible in this same translation unit, so without it the
	// optimizer is free to conclude "calling Adder::Add can't touch
	// g_LastThis" from Add's own (unrelated) body and cache/hoist reads of
	// it across the call -- a real hooked function always lives in a
	// separately-compiled binary the compiler has zero visibility into, so
	// this can't happen for a real caller; it's purely an artifact of
	// hooking a same-TU synthetic test target, same spirit as the nop sled
	// on TargetAdd/Adder::Add above.
	Adder *volatile g_LastThis = nullptr;

	// &Adder::Add is a pointer-to-member-function, not a plain code address
	// -- reinterpret_cast can't convert it directly. On the Itanium C++ ABI
	// (GCC/Clang, which is all this repo targets), a non-virtual member
	// function pointer is a two-word {address, adjustor} struct with
	// adjustor == 0, so the first word is the real code address. This
	// helper is purely test-support: a real caller resolves its target
	// address via its own signature scan, never via &Class::Method.
	template <typename MemFn>
	void *MemberFuncAddr(MemFn mfp)
	{
		union { MemFn in; void *out[2]; } u{};
		u.in = mfp;
		return u.out[0];
	}
}

SH_DECL_INLINEHOOK1(TestInlineMemberAdd, Adder, int, int);

namespace
{
	int MemberDetour(int x)
	{
		g_LastThis = SH_IFACEPTR(Adder);
		(void)x;
		return 0;
	}
}

#define CHECK(cond, msg) \
	do { if (!(cond)) { error = (msg); return false; } } while (0)

bool TestInlineHook(std::string &error)
{
	void *addr = reinterpret_cast<void *>(&TargetAdd);
	auto &guard = SourceHook::Impl::CInlineHookAddressGuard::Get();
	std::size_t baseline = guard.TargetCount();

	// Plugin A hooks Pre.
	int idA = SH_ADD_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPre), false);
	CHECK(idA != 0, "plugin A failed to register its Pre hook");

	// Plugin B hooks Post via a member callback -- same address.
	FakePlugin pluginB;
	int idB = SH_ADD_INLINEHOOK(TestInlineAdd, addr, SH_MEMBER(&pluginB, &FakePlugin::MemberPost), true);
	CHECK(idB != 0, "plugin B failed to register its Post hook");

	// The whole point: two plugins on the same address share ONE real hook.
	CHECK(guard.TargetCount() == baseline + 1, "expected exactly one shared dispatcher for the target address");

	int result = TargetAdd(1);
	CHECK(result == 42, "hooked call did not return the original function's result");
	CHECK(g_PreCalls == 1, "Pre handler did not run exactly once");
	CHECK(g_PostCalls == 1, "Post handler did not run exactly once");
	CHECK(g_LastOrigRet == 42, "Post handler did not see the original return value via SH_RESULT_ORIG_RET");

	// Plugin C independently "found the same signature" and registers the
	// exact same Pre handler again -- still must not create a second hook.
	int idC = SH_ADD_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPre), false);
	CHECK(idC != 0 && idC != idA, "plugin C failed to register (or got plugin A's handle back)");
	CHECK(guard.TargetCount() == baseline + 1, "a third registration on the same address must not create a second dispatcher");

	g_PreCalls = 0;
	int result2 = TargetAdd(2);
	CHECK(result2 == 43, "hooked call result changed unexpectedly after adding a third handler");
	CHECK(g_PreCalls == 2, "both Pre handlers (A and C) should have run");

	// MRES_SUPERCEDE from a Pre handler must skip the original call and use
	// the override value -- same semantics as vtable hooks.
	int idOverride = SH_ADD_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPreOverride), false);
	CHECK(idOverride != 0, "override Pre handler failed to register");

	int result3 = TargetAdd(100);
	CHECK(result3 == 1234, "RETURN_SH_VALUE(MRES_SUPERCEDE, ...) override was not honored");

	// SH_CALL(hookname, targetAddr): the override Pre handler above is still
	// registered and would SUPERCEDE a normal call -- SH_CALL must bypass it
	// (and every other registered handler) entirely and reach the real
	// original, same "ignore hooks for this one call" contract as vtable
	// hooks' SH_CALL(ptr, mfp).
	int bypassResult = SH_CALL(TestInlineAdd, addr)(100);
	CHECK(bypassResult == 141, "SH_CALL did not bypass the registered handlers and call the real original");

	CHECK(SH_REMOVE_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPreOverride), false),
		"failed to remove the override Pre handler");

	// Remove every handler. Unlike a naive "last handler out tears the real
	// hook down" design, the dispatcher (and its safetyhook detour) is
	// deliberately left installed permanently once created -- same choice
	// KHook itself makes, and for the same reason: physically uninstalling
	// on last-remove is fundamentally unsafe to do concurrently with an
	// in-flight call through the same target (see RemoveHook()'s comment in
	// sourcehook_inline.h). An empty handler list just makes it a
	// transparent passthrough, which is functionally indistinguishable from
	// "unhooked" to a caller -- verified below.
	CHECK(SH_REMOVE_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPre), false), "failed to remove plugin A's handler");
	CHECK(SH_REMOVE_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPre), false), "failed to remove plugin C's handler");
	CHECK(SH_REMOVE_INLINEHOOK(TestInlineAdd, addr, SH_MEMBER(&pluginB, &FakePlugin::MemberPost), true),
		"failed to remove plugin B's handler");

	CHECK(guard.TargetCount() == baseline + 1, "the dispatcher/claim must survive removing every handler (never auto-torn-down)");

	int result4 = TargetAdd(3);
	CHECK(result4 == 44, "function did not behave like its original, unhooked self once its handler list went empty");

	// SH_CALL against an address whose dispatcher is still installed but has
	// zero handlers: Find() locates the (empty) dispatcher and calls through
	// its trampoline -- same real original, same result, still bypasses
	// nothing since there's nothing registered to bypass.
	int bypassResultNoHook = SH_CALL(TestInlineAdd, addr)(3);
	CHECK(bypassResultNoHook == 44, "SH_CALL against an emptied dispatcher did not reach the real original");
	CHECK(guard.TargetCount() == baseline + 1, "SH_CALL must not change the dispatcher/claim count");

	// -------- member function: `this` via SH_IFACEPTR --------
	// volatile + calling through a raw pointer derived once, same reasoning
	// as g_LastThis above: Adder::Add's definition is visible in this TU, so
	// without this the optimizer can decide adder's storage is dead after
	// its "last" (as far as it can see) use and reuse/invalidate it before
	// the CHECK below reads &adder again -- it has no way to know the call
	// through pAdder actually runs MemberDetour instead, which captures and
	// compares against that same address. Not a concern for a real target,
	// which always lives in a separately-compiled binary the compiler can't
	// see through at all.
	volatile Adder adder{ 10 };
	Adder *pAdder = const_cast<Adder *>(&adder);
	void *memberAddr = MemberFuncAddr(&Adder::Add);
	int idMember = SH_ADD_INLINEHOOK(TestInlineMemberAdd, memberAddr, SH_STATIC(MemberDetour), false);
	CHECK(idMember != 0, "member-function hook failed to register");

	int memberResult = pAdder->Add(5);
	CHECK(memberResult == 15, "member-function call did not return the original result");
	CHECK(g_LastThis == pAdder, "SH_IFACEPTR did not return the real `this` pointer");

	// SH_CALL(hookname, targetAddr, thisptr): three-argument form for a
	// non-void thisclass -- same bypass contract, just with `this` supplied
	// explicitly since the target isn't a free function.
	g_LastThis = nullptr;
	int memberBypassResult = SH_CALL(TestInlineMemberAdd, memberAddr, pAdder)(5);
	CHECK(memberBypassResult == 15, "SH_CALL (member) did not bypass the handler and call the real original");
	CHECK(g_LastThis == nullptr, "SH_CALL (member) must not have run MemberDetour at all");

	CHECK(SH_REMOVE_INLINEHOOK(TestInlineMemberAdd, memberAddr, SH_STATIC(MemberDetour), false),
		"failed to remove the member-function hook");

	// And again with an emptied (but still installed) dispatcher.
	int memberBypassNoHook = SH_CALL(TestInlineMemberAdd, memberAddr, pAdder)(5);
	CHECK(memberBypassNoHook == 15, "SH_CALL (member) against an emptied dispatcher did not reach the real original");

	return true;
}

// -------------------- META_* macro parity for inline hooks --------------------
//
// Exercises the macros added specifically so a handler doesn't need to know
// or care whether it's attached to a typed/manual (vtable) hook or an
// inline one: META_RESULT_STATUS/META_RESULT_PREVIOUS/META_RESULT_OVERRIDE_RET
// (all auto-detecting aliases -- see sourcehook_inline.h's #undef/#define
// block), plus the inline-specific SHINT_RETURN_INLINE_VALUE_NEWPARAMS
// (no vtable-side auto-detected name is possible for this one -- see its
// comment in sourcehook_inline.h for why).

SH_DECL_INLINEHOOK1(TestInlineMeta, void, int, int);

namespace
{
	int __attribute__((noinline)) TargetMeta(int x)
	{
		volatile int y = x * 2;
		asm volatile(
			"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
			"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
			::: "memory");
		return y + 0;
	}

	META_RES g_SeenPrevious = MRES_IGNORED;
	int g_SeenOverrideRet = -1;

	// Runs first: overrides the return value, doesn't supersede -- the
	// second handler below still gets to run.
	int MetaPreFirst(int)
	{
		RETURN_META_VALUE(MRES_OVERRIDE, 100);
	}

	// Runs second: reads what the first handler already decided via the
	// exact same META_RESULT_* names a vtable-hook handler would use, then
	// supersedes with a NEWPARAMS call -- the real original run again, with
	// a *different* argument than what this call actually received.
	int MetaPreSecond(int x)
	{
		g_SeenPrevious = META_RESULT_PREVIOUS;
		if (META_RESULT_STATUS >= MRES_OVERRIDE)
			g_SeenOverrideRet = META_RESULT_OVERRIDE_RET(int);

		SHINT_RETURN_INLINE_VALUE_NEWPARAMS(MRES_SUPERCEDE, TestInlineMeta, (x + 1000));
	}
}

bool TestInlineHookMetaMacros(std::string &error)
{
	void *addr = reinterpret_cast<void *>(&TargetMeta);

	int idFirst = SH_ADD_INLINEHOOK(TestInlineMeta, addr, SH_STATIC(MetaPreFirst), false);
	CHECK(idFirst != 0, "failed to register the first Pre handler");
	int idSecond = SH_ADD_INLINEHOOK(TestInlineMeta, addr, SH_STATIC(MetaPreSecond), false);
	CHECK(idSecond != 0, "failed to register the second Pre handler");

	int result = TargetMeta(5);

	CHECK(g_SeenPrevious == MRES_OVERRIDE, "META_RESULT_PREVIOUS did not see the first handler's MRES_OVERRIDE");
	CHECK(g_SeenOverrideRet == 100, "META_RESULT_OVERRIDE_RET did not see the first handler's override value");
	// TargetMeta(1005) = 1005 * 2 = 2010 -- proves SHINT_RETURN_INLINE_VALUE_NEWPARAMS
	// actually called through with (x + 1000), not the original x = 5.
	CHECK(result == 2010, "SHINT_RETURN_INLINE_VALUE_NEWPARAMS did not call through with the modified argument");

	CHECK(SH_REMOVE_INLINEHOOK(TestInlineMeta, addr, SH_STATIC(MetaPreFirst), false), "failed to remove the first Pre handler");
	CHECK(SH_REMOVE_INLINEHOOK(TestInlineMeta, addr, SH_STATIC(MetaPreSecond), false), "failed to remove the second Pre handler");

	return true;
}

// -------------------- SH_DECL_INLINEHOOK*_void --------------------
//
// The _void family just drops `rettype` (hardcoded to void) -- same
// relationship SH_DECL_HOOKn_void/SH_DECL_MANUALHOOKn_void already have to
// their rettype-taking counterparts. Exercises that the generated macro
// actually compiles and dispatches correctly, not just that it expands.

SH_DECL_INLINEHOOK1_void(TestInlineVoidHook, void, int);

namespace
{
	int g_VoidHookLastArg = -1;
	bool g_VoidHookOrigCalled = false;

	void __attribute__((noinline)) TargetVoidHook(int x)
	{
		g_VoidHookOrigCalled = true;
		volatile int y = x;
		asm volatile(
			"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
			"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
			::: "memory");
		(void)y;
	}

	void VoidHookPre(int x)
	{
		g_VoidHookLastArg = x;
	}
}

bool TestInlineHookVoidDecl(std::string &error)
{
	void *addr = reinterpret_cast<void *>(&TargetVoidHook);

	int id = SH_ADD_INLINEHOOK(TestInlineVoidHook, addr, SH_STATIC(VoidHookPre), false);
	CHECK(id != 0, "SH_DECL_INLINEHOOK1_void: failed to register the Pre handler");

	g_VoidHookOrigCalled = false;
	TargetVoidHook(42);

	CHECK(g_VoidHookLastArg == 42, "SH_DECL_INLINEHOOK1_void: Pre handler did not see the argument");
	CHECK(g_VoidHookOrigCalled, "SH_DECL_INLINEHOOK1_void: the real original was not called");

	CHECK(SH_REMOVE_INLINEHOOK(TestInlineVoidHook, addr, SH_STATIC(VoidHookPre), false),
		"SH_DECL_INLINEHOOK1_void: failed to remove the Pre handler");

	return true;
}

// -------------------- concurrency stress test --------------------
//
// Reproduces, under real concurrent threads, the exact race the
// never-tear-down design in RemoveHook() (sourcehook_inline.h) closes:
// N threads continuously calling through the hooked address while another
// thread continuously adds and removes the *last* handler on that same
// address. With the old "delete the dispatcher once the handler list goes
// empty" behavior, this reliably use-after-frees within a handful of
// iterations (the caller threads dereference a dispatcher/safetyhook::
// InlineHook that the remover thread just freed); with the fix, it must run
// to completion cleanly regardless of interleaving. Build with
// -fsanitize=thread for the strongest signal; even without a sanitizer, a
// UAF here reliably crashes or corrupts g_CallCount within a few thousand
// iterations on a debug allocator, so a clean run without a sanitizer is
// still meaningful evidence, just not a proof.

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

SH_DECL_INLINEHOOK1(TestInlineConcurrency, void, int, int);

namespace
{
	std::atomic<int> g_ConcurrencyPreCalls{ 0 };

	int __attribute__((noinline)) TargetConcurrency(int x)
	{
		volatile int y = x + 1;
		asm volatile(
			"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
			"nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
			::: "memory");
		return y;
	}

	int ConcurrencyPre(int x)
	{
		g_ConcurrencyPreCalls.fetch_add(1, std::memory_order_relaxed);
		(void)x;
		return 0; // no RETURN_SH_INLINE_VALUE call -> MRES_IGNORED, same as StaticPre above
	}
}

bool TestInlineHookConcurrency(std::string &error)
{
	void *addr = reinterpret_cast<void *>(&TargetConcurrency);

	static constexpr int kCallerThreads = 4;

	// Wall-clock deadline rather than a fixed iteration count: a fixed count
	// is fragile across environments -- a fast, uncontended churn loop (just
	// two mutex-protected map operations each) can easily finish all its
	// iterations before the OS has even scheduled the caller threads onto a
	// core for the first time, especially under a hypervisor/container CPU
	// scheduler, which would "pass" without ever actually exercising the
	// overlap this test exists to hit. Running both sides until a shared
	// deadline guarantees they're live at the same time regardless of how
	// fast either loop body happens to be on a given machine.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);

	std::atomic<bool> stop{ false };
	std::atomic<bool> sawBadResult{ false };

	// Establish the hook -- i.e. let safetyhook::InlineHook::create() (via
	// CInlineDispatcher::Install(), the *first* GetOrCreate() call for this
	// address) actually patch TargetConcurrency's bytes -- BEFORE any caller
	// thread starts calling it. This is deliberate, not incidental: the
	// vendored safetyhook's trap_threads() (safetyhook.cpp, the Linux
	// SAFETYHOOK_OS_LINUX branch) is a no-op passthrough on Linux --
	// `run_fn()` executes with no thread suspension/instruction-pointer
	// fixup around it at all, unlike the real Windows implementation (which
	// uses a vectored exception handler to catch and redirect any thread
	// caught mid-region). That means the very first physical patch of a
	// function's bytes is NOT safe to race against a thread already
	// executing inside it on this platform -- a thread can be caught
	// mid-instruction when the bytes underneath it change, which is exactly
	// what crashed this test before this ordering was pinned down (not a
	// SourceHook bug: same underlying gap KHook would hit installing its own
	// first hook on an already-hot function). What this test actually
	// verifies -- and what CInlineDispatcher::RemoveHook() never tearing the
	// dispatcher down on empty fixes -- is safe *after* that first install:
	// N threads calling concurrently while another thread repeatedly adds
	// and removes handlers (pure list mutation from here on, no re-patching)
	// must never race against a concurrent Uninstall()+delete, because there
	// no longer is one.
	{
		int id = SH_ADD_INLINEHOOK(TestInlineConcurrency, addr, SH_STATIC(ConcurrencyPre), false);
		CHECK(id != 0, "failed to establish the initial hook before starting the concurrency stress");
		SH_REMOVE_INLINEHOOK(TestInlineConcurrency, addr, SH_STATIC(ConcurrencyPre), false);
	}

	// Caller threads: hammer the hooked address the whole time. Whether a
	// given call happens to see the handler installed or not is irrelevant
	// -- TargetConcurrency(x) must ALWAYS return x + 1 either way, and must
	// never crash/UB regardless of what the churn thread is doing
	// concurrently.
	std::vector<std::thread> callers;
	for (int t = 0; t < kCallerThreads; ++t)
	{
		callers.emplace_back([&, t] {
			int i = 0;
			while (!stop.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() < deadline)
			{
				int x = (t * 1000000) + i++;
				int result = TargetConcurrency(x);
				if (result != x + 1)
				{
					sawBadResult.store(true, std::memory_order_relaxed);
					stop.store(true, std::memory_order_relaxed);
					break;
				}
			}
		});
	}

	// Churn thread: repeatedly add then remove the only handler on this
	// address, from a different thread than every caller above -- this is
	// what used to race the callers' CallOriginal()/RunChain() against a
	// concurrent Uninstall()+delete.
	std::thread churner([&] {
		while (!stop.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() < deadline)
		{
			int id = SH_ADD_INLINEHOOK(TestInlineConcurrency, addr, SH_STATIC(ConcurrencyPre), false);
			if (id == 0)
			{
				sawBadResult.store(true, std::memory_order_relaxed);
				break;
			}
			SH_REMOVE_INLINEHOOK(TestInlineConcurrency, addr, SH_STATIC(ConcurrencyPre), false);
		}
		stop.store(true, std::memory_order_relaxed);
	});

	churner.join();
	for (auto &th : callers)
		th.join();

	CHECK(!sawBadResult.load(), "concurrent add/remove vs. dispatch produced a wrong result (or add failed) -- see comment above");

	// Sanity: the Pre handler must have actually fired at least once while
	// installed (otherwise the churn thread's add/remove was too fast
	// relative to the callers to prove anything, and the run above wasn't
	// actually exercising the hooked path at all).
	CHECK(g_ConcurrencyPreCalls.load() > 0, "Pre handler never ran during the concurrency test -- churn/caller timing didn't overlap at all");

	return true;
}
