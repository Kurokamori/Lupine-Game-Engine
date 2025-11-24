#include "lupine/asset/Asset.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace asset {

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
        case AssetType::Font:       return "Font";
        case AssetType::Audio:      return "Audio";
        case AssetType::Shader:     return "Shader";
        default:                    return "Unknown";
    }
}

Asset::Asset()
    : m_UUID() {
}

Asset::Asset(const core::UUID& uuid)
    : m_UUID(uuid) {
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

}

void ShutdownAssets() {

}

}
}

