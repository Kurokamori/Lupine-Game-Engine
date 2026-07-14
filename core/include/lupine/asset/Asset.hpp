#pragma once

#include "lupine/core/UUID.hpp"
#include <string>
#include <memory>
#include <atomic>

namespace lupine {
namespace asset {

/**
 * Asset type enumeration
 */
enum class AssetType {
    Unknown,
    Image,
    Model,
    Mesh,
    Material,
    Texture,
    Skeleton,
    Animation,
    SpriteAnimation,
    Font,
    Audio,
    Shader,
    Archetype,
    KeyframeAnimation,   // .animclip - keyframe animation clip
    AnimationGraph,      // .animgraph - blend tree / state machine
    Theme,               // .uitheme - UI theme
    ColorPalette         // .palette - colour palette
};

/**
 * Convert asset type to string
 */
const char* AssetTypeToString(AssetType type);

/**
 * Base class for all assets
 * Provides UUID tracking, reference counting, and res:// path management
 *
 * All asset paths are stored internally as res:// paths.
 * The SetPath method automatically converts absolute paths to res:// format.
 */
class Asset {
public:
    Asset();
    explicit Asset(const core::UUID& uuid);
    virtual ~Asset();

    // UUID access
    const core::UUID& GetUUID() const { return m_UUID; }

    /**
     * Set the UUID for this asset
     * Used when loading from .meta files
     */
    void SetUUID(const core::UUID& uuid) { m_UUID = uuid; }

    // Asset type
    virtual AssetType GetType() const = 0;

    /**
     * Get the asset path (always in res:// format)
     */
    const std::string& GetPath() const { return m_Path; }

    /**
     * Set the asset path
     * Automatically converts absolute paths to res:// format
     * @param path Any valid path (absolute, relative, or res://)
     */
    void SetPath(const std::string& path);

    /**
     * Get the resolved physical path for file operations
     * Resolves res:// path to actual filesystem path
     */
    std::string GetPhysicalPath() const;

    /**
     * Check if the path is a valid res:// path
     */
    bool HasValidPath() const;

    // Asset name
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    // Reference counting
    void AddRef();
    void Release();
    int GetRefCount() const { return m_RefCount.load(); }

    // Loaded state
    bool IsLoaded() const { return m_Loaded; }

    /**
     * Resolve a path (potentially absolute or res://) to a physical path
     * Uses UUID fallback if AssetDatabase is available
     * @param path Path to resolve
     * @return Physical filesystem path
     */
    static std::string ResolveAssetPath(const std::string& path);

    /**
     * Process-wide loaded-asset byte accounting (used by the profiler).
     * Totals reflect the in-memory (CPU-side) size assets report via
     * SetTrackedBytes; they are decremented automatically on asset destruction.
     */
    static int64_t GetTotalTrackedBytes();
    static int64_t GetTrackedBytesByType(AssetType type);

protected:
    void SetLoaded(bool loaded) { m_Loaded = loaded; }

    /**
     * Report this asset's current in-memory byte size. Call after (re)loading or
     * clearing the asset's data; the base computes the delta against the previous
     * value and updates the process-wide totals. Safe to call repeatedly.
     */
    void SetTrackedBytes(size_t bytes);

private:
    core::UUID m_UUID;
    std::string m_Path;       // Always stored as res:// path
    std::string m_Name;
    std::atomic<int> m_RefCount{1};
    bool m_Loaded{false};
    int64_t m_TrackedBytes{0};
    int m_TrackedTypeIndex{-1};   // cached type bucket for destruction-time subtraction
};

/**
 * Shared pointer type for assets with custom deleter
 */
template<typename T>
class AssetRef {
public:
    AssetRef() : m_Asset(nullptr) {}
    
    explicit AssetRef(T* asset) : m_Asset(asset) {
        if (m_Asset) {
            m_Asset->AddRef();
        }
    }
    
    AssetRef(const AssetRef& other) : m_Asset(other.m_Asset) {
        if (m_Asset) {
            m_Asset->AddRef();
        }
    }
    
    AssetRef(AssetRef&& other) noexcept : m_Asset(other.m_Asset) {
        other.m_Asset = nullptr;
    }
    
    ~AssetRef() {
        if (m_Asset) {
            m_Asset->Release();
        }
    }
    
    AssetRef& operator=(const AssetRef& other) {
        if (this != &other) {
            if (m_Asset) {
                m_Asset->Release();
            }
            m_Asset = other.m_Asset;
            if (m_Asset) {
                m_Asset->AddRef();
            }
        }
        return *this;
    }
    
    AssetRef& operator=(AssetRef&& other) noexcept {
        if (this != &other) {
            if (m_Asset) {
                m_Asset->Release();
            }
            m_Asset = other.m_Asset;
            other.m_Asset = nullptr;
        }
        return *this;
    }
    
    T* Get() const { return m_Asset; }
    T* operator->() const { return m_Asset; }
    T& operator*() const { return *m_Asset; }
    
    bool IsValid() const { return m_Asset != nullptr; }
    operator bool() const { return IsValid(); }
    
    void Reset() {
        if (m_Asset) {
            m_Asset->Release();
            m_Asset = nullptr;
        }
    }
    
private:
    T* m_Asset;
};

} // namespace asset
} // namespace lupine

