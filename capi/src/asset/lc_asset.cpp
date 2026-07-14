/**
 * @file lc_asset.cpp
 * @brief Implementation of Lupine Engine C API - Asset Loading System
 */

#include "asset/lc_asset.h"
#include "../core/lc_internal.h"

#include <lupine/asset/Assets.hpp>
#include <lupine/asset/ImageAsset.hpp>
#include <lupine/asset/ModelAsset.hpp>
#include <lupine/asset/AudioAsset.hpp>
#include <lupine/asset/FontAsset.hpp>

#include <unordered_map>
#include <mutex>
#include <cstring>

namespace {

// Asset handle management
std::unordered_map<void*, lupine::asset::AssetRef<lupine::asset::ImageAsset>> g_imageAssets;
std::unordered_map<void*, lupine::asset::AssetRef<lupine::asset::ModelAsset>> g_modelAssets;
std::unordered_map<void*, lupine::asset::AssetRef<lupine::asset::AudioAsset>> g_audioAssets;
std::unordered_map<void*, lupine::asset::AssetRef<lupine::asset::FontAsset>> g_fontAssets;
std::mutex g_assetMutex;

// Helper to create unique handle
void* CreateUniqueHandle() {
    static uint64_t s_nextId = 1;
    return reinterpret_cast<void*>(s_nextId++);
}

// Type conversion helpers
lupine::asset::ImageColorSpace ToEngineColorSpace(LCImageColorSpace colorSpace) {
    switch (colorSpace) {
        case LC_IMAGE_COLOR_SPACE_LINEAR: return lupine::asset::ImageColorSpace::Linear;
        case LC_IMAGE_COLOR_SPACE_SRGB: return lupine::asset::ImageColorSpace::sRGB;
        default: return lupine::asset::ImageColorSpace::sRGB;
    }
}

LCImageColorSpace ToCColorSpace(lupine::asset::ImageColorSpace colorSpace) {
    switch (colorSpace) {
        case lupine::asset::ImageColorSpace::Linear: return LC_IMAGE_COLOR_SPACE_LINEAR;
        case lupine::asset::ImageColorSpace::sRGB: return LC_IMAGE_COLOR_SPACE_SRGB;
        default: return LC_IMAGE_COLOR_SPACE_SRGB;
    }
}

LCImageFormat ToCImageFormat(lupine::asset::ImageFormat format) {
    switch (format) {
        case lupine::asset::ImageFormat::R8: return LC_IMAGE_FORMAT_R8;
        case lupine::asset::ImageFormat::RG8: return LC_IMAGE_FORMAT_RG8;
        case lupine::asset::ImageFormat::RGB8: return LC_IMAGE_FORMAT_RGB8;
        case lupine::asset::ImageFormat::RGBA8: return LC_IMAGE_FORMAT_RGBA8;
        case lupine::asset::ImageFormat::R16F: return LC_IMAGE_FORMAT_R16F;
        case lupine::asset::ImageFormat::RG16F: return LC_IMAGE_FORMAT_RG16F;
        case lupine::asset::ImageFormat::RGB16F: return LC_IMAGE_FORMAT_RGB16F;
        case lupine::asset::ImageFormat::RGBA16F: return LC_IMAGE_FORMAT_RGBA16F;
        case lupine::asset::ImageFormat::R32F: return LC_IMAGE_FORMAT_R32F;
        case lupine::asset::ImageFormat::RG32F: return LC_IMAGE_FORMAT_RG32F;
        case lupine::asset::ImageFormat::RGB32F: return LC_IMAGE_FORMAT_RGB32F;
        case lupine::asset::ImageFormat::RGBA32F: return LC_IMAGE_FORMAT_RGBA32F;
        default: return LC_IMAGE_FORMAT_UNKNOWN;
    }
}

LCAudioFormat ToCAudioFormat(lupine::asset::AudioFormat format) {
    switch (format) {
        case lupine::asset::AudioFormat::Mono8: return LC_AUDIO_FORMAT_MONO8;
        case lupine::asset::AudioFormat::Mono16: return LC_AUDIO_FORMAT_MONO16;
        case lupine::asset::AudioFormat::Stereo8: return LC_AUDIO_FORMAT_STEREO8;
        case lupine::asset::AudioFormat::Stereo16: return LC_AUDIO_FORMAT_STEREO16;
        default: return LC_AUDIO_FORMAT_UNKNOWN;
    }
}

lupine::asset::AudioLoadMode ToEngineAudioLoadMode(LCAudioLoadMode mode) {
    switch (mode) {
        case LC_AUDIO_LOAD_PRELOAD: return lupine::asset::AudioLoadMode::Preload;
        case LC_AUDIO_LOAD_STREAMING: return lupine::asset::AudioLoadMode::Streaming;
        default: return lupine::asset::AudioLoadMode::Preload;
    }
}

LCAudioLoadMode ToCAudioLoadMode(lupine::asset::AudioLoadMode mode) {
    switch (mode) {
        case lupine::asset::AudioLoadMode::Preload: return LC_AUDIO_LOAD_PRELOAD;
        case lupine::asset::AudioLoadMode::Streaming: return LC_AUDIO_LOAD_STREAMING;
        default: return LC_AUDIO_LOAD_PRELOAD;
    }
}

// Get asset from generic handle
lupine::asset::Asset* GetAssetFromHandle(LCAssetHandle handle, LCAssetType* outType) {
    std::lock_guard<std::mutex> lock(g_assetMutex);

    auto imageIt = g_imageAssets.find(handle);
    if (imageIt != g_imageAssets.end()) {
        if (outType) *outType = LC_ASSET_TYPE_IMAGE;
        return imageIt->second.Get();
    }

    auto modelIt = g_modelAssets.find(handle);
    if (modelIt != g_modelAssets.end()) {
        if (outType) *outType = LC_ASSET_TYPE_MODEL;
        return modelIt->second.Get();
    }

    auto audioIt = g_audioAssets.find(handle);
    if (audioIt != g_audioAssets.end()) {
        if (outType) *outType = LC_ASSET_TYPE_AUDIO;
        return audioIt->second.Get();
    }

    auto fontIt = g_fontAssets.find(handle);
    if (fontIt != g_fontAssets.end()) {
        if (outType) *outType = LC_ASSET_TYPE_FONT;
        return fontIt->second.Get();
    }

    return nullptr;
}

} // anonymous namespace

/* ============================================================================
 * Common Asset Functions
 * ============================================================================ */

LC_API LCResult lc_asset_add_ref(LCAssetHandle asset) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::Asset* assetPtr = GetAssetFromHandle(asset, nullptr);
    if (!assetPtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    assetPtr->AddRef();
    return LC_SUCCESS;
}

namespace {

// Drop one reference to the asset stored under `handle` in `map`.
// A loaded asset carries two baseline references: the orphaned construction
// reference (logically owned by the C handle) and the registry's AssetRef.
// While additional references exist (e.g. from lc_asset_add_ref) the call only
// decrements. On the final release both baseline references are dropped, which
// destroys the asset and removes the map entry. Returns true if handled here.
template<typename MapT>
bool ReleaseAssetFromMap(MapT& map, void* handle) {
    auto it = map.find(handle);
    if (it == map.end()) {
        return false;
    }

    lupine::asset::Asset* raw = it->second.Get();
    int refCount = raw ? raw->GetRefCount() : 0;
    if (refCount > 2) {
        raw->Release();
    } else {
        if (refCount >= 2) {
            raw->Release();
        }
        map.erase(it);
    }
    return true;
}

} // anonymous namespace

LC_API LCResult lc_asset_release(LCAssetHandle asset) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);

    if (ReleaseAssetFromMap(g_imageAssets, asset)) return LC_SUCCESS;
    if (ReleaseAssetFromMap(g_modelAssets, asset)) return LC_SUCCESS;
    if (ReleaseAssetFromMap(g_audioAssets, asset)) return LC_SUCCESS;
    if (ReleaseAssetFromMap(g_fontAssets, asset)) return LC_SUCCESS;

    SetError(LC_ERROR_INVALID_HANDLE, "Invalid asset handle");
    return LC_ERROR_INVALID_HANDLE;
}

LC_API LCResult lc_asset_get_ref_count(LCAssetHandle asset, int* count) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!count) {
        SetError(LC_ERROR_NULL_POINTER, "Count pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    lupine::asset::Asset* assetPtr = GetAssetFromHandle(asset, nullptr);
    if (!assetPtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *count = assetPtr->GetRefCount();
    return LC_SUCCESS;
}

LC_API LCResult lc_asset_get_type(LCAssetHandle asset, LCAssetType* type) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!type) {
        SetError(LC_ERROR_NULL_POINTER, "Type pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    LCAssetType foundType = LC_ASSET_TYPE_UNKNOWN;
    lupine::asset::Asset* assetPtr = GetAssetFromHandle(asset, &foundType);
    if (!assetPtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *type = foundType;
    return LC_SUCCESS;
}

LC_API LCResult lc_asset_get_path(LCAssetHandle asset, char* path, size_t pathSize) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!path || pathSize == 0) {
        SetError(LC_ERROR_NULL_POINTER, "Path buffer is null or size is zero");
        return LC_ERROR_NULL_POINTER;
    }

    lupine::asset::Asset* assetPtr = GetAssetFromHandle(asset, nullptr);
    if (!assetPtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const std::string& assetPath = assetPtr->GetPath();
    CopyStringToBuffer(path, pathSize, assetPath.c_str());

    return LC_SUCCESS;
}

LC_API LCResult lc_asset_get_name(LCAssetHandle asset, char* name, size_t nameSize) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!name || nameSize == 0) {
        SetError(LC_ERROR_NULL_POINTER, "Name buffer is null or size is zero");
        return LC_ERROR_NULL_POINTER;
    }

    lupine::asset::Asset* assetPtr = GetAssetFromHandle(asset, nullptr);
    if (!assetPtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const std::string& assetName = assetPtr->GetName();
    CopyStringToBuffer(name, nameSize, assetName.c_str());

    return LC_SUCCESS;
}

LC_API LCResult lc_asset_is_loaded(LCAssetHandle asset, bool* loaded) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!loaded) {
        SetError(LC_ERROR_NULL_POINTER, "Loaded pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    lupine::asset::Asset* assetPtr = GetAssetFromHandle(asset, nullptr);
    if (!assetPtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *loaded = assetPtr->IsLoaded();
    return LC_SUCCESS;
}

LC_API const char* lc_asset_type_to_string(LCAssetType type) {
    switch (type) {
        case LC_ASSET_TYPE_UNKNOWN: return "Unknown";
        case LC_ASSET_TYPE_IMAGE: return "Image";
        case LC_ASSET_TYPE_MODEL: return "Model";
        case LC_ASSET_TYPE_MESH: return "Mesh";
        case LC_ASSET_TYPE_MATERIAL: return "Material";
        case LC_ASSET_TYPE_TEXTURE: return "Texture";
        case LC_ASSET_TYPE_SKELETON: return "Skeleton";
        case LC_ASSET_TYPE_ANIMATION: return "Animation";
        case LC_ASSET_TYPE_SPRITE_ANIMATION: return "SpriteAnimation";
        case LC_ASSET_TYPE_FONT: return "Font";
        case LC_ASSET_TYPE_AUDIO: return "Audio";
        case LC_ASSET_TYPE_SHADER: return "Shader";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Image Asset Functions
 * ============================================================================ */

LC_API LCResult lc_image_load(const char* filepath, bool generateMips, LCImageColorSpace colorSpace, LCImageAssetHandle* asset) {
    if (!filepath) {
        SetError(LC_ERROR_NULL_POINTER, "Filepath is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (!asset) {
        SetError(LC_ERROR_NULL_POINTER, "Asset output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    auto imageAsset = new lupine::asset::ImageAsset();
    if (!imageAsset->LoadFromFile(filepath, generateMips, ToEngineColorSpace(colorSpace))) {
        delete imageAsset;
        SetError(LC_ERROR_ASSET_LOAD_FAILED, "Failed to load image from file");
        return LC_ERROR_ASSET_LOAD_FAILED;
    }

    void* handle = CreateUniqueHandle();
    {
        std::lock_guard<std::mutex> lock(g_assetMutex);
        g_imageAssets[handle] = lupine::asset::AssetRef<lupine::asset::ImageAsset>(imageAsset);
    }

    *asset = static_cast<LCImageAssetHandle>(handle);
    return LC_SUCCESS;
}

LC_API LCResult lc_image_load_from_memory(const uint8_t* data, size_t dataSize, bool generateMips, LCImageColorSpace colorSpace, LCImageAssetHandle* asset) {
    if (!data) {
        SetError(LC_ERROR_NULL_POINTER, "Data is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (!asset) {
        SetError(LC_ERROR_NULL_POINTER, "Asset output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    auto imageAsset = new lupine::asset::ImageAsset();
    if (!imageAsset->LoadFromMemory(data, dataSize, generateMips, ToEngineColorSpace(colorSpace))) {
        delete imageAsset;
        SetError(LC_ERROR_ASSET_LOAD_FAILED, "Failed to load image from memory");
        return LC_ERROR_ASSET_LOAD_FAILED;
    }

    void* handle = CreateUniqueHandle();
    {
        std::lock_guard<std::mutex> lock(g_assetMutex);
        g_imageAssets[handle] = lupine::asset::AssetRef<lupine::asset::ImageAsset>(imageAsset);
    }

    *asset = static_cast<LCImageAssetHandle>(handle);
    return LC_SUCCESS;
}

LC_API LCResult lc_image_load_from_raw(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels, bool generateMips, LCImageColorSpace colorSpace, LCImageAssetHandle* asset) {
    if (!data) {
        SetError(LC_ERROR_NULL_POINTER, "Data is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (!asset) {
        SetError(LC_ERROR_NULL_POINTER, "Asset output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (width == 0 || height == 0) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid image dimensions");
        return LC_ERROR_INVALID_PARAMETER;
    }
    if (channels < 1 || channels > 4) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid channel count");
        return LC_ERROR_INVALID_PARAMETER;
    }

    auto imageAsset = new lupine::asset::ImageAsset();
    if (!imageAsset->LoadFromRawData(data, width, height, channels, generateMips, ToEngineColorSpace(colorSpace))) {
        delete imageAsset;
        SetError(LC_ERROR_ASSET_LOAD_FAILED, "Failed to load image from raw data");
        return LC_ERROR_ASSET_LOAD_FAILED;
    }

    void* handle = CreateUniqueHandle();
    {
        std::lock_guard<std::mutex> lock(g_assetMutex);
        g_imageAssets[handle] = lupine::asset::AssetRef<lupine::asset::ImageAsset>(imageAsset);
    }

    *asset = static_cast<LCImageAssetHandle>(handle);
    return LC_SUCCESS;
}

LC_API LCResult lc_image_get_info(LCImageAssetHandle asset, LCImageInfo* info) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!info) {
        SetError(LC_ERROR_NULL_POINTER, "Info pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_imageAssets.find(asset);
    if (it == g_imageAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid image asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ImageAsset* imageAsset = it->second.Get();
    info->width = imageAsset->GetWidth();
    info->height = imageAsset->GetHeight();
    info->channels = imageAsset->GetChannels();
    info->mipLevels = imageAsset->GetMipLevels();
    info->format = ToCImageFormat(imageAsset->GetFormat());
    info->colorSpace = ToCColorSpace(imageAsset->GetColorSpace());
    info->hasAlpha = imageAsset->HasAlpha();

    return LC_SUCCESS;
}

LC_API LCResult lc_image_get_data(LCImageAssetHandle asset, const uint8_t** data, size_t* dataSize) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!data || !dataSize) {
        SetError(LC_ERROR_NULL_POINTER, "Output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_imageAssets.find(asset);
    if (it == g_imageAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid image asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ImageAsset* imageAsset = it->second.Get();
    *data = imageAsset->GetData();
    *dataSize = imageAsset->GetDataSize();

    return LC_SUCCESS;
}

LC_API LCResult lc_image_get_mip_data(LCImageAssetHandle asset, uint32_t level, const uint8_t** data, uint32_t* width, uint32_t* height) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!data || !width || !height) {
        SetError(LC_ERROR_NULL_POINTER, "Output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_imageAssets.find(asset);
    if (it == g_imageAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid image asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ImageAsset* imageAsset = it->second.Get();
    if (level >= imageAsset->GetMipLevels()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Mip level out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    const lupine::asset::MipLevel& mip = imageAsset->GetMipLevel(level);
    *data = mip.data.data();
    *width = mip.width;
    *height = mip.height;

    return LC_SUCCESS;
}

LC_API LCResult lc_image_release(LCImageAssetHandle asset) {
    return lc_asset_release(reinterpret_cast<LCAssetHandle>(asset));
}

/* ============================================================================
 * Model Asset Functions
 * ============================================================================ */

LC_API LCResult lc_model_load(const char* filepath, bool optimizeMesh, LCModelAssetHandle* asset) {
    if (!filepath) {
        SetError(LC_ERROR_NULL_POINTER, "Filepath is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (!asset) {
        SetError(LC_ERROR_NULL_POINTER, "Asset output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    auto modelAsset = new lupine::asset::ModelAsset();
    if (!modelAsset->LoadFromFile(filepath, optimizeMesh)) {
        delete modelAsset;
        SetError(LC_ERROR_ASSET_LOAD_FAILED, "Failed to load model from file");
        return LC_ERROR_ASSET_LOAD_FAILED;
    }

    void* handle = CreateUniqueHandle();
    {
        std::lock_guard<std::mutex> lock(g_assetMutex);
        g_modelAssets[handle] = lupine::asset::AssetRef<lupine::asset::ModelAsset>(modelAsset);
    }

    *asset = static_cast<LCModelAssetHandle>(handle);
    return LC_SUCCESS;
}

LC_API LCResult lc_model_get_info(LCModelAssetHandle asset, LCModelInfo* info) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!info) {
        SetError(LC_ERROR_NULL_POINTER, "Info pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_modelAssets.find(asset);
    if (it == g_modelAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid model asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ModelAsset* modelAsset = it->second.Get();
    info->meshCount = modelAsset->GetMeshCount();
    info->materialCount = modelAsset->GetMaterialCount();
    info->animationCount = modelAsset->GetAnimationCount();
    info->boneCount = modelAsset->HasSkeleton() ? static_cast<uint32_t>(modelAsset->GetSkeleton().bones.size()) : 0;
    info->hasSkeleton = modelAsset->HasSkeleton();
    info->hasAnimations = modelAsset->HasAnimations();
    info->hasEmbeddedTextures = modelAsset->HasEmbeddedTextures();

    return LC_SUCCESS;
}

LC_API LCResult lc_model_get_mesh_info(LCModelAssetHandle asset, uint32_t meshIndex, LCMeshInfo* info) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!info) {
        SetError(LC_ERROR_NULL_POINTER, "Info pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_modelAssets.find(asset);
    if (it == g_modelAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid model asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ModelAsset* modelAsset = it->second.Get();
    const auto& meshes = modelAsset->GetMeshes();
    if (meshIndex >= meshes.size()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Mesh index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    const lupine::asset::Mesh& mesh = meshes[meshIndex];
    CopyStringToBuffer(info->name, sizeof(info->name), mesh.name.c_str());
    info->vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    info->indexCount = static_cast<uint32_t>(mesh.indices.size());
    info->materialIndex = mesh.materialIndex;
    info->boundsMin = { mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z };
    info->boundsMax = { mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z };

    return LC_SUCCESS;
}

LC_API LCResult lc_model_get_mesh_vertices(LCModelAssetHandle asset, uint32_t meshIndex, LCVertex* vertices, uint32_t maxVertices, uint32_t* verticesCopied) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!vertices || !verticesCopied) {
        SetError(LC_ERROR_NULL_POINTER, "Output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_modelAssets.find(asset);
    if (it == g_modelAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid model asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ModelAsset* modelAsset = it->second.Get();
    const auto& meshes = modelAsset->GetMeshes();
    if (meshIndex >= meshes.size()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Mesh index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    const lupine::asset::Mesh& mesh = meshes[meshIndex];
    uint32_t copyCount = std::min(maxVertices, static_cast<uint32_t>(mesh.vertices.size()));

    for (uint32_t i = 0; i < copyCount; ++i) {
        const lupine::asset::Vertex& src = mesh.vertices[i];
        LCVertex& dst = vertices[i];

        dst.position = { src.position.x, src.position.y, src.position.z };
        dst.normal = { src.normal.x, src.normal.y, src.normal.z };
        dst.texCoord = { src.texCoord.x, src.texCoord.y };
        dst.tangent = { src.tangent.x, src.tangent.y, src.tangent.z };
        dst.bitangent = { src.bitangent.x, src.bitangent.y, src.bitangent.z };

        for (int j = 0; j < 4; ++j) {
            dst.boneIDs[j] = src.boneIDs[j];
            dst.boneWeights[j] = src.boneWeights[j];
        }
    }

    *verticesCopied = copyCount;
    return LC_SUCCESS;
}

LC_API LCResult lc_model_get_mesh_indices(LCModelAssetHandle asset, uint32_t meshIndex, uint32_t* indices, uint32_t maxIndices, uint32_t* indicesCopied) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!indices || !indicesCopied) {
        SetError(LC_ERROR_NULL_POINTER, "Output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_modelAssets.find(asset);
    if (it == g_modelAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid model asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ModelAsset* modelAsset = it->second.Get();
    const auto& meshes = modelAsset->GetMeshes();
    if (meshIndex >= meshes.size()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Mesh index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    const lupine::asset::Mesh& mesh = meshes[meshIndex];
    uint32_t copyCount = std::min(maxIndices, static_cast<uint32_t>(mesh.indices.size()));

    std::memcpy(indices, mesh.indices.data(), copyCount * sizeof(uint32_t));

    *indicesCopied = copyCount;
    return LC_SUCCESS;
}

LC_API LCResult lc_model_get_material_info(LCModelAssetHandle asset, uint32_t materialIndex, LCMaterialInfo* info) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!info) {
        SetError(LC_ERROR_NULL_POINTER, "Info pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_modelAssets.find(asset);
    if (it == g_modelAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid model asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ModelAsset* modelAsset = it->second.Get();
    const auto& materials = modelAsset->GetMaterials();
    if (materialIndex >= materials.size()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Material index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    const lupine::asset::Material& mat = materials[materialIndex];
    CopyStringToBuffer(info->name, sizeof(info->name), mat.name.c_str());

    info->albedo = { mat.albedo.x, mat.albedo.y, mat.albedo.z };
    info->metallic = mat.metallic;
    info->roughness = mat.roughness;
    info->ao = mat.ao;
    info->emissive = { mat.emissive.x, mat.emissive.y, mat.emissive.z };
    info->opacity = mat.opacity;

    CopyStringToBuffer(info->albedoMap, sizeof(info->albedoMap), mat.albedoMap.c_str());
    CopyStringToBuffer(info->normalMap, sizeof(info->normalMap), mat.normalMap.c_str());
    CopyStringToBuffer(info->metallicMap, sizeof(info->metallicMap), mat.metallicMap.c_str());
    CopyStringToBuffer(info->roughnessMap, sizeof(info->roughnessMap), mat.roughnessMap.c_str());
    CopyStringToBuffer(info->aoMap, sizeof(info->aoMap), mat.aoMap.c_str());
    CopyStringToBuffer(info->emissiveMap, sizeof(info->emissiveMap), mat.emissiveMap.c_str());

    return LC_SUCCESS;
}

LC_API LCResult lc_model_get_bone(LCModelAssetHandle asset, uint32_t boneIndex, LCBone* bone) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!bone) {
        SetError(LC_ERROR_NULL_POINTER, "Bone pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_modelAssets.find(asset);
    if (it == g_modelAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid model asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ModelAsset* modelAsset = it->second.Get();
    if (!modelAsset->HasSkeleton()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Model has no skeleton");
        return LC_ERROR_INVALID_PARAMETER;
    }

    const lupine::asset::Skeleton& skeleton = modelAsset->GetSkeleton();
    if (boneIndex >= skeleton.bones.size()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Bone index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    const lupine::asset::Bone& srcBone = skeleton.bones[boneIndex];
    CopyStringToBuffer(bone->name, sizeof(bone->name), srcBone.name.c_str());
    bone->parentIndex = srcBone.parentIndex;

    // Copy matrices (column-major, 4 columns of 4 floats)
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            bone->offsetMatrix.m[col * 4 + row] = srcBone.offsetMatrix[col][row];
            bone->localTransform.m[col * 4 + row] = srcBone.localTransform[col][row];
        }
    }

    return LC_SUCCESS;
}

LC_API LCResult lc_model_get_animation_info(LCModelAssetHandle asset, uint32_t animIndex, LCAnimationInfo* info) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!info) {
        SetError(LC_ERROR_NULL_POINTER, "Info pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_modelAssets.find(asset);
    if (it == g_modelAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid model asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::ModelAsset* modelAsset = it->second.Get();
    const auto& animations = modelAsset->GetAnimations();
    if (animIndex >= animations.size()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Animation index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    const lupine::asset::AnimationClip& anim = animations[animIndex];
    CopyStringToBuffer(info->name, sizeof(info->name), anim.name.c_str());
    info->duration = anim.duration;
    info->ticksPerSecond = anim.ticksPerSecond;
    info->channelCount = static_cast<uint32_t>(anim.channels.size());

    return LC_SUCCESS;
}

LC_API LCResult lc_model_release(LCModelAssetHandle asset) {
    return lc_asset_release(reinterpret_cast<LCAssetHandle>(asset));
}

/* ============================================================================
 * Audio Asset Functions
 * ============================================================================ */

LC_API LCResult lc_audio_asset_load(const char* filepath, LCAudioLoadMode loadMode, LCAudioAssetHandle* asset) {
    if (!filepath) {
        SetError(LC_ERROR_NULL_POINTER, "Filepath is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (!asset) {
        SetError(LC_ERROR_NULL_POINTER, "Asset output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    auto audioAsset = new lupine::asset::AudioAsset();
    if (!audioAsset->LoadFromFile(filepath, ToEngineAudioLoadMode(loadMode))) {
        delete audioAsset;
        SetError(LC_ERROR_ASSET_LOAD_FAILED, "Failed to load audio from file");
        return LC_ERROR_ASSET_LOAD_FAILED;
    }

    void* handle = CreateUniqueHandle();
    {
        std::lock_guard<std::mutex> lock(g_assetMutex);
        g_audioAssets[handle] = lupine::asset::AssetRef<lupine::asset::AudioAsset>(audioAsset);
    }

    *asset = static_cast<LCAudioAssetHandle>(handle);
    return LC_SUCCESS;
}

LC_API LCResult lc_audio_asset_get_info(LCAudioAssetHandle asset, LCAudioInfo* info) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!info) {
        SetError(LC_ERROR_NULL_POINTER, "Info pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_audioAssets.find(asset);
    if (it == g_audioAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid audio asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::AudioAsset* audioAsset = it->second.Get();
    info->sampleRate = audioAsset->GetSampleRate();
    info->channels = audioAsset->GetChannels();
    info->bitsPerSample = audioAsset->GetBitsPerSample();
    info->format = ToCAudioFormat(audioAsset->GetFormat());
    info->loadMode = ToCAudioLoadMode(audioAsset->GetLoadMode());
    info->duration = audioAsset->GetDuration();
    info->dataSize = audioAsset->GetDataSize();
    info->isStreaming = audioAsset->IsStreaming();

    return LC_SUCCESS;
}

LC_API LCResult lc_audio_asset_get_data(LCAudioAssetHandle asset, const uint8_t** data, size_t* dataSize) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!data || !dataSize) {
        SetError(LC_ERROR_NULL_POINTER, "Output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_audioAssets.find(asset);
    if (it == g_audioAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid audio asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::AudioAsset* audioAsset = it->second.Get();
    if (audioAsset->IsStreaming()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Cannot get data pointer for streaming audio");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *data = audioAsset->GetData();
    *dataSize = audioAsset->GetDataSize();

    return LC_SUCCESS;
}

LC_API LCResult lc_audio_asset_release(LCAudioAssetHandle asset) {
    return lc_asset_release(reinterpret_cast<LCAssetHandle>(asset));
}

/* ============================================================================
 * Font Asset Functions
 * ============================================================================ */

LC_API LCResult lc_font_load(const char* filepath, float fontSize, uint32_t atlasWidth, uint32_t atlasHeight, LCFontAssetHandle* asset) {
    if (!filepath) {
        SetError(LC_ERROR_NULL_POINTER, "Filepath is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (!asset) {
        SetError(LC_ERROR_NULL_POINTER, "Asset output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (fontSize <= 0.0f) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid font size");
        return LC_ERROR_INVALID_PARAMETER;
    }
    if (atlasWidth == 0 || atlasHeight == 0) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid atlas dimensions");
        return LC_ERROR_INVALID_PARAMETER;
    }

    auto fontAsset = new lupine::asset::FontAsset();
    if (!fontAsset->LoadFromFile(filepath, fontSize, atlasWidth, atlasHeight)) {
        delete fontAsset;
        SetError(LC_ERROR_ASSET_LOAD_FAILED, "Failed to load font from file");
        return LC_ERROR_ASSET_LOAD_FAILED;
    }

    void* handle = CreateUniqueHandle();
    {
        std::lock_guard<std::mutex> lock(g_assetMutex);
        g_fontAssets[handle] = lupine::asset::AssetRef<lupine::asset::FontAsset>(fontAsset);
    }

    *asset = static_cast<LCFontAssetHandle>(handle);
    return LC_SUCCESS;
}

LC_API LCResult lc_font_get_info(LCFontAssetHandle asset, LCFontInfo* info) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!info) {
        SetError(LC_ERROR_NULL_POINTER, "Info pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_fontAssets.find(asset);
    if (it == g_fontAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid font asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::FontAsset* fontAsset = it->second.Get();
    info->fontSize = fontAsset->GetFontSize();
    info->lineHeight = fontAsset->GetLineHeight();
    info->ascent = fontAsset->GetAscent();
    info->descent = fontAsset->GetDescent();
    info->atlasWidth = fontAsset->GetAtlasWidth();
    info->atlasHeight = fontAsset->GetAtlasHeight();
    info->glyphCount = static_cast<uint32_t>(fontAsset->GetAllGlyphs().size());

    return LC_SUCCESS;
}

LC_API LCResult lc_font_get_glyph(LCFontAssetHandle asset, uint32_t codepoint, LCGlyph* glyph) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!glyph) {
        SetError(LC_ERROR_NULL_POINTER, "Glyph pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_fontAssets.find(asset);
    if (it == g_fontAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid font asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::FontAsset* fontAsset = it->second.Get();
    const lupine::asset::Glyph* srcGlyph = fontAsset->GetGlyph(codepoint);
    if (!srcGlyph) {
        SetError(LC_ERROR_ASSET_NOT_FOUND, "Glyph not found");
        return LC_ERROR_ASSET_NOT_FOUND;
    }

    glyph->codepoint = srcGlyph->codepoint;
    glyph->advance = srcGlyph->advance;
    glyph->bearingX = srcGlyph->bearingX;
    glyph->bearingY = srcGlyph->bearingY;
    glyph->width = srcGlyph->width;
    glyph->height = srcGlyph->height;
    glyph->atlasX = srcGlyph->atlasX;
    glyph->atlasY = srcGlyph->atlasY;
    glyph->atlasWidth = srcGlyph->atlasWidth;
    glyph->atlasHeight = srcGlyph->atlasHeight;
    glyph->pixelX = srcGlyph->pixelX;
    glyph->pixelY = srcGlyph->pixelY;
    glyph->pixelWidth = srcGlyph->pixelWidth;
    glyph->pixelHeight = srcGlyph->pixelHeight;

    return LC_SUCCESS;
}

LC_API LCResult lc_font_has_glyph(LCFontAssetHandle asset, uint32_t codepoint, bool* hasGlyph) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!hasGlyph) {
        SetError(LC_ERROR_NULL_POINTER, "HasGlyph pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_fontAssets.find(asset);
    if (it == g_fontAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid font asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::FontAsset* fontAsset = it->second.Get();
    *hasGlyph = fontAsset->HasGlyph(codepoint);

    return LC_SUCCESS;
}

LC_API LCResult lc_font_get_atlas_data(LCFontAssetHandle asset, const uint8_t** data, size_t* dataSize) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!data || !dataSize) {
        SetError(LC_ERROR_NULL_POINTER, "Output pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_fontAssets.find(asset);
    if (it == g_fontAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid font asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::FontAsset* fontAsset = it->second.Get();
    *data = fontAsset->GetAtlasData();
    *dataSize = fontAsset->GetAtlasDataSize();

    return LC_SUCCESS;
}

LC_API LCResult lc_font_get_kerning(LCFontAssetHandle asset, uint32_t first, uint32_t second, float* kerning) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!kerning) {
        SetError(LC_ERROR_NULL_POINTER, "Kerning pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_fontAssets.find(asset);
    if (it == g_fontAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid font asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::FontAsset* fontAsset = it->second.Get();
    *kerning = fontAsset->GetKerning(first, second);

    return LC_SUCCESS;
}

LC_API LCResult lc_font_measure_text(LCFontAssetHandle asset, const char* text, float fontSize, LCVec2* size) {
    if (!asset) {
        SetError(LC_ERROR_INVALID_HANDLE, "Asset handle is null");
        return LC_ERROR_INVALID_HANDLE;
    }
    if (!text) {
        SetError(LC_ERROR_NULL_POINTER, "Text is null");
        return LC_ERROR_NULL_POINTER;
    }
    if (!size) {
        SetError(LC_ERROR_NULL_POINTER, "Size pointer is null");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_assetMutex);
    auto it = g_fontAssets.find(asset);
    if (it == g_fontAssets.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid font asset handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    lupine::asset::FontAsset* fontAsset = it->second.Get();
    lupine::math::Vec2 measured = fontAsset->MeasureText(text, fontSize);
    size->x = measured.x;
    size->y = measured.y;

    return LC_SUCCESS;
}

LC_API LCResult lc_font_release(LCFontAssetHandle asset) {
    return lc_asset_release(reinterpret_cast<LCAssetHandle>(asset));
}
