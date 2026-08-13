# Copyright (C) 2026 Michal Přikryl (Slynx) / (˙·٠● S l y n x ●٠·˙)
# Compiler/linker flags shared by the sourcehook library and its test binary.
# Modeled after DynLibUtils' CMakeLists.txt flag set (a standalone-library
# sibling repo), since sourcehook has no HL2SDK/engine dependency to fold in.

function(sourcehook_apply_platform_flags target)
    if(LINUX)
        target_compile_options(${target} PRIVATE
            -mfpmath=sse -msse -fno-strict-aliasing
            -fno-threadsafe-statics -fvisibility=hidden -fvisibility-inlines-hidden
            -Wall -Wno-uninitialized -Wno-switch -Wno-unused
            -Wno-non-virtual-dtor -Wno-overloaded-virtual
            -Wno-invalid-offsetof -Wno-reorder
            -frtti
        )
        target_compile_definitions(${target} PRIVATE
            _LINUX POSIX LINUX GNUC COMPILER_GCC
            _FILE_OFFSET_BITS=64
        )
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            target_compile_definitions(${target} PRIVATE PLATFORM_64BITS X64BITS)
        endif()
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_definitions(${target} PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
        endif()
    elseif(WIN32)
        target_compile_definitions(${target} PRIVATE
            COMPILER_MSVC WIN32 _WINDOWS
            _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE
        )
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            target_compile_definitions(${target} PRIVATE COMPILER_MSVC64)
        endif()
        target_compile_options(${target} PRIVATE /utf-8 /permissive- /EHsc)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${target} PRIVATE DEBUG _DEBUG)
    else()
        target_compile_definitions(${target} PRIVATE NDEBUG)
    endif()
endfunction()
