#pragma once

// Platform Detection Macros for Lupine Engine
// These macros are used to detect the target platform at compile time

// Windows
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__)
    #define LUPINE_PLATFORM_WINDOWS 1
    #ifdef _WIN64
        #define LUPINE_PLATFORM_64BIT 1
    #else
        #define LUPINE_PLATFORM_32BIT 1
    #endif
#endif

// Linux
#if defined(__linux__) || defined(__linux)
    #define LUPINE_PLATFORM_LINUX 1
    #if defined(__x86_64__) || defined(__amd64__)
        #define LUPINE_PLATFORM_64BIT 1
    #else
        #define LUPINE_PLATFORM_32BIT 1
    #endif
#endif

// macOS / iOS
#if defined(__APPLE__) || defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define LUPINE_PLATFORM_MACOS 1
        #define LUPINE_PLATFORM_64BIT 1
    #elif TARGET_OS_IPHONE
        #define LUPINE_PLATFORM_IOS 1
    #endif
#endif

// Web (Emscripten)
#if defined(__EMSCRIPTEN__)
    #define LUPINE_PLATFORM_WEB 1
#endif

// Ensure at least one platform is defined
#if !defined(LUPINE_PLATFORM_WINDOWS) && !defined(LUPINE_PLATFORM_LINUX) && \
    !defined(LUPINE_PLATFORM_MACOS) && !defined(LUPINE_PLATFORM_WEB) && \
    !defined(LUPINE_PLATFORM_IOS)
    #error "Unknown platform! Lupine Engine requires Windows, Linux, macOS, iOS, or Web (Emscripten)"
#endif

// Compiler Detection
#if defined(_MSC_VER)
    #define LUPINE_COMPILER_MSVC 1
#elif defined(__clang__)
    #define LUPINE_COMPILER_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
    #define LUPINE_COMPILER_GCC 1
#endif

// Debug vs Release
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
    #define LUPINE_DEBUG 1
#else
    #define LUPINE_RELEASE 1
#endif

// Force inline
#if defined(LUPINE_COMPILER_MSVC)
    #define LUPINE_FORCE_INLINE __forceinline
#elif defined(LUPINE_COMPILER_CLANG) || defined(LUPINE_COMPILER_GCC)
    #define LUPINE_FORCE_INLINE inline __attribute__((always_inline))
#else
    #define LUPINE_FORCE_INLINE inline
#endif

// DLL Import/Export (for future use with shared libraries)
#if defined(LUPINE_PLATFORM_WINDOWS)
    #define LUPINE_API_EXPORT __declspec(dllexport)
    #define LUPINE_API_IMPORT __declspec(dllimport)
#else
    #define LUPINE_API_EXPORT __attribute__((visibility("default")))
    #define LUPINE_API_IMPORT
#endif

// Disable warnings
#if defined(LUPINE_COMPILER_MSVC)
    #define LUPINE_DISABLE_WARNING_PUSH __pragma(warning(push))
    #define LUPINE_DISABLE_WARNING_POP __pragma(warning(pop))
    #define LUPINE_DISABLE_WARNING(warningNumber) __pragma(warning(disable : warningNumber))
#elif defined(LUPINE_COMPILER_CLANG) || defined(LUPINE_COMPILER_GCC)
    #define LUPINE_DO_PRAGMA(X) _Pragma(#X)
    #define LUPINE_DISABLE_WARNING_PUSH LUPINE_DO_PRAGMA(GCC diagnostic push)
    #define LUPINE_DISABLE_WARNING_POP LUPINE_DO_PRAGMA(GCC diagnostic pop)
    #define LUPINE_DISABLE_WARNING(warningName) LUPINE_DO_PRAGMA(GCC diagnostic ignored #warningName)
#else
    #define LUPINE_DISABLE_WARNING_PUSH
    #define LUPINE_DISABLE_WARNING_POP
    #define LUPINE_DISABLE_WARNING(...)
#endif
