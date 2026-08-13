/* ======== SourceHook ========
* Copyright (C) 2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

// Exercises the inline-hook dispatch added in sourcehook_inline.h: two
// separate registrations against the *same* target address (one static
// Pre handler registered twice, standing in for "two plugins found the same
// signature"; one member Post handler, standing in for "static or member
// callback") must share exactly one safetyhook::InlineHook/dispatcher, run
// in Pre-then-original-then-Post order, honor MRES_* override semantics, and
// fully restore the original function once every handler is removed.

#include <string>

#include "sourcehook_inline.h"

SH_DECL_INLINEHOOK1(TestInlineAdd, int, int);

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

	META_RES StaticPre(SourceHook::InlineHookContext<int, int> &ctx)
	{
		++g_PreCalls;
		(void)ctx.Arg<0>();
		return MRES_IGNORED;
	}

	META_RES StaticPreOverride(SourceHook::InlineHookContext<int, int> &ctx)
	{
		ctx.SetOverrideRet(1234);
		return MRES_SUPERCEDE;
	}

	struct FakePlugin
	{
		META_RES MemberPost(SourceHook::InlineHookContext<int, int> &ctx)
		{
			++g_PostCalls;
			g_LastOrigRet = ctx.GetOrigRet();
			return MRES_IGNORED;
		}
	};
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
	CHECK(g_LastOrigRet == 42, "Post handler did not see the original return value");

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
	CHECK(result3 == 1234, "MRES_SUPERCEDE override return value was not honored");
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

	return true;
}
