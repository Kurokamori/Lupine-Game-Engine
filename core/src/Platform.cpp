#include "lupine/platform/Platform.hpp"
#include <sstream>

namespace lupine {
namespace platform {

const char* Platform::GetPlatformName() {
#if defined(LUPINE_PLATFORM_WINDOWS)
    return "Windows";
#elif defined(LUPINE_PLATFORM_LINUX)
    return "Linux";
#elif defined(LUPINE_PLATFORM_MACOS)
    return "macOS";
#elif defined(LUPINE_PLATFORM_WEB)
    return "Web";
#else
    return "Unknown";
#endif
}

const char* Platform::GetCompilerName() {
#if defined(LUPINE_COMPILER_MSVC)
    return "MSVC";
#elif defined(LUPINE_COMPILER_CLANG)
    return "Clang";
#elif defined(LUPINE_COMPILER_GCC)
    return "GCC";
#else
    return "Unknown";
#endif
}

const char* Platform::GetArchitectureName() {
#if defined(LUPINE_PLATFORM_64BIT)
    return "64-bit";
#elif defined(LUPINE_PLATFORM_32BIT)
    return "32-bit";
#else
    return "Unknown";
#endif
}

const char* Platform::GetBuildConfiguration() {
#if defined(LUPINE_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

bool Platform::IsDebug() {
#if defined(LUPINE_DEBUG)
    return true;
#else
    return false;
#endif
}

bool Platform::IsWindows() {
#if defined(LUPINE_PLATFORM_WINDOWS)
    return true;
#else
    return false;
#endif
}

bool Platform::IsLinux() {
#if defined(LUPINE_PLATFORM_LINUX)
    return true;
#else
    return false;
#endif
}

bool Platform::IsMacOS() {
#if defined(LUPINE_PLATFORM_MACOS)
    return true;
#else
    return false;
#endif
}

bool Platform::IsWeb() {
#if defined(LUPINE_PLATFORM_WEB)
    return true;
#else
    return false;
#endif
}

std::string Platform::GetPlatformInfo() {
    std::ostringstream oss;
    oss << "Lupine Engine - Platform Information\n";
    oss << "=====================================\n";
    oss << "Platform:      " << GetPlatformName() << "\n";
    oss << "Architecture:  " << GetArchitectureName() << "\n";
    oss << "Compiler:      " << GetCompilerName() << "\n";
    oss << "Configuration: " << GetBuildConfiguration() << "\n";
    return oss.str();
}

}
}
