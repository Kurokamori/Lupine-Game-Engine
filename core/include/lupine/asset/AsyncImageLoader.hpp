#pragma once

#include "lupine/platform/Threading.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <memory>

namespace lupine {
namespace asset {

/**
 * AsyncImageLoader - background image decoding into ImageCache.
 *
 * Decodes textures on a worker thread and publishes them to ImageCache, so a
 * scene's heavy images can be warmed during a transition (the fade-out) instead
 * of stalling the main thread when the scene is swapped in. Decoded ImageAssets
 * are self-contained (no GPU / registry work), so unlike AsyncAssetLoader there
 * is no main-thread finalize step: the worker inserts straight into the
 * thread-safe ImageCache and there are no handles to track.
 *
 * Requests are fire-and-forget and deduplicated: asking for a path that is
 * already cached or already in-flight is a no-op. All public methods are
 * main-thread only; the worker side publishes through ImageCache's own lock.
 */
class AsyncImageLoader {
public:
    static AsyncImageLoader& GetInstance();

    /**
     * Queue `resPath` for background decode into ImageCache. No-op if it is empty,
     * already cached, or already queued/in-flight.
     */
    void PreloadAsync(const std::string& resPath);

    /** Queue many paths at once (see PreloadAsync). */
    void PreloadManyAsync(const std::vector<std::string>& resPaths);

    /** Number of decodes still queued or running (diagnostics). */
    size_t GetPendingCount() const;

    /**
     * Cancel/drain all background work and clear in-flight tracking. Blocks until
     * worker jobs finish so nothing touches engine state afterward. Call on
     * project unload/reload (alongside ImageCache::Clear()).
     */
    void Reset();

    /**
     * Read a .scene file and collect every distinct image texture path it
     * references (Image2D/Sprite texture properties and embedded image refs),
     * skipping shaders/materials. Used to warm a destination scene's textures
     * before change_scene. Returns an empty vector if the file cannot be read.
     */
    static std::vector<std::string> ScanSceneTextures(const std::string& scenePath);

private:
    AsyncImageLoader() = default;
    ~AsyncImageLoader();
    AsyncImageLoader(const AsyncImageLoader&) = delete;
    AsyncImageLoader& operator=(const AsyncImageLoader&) = delete;

    void EnsurePool();

    mutable platform::Mutex m_Mutex;
    std::unique_ptr<platform::ThreadPool> m_Pool;
    std::unordered_set<std::string> m_InProgress;
};

} // namespace asset
} // namespace lupine
