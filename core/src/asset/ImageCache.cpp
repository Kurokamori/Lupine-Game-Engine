#include "lupine/asset/ImageCache.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include <algorithm>

namespace lupine {
namespace asset {

ImageCache& ImageCache::GetInstance() {
    static ImageCache instance;
    return instance;
}

ImageCache::ImageCache() {
    // Hook the hot-reload system so an edited texture drops its cached decode and
    // is re-decoded on next use, mirroring the per-component texture caches.
    rendering::TextureCache::RegisterCache(
        "ImageCache",
        [](const std::string& path) -> bool {
            return ImageCache::GetInstance().Remove(path);
        },
        []() {
            ImageCache::GetInstance().Clear();
        });
}

std::string ImageCache::NormalizeKey(const std::string& path) {
    // Mirror ImageAsset::LoadFromFile's path normalization so a key built here
    // matches the path a component decodes under: forward slashes, and collapse a
    // doubled/leading "res:/" down to a single canonical "res://" prefix.
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    size_t lastResPos = normalized.rfind("res:/");
    if (lastResPos != std::string::npos && lastResPos > 0) {
        size_t startPos = lastResPos + 5;
        while (startPos < normalized.size() && normalized[startPos] == '/') {
            startPos++;
        }
        if (startPos < normalized.size()) {
            normalized = "res://" + normalized.substr(startPos);
        }
    } else if (lastResPos == 0) {
        size_t startPos = 5;
        while (startPos < normalized.size() && normalized[startPos] == '/') {
            startPos++;
        }
        if (startPos < normalized.size()) {
            normalized = "res://" + normalized.substr(startPos);
        }
    }
    return normalized;
}

AssetRef<ImageAsset> ImageCache::Get(const std::string& resPath) const {
    std::string key = NormalizeKey(resPath);
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<std::string, AssetRef<ImageAsset>>::const_iterator it = m_Cache.find(key);
    if (it != m_Cache.end()) {
        return it->second;
    }
    return AssetRef<ImageAsset>();
}

bool ImageCache::Contains(const std::string& resPath) const {
    std::string key = NormalizeKey(resPath);
    platform::LockGuard lock(m_Mutex);
    return m_Cache.find(key) != m_Cache.end();
}

void ImageCache::Insert(const std::string& resPath, const AssetRef<ImageAsset>& image) {
    if (!image.IsValid()) {
        return;
    }
    std::string key = NormalizeKey(resPath);
    platform::LockGuard lock(m_Mutex);
    m_Cache[key] = image;
}

AssetRef<ImageAsset> ImageCache::GetOrLoad(const std::string& resPath) {
    std::string key = NormalizeKey(resPath);
    {
        platform::LockGuard lock(m_Mutex);
        std::unordered_map<std::string, AssetRef<ImageAsset>>::iterator it = m_Cache.find(key);
        if (it != m_Cache.end()) {
            return it->second;
        }
    }

    // Decode outside the lock so a long decode never blocks concurrent readers or
    // the async worker. A rare race (main thread and a worker decoding the same
    // path at once) costs one wasted decode and is otherwise harmless: stb_image
    // is safe for concurrent decodes and the second Insert simply overwrites with
    // identical data. The full decode is sequenced before the guarded Insert, so
    // a guarded Get on another thread observes complete pixel data.
    AssetRef<ImageAsset> image(new ImageAsset());
    if (!image->LoadFromFile(key, true, ImageColorSpace::sRGB)) {
        return AssetRef<ImageAsset>();
    }

    platform::LockGuard lock(m_Mutex);
    m_Cache[key] = image;
    return image;
}

bool ImageCache::Remove(const std::string& resPath) {
    // Hot-reload may pass either a res:// or a physical path. Drop the direct key
    // and, when the AssetDatabase is available, any entry that resolves to the
    // same physical asset.
    std::string key = NormalizeKey(resPath);

    AssetDatabase& db = AssetDatabase::GetInstance();
    bool dbReady = db.IsInitialized();
    std::string resolvedIn = dbReady ? db.ResolveAsset(resPath) : std::string();

    platform::LockGuard lock(m_Mutex);
    bool removed = false;

    std::unordered_map<std::string, AssetRef<ImageAsset>>::iterator direct = m_Cache.find(key);
    if (direct != m_Cache.end()) {
        m_Cache.erase(direct);
        removed = true;
    }

    if (!resolvedIn.empty()) {
        for (std::unordered_map<std::string, AssetRef<ImageAsset>>::iterator it = m_Cache.begin();
             it != m_Cache.end();) {
            std::string entryResolved = db.ResolveAsset(it->first);
            if (!entryResolved.empty() && entryResolved == resolvedIn) {
                it = m_Cache.erase(it);
                removed = true;
            } else {
                ++it;
            }
        }
    }

    return removed;
}

void ImageCache::Clear() {
    platform::LockGuard lock(m_Mutex);
    m_Cache.clear();
}

size_t ImageCache::Size() const {
    platform::LockGuard lock(m_Mutex);
    return m_Cache.size();
}

} // namespace asset
} // namespace lupine
