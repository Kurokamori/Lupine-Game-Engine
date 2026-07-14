#pragma once

#include "lupine/core/UUID.hpp"
#include <string>

namespace lupine {
namespace asset {

/**
 * AssetPath - Utility class for handling asset paths
 * 
 * Provides static methods for:
 * - Converting between absolute paths and res:// paths
 * - Validating paths (ensuring they're within project boundaries)
 * - Normalizing paths for consistent storage
 * - Resolving paths with UUID fallback
 */
class AssetPath {
public:
    // Virtual path prefixes
    static constexpr const char* RES_PREFIX = "res://";
    static constexpr const char* USER_PREFIX = "user://";
    static constexpr const char* TEMP_PREFIX = "temp://";
    static constexpr const char* APP_PREFIX = "app://";
    
    /**
     * Check if a path is a res:// path
     */
    static bool IsResourcePath(const std::string& path);
    
    /**
     * Check if a path is any virtual path (res://, user://, etc.)
     */
    static bool IsVirtualPath(const std::string& path);
    
    /**
     * Check if a path is an absolute filesystem path
     */
    static bool IsAbsolutePath(const std::string& path);
    
    /**
     * Get the virtual path prefix from a path (e.g., "res://", "user://")
     * @return The prefix, or empty string if not a virtual path
     */
    static std::string GetVirtualPrefix(const std::string& path);
    
    /**
     * Get the relative part of a virtual path (without the prefix)
     */
    static std::string GetRelativePath(const std::string& virtualPath);
    
    /**
     * Normalize a res:// path
     * - Resolves . and .. components
     * - Converts backslashes to forward slashes
     * - Removes redundant slashes
     */
    static std::string Normalize(const std::string& path);
    
    /**
     * Convert any path to a res:// path
     * Uses AssetDatabase for project root resolution
     * @param path Any valid path (absolute, relative, or already res://)
     * @return res:// path, or empty string if conversion fails
     */
    static std::string ToResourcePath(const std::string& path);
    
    /**
     * Convert a res:// path to a physical filesystem path
     * Uses VFS for resolution
     */
    static std::string ToPhysicalPath(const std::string& resPath);
    
    /**
     * Resolve an asset path, trying UUID first then path
     * @param pathOrUUID Either a res:// path or a UUID string
     * @return Physical path to the asset, or empty if not found
     */
    static std::string Resolve(const std::string& pathOrUUID);
    
    /**
     * Resolve with UUID and path fallback
     * @param uuid Primary UUID to try
     * @param fallbackPath Path to use if UUID not found
     * @return Physical path to the asset
     */
    static std::string ResolveWithFallback(const core::UUID& uuid, const std::string& fallbackPath);
    
    /**
     * Check if a path is valid (exists and is within project)
     */
    static bool IsValidAssetPath(const std::string& path);
    
    /**
     * Validate and sanitize a path for storage
     * - Converts to res:// if not already
     * - Normalizes the path
     * - Returns empty if path is invalid
     */
    static std::string SanitizeForStorage(const std::string& path);
    
    /**
     * Get the file extension from a path (including the dot)
     */
    static std::string GetExtension(const std::string& path);
    
    /**
     * Get the filename from a path (without directory)
     */
    static std::string GetFilename(const std::string& path);
    
    /**
     * Get the directory part of a path
     */
    static std::string GetDirectory(const std::string& path);
    
    /**
     * Join two path components
     */
    static std::string Join(const std::string& base, const std::string& relative);
    
    /**
     * Change the extension of a path
     */
    static std::string ChangeExtension(const std::string& path, const std::string& newExtension);
};

} // namespace asset
} // namespace lupine

