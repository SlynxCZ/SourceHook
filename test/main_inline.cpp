/* ======== SourceHook ========
* Copyright (C) 2026 Michal Přikryl (Slynx) / (˙·٠● S l y n x ●٠·˙) -- inline hook dispatch
* No warranties of any kind
*
* License: zlib/libpng
* ============================
*/

// Standalone driver for the inline-hook dispatch test (testinlinehook.cpp).
// Kept separate from main.cpp/DECL_TEST: that harness drives the legacy,
// x86-only vtable-hook suite through SourceHook::Impl::CSourceHookImpl, none
// of which the inline-hook dispatch touches or depends on.

#include <iostream>
#include <string>

bool TestInlineHook(std::string &error);

int main()
{
	std::string error;
	if (!TestInlineHook(error))
	{
		std::cout << "TestInlineHook FAILED: " << error << std::endl;
		return 1;
	}

	std::cout << "TestInlineHook passed" << std::endl;
	return 0;
}
