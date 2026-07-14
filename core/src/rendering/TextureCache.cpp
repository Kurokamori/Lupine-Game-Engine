#include "lupine/rendering/TextureCache.hpp"
#include "lupine/logger/Logger.hpp"
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace lupine {
namespace rendering {

// ============================================================================
// Asset type detection from file extension
// ============================================================================

AssetType GetAssetTypeFromExtension(const std::string& path) {
    // Find the extension
    size_t dotPos = path.rfind('.');
    if (dotPos == std::string::npos) {
        return AssetType::Unknown;
    }

    std::string ext = path.substr(dotPos);
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Texture extensions
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
        ext == ".tga" || ext == ".hdr" || ext == ".gif" || ext == ".webp") {
        return AssetType::Texture;
    }

    // Model extensions
    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj" || ext == ".dae") {
        return AssetType::Model;
    }

    // Font extensions
    if (ext == ".ttf" || ext == ".otf" || ext == ".woff" || ext == ".woff2") {
        return AssetType::Font;
    }

    // Audio extensions
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") {
        return AssetType::Audio;
    }

    // Script extensions
    if (ext == ".py" || ext == ".lua" || ext == ".rb") {
        return AssetType::Script;
    }

    return AssetType::Unknown;
}

// ============================================================================
// TextureCache implementation
// ============================================================================

struct CacheEntry {
    std::string name;
    std::function<bool(const std::string&)> invalidateFunc;
    std::function<void()> clearAllFunc;
};

// Use lazy initialization to avoid static initialization order issues on Windows
// Static objects with std::function members can fail during DLL load
static std::unordered_map<std::string, CacheEntry>& GetRegisteredCaches() {
    static std::unordered_map<std::string, CacheEntry> s_RegisteredCaches;
    return s_RegisteredCaches;
}

static std::mutex& GetCacheRegistryMutex() {
    static std::mutex s_CacheRegistryMutex;
    return s_CacheRegistryMutex;
}

int TextureCache::InvalidateTexture(const std::string& path) {
    std::lock_guard<std::mutex> lock(GetCacheRegistryMutex());

    int invalidatedCount = 0;

    for (auto& [name, entry] : GetRegisteredCaches()) {
        if (entry.invalidateFunc && entry.invalidateFunc(path)) {
            
            invalidatedCount++;
        }
    }

    if (invalidatedCount > 0) {
        
    }

    return invalidatedCount;
}

void TextureCache::InvalidateAll() {
    std::lock_guard<std::mutex> lock(GetCacheRegistryMutex());

    for (auto& [name, entry] : GetRegisteredCaches()) {
        if (entry.clearAllFunc) {
            entry.clearAllFunc();
            
        }
    }
}

void TextureCache::RegisterCache(const std::string& name,
                                  std::function<bool(const std::string&)> invalidateFunc,
                                  std::function<void()> clearAllFunc) {
    std::lock_guard<std::mutex> lock(GetCacheRegistryMutex());

    CacheEntry entry;
    entry.name = name;
    entry.invalidateFunc = invalidateFunc;
    entry.clearAllFunc = clearAllFunc;

    GetRegisteredCaches()[name] = entry;
    
}

void TextureCache::UnregisterCache(const std::string& name) {
    std::lock_guard<std::mutex> lock(GetCacheRegistryMutex());
    GetRegisteredCaches().erase(name);
}

std::vector<std::string> TextureCache::GetRegisteredCacheNames() {
    std::lock_guard<std::mutex> lock(GetCacheRegistryMutex());

    std::vector<std::string> names;
    names.reserve(GetRegisteredCaches().size());

    for (const auto& [name, entry] : GetRegisteredCaches()) {
        names.push_back(name);
    }

    return names;
}

// ============================================================================
// AssetReloadManager implementation
// ============================================================================

struct ReloadCallbackEntry {
    std::string name;
    AssetType type;
    AssetReloadManager::ReloadCallback callback;
};

// Use lazy initialization to avoid static initialization order issues on Windows
static std::unordered_map<std::string, ReloadCallbackEntry>& GetReloadCallbacks() {
    static std::unordered_map<std::string, ReloadCallbackEntry> s_ReloadCallbacks;
    return s_ReloadCallbacks;
}

static std::mutex& GetReloadCallbacksMutex() {
    static std::mutex s_ReloadCallbacksMutex;
    return s_ReloadCallbacksMutex;
}

void AssetReloadManager::RegisterReloadCallback(AssetType type, const std::string& name, ReloadCallback callback) {
    std::lock_guard<std::mutex> lock(GetReloadCallbacksMutex());

    ReloadCallbackEntry entry;
    entry.name = name;
    entry.type = type;
    entry.callback = callback;

    GetReloadCallbacks()[name] = entry;
    
}

void AssetReloadManager::UnregisterReloadCallback(const std::string& name) {
    std::lock_guard<std::mutex> lock(GetReloadCallbacksMutex());
    GetReloadCallbacks().erase(name);
}

int AssetReloadManager::NotifyAssetChanged(const std::string& path, AssetType type) {
    // Auto-detect type if not specified
    if (type == AssetType::Unknown) {
        type = GetAssetTypeFromExtension(path);
    }

    // For textures, also use the TextureCache system
    if (type == AssetType::Texture) {
        TextureCache::InvalidateTexture(path);
    }

    std::lock_guard<std::mutex> lock(GetReloadCallbacksMutex());

    int notifiedCount = 0;

    for (auto& [name, entry] : GetReloadCallbacks()) {
        // Notify callbacks that match the asset type, or callbacks that want all types
        if (entry.type == type || entry.type == AssetType::Unknown) {
            if (entry.callback && entry.callback(path)) {
                
                notifiedCount++;
            }
        }
    }

    if (notifiedCount > 0) {
        
    }

    return notifiedCount;
}

std::vector<std::string> AssetReloadManager::GetRegisteredCallbackNames() {
    std::lock_guard<std::mutex> lock(GetReloadCallbacksMutex());

    std::vector<std::string> names;
    names.reserve(GetReloadCallbacks().size());

    for (const auto& [name, entry] : GetReloadCallbacks()) {
        names.push_back(name);
    }

    return names;
}

} // namespace rendering
} // namespace lupine

