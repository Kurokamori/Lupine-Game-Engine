#pragma once

#include "PlatformDetection.hpp"
#include "FileSystem.hpp"
#include "Threading.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace lupine {
namespace platform {

/**
 * Virtual file system that allows mounting directories to virtual paths.
 * This enables using paths like "res://textures/icon.png" or "user://config.json"
 * which are mapped to actual file system locations.
 *
 * Common mount points:
 * - res://  - Resource files (assets, textures, models, etc.) - read-only in release
 * - user:// - User-specific data (save files, settings, etc.) - read-write
 * - temp:// - Temporary files - read-write
 * - app://  - Application directory - read-only
 *
 * The VFS is thread-safe and uses a singleton pattern.
 */
class VirtualFileSystem {
public:
    /**
     * Gets the singleton instance of the VFS.
     * @return Reference to the VFS instance
     */
    static VirtualFileSystem& GetInstance();

    /**
     * Initializes the VFS with default mount points.
     * This should be called once at application startup.
     * @param appPath Path to the application directory
     * @param resPath Path to the resources directory (default: appPath/resources)
     * @param userPath Path to user data directory (default: platform-specific user dir)
     * @return True if initialization was successful, false otherwise
     */
    bool Initialize(const std::string& appPath, const std::string& resPath = "", const std::string& userPath = "");

    /**
     * Shuts down the VFS and clears all mount points.
     */
    void Shutdown();

    /**
     * Checks if the VFS is initialized.
     * @return True if initialized, false otherwise
     */
    bool IsInitialized() const;

    /**
     * Mounts a directory to a virtual path prefix.
     * @param mountPoint Virtual path prefix (e.g., "res://")
     * @param physicalPath Actual file system path
     * @param readOnly If true, write operations will fail for this mount point
     * @return True if mount was successful, false otherwise
     */
    bool Mount(const std::string& mountPoint, const std::string& physicalPath, bool readOnly = false);

    /**
     * Unmounts a virtual path prefix.
     * @param mountPoint Virtual path prefix to unmount
     * @return True if unmount was successful, false otherwise
     */
    bool Unmount(const std::string& mountPoint);

    /**
     * Checks if a mount point exists.
     * @param mountPoint Virtual path prefix to check
     * @return True if mounted, false otherwise
     */
    bool IsMounted(const std::string& mountPoint) const;

    /**
     * Gets the physical path for a mount point.
     * @param mountPoint Virtual path prefix
     * @return Physical path, or empty string if not mounted
     */
    std::string GetMountPath(const std::string& mountPoint) const;

    /**
     * Resolves a virtual path to a physical path.
     * @param virtualPath Virtual path (e.g., "res://textures/icon.png")
     * @return FileResult containing the physical path or error
     */
    FileResult<std::string> ResolvePath(const std::string& virtualPath) const;

    /**
     * Lists all registered mount points.
     * @return Vector of mount point prefixes
     */
    std::vector<std::string> GetMountPoints() const;

    // ========================================================================
    // File Operations (using virtual paths)
    // ========================================================================

    /**
     * Reads a file using a virtual path.
     * @param virtualPath Virtual path to the file
     * @return FileResult containing the file contents or error
     */
    FileResult<std::string> ReadFile(const std::string& virtualPath);

    /**
     * Reads a binary file using a virtual path.
     * @param virtualPath Virtual path to the file
     * @return FileResult containing the binary data or error
     */
    FileResult<std::vector<uint8_t>> ReadBinaryFile(const std::string& virtualPath);

    /**
     * Writes a file using a virtual path.
     * @param virtualPath Virtual path to the file
     * @param contents Contents to write
     * @return FileResult indicating success or failure
     */
    FileResult<> WriteFile(const std::string& virtualPath, const std::string& contents);

    /**
     * Writes a binary file using a virtual path.
     * @param virtualPath Virtual path to the file
     * @param data Binary data to write
     * @return FileResult indicating success or failure
     */
    FileResult<> WriteBinaryFile(const std::string& virtualPath, const std::vector<uint8_t>& data);

    /**
     * Checks if a file exists using a virtual path.
     * @param virtualPath Virtual path to check
     * @return True if exists, false otherwise
     */
    bool Exists(const std::string& virtualPath);

    /**
     * Checks if a path is a file using a virtual path.
     * @param virtualPath Virtual path to check
     * @return True if it's a file, false otherwise
     */
    bool IsFile(const std::string& virtualPath);

    /**
     * Checks if a path is a directory using a virtual path.
     * @param virtualPath Virtual path to check
     * @return True if it's a directory, false otherwise
     */
    bool IsDirectory(const std::string& virtualPath);

    /**
     * Gets the size of a file using a virtual path.
     * @param virtualPath Virtual path to the file
     * @return FileResult containing the file size or error
     */
    FileResult<uint64_t> GetFileSize(const std::string& virtualPath);

    /**
     * Gets the modification time of a file using a virtual path.
     * @param virtualPath Virtual path to the file
     * @return FileResult containing the modification time or error
     */
    FileResult<int64_t> GetModificationTime(const std::string& virtualPath);

    /**
     * Gets metadata for a file using a virtual path.
     * @param virtualPath Virtual path to check
     * @return FileResult containing the metadata or error
     */
    FileResult<FileMetadata> GetMetadata(const std::string& virtualPath);

    /**
     * Creates a directory using a virtual path.
     * @param virtualPath Virtual path to the directory
     * @param recursive If true, creates parent directories as needed
     * @return FileResult indicating success or failure
     */
    FileResult<> CreateDirectory(const std::string& virtualPath, bool recursive = false);

    /**
     * Deletes a file using a virtual path.
     * @param virtualPath Virtual path to the file
     * @return FileResult indicating success or failure
     */
    FileResult<> DeleteFile(const std::string& virtualPath);

    /**
     * Lists all files in a directory using a virtual path.
     * @param virtualPath Virtual path to the directory
     * @param recursive If true, lists recursively
     * @return FileResult containing the list of paths (as virtual paths) or error
     */
    FileResult<std::vector<std::string>> ListDirectory(const std::string& virtualPath, bool recursive = false);

    /**
     * Copies a file using virtual paths.
     * @param sourceVirtualPath Source virtual path
     * @param destVirtualPath Destination virtual path
     * @param overwrite If true, overwrites existing destination
     * @return FileResult indicating success or failure
     */
    FileResult<> CopyFile(const std::string& sourceVirtualPath, const std::string& destVirtualPath, bool overwrite = false);

    /**
     * Gets the default user data directory for the current platform.
     * - Windows: %APPDATA%/Lupine
     * - Linux: ~/.local/share/Lupine
     * - macOS: ~/Library/Application Support/Lupine
     * @param appName Application name (default: "Lupine")
     * @return Path to user data directory
     */
    static std::string GetDefaultUserDataPath(const std::string& appName = "Lupine");

    /**
     * Gets the default temporary directory for the current platform.
     * @return Path to temporary directory
     */
    static std::string GetDefaultTempPath();

private:
    VirtualFileSystem() = default;
    ~VirtualFileSystem() = default;

    // Non-copyable
    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

    /**
     * Mount point information.
     */
    struct MountInfo {
        std::string physicalPath;
        bool readOnly;
    };

    /**
     * Internal mount helper without locking (assumes caller has locked).
     * @param mountPoint Virtual path prefix
     * @param physicalPath Actual file system path
     * @param readOnly If true, write operations will fail
     * @return True if successful, false otherwise
     */
    bool MountInternal(const std::string& mountPoint, const std::string& physicalPath, bool readOnly);

    /**
     * Internal helper to resolve path without locking (assumes caller has locked).
     * @param virtualPath Virtual path to resolve
     * @return Physical path or empty string on failure
     */
    std::string ResolvePathInternal(const std::string& virtualPath) const;

    /**
     * Internal helper to check read-only violation without locking.
     * @param virtualPath Virtual path to check
     * @return True if read-only violation, false otherwise
     */
    bool IsReadOnlyViolationInternal(const std::string& virtualPath) const;

    /**
     * Converts a physical path back to a virtual path for a given mount point.
     * @param physicalPath Physical path
     * @param mountPoint Mount point prefix
     * @param mountPath Physical mount path
     * @return Virtual path
     */
    std::string PhysicalToVirtual(const std::string& physicalPath, const std::string& mountPoint, const std::string& mountPath) const;

    mutable Mutex m_Mutex;
    std::unordered_map<std::string, MountInfo> m_MountPoints;
    bool m_Initialized = false;
};

// Convenience macros for common VFS operations
#define VFS_READ_FILE(path) lupine::platform::VirtualFileSystem::GetInstance().ReadFile(path)
#define VFS_WRITE_FILE(path, contents) lupine::platform::VirtualFileSystem::GetInstance().WriteFile(path, contents)
#define VFS_EXISTS(path) lupine::platform::VirtualFileSystem::GetInstance().Exists(path)
#define VFS_RESOLVE(path) lupine::platform::VirtualFileSystem::GetInstance().ResolvePath(path)

} // namespace platform
} // namespace lupine
