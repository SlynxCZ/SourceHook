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
//    RETURN_SH_INLINE_VALUE, and fully restore the original function once
//    every handler is removed.
//  - a non-virtual member function (has a `this`), read back via
//    SH_INLINE_IFACEPTR inside the handler.

#include <string>

#include "sourcehook_inline.h"

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
		RETURN_SH_INLINE_VALUE(MRES_SUPERCEDE, 1234);
	}

	struct FakePlugin
	{
		int MemberPost(int)
		{
			++g_PostCalls;
			// An earlier Pre handler may have SUPERCEDEd the call (see the
			// override test below) -- SH_INLINE_ORIG_RET is only safe to
			// read once the original actually ran.
			if (SH_INLINE_ORIG_CALLED())
				g_LastOrigRet = SH_INLINE_ORIG_RET(int);
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
		g_LastThis = SH_INLINE_IFACEPTR(Adder);
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
	CHECK(g_LastOrigRet == 42, "Post handler did not see the original return value via SH_INLINE_ORIG_RET");

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
	CHECK(result3 == 1234, "RETURN_SH_INLINE_VALUE(MRES_SUPERCEDE, ...) override was not honored");
	CHECK(SH_REMOVE_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPreOverride), false),
		"failed to remove the override Pre handler");

	// Tear everything down; once the last handler goes, the real hook (and
	// the address claim) must go with it.
	CHECK(SH_REMOVE_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPre), false), "failed to remove plugin A's handler");
	CHECK(SH_REMOVE_INLINEHOOK(TestInlineAdd, addr, SH_STATIC(StaticPre), false), "failed to remove plugin C's handler");
	CHECK(SH_REMOVE_INLINEHOOK(TestInlineAdd, addr, SH_MEMBER(&pluginB, &FakePlugin::MemberPost), true),
		"failed to remove plugin B's handler");

	CHECK(guard.TargetCount() == baseline, "dispatcher was not fully torn down after removing every handler");

	int result4 = TargetAdd(3);
	CHECK(result4 == 44, "function did not return to its original, unhooked behavior after teardown");

	// -------- member function: `this` via SH_INLINE_IFACEPTR --------
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
	CHECK(g_LastThis == pAdder, "SH_INLINE_IFACEPTR did not return the real `this` pointer");

	CHECK(SH_REMOVE_INLINEHOOK(TestInlineMemberAdd, memberAddr, SH_STATIC(MemberDetour), false),
		"failed to remove the member-function hook");

	return true;
}
