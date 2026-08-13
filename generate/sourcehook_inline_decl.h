/* ======== SourceHook ========
* Copyright (C) 2026 Metamod:Source Development Team
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
* a SourceHookInlineDecl::<hookname>_Tag type carrying the
* SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1..N>
* specialization that SH_ADD_INLINEHOOK/SH_REMOVE_INLINEHOOK
* (sourcehook_inline.h) dispatch through for that hook name -- plus a
* constexpr *value* actually named <hookname> (of that Tag type), so
* SH_CALL(hookname, targetAddr[, thisptr]) can pass it as a real argument
* and let ordinary C++ overload resolution pick it over vtable hooks'
* SH_CALL(ptr, mfp) (see sourcehook_inline.h's SH_CALL2 overload).
*
* `thisclass` is `void` for a hooked function with no `this` (a real free
* function), or the class type for a non-virtual member function -- see
* sourcehook_inline.h's file header for why that's a separate template
* parameter instead of Args...[0], and SH_INLINE_IFACEPTR for how to read it
* back out inside a handler.
* ============================
*/

#ifndef __SOURCEHOOK_INLINE_DECL_H__
#define __SOURCEHOOK_INLINE_DECL_H__

#define SH_DECL_INLINEHOOK0(hookname, thisclass, rettype) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK1(hookname, thisclass, rettype, param1) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK2(hookname, thisclass, rettype, param1, param2) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK3(hookname, thisclass, rettype, param1, param2, param3) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK4(hookname, thisclass, rettype, param1, param2, param3, param4) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK5(hookname, thisclass, rettype, param1, param2, param3, param4, param5) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK6(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK7(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK8(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK9(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK10(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK11(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK12(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK13(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK14(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK15(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK16(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK17(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK18(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK19(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;

#define SH_DECL_INLINEHOOK20(hookname, thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) \
	namespace SourceHookInlineDecl { \
		struct hookname##_Tag \
		{ \
			using Dispatcher = ::SourceHook::Impl::CInlineDispatcher<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20>; \
			using ThisClassT = thisclass; \
			using Callable = ::SourceHook::Impl::InlineExecutable<thisclass, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20>; \
		}; \
		inline constexpr hookname##_Tag hookname{}; \
	} \
	using SourceHookInlineDecl::hookname;


#endif //__SOURCEHOOK_INLINE_DECL_H__
