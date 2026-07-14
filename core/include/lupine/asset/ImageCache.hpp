#pragma once

#include "lupine/asset/Asset.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/platform/Threading.hpp"
#include <string>
#include <unordered_map>

namespace lupine {
namespace asset {

/**
 * ImageCache - process-wide cache of decoded ImageAssets, keyed by resource path.
 *
 * Image decoding (stb_image inflate + mipmap generation) is the dominant cost of
 * loading a texture; before this cache every component decoded its own copy on
 * the main thread, every time, including during a scene swap. The cache lets the
 * decode happen once and, via AsyncImageLoader, ahead of time on a worker thread
 * so a scene change does not stall on it.
 *
 * Lifetime: entries SURVIVE change_scene (that is the whole point — the
 * destination scene's textures are warmed before the swap). The cache is cleared
 * only on project (re)load and on hot-reload of the underlying file (it registers
 * itself with rendering::TextureCache so an edited asset re-decodes on next use).
 *
 * Thread-safety: GetOrLoad / Get / Insert / Contains are safe to call from any
 * thread. AsyncImageLoader inserts from worker threads while the main thread
 * reads during rendering; the map is mutex-guarded and the asset refcount is
 * atomic. Decoded ImageAsset data is written fully on the decoding thread before
 * the guarded Insert, so a guarded Get on another thread observes complete data.
 */
class ImageCache {
public:
    static ImageCache& GetInstance();

    /**
     * Return the decoded image for `resPath`, decoding synchronously on a cache
     * miss and caching the result. Always decodes with mipmaps in the sRGB color
     * space (the standard UI/sprite image configuration); callers needing other
     * decode parameters must not route through this cache. Returns an invalid ref
     * if the file cannot be decoded. Safe to call from any thread.
     */
    AssetRef<ImageAsset> GetOrLoad(const std::string& resPath);

    /** Lookup without loading. Returns an invalid ref on a miss. */
    AssetRef<ImageAsset> Get(const std::string& resPath) const;

    /** True if a decoded image for `resPath` is already cached. */
    bool Contains(const std::string& resPath) const;

    /** Insert (or overwrite) a decoded image. */
    void Insert(const std::string& resPath, const AssetRef<ImageAsset>& image);

    /** Drop one entry (hot-reload). Returns true if an entry was removed. */
    bool Remove(const std::string& resPath);

    /** Drop every entry (project unload / global invalidation). */
    void Clear();

    /** Number of cached images (diagnostics). */
    size_t Size() const;

    /** Normalize a resource path into the canonical cache key. */
    static std::string NormalizeKey(const std::string& path);

private:
    ImageCache();
    ~ImageCache() = default;
    ImageCache(const ImageCache&) = delete;
    ImageCache& operator=(const ImageCache&) = delete;

    mutable platform::Mutex m_Mutex;
    std::unordered_map<std::string, AssetRef<ImageAsset>> m_Cache;
};

} // namespace asset
} // namespace lupine
