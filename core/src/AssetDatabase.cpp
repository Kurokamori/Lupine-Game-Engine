#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

namespace lupine {
namespace asset {

using namespace platform;
using json = nlohmann::json;

// ============================================================================
// Helper Functions
// ============================================================================

AssetImportType GetAssetImportTypeFromExtension(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Remove leading dot if present
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    
    // Image formats
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || 
        ext == "tga" || ext == "hdr" || ext == "gif" || ext == "psd") {
        return AssetImportType::Image;
    }
    
    // Model formats
    if (ext == "gltf" || ext == "glb" || ext == "fbx" || ext == "obj" || 
        ext == "dae" || ext == "blend" || ext == "3ds") {
        return AssetImportType::Model;
    }
    
    // Audio formats
    if (ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac" || ext == "aiff") {
        return AssetImportType::Audio;
    }
    
    // Font formats
    if (ext == "ttf" || ext == "otf" || ext == "woff" || ext == "woff2") {
        return AssetImportType::Font;
    }
    
    // Scene/Prefab formats
    if (ext == "scene" || ext == "lupine") {
        return AssetImportType::Scene;
    }
    if (ext == "prefab") {
        return AssetImportType::Prefab;
    }
    
    // Script formats
    if (ext == "lua" || ext == "py" || ext == "rb") {
        return AssetImportType::Script;
    }
    
    // Shader formats
    if (ext == "glsl" || ext == "hlsl" || ext == "vert" || ext == "frag" || 
        ext == "geom" || ext == "comp" || ext == "shader") {
        return AssetImportType::Shader;
    }
    
    // Animation formats
    if (ext == "anim") {
        return AssetImportType::Animation;
    }

    // Archetype formats
    if (ext == "archetype" || ext == "ares") {
        return AssetImportType::Archetype;
    }

    // UI theme formats
    if (ext == "uitheme") {
        return AssetImportType::Theme;
    }
    if (ext == "palette") {
        return AssetImportType::ColorPalette;
    }

    return AssetImportType::Unknown;
}

std::string GetAssetImportTypeExtensions(AssetImportType type) {
    switch (type) {
        case AssetImportType::Image:
            return "*.png,*.jpg,*.jpeg,*.bmp,*.tga,*.hdr,*.gif,*.psd";
        case AssetImportType::Model:
            return "*.gltf,*.glb,*.fbx,*.obj,*.dae,*.blend,*.3ds";
        case AssetImportType::Audio:
            return "*.wav,*.mp3,*.ogg,*.flac,*.aiff";
        case AssetImportType::Font:
            return "*.ttf,*.otf,*.woff,*.woff2";
        case AssetImportType::Scene:
            return "*.scene,*.lupine";
        case AssetImportType::Prefab:
            return "*.prefab";
        case AssetImportType::Script:
            return "*.lua,*.py,*.rb";
        case AssetImportType::Shader:
            return "*.glsl,*.hlsl,*.vert,*.frag,*.geom,*.comp,*.shader";
        case AssetImportType::Animation:
            return "*.anim";
        case AssetImportType::Archetype:
            return "*.archetype,*.ares";
        case AssetImportType::Theme:
            return "*.uitheme";
        case AssetImportType::ColorPalette:
            return "*.palette";
        default:
            return "*.*";
    }
}

// ============================================================================
// AssetMeta Serialization
// ============================================================================

std::string AssetMeta::ToJson() const {
    json j;
    j["uuid"] = uuid.ToString();
    j["path"] = path;
    j["originalPath"] = originalPath;
    j["importType"] = static_cast<int>(importType);
    j["lastModified"] = lastModified;
    j["metaVersion"] = metaVersion;
    
    json settings = json::object();
    for (const auto& [key, value] : importSettings) {
        settings[key] = value;
    }
    j["importSettings"] = settings;
    
    return j.dump(2);
}

AssetMeta AssetMeta::FromJson(const std::string& jsonStr) {
    AssetMeta meta;
    
    try {
        json j = json::parse(jsonStr);
        
        if (j.contains("uuid") && j["uuid"].is_string()) {
            meta.uuid = core::UUID::FromString(j["uuid"].get<std::string>());
        }
        if (j.contains("path") && j["path"].is_string()) {
            meta.path = j["path"].get<std::string>();
        }
        if (j.contains("originalPath") && j["originalPath"].is_string()) {
            meta.originalPath = j["originalPath"].get<std::string>();
        }
        if (j.contains("importType") && j["importType"].is_number()) {
            meta.importType = static_cast<AssetImportType>(j["importType"].get<int>());
        }
        if (j.contains("lastModified") && j["lastModified"].is_number()) {
            meta.lastModified = j["lastModified"].get<int64_t>();
        }
        if (j.contains("metaVersion") && j["metaVersion"].is_number()) {
            meta.metaVersion = j["metaVersion"].get<int64_t>();
        }
        if (j.contains("importSettings") && j["importSettings"].is_object()) {
            for (auto& [key, value] : j["importSettings"].items()) {
                if (value.is_string()) {
                    meta.importSettings[key] = value.get<std::string>();
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Core, "Failed to parse AssetMeta JSON: {}", e.what());
    }

    return meta;
}

// ============================================================================
// AssetDatabase Implementation
// ============================================================================

AssetDatabase& AssetDatabase::GetInstance() {
    static AssetDatabase instance;
    return instance;
}

bool AssetDatabase::Initialize(const std::string& projectRoot) {
    LockGuard lock(m_Mutex);

    if (m_Initialized) {
        LOG_WARN(LogCategory::Core, "AssetDatabase already initialized");
        return true;
    }

    // In pack/export mode there is no physical project root on disk; assets are
    // served from the mounted pack. Initialize without a disk root in that case
    // so font/theme/asset resolution can proceed against the pack.
    if (platform::PackFileSystem::Instance().isPackMode()) {
        m_ProjectRoot = projectRoot.empty() ? std::string() : Path::Normalize(projectRoot);
        m_Initialized = true;
        return true;
    }

    if (projectRoot.empty()) {
        LOG_ERROR(LogCategory::Core, "AssetDatabase: project root cannot be empty");
        return false;
    }

    // Normalize and store project root
    m_ProjectRoot = Path::Normalize(projectRoot);

    if (!FileSystem::Exists(m_ProjectRoot)) {
        LOG_ERROR(LogCategory::Core, "AssetDatabase: project root does not exist: {}", m_ProjectRoot);
        return false;
    }

    m_Initialized = true;

    return true;
}

void AssetDatabase::Shutdown() {
    LockGuard lock(m_Mutex);

    m_UUIDToMeta.clear();
    m_PathToUUID.clear();
    m_ProjectRoot.clear();
    m_Initialized = false;

}

bool AssetDatabase::IsInitialized() const {
    LockGuard lock(m_Mutex);
    return m_Initialized;
}

// ============================================================================
// Path Conversion
// ============================================================================

std::string AssetDatabase::ToResourcePath(const std::string& path) const {
    if (path.empty()) {
        return "";
    }

    // Already a res:// path - normalize it
    if (path.size() >= 6 && path.substr(0, 6) == "res://") {
        // Normalize the path
        std::string relativePart = path.substr(6);
        relativePart = Path::Normalize(relativePart);
        return "res://" + Path::ToUnixSeparators(relativePart);
    }

    // Check for other virtual paths (user://, temp://, app://)
    size_t schemeEnd = path.find("://");
    if (schemeEnd != std::string::npos) {
        std::string scheme = path.substr(0, schemeEnd + 3);
        if (scheme != "res://") {
            // Other virtual paths - convert through VFS if possible
            auto& vfs = VirtualFileSystem::GetInstance();
            auto resolveResult = vfs.ResolvePath(path);
            if (resolveResult.success) {
                return PhysicalToResourcePath(resolveResult.data);
            }
            LOG_WARN(LogCategory::Core, "Cannot convert {} path to res:// path: {}", scheme, path);
            return "";
        }
    }

    // It's a regular path - convert to res://
    return PhysicalToResourcePath(path);
}

std::string AssetDatabase::PhysicalToResourcePath(const std::string& physicalPath) const {
    if (physicalPath.empty() || m_ProjectRoot.empty()) {
        return "";
    }

    // Normalize both paths for comparison
    std::string normalizedPath = Path::Normalize(physicalPath);
    std::string normalizedRoot = m_ProjectRoot;

    // Convert to unix separators for consistent comparison
    normalizedPath = Path::ToUnixSeparators(normalizedPath);
    normalizedRoot = Path::ToUnixSeparators(normalizedRoot);

    // Ensure root ends without separator for comparison
    if (!normalizedRoot.empty() && normalizedRoot.back() == '/') {
        normalizedRoot = normalizedRoot.substr(0, normalizedRoot.size() - 1);
    }

    // Build comparison keys. On Windows the filesystem is case-insensitive and
    // the drive-letter case in particular varies depending on how a path was
    // obtained (recent-projects list, file dialog, command line, drag-drop), so
    // compare case-insensitively to avoid spuriously rejecting in-project paths.
    std::string compPath = normalizedPath;
    std::string compRoot = normalizedRoot;
#if defined(LUPINE_PLATFORM_WINDOWS)
    std::transform(compPath.begin(), compPath.end(), compPath.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(compRoot.begin(), compRoot.end(), compRoot.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif

    // Check if path is within project root. Require a path-segment boundary
    // right after the root so a root like "H:/Proj" does not match "H:/Project2".
    if (compPath.size() >= compRoot.size() &&
        compPath.compare(0, compRoot.size(), compRoot) == 0 &&
        (compPath.size() == compRoot.size() || compPath[compRoot.size()] == '/')) {

        std::string relativePath = normalizedPath.substr(normalizedRoot.size());

        // Remove leading separator if present
        if (!relativePath.empty() && relativePath[0] == '/') {
            relativePath = relativePath.substr(1);
        }

        return "res://" + relativePath;
    }

    // Path is outside project root
    LOG_WARN(LogCategory::Core, "Path is outside project root, cannot convert to res://: {}", physicalPath);
    return "";
}

std::string AssetDatabase::ResourceToPhysicalPath(const std::string& resPath) const {
    if (resPath.empty() || m_ProjectRoot.empty()) {
        return "";
    }

    if (resPath.size() < 6 || resPath.substr(0, 6) != "res://") {
        // Not a res:// path - return as-is
        return resPath;
    }

    std::string relativePath = resPath.substr(6);

    // res:// paths come from scenes, prefabs and project config, so the relative segment
    // is untrusted: resolve it without letting ".." climb out of the project root.
    std::string physicalPath = Path::JoinSandboxed(m_ProjectRoot, relativePath);
    if (physicalPath.empty()) {
        LOG_ERROR(LogCategory::Core,
                  "AssetDatabase: rejected resource path '{}' - it escapes the project root", resPath);
        return "";
    }

    return physicalPath;
}

bool AssetDatabase::IsResourcePath(const std::string& path) const {
    return path.size() >= 6 && path.substr(0, 6) == "res://";
}

bool AssetDatabase::IsAbsolutePath(const std::string& path) const {
    return Path::IsAbsolute(path);
}

// ============================================================================
// Asset Resolution
// ============================================================================

std::string AssetDatabase::ResolveAsset(const std::string& pathOrUUID) const {
    if (pathOrUUID.empty()) {
        return "";
    }

    // First, try to parse as UUID (16 hex characters)
    if (pathOrUUID.size() == 16) {
        bool allHex = true;
        for (char c : pathOrUUID) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                allHex = false;
                break;
            }
        }

        if (allHex) {
            core::UUID uuid = core::UUID::FromString(pathOrUUID);
            if (uuid.IsValid()) {
                std::string path = GetPathForUUID(uuid);
                if (!path.empty()) {
                    return ResourceToPhysicalPath(path);
                }
            }
        }
    }

    // Try as res:// path
    if (IsResourcePath(pathOrUUID)) {
        return ResourceToPhysicalPath(pathOrUUID);
    }

    // Try to convert to res:// path
    std::string resPath = ToResourcePath(pathOrUUID);
    if (!resPath.empty()) {
        return ResourceToPhysicalPath(resPath);
    }

    // As last resort, return the original path
    return pathOrUUID;
}

std::string AssetDatabase::ResolveAssetWithFallback(const core::UUID& uuid, const std::string& fallbackPath) const {
    // Try UUID first
    if (uuid.IsValid()) {
        std::string path = GetPathForUUID(uuid);
        if (!path.empty()) {
            std::string physicalPath = ResourceToPhysicalPath(path);
            if (FileSystem::Exists(physicalPath)) {
                return physicalPath;
            }
        }
    }

    // Fall back to path
    if (!fallbackPath.empty()) {
        return ResolveAsset(fallbackPath);
    }

    return "";
}

// ============================================================================
// UUID and Asset Management
// ============================================================================

bool AssetDatabase::NeedsReimport(const std::string& path) const {
    if (!m_Initialized) {
        return true;  // Can't check, assume needs import
    }

    std::string resPath = ToResourcePath(path);
    if (resPath.empty()) {
        return true;  // New asset or invalid path
    }

    std::string physicalPath = ResourceToPhysicalPath(resPath);
    if (physicalPath.empty() || !FileSystem::Exists(physicalPath)) {
        return false;  // File doesn't exist, nothing to import
    }

    // Check if meta file exists
    std::string metaPath = GetMetaFilePath(physicalPath);
    if (!FileSystem::Exists(metaPath)) {
        return true;  // No meta file, needs import
    }

    // Load meta file and check modification time
    auto readResult = FileSystem::ReadFile(metaPath);
    if (!readResult.success) {
        return true;  // Can't read meta, needs reimport
    }

    AssetMeta meta = AssetMeta::FromJson(readResult.data);
    if (!meta.uuid.IsValid()) {
        return true;  // Invalid meta, needs reimport
    }

    // Get current file modification time
    auto modTimeResult = FileSystem::GetModificationTime(physicalPath);
    if (!modTimeResult.success) {
        return true;  // Can't get mod time, assume needs import
    }

    // Compare modification times
    return modTimeResult.data > meta.lastModified;
}

AssetMeta AssetDatabase::ImportAsset(const std::string& path) {
    std::string resPath = ToResourcePath(path);
    if (resPath.empty()) {
        LOG_ERROR(LogCategory::Core, "Cannot import asset with invalid path: {}", path);
        return AssetMeta();
    }

    std::string physicalPath = ResourceToPhysicalPath(resPath);

    // Load or create meta file
    AssetMeta meta = LoadOrCreateMetaFile(physicalPath);
    meta.path = resPath;
    meta.originalPath = path;

    // Register in database
    {
        LockGuard lock(m_Mutex);
        m_UUIDToMeta[meta.uuid] = meta;
        m_PathToUUID[resPath] = meta.uuid;
    }

    return meta;
}

const AssetMeta* AssetDatabase::GetAssetByUUID(const core::UUID& uuid) const {
    LockGuard lock(m_Mutex);

    auto it = m_UUIDToMeta.find(uuid);
    if (it != m_UUIDToMeta.end()) {
        return &it->second;
    }
    return nullptr;
}

const AssetMeta* AssetDatabase::GetAssetByPath(const std::string& path) const {
    std::string resPath = path;
    if (!IsResourcePath(path)) {
        resPath = ToResourcePath(path);
    }

    LockGuard lock(m_Mutex);

    auto it = m_PathToUUID.find(resPath);
    if (it != m_PathToUUID.end()) {
        auto metaIt = m_UUIDToMeta.find(it->second);
        if (metaIt != m_UUIDToMeta.end()) {
            return &metaIt->second;
        }
    }
    return nullptr;
}

bool AssetDatabase::AssetExists(const core::UUID& uuid) const {
    LockGuard lock(m_Mutex);
    return m_UUIDToMeta.find(uuid) != m_UUIDToMeta.end();
}

bool AssetDatabase::AssetExists(const std::string& path) const {
    return GetAssetByPath(path) != nullptr;
}

core::UUID AssetDatabase::GetUUIDForPath(const std::string& path) const {
    const AssetMeta* meta = GetAssetByPath(path);
    return meta ? meta->uuid : core::UUID(0);
}

std::string AssetDatabase::GetPathForUUID(const core::UUID& uuid) const {
    const AssetMeta* meta = GetAssetByUUID(uuid);
    return meta ? meta->path : "";
}

// ============================================================================
// Meta File Management
// ============================================================================

std::string AssetDatabase::GetMetaFilePath(const std::string& assetPath) {
    return assetPath + ".meta";
}

bool AssetDatabase::MetaFileExists(const std::string& assetPath) {
    return FileSystem::Exists(GetMetaFilePath(assetPath));
}

AssetMeta AssetDatabase::LoadOrCreateMetaFile(const std::string& assetPath) {
    std::string metaPath = GetMetaFilePath(assetPath);

    AssetMeta meta;

    if (FileSystem::Exists(metaPath)) {
        // Load existing meta file
        auto readResult = FileSystem::ReadFile(metaPath);
        if (readResult.success) {
            meta = AssetMeta::FromJson(readResult.data);

            // Verify UUID is valid
            if (!meta.uuid.IsValid()) {
                LOG_WARN(LogCategory::Core, "Meta file has invalid UUID, regenerating: {}", metaPath);
                meta.uuid = core::UUID();
            }
        } else {
            LOG_WARN(LogCategory::Core, "Failed to read meta file, creating new: {}", metaPath);
            meta.uuid = core::UUID();
        }
    } else {
        // Create new meta with fresh UUID
        meta.uuid = core::UUID();
    }

    // Set the path - convert physical path to res:// path
    meta.path = PhysicalToResourcePath(assetPath);
    if (meta.path.empty()) {
        // If conversion fails, the path might already be a res:// path or outside project
        LOG_WARN(LogCategory::Core, "Cannot convert path to res:// format: {}", assetPath);
    }

    // Update metadata
    std::string ext = Path::GetExtension(assetPath);
    meta.importType = GetAssetImportTypeFromExtension(ext);

    // Get file modification time
    auto modTimeResult = FileSystem::GetModificationTime(assetPath);
    if (modTimeResult.success) {
        meta.lastModified = modTimeResult.data;
    }

    // Only save if we have a valid path
    if (!meta.path.empty() && meta.uuid.IsValid()) {
        SaveMetaFile(meta);
    }

    return meta;
}

bool AssetDatabase::SaveMetaFile(const AssetMeta& meta) {
    if (!meta.uuid.IsValid() || meta.path.empty()) {
        LOG_ERROR(LogCategory::Core, "Cannot save meta file with invalid UUID or path");
        return false;
    }

    std::string physicalPath = ResourceToPhysicalPath(meta.path);
    std::string metaPath = GetMetaFilePath(physicalPath);

    std::string jsonContent = meta.ToJson();

    auto writeResult = FileSystem::WriteFile(metaPath, jsonContent);
    if (!writeResult.success) {
        LOG_ERROR(LogCategory::Core, "Failed to write meta file: {} - {}", metaPath, writeResult.error);
        return false;
    }

    return true;
}

// ============================================================================
// Scanning and Refresh
// ============================================================================

void AssetDatabase::ScanAndImportAll(std::function<void(int, int, const std::string&)> progressCallback) {
    if (!m_Initialized) {
        LOG_ERROR(LogCategory::Core, "AssetDatabase not initialized");
        return;
    }

    // Collect all asset files
    std::vector<std::string> assetFiles;
    ScanDirectory(m_ProjectRoot, assetFiles);

    int total = static_cast<int>(assetFiles.size());
    int current = 0;

    for (const std::string& path : assetFiles) {
        if (progressCallback) {
            progressCallback(current, total, path);
        }

        ImportAsset(path);
        current++;
    }

}

void AssetDatabase::RefreshAsset(const std::string& path) {
    std::string resPath = ToResourcePath(path);
    if (resPath.empty()) {
        return;
    }

    std::string physicalPath = ResourceToPhysicalPath(resPath);

    if (!FileSystem::Exists(physicalPath)) {
        // Asset was deleted - remove from database
        LockGuard lock(m_Mutex);
        auto it = m_PathToUUID.find(resPath);
        if (it != m_PathToUUID.end()) {
            m_UUIDToMeta.erase(it->second);
            m_PathToUUID.erase(it);
        }
        return;
    }

    // Re-import the asset
    ImportAsset(path);
}

bool AssetDatabase::RemoveAsset(const std::string& resPath) {
    std::string normalizedPath = resPath;

    // Convert to res:// path if needed
    if (!IsResourcePath(resPath)) {
        normalizedPath = ToResourcePath(resPath);
        if (normalizedPath.empty()) {
            return false;
        }
    }

    LockGuard lock(m_Mutex);

    // Find the asset
    auto pathIt = m_PathToUUID.find(normalizedPath);
    if (pathIt == m_PathToUUID.end()) {
        return false;
    }

    core::UUID uuid = pathIt->second;

    // Get physical path for .meta file deletion
    std::string physicalPath = ResourceToPhysicalPath(normalizedPath);
    std::string metaPath = GetMetaFilePath(physicalPath);

    // Remove from maps
    m_PathToUUID.erase(pathIt);
    m_UUIDToMeta.erase(uuid);

    // Delete the .meta file
    if (FileSystem::Exists(metaPath)) {
        FileSystem::DeleteFile(metaPath);
    }

    return true;
}

std::vector<AssetMeta> AssetDatabase::GetAllAssets() const {
    LockGuard lock(m_Mutex);

    std::vector<AssetMeta> assets;
    assets.reserve(m_UUIDToMeta.size());

    for (const auto& [uuid, meta] : m_UUIDToMeta) {
        assets.push_back(meta);
    }

    return assets;
}

std::vector<AssetMeta> AssetDatabase::GetAssetsByType(AssetImportType type) const {
    LockGuard lock(m_Mutex);

    std::vector<AssetMeta> assets;

    for (const auto& [uuid, meta] : m_UUIDToMeta) {
        if (meta.importType == type) {
            assets.push_back(meta);
        }
    }

    return assets;
}

void AssetDatabase::ScanDirectory(const std::string& directory, std::vector<std::string>& outAssets) {
    auto listResult = FileSystem::ListDirectory(directory, false);
    if (!listResult.success) {
        return;
    }

    for (const std::string& path : listResult.data) {
        // Skip .meta files
        if (path.size() > 5 && path.substr(path.size() - 5) == ".meta") {
            continue;
        }

        // Skip hidden files/directories
        std::string filename = Path::GetFilename(path);
        if (!filename.empty() && filename[0] == '.') {
            continue;
        }

        if (FileSystem::IsDirectory(path)) {
            // Recurse into subdirectory
            ScanDirectory(path, outAssets);
        } else if (ShouldImportFile(path)) {
            outAssets.push_back(path);
        }
    }
}

bool AssetDatabase::ShouldImportFile(const std::string& path) const {
    std::string ext = Path::GetExtension(path);
    AssetImportType type = GetAssetImportTypeFromExtension(ext);
    return type != AssetImportType::Unknown;
}

// ============================================================================
// UUID Mapping Export/Import for Pack Files
// ============================================================================

std::string AssetDatabase::ExportUUIDMappings() const {
    LockGuard lock(m_Mutex);

    json j = json::object();
    json mappings = json::array();

    for (const auto& [uuid, meta] : m_UUIDToMeta) {
        json mapping;
        mapping["uuid"] = meta.uuid.ToString();
        mapping["path"] = meta.path;
        mapping["type"] = static_cast<int>(meta.importType);
        mappings.push_back(mapping);
    }

    j["version"] = 1;
    j["mappings"] = mappings;

    return j.dump();
}

void AssetDatabase::ImportUUIDMappings(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        if (!j.contains("mappings") || !j["mappings"].is_array()) {
            LOG_ERROR(LogCategory::Core, "Invalid UUID mapping format");
            return;
        }

        LockGuard lock(m_Mutex);

        for (const auto& mapping : j["mappings"]) {
            if (!mapping.contains("uuid") || !mapping.contains("path")) {
                continue;
            }

            AssetMeta meta;
            meta.uuid = core::UUID::FromString(mapping["uuid"].get<std::string>());
            meta.path = mapping["path"].get<std::string>();

            if (mapping.contains("type")) {
                meta.importType = static_cast<AssetImportType>(mapping["type"].get<int>());
            }

            if (meta.uuid.IsValid() && !meta.path.empty()) {
                m_UUIDToMeta[meta.uuid] = meta;
                m_PathToUUID[meta.path] = meta.uuid;
            }
        }

    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Core, "Failed to import UUID mappings: {}", e.what());
    }
}

// ============================================================================
// Default Font Management
// ============================================================================

void AssetDatabase::SetDefaultFontPath(const std::string& fontPath) {
    LockGuard lock(m_Mutex);
    m_DefaultFontPath = fontPath;
}

std::string AssetDatabase::GetDefaultFontPath() const {
    LockGuard lock(m_Mutex);

    auto& packFS = platform::PackFileSystem::Instance();

    // If project has a default font set, use it. In pack/export mode the font
    // lives only inside the pack, so check the pack before the physical disk.
    if (!m_DefaultFontPath.empty()) {
        if (packFS.isPackMode() && packFS.exists(m_DefaultFontPath)) {
            return m_DefaultFontPath;
        }
        std::string physicalPath = ResourceToPhysicalPath(m_DefaultFontPath);
        if (!physicalPath.empty() && FileSystem::Exists(physicalPath)) {
            return m_DefaultFontPath;
        }
    }

    // Fallback to the standard default font location in project
    const std::string standardDefault = "res://assets/fonts/DefaultFont.ttf";
    if (packFS.isPackMode() && packFS.exists(standardDefault)) {
        return standardDefault;
    }
    std::string physicalPath = ResourceToPhysicalPath(standardDefault);
    if (!physicalPath.empty() && FileSystem::Exists(physicalPath)) {
        return standardDefault;
    }

    // No default font available
    return "";
}

} // namespace asset
} // namespace lupine

