#pragma once

#include "PlatformDetection.hpp"
#include "Path.hpp"
#include <string>
#include <vector>
#include <cstdint>

// Undefine Windows macros that conflict with our code
// This must be done BEFORE the class declarations to avoid conflicts
#if defined(LUPINE_PLATFORM_WINDOWS)
    #ifdef CreateDirectory
        #undef CreateDirectory
    #endif
    #ifdef DeleteFile
        #undef DeleteFile
    #endif
    #ifdef CopyFile
        #undef CopyFile
    #endif
    #ifdef MoveFile
        #undef MoveFile
    #endif
    #ifdef GetCurrentDirectory
        #undef GetCurrentDirectory
    #endif
    #ifdef SetCurrentDirectory
        #undef SetCurrentDirectory
    #endif
#endif

namespace lupine {
namespace platform {

/**
 * Result structure for file operations.
 * Contains success status, error message, and optional data.
 */
template<typename T = void>
struct FileResult {
    bool success = false;
    std::string error;
    T data;

    FileResult() = default;
    FileResult(bool success, const std::string& error = "") : success(success), error(error) {}
    FileResult(bool success, const std::string& error, const T& data) : success(success), error(error), data(data) {}

    operator bool() const { return success; }
};

// Specialization for void type
template<>
struct FileResult<void> {
    bool success = false;
    std::string error;

    FileResult() = default;
    FileResult(bool success, const std::string& error = "") : success(success), error(error) {}

    operator bool() const { return success; }
};

/**
 * File metadata structure.
 */
struct FileMetadata {
    std::string path;
    uint64_t size = 0;           // File size in bytes
    int64_t modifiedTime = 0;    // Last modification time (Unix timestamp)
    bool isDirectory = false;
    bool isFile = false;
    bool exists = false;
};

/**
 * Cross-platform file system operations.
 * All operations use safe error handling and return FileResult structures.
 */
class FileSystem {
public:
    /**
     * Reads an entire file into memory as a string.
     * @param path Path to the file
     * @return FileResult containing the file contents or error
     */
    static FileResult<std::string> ReadFile(const std::string& path);

    /**
     * Reads an entire file into memory as binary data.
     * @param path Path to the file
     * @return FileResult containing the binary data or error
     */
    static FileResult<std::vector<uint8_t>> ReadBinaryFile(const std::string& path);

    /**
     * Writes a string to a file, creating or overwriting it.
     * @param path Path to the file
     * @param contents Contents to write
     * @return FileResult indicating success or failure
     */
    static FileResult<> WriteFile(const std::string& path, const std::string& contents);

    /**
     * Writes binary data to a file, creating or overwriting it.
     * @param path Path to the file
     * @param data Binary data to write
     * @return FileResult indicating success or failure
     */
    static FileResult<> WriteBinaryFile(const std::string& path, const std::vector<uint8_t>& data);

    /**
     * Appends a string to a file.
     * @param path Path to the file
     * @param contents Contents to append
     * @return FileResult indicating success or failure
     */
    static FileResult<> AppendFile(const std::string& path, const std::string& contents);

    /**
     * Checks if a file or directory exists.
     * @param path Path to check
     * @return True if exists, false otherwise
     */
    static bool Exists(const std::string& path);

    /**
     * Checks if a path points to a file.
     * @param path Path to check
     * @return True if it's a file, false otherwise
     */
    static bool IsFile(const std::string& path);

    /**
     * Checks if a path points to a directory.
     * @param path Path to check
     * @return True if it's a directory, false otherwise
     */
    static bool IsDirectory(const std::string& path);

    /**
     * Gets the size of a file in bytes.
     * @param path Path to the file
     * @return FileResult containing the file size or error
     */
    static FileResult<uint64_t> GetFileSize(const std::string& path);

    /**
     * Gets the last modification time of a file.
     * @param path Path to the file
     * @return FileResult containing the modification time (Unix timestamp) or error
     */
    static FileResult<int64_t> GetModificationTime(const std::string& path);

    /**
     * Gets complete metadata for a file or directory.
     * @param path Path to check
     * @return FileResult containing the metadata or error
     */
    static FileResult<FileMetadata> GetMetadata(const std::string& path);

    /**
     * Creates a directory.
     * @param path Path to the directory
     * @param recursive If true, creates parent directories as needed
     * @return FileResult indicating success or failure
     */
    static FileResult<> CreateDirectory(const std::string& path, bool recursive = false);

    /**
     * Deletes a file.
     * @param path Path to the file
     * @return FileResult indicating success or failure
     */
    static FileResult<> DeleteFile(const std::string& path);

    /**
     * Deletes a directory.
     * @param path Path to the directory
     * @param recursive If true, deletes contents recursively
     * @return FileResult indicating success or failure
     */
    static FileResult<> DeleteDirectory(const std::string& path, bool recursive = false);

    /**
     * Copies a file.
     * @param source Source file path
     * @param destination Destination file path
     * @param overwrite If true, overwrites existing destination file
     * @return FileResult indicating success or failure
     */
    static FileResult<> CopyFile(const std::string& source, const std::string& destination, bool overwrite = false);

    /**
     * Moves/renames a file.
     * @param source Source file path
     * @param destination Destination file path
     * @return FileResult indicating success or failure
     */
    static FileResult<> MoveFile(const std::string& source, const std::string& destination);

    /**
     * Lists all files and directories in a directory.
     * @param path Path to the directory
     * @param recursive If true, lists recursively
     * @return FileResult containing the list of paths or error
     */
    static FileResult<std::vector<std::string>> ListDirectory(const std::string& path, bool recursive = false);

    /**
     * Gets the current working directory.
     * @return FileResult containing the current directory or error
     */
    static FileResult<std::string> GetCurrentDirectory();

    /**
     * Sets the current working directory.
     * @param path Path to set as current directory
     * @return FileResult indicating success or failure
     */
    static FileResult<> SetCurrentDirectory(const std::string& path);

    /**
     * Gets the absolute path from a relative path.
     * @param path Relative path
     * @return FileResult containing the absolute path or error
     */
    static FileResult<std::string> GetAbsolutePath(const std::string& path);

    /**
     * Gets the canonical path (resolved symlinks, normalized).
     * @param path Path to resolve
     * @return FileResult containing the canonical path or error
     */
    static FileResult<std::string> GetCanonicalPath(const std::string& path);

private:
    /**
     * Helper to list directory contents recursively.
     * @param path Directory path
     * @param results Output vector to store results
     * @param baseDepth Base depth for recursion tracking
     */
    static void ListDirectoryRecursive(const std::string& path, std::vector<std::string>& results, size_t baseDepth = 0);
};

} // namespace platform
} // namespace lupine
