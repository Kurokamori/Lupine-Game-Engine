#include "lupine/asset/Asset.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/logger/Logger.hpp"

#include <array>

namespace lupine {
namespace asset {

using namespace platform;

namespace {
// Process-wide loaded-asset byte totals. Lock-free atomics: assets report size
// changes from the main thread (loads) and worker threads (async loads), and the
// profiler reads them once per frame.
constexpr int kAssetTypeBuckets = 32;
std::atomic<int64_t> g_TotalAssetBytes{0};
std::array<std::atomic<int64_t>, kAssetTypeBuckets> g_TypeBytes{};

int AssetTypeBucket(AssetType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= kAssetTypeBuckets) {
        return static_cast<int>(AssetType::Unknown);
    }
    return idx;
}
} // anonymous namespace

const char* AssetTypeToString(AssetType type) {
    switch (type) {
        case AssetType::Unknown:    return "Unknown";
        case AssetType::Image:      return "Image";
        case AssetType::Model:      return "Model";
        case AssetType::Mesh:       return "Mesh";
        case AssetType::Material:   return "Material";
        case AssetType::Texture:    return "Texture";
        case AssetType::Skeleton:   return "Skeleton";
        case AssetType::Animation:  return "Animation";
        case AssetType::SpriteAnimation: return "SpriteAnimation";
        case AssetType::Font:       return "Font";
        case AssetType::Audio:      return "Audio";
        case AssetType::Shader:     return "Shader";
        case AssetType::Archetype:  return "Archetype";
        case AssetType::KeyframeAnimation: return "KeyframeAnimation";
        case AssetType::AnimationGraph:    return "AnimationGraph";
        case AssetType::Theme:             return "Theme";
        case AssetType::ColorPalette:      return "ColorPalette";
        default:                    return "Unknown";
    }
}

Asset::Asset()
    : m_UUID() {
}

Asset::Asset(const core::UUID& uuid)
    : m_UUID(uuid) {
}

Asset::~Asset() {
    // Release this asset's contribution to the process-wide byte totals. The type
    // bucket is cached (GetType() is virtual and unavailable during destruction).
    if (m_TrackedBytes != 0) {
        g_TotalAssetBytes.fetch_sub(m_TrackedBytes, std::memory_order_relaxed);
        if (m_TrackedTypeIndex >= 0) {
            g_TypeBytes[m_TrackedTypeIndex].fetch_sub(m_TrackedBytes, std::memory_order_relaxed);
        }
    }
}

void Asset::SetTrackedBytes(size_t bytes) {
    int64_t newBytes = static_cast<int64_t>(bytes);
    int bucket = AssetTypeBucket(GetType());

    // Moving between buckets (shouldn't happen for a stable type, but be safe):
    // retire the old bucket's share before applying to the new one.
    if (m_TrackedTypeIndex >= 0 && m_TrackedTypeIndex != bucket && m_TrackedBytes != 0) {
        g_TypeBytes[m_TrackedTypeIndex].fetch_sub(m_TrackedBytes, std::memory_order_relaxed);
        g_TypeBytes[bucket].fetch_add(m_TrackedBytes, std::memory_order_relaxed);
    }
    m_TrackedTypeIndex = bucket;

    int64_t delta = newBytes - m_TrackedBytes;
    if (delta == 0) {
        return;
    }
    m_TrackedBytes = newBytes;
    g_TotalAssetBytes.fetch_add(delta, std::memory_order_relaxed);
    g_TypeBytes[bucket].fetch_add(delta, std::memory_order_relaxed);
}

int64_t Asset::GetTotalTrackedBytes() {
    return g_TotalAssetBytes.load(std::memory_order_relaxed);
}

int64_t Asset::GetTrackedBytesByType(AssetType type) {
    return g_TypeBytes[AssetTypeBucket(type)].load(std::memory_order_relaxed);
}

void Asset::SetPath(const std::string& path) {
    if (path.empty()) {
        m_Path.clear();
        return;
    }

    // Check if already a res:// path
    if (path.size() >= 6 && path.substr(0, 6) == "res://") {
        m_Path = path;
        return;
    }

    // Try to convert to res:// path using AssetDatabase
    auto& assetDb = AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        std::string resPath = assetDb.ToResourcePath(path);
        if (!resPath.empty()) {
            m_Path = resPath;
            return;
        }
    }

    // Store as-is if conversion fails (for backwards compatibility)
    // This allows the path to still work during transition
    m_Path = path;
    LOG_WARN(LogCategory::Core, "Asset path could not be converted to res:// format: {}", path);
}

std::string Asset::GetPhysicalPath() const {
    return ResolveAssetPath(m_Path);
}

bool Asset::HasValidPath() const {
    if (m_Path.empty()) {
        return false;
    }
    return m_Path.size() >= 6 && m_Path.substr(0, 6) == "res://";
}

std::string Asset::ResolveAssetPath(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    // Check if running from pack file first
    auto& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        // Use pack file system's resolution (handles UUID lookup)
        std::string resolved = packFS.resolveAsset(path);
        if (packFS.exists(resolved)) {
            return resolved;
        }
    }

    // For res:// paths, prefer AssetDatabase over VFS since it knows the project root
    // VFS may have res:// mounted to engine resources which is wrong for project assets
    bool isResPath = path.size() >= 6 && path.substr(0, 6) == "res://";

    auto& assetDb = AssetDatabase::GetInstance();
    if (isResPath && assetDb.IsInitialized()) {
        std::string resolved = assetDb.ResolveAsset(path);
        if (!resolved.empty() && FileSystem::Exists(resolved)) {
            return resolved;
        }
    }

    // Try VFS resolution for other virtual paths (user://, app://, etc.)
    auto& vfs = VirtualFileSystem::GetInstance();
    if (vfs.IsInitialized()) {
        auto result = vfs.ResolveAsset(path);
        if (result.success && FileSystem::Exists(result.data)) {
            return result.data;
        }
    }

    // Bare project-relative paths (no scheme, not absolute) are interpreted relative to
    // the project root, which the runtime mounts at res://. Globals/autoload config stores
    // singleton script paths this way (e.g. "scripts/core/boot.lua"), so resolve them
    // against res:// before giving up.
    bool hasScheme = path.find("://") != std::string::npos;
    if (!hasScheme && Path::IsRelative(path)) {
        std::string resPath = "res://" + Path::ToUnixSeparators(path);

        if (assetDb.IsInitialized()) {
            std::string resolved = assetDb.ResolveAsset(resPath);
            if (!resolved.empty() && FileSystem::Exists(resolved)) {
                return resolved;
            }
        }

        if (vfs.IsInitialized()) {
            auto result = vfs.ResolveAsset(resPath);
            if (result.success && FileSystem::Exists(result.data)) {
                return result.data;
            }
        }
    }

    // Fallback: use AssetDatabase for any path
    if (assetDb.IsInitialized()) {
        return assetDb.ResolveAsset(path);
    }

    // Return as-is
    return path;
}

void Asset::AddRef() {
    m_RefCount.fetch_add(1, std::memory_order_relaxed);
}

void Asset::Release() {
    int oldCount = m_RefCount.fetch_sub(1, std::memory_order_acq_rel);
    if (oldCount == 1) {
        delete this;
    }
}

void InitializeAssets() {
    // Initialize AssetDatabase if needed
}

void ShutdownAssets() {
    // Shutdown AssetDatabase if needed
}

}
}

