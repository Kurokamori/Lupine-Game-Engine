#pragma once

#include "PlatformDetection.hpp"
#include <string>
#include <vector>

namespace lupine {
namespace platform {

/**
 * Cross-platform path manipulation utilities.
 * Handles both forward slashes (/) and backslashes (\) as path separators.
 */
class Path {
public:
    // Platform-specific path separator
#if defined(LUPINE_PLATFORM_WINDOWS)
    static constexpr char SEPARATOR = '\\';
    static constexpr char ALT_SEPARATOR = '/';
    static constexpr const char* SEPARATOR_STR = "\\";
#else
    static constexpr char SEPARATOR = '/';
    static constexpr char ALT_SEPARATOR = '\\';
    static constexpr const char* SEPARATOR_STR = "/";
#endif

    /**
     * Joins multiple path segments into a single path.
     * Handles redundant separators and normalizes the result.
     * @param segments Path segments to join
     * @return Joined path
     */
    static std::string Join(const std::vector<std::string>& segments);

    /**
     * Joins two path segments.
     * @param path1 First path segment
     * @param path2 Second path segment
     * @return Joined path
     */
    static std::string Join(const std::string& path1, const std::string& path2);

    /**
     * Joins three path segments.
     * @param path1 First path segment
     * @param path2 Second path segment
     * @param path3 Third path segment
     * @return Joined path
     */
    static std::string Join(const std::string& path1, const std::string& path2, const std::string& path3);

    /**
     * Normalizes a path by resolving . and .. references and standardizing separators.
     * @param path Path to normalize
     * @return Normalized path
     */
    static std::string Normalize(const std::string& path);

    /**
     * Gets the directory portion of a path.
     * Example: "foo/bar/file.txt" -> "foo/bar"
     * @param path Path to process
     * @return Directory portion
     */
    static std::string GetDirectory(const std::string& path);

    /**
     * Gets the filename portion of a path (including extension).
     * Example: "foo/bar/file.txt" -> "file.txt"
     * @param path Path to process
     * @return Filename portion
     */
    static std::string GetFilename(const std::string& path);

    /**
     * Gets the filename without extension.
     * Example: "foo/bar/file.txt" -> "file"
     * @param path Path to process
     * @return Filename without extension
     */
    static std::string GetFilenameWithoutExtension(const std::string& path);

    /**
     * Gets the file extension (including the dot).
     * Example: "foo/bar/file.txt" -> ".txt"
     * @param path Path to process
     * @return File extension
     */
    static std::string GetExtension(const std::string& path);

    /**
     * Checks if a path is absolute.
     * On Windows: Checks for drive letter (C:\) or UNC path (\\server\)
     * On Unix: Checks if path starts with /
     * @param path Path to check
     * @return True if absolute, false if relative
     */
    static bool IsAbsolute(const std::string& path);

    /**
     * Checks if a path is relative.
     * @param path Path to check
     * @return True if relative, false if absolute
     */
    static bool IsRelative(const std::string& path);

    /**
     * Converts all path separators to the platform-specific separator.
     * @param path Path to convert
     * @return Path with platform-specific separators
     */
    static std::string ToNativeSeparators(const std::string& path);

    /**
     * Converts all path separators to forward slashes (/).
     * Useful for consistent path handling across platforms.
     * @param path Path to convert
     * @return Path with forward slashes
     */
    static std::string ToUnixSeparators(const std::string& path);

    /**
     * Checks if a character is a path separator (/ or \).
     * @param c Character to check
     * @return True if separator, false otherwise
     */
    static bool IsSeparator(char c);

    /**
     * Removes trailing separators from a path.
     * @param path Path to process
     * @return Path without trailing separators
     */
    static std::string RemoveTrailingSeparators(const std::string& path);

    /**
     * Splits a path into its component segments.
     * Example: "foo/bar/baz" -> ["foo", "bar", "baz"]
     * @param path Path to split
     * @return Vector of path segments
     */
    static std::vector<std::string> Split(const std::string& path);

    /**
     * Checks if a path contains any invalid characters.
     * @param path Path to check
     * @return True if path is valid, false otherwise
     */
    static bool IsValid(const std::string& path);

private:
    /**
     * Checks if a character is a valid path character.
     * @param c Character to check
     * @return True if valid, false otherwise
     */
    static bool IsValidPathChar(char c);
};

} // namespace platform
} // namespace lupine
