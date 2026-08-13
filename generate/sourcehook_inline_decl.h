/* ======== SourceHook ========
* Copyright (C) 2004-2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
*
* GENERATED FILE -- do not edit by hand.
* Regenerate with: python3 generate/gen_inline_hooks.py
*
* Declares SH_DECL_INLINEHOOK0..20, the inline-hook counterpart of
* upstream's SH_DECL_MANUALHOOK0..20 (see sourcehook.h). Each macro declares
* a SourceHookInlineDecl::<hookname> type carrying the
* SourceHook::Impl::CInlineDispatcher<rettype, param1..N> specialization that
* SH_ADD_INLINEHOOK/SH_REMOVE_INLINEHOOK (sourcehook_inline.h) dispatch
* through for that hook name.
* ============================
*/

#ifndef __SOURCEHOOK_INLINE_DECL_H__
#define __SOURCEHOOK_INLINE_DECL_H__

#define SH_DECL_INLINEHOOK0(hookname, rettype) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype>; \
		}; \
	}

#define SH_DECL_INLINEHOOK1(hookname, rettype, param1) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1>; \
		}; \
	}

#define SH_DECL_INLINEHOOK2(hookname, rettype, param1, param2) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2>; \
		}; \
	}

#define SH_DECL_INLINEHOOK3(hookname, rettype, param1, param2, param3) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3>; \
		}; \
	}

#define SH_DECL_INLINEHOOK4(hookname, rettype, param1, param2, param3, param4) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4>; \
		}; \
	}

#define SH_DECL_INLINEHOOK5(hookname, rettype, param1, param2, param3, param4, param5) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5>; \
		}; \
	}

#define SH_DECL_INLINEHOOK6(hookname, rettype, param1, param2, param3, param4, param5, param6) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6>; \
		}; \
	}

#define SH_DECL_INLINEHOOK7(hookname, rettype, param1, param2, param3, param4, param5, param6, param7) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7>; \
		}; \
	}

#define SH_DECL_INLINEHOOK8(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8>; \
		}; \
	}

#define SH_DECL_INLINEHOOK9(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9>; \
		}; \
	}

#define SH_DECL_INLINEHOOK10(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10>; \
		}; \
	}

#define SH_DECL_INLINEHOOK11(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11>; \
		}; \
	}

#define SH_DECL_INLINEHOOK12(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12>; \
		}; \
	}

#define SH_DECL_INLINEHOOK13(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13>; \
		}; \
	}

#define SH_DECL_INLINEHOOK14(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14>; \
		}; \
	}

#define SH_DECL_INLINEHOOK15(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15>; \
		}; \
	}

#define SH_DECL_INLINEHOOK16(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16>; \
		}; \
	}

#define SH_DECL_INLINEHOOK17(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17>; \
		}; \
	}

#define SH_DECL_INLINEHOOK18(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18>; \
		}; \
	}

#define SH_DECL_INLINEHOOK19(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19>; \
		}; \
	}

#define SH_DECL_INLINEHOOK20(hookname, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) \
	namespace SourceHookInlineDecl { \
		struct hookname \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20>; \
		}; \
	}


#endif //__SOURCEHOOK_INLINE_DECL_H__
