#pragma once

/**
 * Lupine Engine - Platform Module
 *
 * This module provides cross-platform abstractions for OS-specific functionality
 * including file operations, timing, threading, and virtual file systems.
 *
 * Supported platforms:
 * - Windows (64-bit and 32-bit)
 * - Linux (64-bit and 32-bit)
 * - macOS (64-bit)
 * - Web (Emscripten)
 *
 * Key Features:
 * - Platform Detection: Compile-time platform and compiler detection
 * - File System: Cross-platform file I/O with safe error handling
 * - Path Utilities: Path manipulation (join, normalize, etc.)
 * - Timing: High-resolution timers for profiling
 * - Threading: Mutex, LockGuard, ConditionVariable, Thread abstractions
 * - Virtual File System: Mount point system (res://, user://, temp://, app://)
 *
 * Usage:
 *   #include <lupine/platform/Platform.hpp>
 *   using namespace lupine::platform;
 *
 *   // Initialize VFS
 *   VirtualFileSystem::GetInstance().Initialize(".");
 *
 *   // Read a file
 *   auto result = FileSystem::ReadFile("data.txt");
 *   if (result.success) {
 *       std::cout << result.data;
 *   }
 *
 *   // Use virtual paths
 *   auto vfsResult = VFS_READ_FILE("res://config.json");
 *
 *   // Time operations
 *   ScopedTimer timer("MyOperation");
 *   // ... code to profile ...
 *
 *   // Threading
 *   Mutex mutex;
 *   {
 *       LockGuard lock(mutex);
 *       // ... critical section ...
 *   }
 */

// Core platform detection and macros
#include "PlatformDetection.hpp"

// Path manipulation utilities
#include "Path.hpp"

// File system operations
#include "FileSystem.hpp"

// Timing and profiling utilities
#include "Timing.hpp"

// Threading primitives
#include "Threading.hpp"

// Virtual file system
#include "VirtualFileSystem.hpp"

namespace lupine {
namespace platform {

/**
 * Platform information and utilities.
 */
class Platform {
public:
    /**
     * Gets the platform name as a string.
     * @return Platform name (e.g., "Windows", "Linux", "macOS", "Web")
     */
    static const char* GetPlatformName();

    /**
     * Gets the compiler name as a string.
     * @return Compiler name (e.g., "MSVC", "Clang", "GCC")
     */
    static const char* GetCompilerName();

    /**
     * Gets the architecture name as a string.
     * @return Architecture (e.g., "64-bit", "32-bit")
     */
    static const char* GetArchitectureName();

    /**
     * Gets the build configuration as a string.
     * @return Build configuration ("Debug" or "Release")
     */
    static const char* GetBuildConfiguration();

    /**
     * Checks if running in debug mode.
     * @return True if debug, false if release
     */
    static bool IsDebug();

    /**
     * Checks if running on Windows.
     * @return True if Windows, false otherwise
     */
    static bool IsWindows();

    /**
     * Checks if running on Linux.
     * @return True if Linux, false otherwise
     */
    static bool IsLinux();

    /**
     * Checks if running on macOS.
     * @return True if macOS, false otherwise
     */
    static bool IsMacOS();

    /**
     * Checks if running on Web (Emscripten).
     * @return True if Web, false otherwise
     */
    static bool IsWeb();

    /**
     * Gets comprehensive platform information as a formatted string.
     * @return Platform info string
     */
    static std::string GetPlatformInfo();

private:
    Platform() = delete;
};

} // namespace platform
} // namespace lupine
