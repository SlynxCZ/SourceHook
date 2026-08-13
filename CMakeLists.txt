# Copyright (C) 2026 Metamod:Source Development Team
# Author(s): Michal "Slynx" Přikryl
cmake_minimum_required(VERSION 3.18)
project(sourcehook CXX C)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_STATIC_LIBRARY_PREFIX "")

option(SOURCEHOOK_BUILD_TESTS "Build the SourceHook test suite" ON)

include(makefiles/shared.cmake)

set(SOURCEHOOK_SOURCES
    src/sourcehook.cpp
    src/sourcehook_impl_chookidman.cpp
    src/sourcehook_impl_chookmaninfo.cpp
    src/sourcehook_impl_cproto.cpp
    src/sourcehook_impl_cvfnptr.cpp
    src/sourcehook_hookmangen.cpp
    src/sourcehook_impl_inline.cpp
    vendor/safetyhook/safetyhook.cpp
    vendor/zydis/Zydis.c
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(WIN32)
        list(APPEND SOURCEHOOK_SOURCES src/sourcehook_hookmangen_x86_64.cpp)
    else()
        # Upstream metamod-source's own root AMBuilder excludes this file on
        # Linux x86_64 too (core/AMBuilder: `arch == 'x86_64' and platform !=
        # 'linux'`): its GCC/Linux register-marshaling paths are
        # `static_assert(false, ...)` stubs, and separately it uses `xor` as
        # an identifier, which GCC/Clang reject as a reserved alternative
        # token in C++. So on Linux x86_64 -- the actual target platform for
        # inline hooks -- typed/manual (vtable) hooks have no working
        # codegen backend here, matching upstream's own current state; this
        # is a pre-existing upstream gap, not something this port changed.
        # Inline hooks (sourcehook_inline.h) are unaffected -- they don't use
        # this file at all.
        message(STATUS "sourcehook: vtable hookmangen (SH_DECL_HOOK/SH_DECL_MANUALHOOK codegen) is not built on Linux x86_64 -- matches upstream metamod-source's own core/AMBuilder scope. Inline hooks (SH_DECL_INLINEHOOK) are unaffected.")
    endif()
else()
    list(APPEND SOURCEHOOK_SOURCES
        src/sourcehook_hookmangen_x86.cpp
    )
    message(WARNING "Inline-hook dispatch (sourcehook_inline.h) is only implemented/tested for x86_64 SysV right now; SH_DECL_INLINEHOOK* will not have the correct calling convention on 32-bit.")
endif()

add_library(sourcehook STATIC ${SOURCEHOOK_SOURCES})

target_include_directories(sourcehook PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/safetyhook
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tl
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/zydis
)

target_compile_definitions(sourcehook PRIVATE ZYDIS_STATIC_BUILD)

set_target_properties(sourcehook PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

sourcehook_apply_platform_flags(sourcehook)

if(SOURCEHOOK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
