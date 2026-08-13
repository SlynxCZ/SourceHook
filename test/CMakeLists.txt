# Copyright (C) 2026 Metamod:Source Development Team
# Author(s): Michal "Slynx" Přikryl
# Local/dev test build. Two separate binaries, matching test/AMBuilder's
# scope split:
#   test_sourcehook        - legacy suite ported from metamod-source, x86 only
#                             (upstream never built it on x86_64 or Windows,
#                             see AMBuilder for why)
#   test_sourcehook_inline  - new inline-hook dispatch test, x86_64 SysV only
#                             (see sourcehook_inline.h's file header for why)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_executable(test_sourcehook
        main.cpp
        test1.cpp
        test2.cpp
        test3.cpp
        test4.cpp
        testbail.cpp
        testbail2.cpp
        testhookmangen.cpp
        testlist.cpp
        testmanual.cpp
        testmulti.cpp
        testoddthunks.cpp
        testrecall.cpp
        testreentr.cpp
        testref.cpp
        testrefret.cpp
        testvphooks.cpp
    )
    target_compile_definitions(test_sourcehook PRIVATE SOURCEHOOK_TESTS)
    target_link_libraries(test_sourcehook PRIVATE sourcehook)
    # This legacy suite -- unlike a normal consumer -- also reaches directly
    # into the hookmangen JIT internals (main.cpp, testhookmangen.cpp), which
    # are PRIVATE to the sourcehook target and don't come along via
    # target_link_libraries above.
    target_include_directories(test_sourcehook PRIVATE ${CMAKE_SOURCE_DIR}/src/hookmangen)
    sourcehook_apply_platform_flags(test_sourcehook)
    add_test(NAME sourcehook_legacy_tests COMMAND test_sourcehook)
elseif(LINUX)
    add_executable(test_sourcehook_inline
        main_inline.cpp
        testinlinehook.cpp
    )
    find_package(Threads REQUIRED)
    target_link_libraries(test_sourcehook_inline PRIVATE sourcehook Threads::Threads)
    sourcehook_apply_platform_flags(test_sourcehook_inline)
    add_test(NAME sourcehook_inline_tests COMMAND test_sourcehook_inline)
else()
    message(STATUS "sourcehook: no test binary configured for this platform/arch combination")
endif()
