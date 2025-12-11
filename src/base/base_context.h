// Based on information from https://sourceforge.net/p/predef/wiki/Home/

#define BASE_OS_NONE (0)
#define BASE_OS_WINDOWS (1 << 0)
#define BASE_OS_MACOS (1 << 1)
#define BASE_OS_LINUX (1 << 2)
#define BASE_OS_EMSCRIPTEN (1 << 3)
#define BASE_OS_COUNT (1 << 4)
#define BASE_OS_ANY_POSIX (BASE_OS_MACOS | BASE_OS_LINUX | BASE_OS_EMSCRIPTEN)

#if defined(__EMSCRIPTEN__)
    #define BASE_OS BASE_OS_EMSCRIPTEN
#elif defined(__linux__) || defined(__gnu_linux__)
    #define BASE_OS BASE_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
    #define BASE_OS BASE_OS_MACOS
#elif defined(_WIN32)
    #define BASE_OS BASE_OS_WINDOWS
#else
    #define BASE_OS BASE_OS_NONE
#endif // OS_...

#if defined(__clang__)
    #define COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define COMPILER_GCC 1
#elif defined(_MSC_VER)
    #define COMPILER_MSVC 1
#else
    #define COMPILER_STANDARD 1
#endif // COMPILER_...

#ifndef COMPILER_CLANG
    #define COMPILER_CLANG 0
#endif
#ifndef COMPILER_GCC
    #define COMPILER_GCC 0
#endif
#ifndef COMPILER_MSVC
    #define COMPILER_MSVC 0
#endif

#if COMPILER_CLANG
    #if __has_feature(address_sanitizer)
        #define BUILD_ASAN 1
    #endif
#elif COMPILER_GCC || COMPILER_MSVC
    #ifdef __SANITIZE_ADDRESS__
        #define BUILD_ASAN 1
    #endif
#endif // COMPILER_...

#ifndef BUILD_ASAN
    #define BUILD_ASAN 0
#endif
#ifndef BUILD_DEBUG
    #define BUILD_DEBUG 0
#endif

#if BASE_OS == BASE_OS_WINDOWS
    #if !defined(WINDOWS_SUBSYSTEM_WINDOWS) && !defined(WINDOWS_SUBSYSTEM_CONSOLE)
        #define WINDOWS_SUBSYSTEM_WINDOWS 0
        #define WINDOWS_SUBSYSTEM_CONSOLE 1
    #elif !defined(WINDOWS_SUBSYSTEM_WINDOWS)
        static_assert(WINDOWS_SUBSYSTEM_CONSOLE == 1, "");
        #define WINDOWS_SUBSYSTEM_WINDOWS 0
    #elif !defined(WINDOWS_SUBSYSTEM_CONSOLE)
        static_assert(WINDOWS_SUBSYSTEM_WINDOWS == 1, "");
        #define WINDOWS_SUBSYSTEM_CONSOLE 0
    #else
        static_assert(!WINDOWS_SUBSYSTEM_WINDOWS || !WINDOWS_SUBSYSTEM_CONSOLE, "");
    #endif
#endif

#if !defined(PLATFORM_NONE)
    #define PLATFORM_NONE 0
#endif
