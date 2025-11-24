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
    Shader
};

/**
 * Convert asset type to string
 */
const char* AssetTypeToString(AssetType type);

/**
 * Base class for all assets
 * Provides UUID tracking and reference counting
 */
class Asset {
public:
    Asset();
    explicit Asset(const core::UUID& uuid);
    virtual ~Asset() = default;

    // UUID access
    const core::UUID& GetUUID() const { return m_UUID; }
    
    // Asset type
    virtual AssetType GetType() const = 0;
    
    // Asset path (source file path)
    const std::string& GetPath() const { return m_Path; }
    void SetPath(const std::string& path) { m_Path = path; }
    
    // Asset name
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }
    
    // Reference counting
    void AddRef();
    void Release();
    int GetRefCount() const { return m_RefCount.load(); }
    
    // Loaded state
    bool IsLoaded() const { return m_Loaded; }
    
protected:
    void SetLoaded(bool loaded) { m_Loaded = loaded; }
    
private:
    core::UUID m_UUID;
    std::string m_Path;
    std::string m_Name;
    std::atomic<int> m_RefCount{1};
    bool m_Loaded{false};
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

