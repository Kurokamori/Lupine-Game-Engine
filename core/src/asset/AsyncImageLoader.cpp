#include "lupine/asset/AsyncImageLoader.hpp"
#include "lupine/asset/ImageCache.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace lupine {
namespace asset {

AsyncImageLoader& AsyncImageLoader::GetInstance() {
    static AsyncImageLoader instance;
    return instance;
}

AsyncImageLoader::~AsyncImageLoader() = default;

void AsyncImageLoader::EnsurePool() {
    if (m_Pool) {
        return;
    }
    // Image decodes are large but few; a small pool keeps them off the main
    // thread without flooding I/O. Mirrors AsyncAssetLoader's sizing intent.
    unsigned int hardware = platform::Thread::GetHardwareConcurrency();
    size_t threadCount = (hardware > 4) ? 2 : 1;
    m_Pool = std::make_unique<platform::ThreadPool>(threadCount);
}

void AsyncImageLoader::PreloadAsync(const std::string& resPath) {
    if (resPath.empty()) {
        return;
    }
    std::string key = ImageCache::NormalizeKey(resPath);
    if (ImageCache::GetInstance().Contains(key)) {
        return;
    }

    platform::LockGuard lock(m_Mutex);
    if (m_InProgress.find(key) != m_InProgress.end()) {
        return;
    }
    m_InProgress.insert(key);
    EnsurePool();
    m_Pool->Enqueue([this, key]() {
        // GetOrLoad performs the decode and publishes to ImageCache; it is
        // thread-safe and idempotent if the image was meanwhile decoded.
        ImageCache::GetInstance().GetOrLoad(key);
        platform::LockGuard doneLock(m_Mutex);
        m_InProgress.erase(key);
    });
}

void AsyncImageLoader::PreloadManyAsync(const std::vector<std::string>& resPaths) {
    for (const std::string& path : resPaths) {
        PreloadAsync(path);
    }
}

size_t AsyncImageLoader::GetPendingCount() const {
    platform::LockGuard lock(m_Mutex);
    return m_InProgress.size();
}

void AsyncImageLoader::Reset() {
    // Drain workers without holding the mutex (jobs take it on completion), then
    // clear tracking. ImageCache is cleared separately by the caller.
    if (m_Pool) {
        m_Pool->WaitForCompletion();
    }
    platform::LockGuard lock(m_Mutex);
    m_InProgress.clear();
}

// --- Scene scanning --------------------------------------------------------

namespace {

bool HasImageExtension(const std::string& value) {
    size_t dot = value.rfind('.');
    if (dot == std::string::npos) {
        return false;
    }
    std::string ext = value.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
           ext == ".tga" || ext == ".hdr" || ext == ".gif" || ext == ".webp";
}

// Read a res:// (or physical) text file, honoring pack mode like ImageAsset does.
std::string ReadResFileText(const std::string& resPath) {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        std::string packPath = packFS.resolveAsset(resPath);
        if (packFS.exists(packPath)) {
            std::vector<uint8_t> bytes = packFS.readAsset(resPath);
            return std::string(bytes.begin(), bytes.end());
        }
    }

    std::string resolved = Asset::ResolveAssetPath(resPath);
    if (resolved.empty()) {
        resolved = resPath;
    }
    platform::FileResult<std::string> result = platform::FileSystem::ReadFile(resolved);
    if (result.success) {
        return result.data;
    }
    return std::string();
}

// Collect image paths from a parsed scene node tree. Any string value ending in
// a known image extension is treated as a texture reference; shader and material
// override paths (.lsh/.mat) and empty slots fall through. Preloading an
// incidental image path is harmless, so no key filtering is needed.
void CollectTextures(const nlohmann::json& node, std::vector<std::string>& out,
                     std::unordered_set<std::string>& seen) {
    if (node.is_object()) {
        for (nlohmann::json::const_iterator it = node.begin(); it != node.end(); ++it) {
            const nlohmann::json& value = it.value();
            if (value.is_string()) {
                std::string str = value.get<std::string>();
                if (!str.empty() && HasImageExtension(str) && seen.insert(str).second) {
                    out.push_back(str);
                }
            } else {
                CollectTextures(value, out, seen);
            }
        }
    } else if (node.is_array()) {
        for (const nlohmann::json& element : node) {
            CollectTextures(element, out, seen);
        }
    }
}

} // namespace

std::vector<std::string> AsyncImageLoader::ScanSceneTextures(const std::string& scenePath) {
    std::vector<std::string> textures;
    std::string text = ReadResFileText(scenePath);
    if (text.empty()) {
        LOG_WARN(LogCategory::Core, "AsyncImageLoader: could not read scene for texture scan: {}", scenePath);
        return textures;
    }

    nlohmann::json scene = nlohmann::json::parse(text, nullptr, false);
    if (scene.is_discarded()) {
        LOG_WARN(LogCategory::Core, "AsyncImageLoader: scene is not valid JSON: {}", scenePath);
        return textures;
    }

    std::unordered_set<std::string> seen;
    CollectTextures(scene, textures, seen);
    return textures;
}

} // namespace asset
} // namespace lupine
