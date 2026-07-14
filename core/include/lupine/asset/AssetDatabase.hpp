#pragma once

#include "lupine/core/UUID.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/Threading.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace lupine {
namespace asset {

/**
 * Asset types that can be tracked in the database
 */
enum class AssetImportType {
    Unknown,
    Image,      // PNG, JPG, BMP, TGA, HDR
    Model,      // GLTF, GLB, FBX, OBJ
    Audio,      // WAV, MP3, OGG
    Font,       // TTF, OTF
    Scene,      // .scene files
    Prefab,     // .prefab files
    Script,     // .lua, .py, .rb
    Shader,     // .glsl, .hlsl
    Animation,  // .anim
    Archetype,  // .archetype (definition schema), .ares (instance)
    Theme,      // .uitheme (UI theme)
    ColorPalette, // .palette (colour palette)
    Other       // Any other file type
};

/**
 * Get asset import type from file extension
 */
AssetImportType GetAssetImportTypeFromExtension(const std::string& extension);

/**
 * Get file extension filter for asset type
 */
std::string GetAssetImportTypeExtensions(AssetImportType type);

/**
 * Asset metadata structure
 * Stored in .meta files alongside each asset
 */
struct AssetMeta {
    core::UUID uuid;                    // Unique identifier for this asset
    std::string path;                   // res:// relative path
    std::string originalPath;           // Original path before conversion (for debugging)
    AssetImportType importType;         // Type of asset
    int64_t lastModified;               // Last modification time of the source file
    int64_t metaVersion;                // Version of meta file format
    std::unordered_map<std::string, std::string> importSettings; // Import-specific settings
    
    AssetMeta() : importType(AssetImportType::Unknown), lastModified(0), metaVersion(1) {}
    
    // Serialization
    std::string ToJson() const;
    static AssetMeta FromJson(const std::string& json);
    
    bool IsValid() const { return uuid.IsValid() && !path.empty(); }
};

/**
 * AssetDatabase - Central registry for all assets in a project
 * 
 * Features:
 * - UUID-based asset identification
 * - .meta file generation and management
 * - Path resolution (UUID -> path and path -> UUID)
 * - Enforces res:// relative paths for all assets
 * - Thread-safe operations
 */
class AssetDatabase {
public:
    /**
     * Get the singleton instance
     */
    static AssetDatabase& GetInstance();
    
    /**
     * Initialize the database with a project root directory
     * @param projectRoot The root directory of the project (where res:// points to)
     * @return true if initialization was successful
     */
    bool Initialize(const std::string& projectRoot);
    
    /**
     * Shutdown and clear all cached data
     */
    void Shutdown();
    
    /**
     * Check if database is initialized
     */
    bool IsInitialized() const;
    
    /**
     * Get the project root directory
     */
    const std::string& GetProjectRoot() const { return m_ProjectRoot; }
    
    // ========================================================================
    // Asset Registration and UUID Management
    // ========================================================================
    
    /**
     * Import/register an asset, generating a UUID and .meta file if needed
     * @param path Path to the asset (will be converted to res:// if needed)
     * @return The asset metadata (with UUID)
     */
    AssetMeta ImportAsset(const std::string& path);

    /**
     * Check if an asset needs to be reimported (file has been modified since last import)
     * @param path Path to the asset (can be physical or res://)
     * @return true if asset needs reimporting, false if up to date
     */
    bool NeedsReimport(const std::string& path) const;

    /**
     * Get asset metadata by UUID
     * @param uuid The asset's UUID
     * @return Pointer to metadata, or nullptr if not found
     */
    const AssetMeta* GetAssetByUUID(const core::UUID& uuid) const;
    
    /**
     * Get asset metadata by path
     * @param path The res:// path to the asset
     * @return Pointer to metadata, or nullptr if not found
     */
    const AssetMeta* GetAssetByPath(const std::string& path) const;
    
    /**
     * Check if an asset exists (by UUID or path)
     */
    bool AssetExists(const core::UUID& uuid) const;
    bool AssetExists(const std::string& path) const;
    
    /**
     * Get UUID for a path
     * @param path The res:// path
     * @return UUID if found, invalid UUID otherwise
     */
    core::UUID GetUUIDForPath(const std::string& path) const;
    
    /**
     * Get path for a UUID
     * @param uuid The asset's UUID
     * @return res:// path if found, empty string otherwise
     */
    std::string GetPathForUUID(const core::UUID& uuid) const;
    
    // ========================================================================
    // Path Conversion and Resolution
    // ========================================================================
    
    /**
     * Convert any path to a res:// path
     * - Absolute paths are converted to res:// relative paths
     * - Paths already starting with res:// are normalized
     * - Invalid paths (outside project root) return empty string
     * @param path Any file path
     * @return res:// path or empty string if conversion fails
     */
    std::string ToResourcePath(const std::string& path) const;

    /**
     * Resolve a res:// path or UUID string to a physical file path
     * First tries UUID resolution, then falls back to path resolution
     * @param pathOrUUID Either a res:// path or a UUID string
     * @return Physical file path, or empty string if not found
     */
    std::string ResolveAsset(const std::string& pathOrUUID) const;

    /**
     * Resolve by UUID, with path fallback
     * @param uuid Primary UUID to resolve
     * @param fallbackPath res:// path to use if UUID not found
     * @return Physical file path, or empty string if not found
     */
    std::string ResolveAssetWithFallback(const core::UUID& uuid, const std::string& fallbackPath) const;

    /**
     * Check if a path is a valid res:// path
     */
    bool IsResourcePath(const std::string& path) const;

    /**
     * Check if a path is an absolute path
     */
    bool IsAbsolutePath(const std::string& path) const;

    // ========================================================================
    // Meta File Management
    // ========================================================================

    /**
     * Load or create .meta file for an asset
     * @param assetPath Physical path to the asset file
     * @return Asset metadata
     */
    AssetMeta LoadOrCreateMetaFile(const std::string& assetPath);

    /**
     * Save .meta file for an asset
     * @param meta The metadata to save
     * @return true if save was successful
     */
    bool SaveMetaFile(const AssetMeta& meta);

    /**
     * Get the .meta file path for an asset
     */
    static std::string GetMetaFilePath(const std::string& assetPath);

    /**
     * Check if a .meta file exists for an asset
     */
    static bool MetaFileExists(const std::string& assetPath);

    // ========================================================================
    // Scanning and Refresh
    // ========================================================================

    /**
     * Scan the project directory and import all assets
     * @param progressCallback Optional callback for progress updates
     */
    void ScanAndImportAll(std::function<void(int current, int total, const std::string& path)> progressCallback = nullptr);

    /**
     * Refresh a single asset (reload its .meta file)
     */
    void RefreshAsset(const std::string& path);

    /**
     * Remove an asset from the database and delete its .meta file
     * @param resPath The res:// path to the asset
     * @return true if the asset was removed
     */
    bool RemoveAsset(const std::string& resPath);

    /**
     * Get all registered assets
     */
    std::vector<AssetMeta> GetAllAssets() const;

    /**
     * Get all assets of a specific type
     */
    std::vector<AssetMeta> GetAssetsByType(AssetImportType type) const;

    // ========================================================================
    // Default Font Management
    // ========================================================================

    /**
     * Set the default font path (from project settings)
     * Called when project is loaded
     * @param fontPath res:// path to the default font, or empty for engine default
     */
    void SetDefaultFontPath(const std::string& fontPath);

    /**
     * Get the default font path to use when a component has no font set
     * Returns the project's default font, or falls back to engine default
     * @return res:// path to the default font
     */
    std::string GetDefaultFontPath() const;

    // ========================================================================
    // UUID Mapping for Pack Files
    // ========================================================================

    /**
     * Export UUID-to-path mappings for inclusion in pack files
     * @return JSON string of all mappings
     */
    std::string ExportUUIDMappings() const;

    /**
     * Import UUID-to-path mappings (used when loading pack files)
     * @param json JSON string of mappings
     */
    void ImportUUIDMappings(const std::string& json);

private:
    AssetDatabase() = default;
    ~AssetDatabase() = default;

    AssetDatabase(const AssetDatabase&) = delete;
    AssetDatabase& operator=(const AssetDatabase&) = delete;

    // Convert physical path to res:// path
    std::string PhysicalToResourcePath(const std::string& physicalPath) const;

    // Convert res:// path to physical path
    std::string ResourceToPhysicalPath(const std::string& resPath) const;

    // Scan directory recursively for assets
    void ScanDirectory(const std::string& directory, std::vector<std::string>& outAssets);

    // Determine if a file should be imported as an asset
    bool ShouldImportFile(const std::string& path) const;

    mutable platform::Mutex m_Mutex;
    std::string m_ProjectRoot;              // Physical path to project root
    bool m_Initialized = false;

    // Default font path (from project settings, empty = use fallback)
    std::string m_DefaultFontPath;

    // UUID -> Meta mapping
    std::unordered_map<uint64_t, AssetMeta> m_UUIDToMeta;

    // Path -> UUID mapping (for fast path lookups)
    std::unordered_map<std::string, core::UUID> m_PathToUUID;
};

// ============================================================================
// Convenience Macros
// ============================================================================

#define ASSET_DB lupine::asset::AssetDatabase::GetInstance()
#define ASSET_RESOLVE(path) lupine::asset::AssetDatabase::GetInstance().ResolveAsset(path)
#define ASSET_TO_RES(path) lupine::asset::AssetDatabase::GetInstance().ToResourcePath(path)

} // namespace asset
} // namespace lupine

