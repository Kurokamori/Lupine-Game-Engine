#pragma once

#include <string>
#include <functional>
#include <vector>

namespace lupine {
namespace rendering {

/**
 * AssetType - Types of assets that can be hot-reloaded
 */
enum class AssetType {
    Texture,    // .png, .jpg, .bmp, .tga, .hdr
    Model,      // .gltf, .glb, .fbx, .obj
    Font,       // .ttf, .otf
    Audio,      // .wav, .mp3, .ogg
    Script,     // .py, .lua, .rb (handled separately but included for completeness)
    Unknown
};

/**
 * Get asset type from file extension
 */
AssetType GetAssetTypeFromExtension(const std::string& path);

/**
 * TextureCache - Centralized texture cache management for hot-reloading
 *
 * This provides functions to invalidate cached textures when asset files change,
 * allowing the engine to reload textures without restarting.
 */
class TextureCache {
public:
    /**
     * Invalidate a texture by path from ALL component texture caches.
     * This removes the texture from caches so it will be reloaded on next use.
     *
     * @param path The texture path (can be res:// or physical path)
     * @return Number of caches that were invalidated
     */
    static int InvalidateTexture(const std::string& path);

    /**
     * Invalidate all textures from all caches.
     * Use with caution - will cause all textures to be reloaded.
     */
    static void InvalidateAll();

    /**
     * Register a cache invalidation callback.
     * Components with texture caches should register their invalidation functions.
     *
     * @param name Unique name for this cache (e.g., "Sprite2D", "StaticMesh3D")
     * @param invalidateFunc Function that takes a path and returns true if texture was found and invalidated
     */
    static void RegisterCache(const std::string& name,
                              std::function<bool(const std::string&)> invalidateFunc,
                              std::function<void()> clearAllFunc);

    /**
     * Unregister a cache by name.
     */
    static void UnregisterCache(const std::string& name);

    /**
     * Get the list of registered cache names (for debugging).
     */
    static std::vector<std::string> GetRegisteredCacheNames();
};

/**
 * AssetReloadManager - Centralized asset hot-reload management
 *
 * This manages callbacks for all asset types (models, fonts, audio, etc.)
 * that need to be reloaded when external files change.
 */
class AssetReloadManager {
public:
    /**
     * Callback signature for asset reload notifications.
     * @param path The asset path that changed
     * @return true if the component uses this asset and needs refresh
     */
    using ReloadCallback = std::function<bool(const std::string& path)>;

    /**
     * Register a callback to be notified when assets of a given type need reloading.
     * @param type The asset type to listen for
     * @param name Unique name for this callback (for unregistering)
     * @param callback Function to call when an asset of this type changes
     */
    static void RegisterReloadCallback(AssetType type, const std::string& name, ReloadCallback callback);

    /**
     * Unregister a reload callback by name.
     */
    static void UnregisterReloadCallback(const std::string& name);

    /**
     * Notify all registered callbacks that an asset has changed.
     * @param path The path of the changed asset
     * @param type The type of the asset (or Unknown to auto-detect from extension)
     * @return Number of callbacks that returned true (used the asset)
     */
    static int NotifyAssetChanged(const std::string& path, AssetType type = AssetType::Unknown);

    /**
     * Get list of registered callback names (for debugging).
     */
    static std::vector<std::string> GetRegisteredCallbackNames();
};

} // namespace rendering
} // namespace lupine

