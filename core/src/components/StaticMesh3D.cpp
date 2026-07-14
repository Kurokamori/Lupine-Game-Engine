#include "lupine/components/StaticMesh3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include <algorithm>
#include <unordered_map>
#include <mutex>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

// Static texture cache to share textures across all StaticMesh3D instances
// Use lazy initialization to avoid static initialization order issues
static std::unordered_map<std::string, TextureHandle>& GetStaticMesh3DTextureCache() {
    static std::unordered_map<std::string, TextureHandle> s_TextureCache;
    return s_TextureCache;
}

static std::mutex& GetStaticMesh3DTextureCacheMutex() {
    static std::mutex s_TextureCacheMutex;
    return s_TextureCacheMutex;
}

// Lazy registration with TextureCache system for hot-reloading
static void EnsureStaticMesh3DCacheRegistered() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    rendering::TextureCache::RegisterCache(
        "StaticMesh3D",
        [](const std::string& path) -> bool {
            std::lock_guard<std::mutex> lock(GetStaticMesh3DTextureCacheMutex());
            auto it = GetStaticMesh3DTextureCache().find(path);
            if (it != GetStaticMesh3DTextureCache().end()) {
                // Don't destroy - let the GfxDevice handle cleanup
                // Just remove from cache so it gets reloaded
                GetStaticMesh3DTextureCache().erase(it);
                return true;
            }
            return false;
        },
        []() {
            std::lock_guard<std::mutex> lock(GetStaticMesh3DTextureCacheMutex());
            GetStaticMesh3DTextureCache().clear();
        }
    );
}

// Helper to get or create a cached texture
static TextureHandle GetOrCreateCachedTexture(
    IGfxDevice* device,
    const std::string& path,
    asset::AssetRef<asset::ImageAsset>& asset,
    TextureFormat format)
{
    if (path.empty() || !device) {
        return TextureHandle();
    }

    // Ensure cache is registered with TextureCache system (lazy init)
    EnsureStaticMesh3DCacheRegistered();

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(GetStaticMesh3DTextureCacheMutex());
        auto it = GetStaticMesh3DTextureCache().find(path);
        if (it != GetStaticMesh3DTextureCache().end() && it->second.isValid()) {
            return it->second;
        }
    }

    // Not in cache, need to load
    if (!asset.IsValid()) {
        asset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
        if (!asset->LoadFromFile(path)) {
            return TextureHandle();
        }
    }

    if (!asset->IsLoaded()) {
        return TextureHandle();
    }

    // Create texture
    TextureHandle handle = lupine::CreateTexture2DFromImage(device, *asset, format);

    // Cache it
    {
        std::lock_guard<std::mutex> lock(GetStaticMesh3DTextureCacheMutex());
        GetStaticMesh3DTextureCache()[path] = handle;
    }

    return handle;
}

StaticMesh3D::StaticMesh3D()
    : Component("StaticMesh3D")
    , m_MeshesNeedUpload(false)
{
}

StaticMesh3D::StaticMesh3D(const std::string& name)
    : Component(name)
    , m_MeshesNeedUpload(false)
{
}

StaticMesh3D::~StaticMesh3D() {

}

void StaticMesh3D::DefineProperties() {

    DefineProperty(PROPERTY_FILE_GROUP(modelPath, std::string(""), "*.gltf,*.glb,*.fbx,*.obj", "Model"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(castShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(doubleSided, Bool, false, "Rendering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(materialSlotCount, Int, 0, "Material Slots"));
}

void StaticMesh3D::OnAwake() {

    std::string modelPath = GetModelPath();
    if (!modelPath.empty()) {
        LoadModel(modelPath);
    }
}

void StaticMesh3D::OnReady() {

}

void StaticMesh3D::OnRender() {

}

bool StaticMesh3D::LoadModel(const std::string& filepath) {
    if (filepath.empty()) {

        return false;
    }

    m_ModelAsset = asset::AssetRef<asset::ModelAsset>(new asset::ModelAsset());

    bool loaded = m_ModelAsset->LoadFromFile(filepath, true);

    if (!loaded) {

        m_ModelAsset.Reset();
        return false;
    }

    m_MeshesNeedUpload = true;

    CreateMaterialSlotsFromModel();

    SetModelPath(filepath);
    SetPropertyValue<int>("materialSlotCount", static_cast<int>(m_MaterialSlots.size()));

    return true;
}

void StaticMesh3D::CreateMaterialSlotsFromModel() {
    m_MaterialSlots.clear();

    if (!m_ModelAsset.IsValid()) {
        return;
    }

    const auto& materials = m_ModelAsset->GetMaterials();
    std::string modelDir = platform::Path::GetDirectory(m_ModelAsset->GetPath());

    for (uint32_t i = 0; i < materials.size(); ++i) {
        MaterialSlot slot;
        slot.name = materials[i].name.empty() ? ("Material_" + std::to_string(i)) : materials[i].name;
        slot.materialIndex = i;
        slot.enableOverride = false;

        slot.albedoColor = Color(materials[i].albedo.x, materials[i].albedo.y, materials[i].albedo.z, materials[i].opacity);
        slot.metallic = materials[i].metallic;
        slot.roughness = materials[i].roughness;
        slot.emissiveColor = Color(materials[i].emissive.x, materials[i].emissive.y, materials[i].emissive.z, 1.0f);

        if (!materials[i].albedoMap.empty()) {
            slot.albedoTexturePath = ResolveTexturePath(materials[i].albedoMap, modelDir);
        }
        if (!materials[i].normalMap.empty()) {
            slot.normalTexturePath = ResolveTexturePath(materials[i].normalMap, modelDir);
        }
        if (!materials[i].metallicMap.empty()) {
            slot.metallicRoughnessTexturePath = ResolveTexturePath(materials[i].metallicMap, modelDir);
        }
        if (!materials[i].emissiveMap.empty()) {
            slot.emissiveTexturePath = ResolveTexturePath(materials[i].emissiveMap, modelDir);
        }

        m_MaterialSlots.push_back(slot);
    }

}

void StaticMesh3D::UploadMeshesToGPU(RenderContext& ctx) {
    if (!m_MeshesNeedUpload || !m_ModelAsset.IsValid()) {
        return;
    }

    if (!m_MeshHandles.empty() && ctx.getDevice()) {
        for (auto& handle : m_MeshHandles) {
            if (handle.isValid()) {
                ctx.getDevice()->destroyMesh(handle);
            }
        }
        m_MeshHandles.clear();
    }

    const auto& meshes = m_ModelAsset->GetMeshes();
    m_MeshHandles.reserve(meshes.size());

    for (const auto& assetMesh : meshes) {

        MeshData meshData;

        meshData.vertices.reserve(assetMesh.vertices.size());
        for (const auto& assetVertex : assetMesh.vertices) {
            Vertex vertex;
            vertex.position = assetVertex.position;
            vertex.normal = assetVertex.normal;
            vertex.texCoord = assetVertex.texCoord;
            vertex.tangent = assetVertex.tangent;
            vertex.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            meshData.vertices.push_back(vertex);
        }

        meshData.indices = assetMesh.indices;

        meshData.bounds = AABB(assetMesh.boundsMin, assetMesh.boundsMax);

        if (ctx.getDevice()) {
            MeshHandle handle = ctx.getDevice()->createMesh(meshData);
            m_MeshHandles.push_back(handle);

        }
    }

    m_MeshesNeedUpload = false;
}

void StaticMesh3D::UploadMeshesToGPU(IGfxDevice* device) {
    if (!m_MeshesNeedUpload || !m_ModelAsset.IsValid() || !device) {
        return;
    }

    if (!m_MeshHandles.empty()) {
        for (auto& handle : m_MeshHandles) {
            if (handle.isValid()) {
                device->destroyMesh(handle);
            }
        }
        m_MeshHandles.clear();
    }

    const auto& meshes = m_ModelAsset->GetMeshes();
    m_MeshHandles.reserve(meshes.size());

    for (const auto& assetMesh : meshes) {
        MeshData meshData;

        meshData.vertices.reserve(assetMesh.vertices.size());
        for (const auto& assetVertex : assetMesh.vertices) {
            Vertex vertex;
            vertex.position = assetVertex.position;
            vertex.normal = assetVertex.normal;
            vertex.texCoord = assetVertex.texCoord;
            vertex.tangent = assetVertex.tangent;
            vertex.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            meshData.vertices.push_back(vertex);
        }

        meshData.indices = assetMesh.indices;
        meshData.bounds = AABB(assetMesh.boundsMin, assetMesh.boundsMax);

        MeshHandle handle = device->createMesh(meshData);
        m_MeshHandles.push_back(handle);
    }

    m_MeshesNeedUpload = false;
}

void StaticMesh3D::prepareGPUResources(IGfxDevice* device) {
    if (!device) {
        return;
    }

    // Upload meshes to GPU if needed
    if (m_MeshesNeedUpload && m_ModelAsset.IsValid()) {
        UploadMeshesToGPU(device);
    }

    // Pre-load and upload textures for all material slots (using cache for sharing)
    for (auto& slot : m_MaterialSlots) {
        // Albedo texture (use cache to share across instances)
        if (!slot.albedoTexturePath.empty() && !slot.albedoTextureHandle.isValid()) {
            slot.albedoTextureHandle = GetOrCreateCachedTexture(
                device, slot.albedoTexturePath, slot.albedoTextureAsset, TextureFormat::RGBA8_SRGB);
        }

        // Metallic roughness texture
        if (!slot.metallicRoughnessTexturePath.empty() && !slot.metallicRoughnessTextureHandle.isValid()) {
            slot.metallicRoughnessTextureHandle = GetOrCreateCachedTexture(
                device, slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureAsset, TextureFormat::RGBA8_UNORM);
        }

        // Normal texture
        if (!slot.normalTexturePath.empty() && !slot.normalTextureHandle.isValid()) {
            slot.normalTextureHandle = GetOrCreateCachedTexture(
                device, slot.normalTexturePath, slot.normalTextureAsset, TextureFormat::RGBA8_UNORM);
        }

        // Emissive texture
        if (!slot.emissiveTexturePath.empty() && !slot.emissiveTextureHandle.isValid()) {
            slot.emissiveTextureHandle = GetOrCreateCachedTexture(
                device, slot.emissiveTexturePath, slot.emissiveTextureAsset, TextureFormat::RGBA8_SRGB);
        }
    }
}

bool StaticMesh3D::LoadTexture(const std::string& filepath, asset::AssetRef<asset::ImageAsset>& outAsset) {
    if (filepath.empty()) {
        return false;
    }

    if (filepath[0] == '*' && m_ModelAsset.IsValid()) {
        return LoadEmbeddedTexture(filepath, outAsset);
    }

    outAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());

    asset::ImageColorSpace colorSpace = asset::ImageColorSpace::sRGB;
    std::string lowerPath = filepath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    if (lowerPath.find("normal") != std::string::npos ||
        lowerPath.find("metallic") != std::string::npos ||
        lowerPath.find("roughness") != std::string::npos) {
        colorSpace = asset::ImageColorSpace::Linear;
    }

    bool loaded = outAsset->LoadFromFile(filepath, true, colorSpace);

    if (!loaded) {

        outAsset.Reset();
        return false;
    }

    return true;
}

bool StaticMesh3D::LoadEmbeddedTexture(const std::string& textureName, asset::AssetRef<asset::ImageAsset>& outAsset) {
    if (!m_ModelAsset.IsValid()) {

        return false;
    }

    const asset::EmbeddedTexture* embeddedTex = m_ModelAsset->GetEmbeddedTexture(textureName);
    if (!embeddedTex) {

        return false;
    }

    outAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());

    asset::ImageColorSpace colorSpace = asset::ImageColorSpace::sRGB;
    std::string lowerName = textureName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName.find("normal") != std::string::npos ||
        lowerName.find("metallic") != std::string::npos ||
        lowerName.find("roughness") != std::string::npos) {
        colorSpace = asset::ImageColorSpace::Linear;
    }

    bool loaded = false;

    if (embeddedTex->compressed) {

        loaded = outAsset->LoadFromMemory(
            embeddedTex->data.data(),
            embeddedTex->data.size(),
            true,
            colorSpace
        );
    } else {

        loaded = outAsset->LoadFromRawData(
            embeddedTex->data.data(),
            embeddedTex->width,
            embeddedTex->height,
            4,
            true,
            colorSpace
        );
    }

    if (!loaded) {

        outAsset.Reset();
        return false;
    }

    return true;
}

std::string StaticMesh3D::ResolveTexturePath(const std::string& texturePath, const std::string& modelDir) {
    if (texturePath.empty()) {
        return "";
    }

    // Embedded texture reference - pass through
    if (texturePath[0] == '*') {
        return texturePath;
    }

    // If it's already a res:// path, verify it exists and return as-is
    if (texturePath.size() >= 6 && texturePath.substr(0, 6) == "res://") {
        auto& packFS = platform::PackFileSystem::Instance();
        if (packFS.isPackMode()) {
            // In pack mode, just check if it exists in pack
            if (packFS.exists(texturePath)) {
                return texturePath;
            }
        } else {
            // In editor mode, check via AssetDatabase
            auto& assetDb = asset::AssetDatabase::GetInstance();
            if (assetDb.IsInitialized()) {
                std::string physicalPath = assetDb.ResolveAsset(texturePath);
                if (platform::FileSystem::Exists(physicalPath)) {
                    return texturePath;
                }
            }
        }
        // Path doesn't exist but it's a res:// path, return it anyway for error handling
        return texturePath;
    }

    // Check if running from pack file - resolve relative path
    auto& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        // Resolve relative to model directory
        std::string resolvedPath = modelDir + texturePath;
        std::replace(resolvedPath.begin(), resolvedPath.end(), '\\', '/');
        if (packFS.exists(resolvedPath)) {
            return resolvedPath;
        }
    }

    std::string filename = platform::Path::GetFilename(texturePath);

    // For absolute paths, try to convert to res:// format
    if (platform::Path::IsAbsolute(texturePath)) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string resPath = assetDb.ToResourcePath(texturePath);
            if (!resPath.empty() && platform::FileSystem::Exists(texturePath)) {
                return resPath;
            }
        }
        // If conversion failed but file exists, return as-is (backwards compat)
        if (platform::FileSystem::Exists(texturePath)) {
            return texturePath;
        }
    }

    std::string resolvedPath = platform::Path::Join(modelDir, texturePath);

    if (platform::FileSystem::Exists(resolvedPath)) {
        // Convert to res:// if possible
        auto& assetDb = asset::AssetDatabase::GetInstance();
        std::string resPath = assetDb.ToResourcePath(resolvedPath);
        return resPath.empty() ? resolvedPath : resPath;
    }

    std::vector<std::string> searchDirs = {
        "",
        "textures",
        "Textures",
        "texture",
        "Texture",
        "maps",
        "Maps",
        "materials",
        "Materials",
        "../textures",
        "../Textures",
        "../glTF",
        "../GLTF",
        "../Texture",
        "../Materials"
    };

    for (const auto& subdir : searchDirs) {
        std::string testPath;
        if (subdir.empty()) {
            testPath = platform::Path::Join(modelDir, filename);
        } else {
            testPath = platform::Path::Join(modelDir, subdir);
            testPath = platform::Path::Join(testPath, filename);
        }

        if (platform::FileSystem::Exists(testPath)) {
            // Convert to res:// if possible
            auto& assetDb = asset::AssetDatabase::GetInstance();
            std::string resPath = assetDb.ToResourcePath(testPath);
            return resPath.empty() ? testPath : resPath;
        }
    }

    return resolvedPath;
}

void StaticMesh3D::UploadMaterialSlotTextures(RenderContext& ctx, MaterialSlot& slot) {
    if (!slot.texturesNeedUpload) {
        return;
    }

    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    if (slot.albedoTextureAsset && slot.albedoTextureAsset->IsLoaded() && !slot.albedoTextureHandle.isValid()) {
        slot.albedoTextureHandle = lupine::CreateTexture2DFromImage(device, *slot.albedoTextureAsset, TextureFormat::RGBA8_SRGB);

    }

    if (slot.metallicRoughnessTextureAsset && slot.metallicRoughnessTextureAsset->IsLoaded() && !slot.metallicRoughnessTextureHandle.isValid()) {
        slot.metallicRoughnessTextureHandle = lupine::CreateTexture2DFromImage(device, *slot.metallicRoughnessTextureAsset, TextureFormat::RGBA8_UNORM);

    }

    if (slot.normalTextureAsset && slot.normalTextureAsset->IsLoaded() && !slot.normalTextureHandle.isValid()) {
        slot.normalTextureHandle = lupine::CreateTexture2DFromImage(device, *slot.normalTextureAsset, TextureFormat::RGBA8_UNORM);

    }

    if (slot.emissiveTextureAsset && slot.emissiveTextureAsset->IsLoaded() && !slot.emissiveTextureHandle.isValid()) {
        slot.emissiveTextureHandle = lupine::CreateTexture2DFromImage(device, *slot.emissiveTextureAsset, TextureFormat::RGBA8_SRGB);

    }

    slot.texturesNeedUpload = false;
}

void StaticMesh3D::LoadAndUploadMaterialTextures(RenderContext& ctx, MaterialSlot& slot) {
    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    // Use texture cache to share textures across instances
    if (!slot.albedoTexturePath.empty() && !slot.albedoTextureHandle.isValid()) {
        slot.albedoTextureHandle = GetOrCreateCachedTexture(
            device, slot.albedoTexturePath, slot.albedoTextureAsset, TextureFormat::RGBA8_SRGB);
    }

    if (!slot.metallicRoughnessTexturePath.empty() && !slot.metallicRoughnessTextureHandle.isValid()) {
        slot.metallicRoughnessTextureHandle = GetOrCreateCachedTexture(
            device, slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureAsset, TextureFormat::RGBA8_UNORM);
    }

    if (!slot.normalTexturePath.empty() && !slot.normalTextureHandle.isValid()) {
        slot.normalTextureHandle = GetOrCreateCachedTexture(
            device, slot.normalTexturePath, slot.normalTextureAsset, TextureFormat::RGBA8_UNORM);
    }

    if (!slot.emissiveTexturePath.empty() && !slot.emissiveTextureHandle.isValid()) {
        slot.emissiveTextureHandle = GetOrCreateCachedTexture(
            device, slot.emissiveTexturePath, slot.emissiveTextureAsset, TextureFormat::RGBA8_SRGB);
    }
}

void StaticMesh3D::buildDrawCommands(RenderContext& ctx) {

    std::string currentPath = GetModelPath();
    if (currentPath != m_CurrentModelPath) {

        if (!m_MeshHandles.empty()) {
            IGfxDevice* device = ctx.getDevice();
            if (device) {
                for (const auto& handle : m_MeshHandles) {
                    if (handle.isValid()) {
                        device->destroyMesh(handle);
                    }
                }
                m_MeshHandles.clear();

            }
        }

        m_ModelAsset.Reset();
        m_MaterialSlots.clear();

        if (!currentPath.empty()) {

            m_ModelAsset = asset::AssetRef<asset::ModelAsset>(new asset::ModelAsset());

            bool loaded = m_ModelAsset->LoadFromFile(currentPath, true);

            if (!loaded) {

                m_ModelAsset.Reset();
            } else {

                m_MeshesNeedUpload = true;

                CreateMaterialSlotsFromModel();

                SetPropertyValue<int>("materialSlotCount", static_cast<int>(m_MaterialSlots.size()));
            }
        }

        m_CurrentModelPath = currentPath;
    }

    if (!m_ModelAsset.IsValid()) {
        return;
    }

    UploadMeshesToGPU(ctx);

    Node3D* node3D = dynamic_cast<Node3D*>(GetOwner());
    if (!node3D) {
        return;
    }

    Mat4 transform = node3D->GetGlobalTransformMatrix();

    const auto& meshes = m_ModelAsset->GetMeshes();
    const auto& materials = m_ModelAsset->GetMaterials();

    for (size_t meshIdx = 0; meshIdx < m_MeshHandles.size(); ++meshIdx) {
        const MeshHandle& meshHandle = m_MeshHandles[meshIdx];
        if (!meshHandle.isValid()) {
            continue;
        }

        const auto& assetMesh = meshes[meshIdx];
        uint32_t materialIndex = assetMesh.materialIndex;

        MaterialPropertyBlock overrides;
        bool useOverride = false;

        MaterialSlot* slot = nullptr;
        for (auto& s : m_MaterialSlots) {
            if (s.materialIndex == materialIndex) {
                slot = &s;
                break;
            }
        }

        // Select material based on shader type using the material registry
        ShaderType shaderType = slot ? slot->shaderType : ShaderType::PBR;
        MaterialHandle selectedMaterial;

        // A custom .lsh shader takes precedence: translated at runtime for the active
        // backend with the 3D-mesh vertex layout; its optional #render_mode drives the
        // pipeline blend/cull/depth state (opaque default by 3=opaque).
        const bool hasLsh = (slot && !slot->customLshShaderPath.empty());
        if (hasLsh) {
            selectedMaterial = ctx.getOrCreateLshMaterial(
                slot->customLshShaderPath, 3 /*opaque default*/, LshMaterialLayout::Mesh3D);
        } else if (shaderType == ShaderType::Custom && slot &&
            (!slot->customVertShaderPath.empty() || !slot->customFragShaderPath.empty())) {
            // Handle custom shaders (supports single-file shaders like HLSL)
            selectedMaterial = ctx.getOrCreateCustomMaterial(
                slot->customVertShaderPath,
                slot->customFragShaderPath,
                false  // not skeletal
            );
        } else {
            selectedMaterial = ctx.getMaterial(shaderType, false);  // false = not skeletal
        }

        if (!selectedMaterial.isValid()) {
            continue;
        }

        if (slot && (slot->enableOverride || hasLsh)) {

            useOverride = true;

            UploadMaterialSlotTextures(ctx, *slot);

            overrides.setColor("u_AlbedoColor", slot->albedoColor);
            overrides.setColor("u_EmissiveColor", slot->emissiveColor);
            overrides.setFloat("u_AlphaCutoff", slot->alphaCutoff);

            // Set ALL material uniforms - shaders use what they need and ignore the rest
            // This approach supports custom shaders without requiring per-shader-type code paths

            // PBR uniforms: u_MaterialParams1 = metallic, roughness, normalScale, emissiveStrength
            overrides.setVec4("u_MaterialParams1", Vec4(slot->metallic, slot->roughness, slot->normalScale, slot->emissiveStrength));
            // PBR uniforms: u_MaterialParams2 = alphaCutoff, aoStrength, heightScale, unused
            overrides.setVec4("u_MaterialParams2", Vec4(slot->alphaCutoff, 1.0f, 1.0f, 0.0f));

            // Toon uniforms: u_ToonMaterialParams = shadowBands, specularBands, normalScale, emissiveStrength
            overrides.setVec4("u_ToonMaterialParams", Vec4(slot->shadowBands, slot->specularBands, slot->normalScale, slot->emissiveStrength));
            // Toon uniforms: u_ToonMaterialParams2 = alphaCutoff, rimPower, rimIntensity, specularPower
            overrides.setVec4("u_ToonMaterialParams2", Vec4(slot->alphaCutoff, slot->rimPower, slot->rimIntensity, slot->specularPower));
            // Toon uniforms: u_ToonParams = shadowThreshold, shadowSoftness, specularThreshold, specularSoftness
            overrides.setVec4("u_ToonParams", Vec4(slot->shadowThreshold, slot->shadowSoftness, slot->shadowThreshold, slot->shadowSoftness));

            // Stylized uniforms: u_MaterialParams1 = shadowSoftness, specularSoftness, normalScale, emissiveStrength
            if (shaderType == ShaderType::Stylized) {
                overrides.setVec4("u_MaterialParams1", Vec4(slot->stylizedShadowSoftness, slot->stylizedSpecularSoftness, slot->normalScale, slot->emissiveStrength));
                // u_MaterialParams2 = alphaCutoff, rimPower, rimIntensity, specularPower (shared with toon)
                overrides.setVec4("u_MaterialParams2", Vec4(slot->alphaCutoff, slot->rimPower, slot->rimIntensity, slot->specularPower));
                // u_StylizedParams = shadowBrightness, shadowWarmth, specularIntensity, halfLambertPower
                overrides.setVec4("u_StylizedParams", Vec4(slot->stylizedShadowBrightness, slot->stylizedShadowWarmth, slot->stylizedSpecularIntensity, slot->stylizedHalfLambertPower));
            }

            // Apply generic shader parameters from shaderParams map
            // This allows custom shaders to receive their parameters without C++ code changes
            for (const auto& [uniformName, value] : slot->shaderParams) {
                std::visit([&overrides, &uniformName](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, float>) {
                        overrides.setFloat(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, int>) {
                        overrides.setInt(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, bool>) {
                        overrides.setBool(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec2>) {
                        overrides.setVec2(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec3>) {
                        overrides.setVec3(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec4>) {
                        overrides.setVec4(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Color>) {
                        overrides.setColor(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, TextureHandle>) {
                        overrides.setTexture(uniformName, arg);
                    }
                    // Mat4 arrays handled separately if needed
                }, value);
            }

            Vec4 textureFlags(
                slot->albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->normalTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );
            overrides.setVec4("u_TextureFlags", textureFlags);

            // Always bind textures (valid or not) to prevent stale texture bleeding
            overrides.setTexture("u_AlbedoTexture", slot->albedoTextureHandle);
            overrides.setTexture("u_MetallicRoughnessTexture", slot->metallicRoughnessTextureHandle);
            overrides.setTexture("u_NormalTexture", slot->normalTextureHandle);
            overrides.setTexture("u_EmissiveTexture", slot->emissiveTextureHandle);

        } else if (materialIndex < materials.size() && slot) {

            const auto& material = materials[materialIndex];

            overrides.setColor("u_AlbedoColor", Color(material.albedo.x, material.albedo.y, material.albedo.z, material.opacity));
            overrides.setColor("u_EmissiveColor", Color(material.emissive.x, material.emissive.y, material.emissive.z, 1.0f));
            overrides.setFloat("u_AlphaCutoff", 0.5f);

            // Set ALL material uniforms - shaders use what they need and ignore the rest
            // This approach supports custom shaders without requiring per-shader-type code paths

            // PBR uniforms: u_MaterialParams1 = metallic, roughness, normalScale, emissiveStrength
            overrides.setVec4("u_MaterialParams1", Vec4(material.metallic, material.roughness, 1.0f, 1.0f));
            // PBR uniforms: u_MaterialParams2 = alphaCutoff, aoStrength, heightScale, unused
            overrides.setVec4("u_MaterialParams2", Vec4(0.5f, 1.0f, 1.0f, 0.0f));

            // Toon uniforms: u_ToonMaterialParams = shadowBands, specularBands, normalScale, emissiveStrength
            overrides.setVec4("u_ToonMaterialParams", Vec4(slot->shadowBands, slot->specularBands, 1.0f, 1.0f));
            // Toon uniforms: u_ToonMaterialParams2 = alphaCutoff, rimPower, rimIntensity, specularPower
            overrides.setVec4("u_ToonMaterialParams2", Vec4(0.5f, slot->rimPower, slot->rimIntensity, slot->specularPower));
            // Toon uniforms: u_ToonParams = shadowThreshold, shadowSoftness, specularThreshold, specularSoftness
            overrides.setVec4("u_ToonParams", Vec4(slot->shadowThreshold, slot->shadowSoftness, slot->shadowThreshold, slot->shadowSoftness));

            // Apply generic shader parameters from shaderParams map
            for (const auto& [uniformName, value] : slot->shaderParams) {
                std::visit([&overrides, &uniformName](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, float>) {
                        overrides.setFloat(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, int>) {
                        overrides.setInt(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, bool>) {
                        overrides.setBool(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec2>) {
                        overrides.setVec2(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec3>) {
                        overrides.setVec3(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec4>) {
                        overrides.setVec4(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Color>) {
                        overrides.setColor(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, TextureHandle>) {
                        overrides.setTexture(uniformName, arg);
                    }
                }, value);
            }

            LoadAndUploadMaterialTextures(ctx, *slot);

            Vec4 textureFlags(
                slot->albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->normalTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );
            overrides.setVec4("u_TextureFlags", textureFlags);

            // DEBUG: Log texture validity for WebGL debugging
            static bool loggedOnce = false;
            if (!loggedOnce && slot->albedoTextureHandle.isValid()) {
                
                loggedOnce = true;
            }

            // Always bind textures (valid or not) to prevent stale texture bleeding
            overrides.setTexture("u_AlbedoTexture", slot->albedoTextureHandle);
            overrides.setTexture("u_MetallicRoughnessTexture", slot->metallicRoughnessTextureHandle);
            overrides.setTexture("u_NormalTexture", slot->normalTextureHandle);
            overrides.setTexture("u_EmissiveTexture", slot->emissiveTextureHandle);

        }

        ctx.drawMesh(meshHandle, selectedMaterial, transform, overrides, static_cast<uint32_t>(meshIdx), GetCastShadow(), GetReceiveShadow());
    }
}

AABB StaticMesh3D::getWorldBounds() const {

    AABB localBounds = CalculateCombinedBounds();

    if (!m_Owner) {
        return localBounds;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return localBounds;
    }

    Mat4 worldTransform = node3D->GetGlobalTransformMatrix();
    return localBounds.Transform(worldTransform);
}

AABB StaticMesh3D::CalculateCombinedBounds() const {
    if (!m_ModelAsset.IsValid()) {
        return AABB();
    }

    const auto& meshes = m_ModelAsset->GetMeshes();
    if (meshes.empty()) {
        return AABB();
    }

    AABB combined(meshes[0].boundsMin, meshes[0].boundsMax);

    for (size_t i = 1; i < meshes.size(); ++i) {
        AABB meshBounds(meshes[i].boundsMin, meshes[i].boundsMax);
        combined = AABB::Merge(combined, meshBounds);
    }

    return combined;
}

bool StaticMesh3D::IntersectRay(const math::Ray& ray, float& outDistance) const {
    if (!m_ModelAsset.IsValid()) {

        return IRenderableComponent::IntersectRay(ray, outDistance);
    }

    if (!m_Owner) {
        return false;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return false;
    }

    Mat4 worldTransform = node3D->GetGlobalTransformMatrix();
    Mat4 invWorldTransform = worldTransform.Inverse();

    Vec4 localOrigin4 = invWorldTransform * Vec4(ray.origin.x, ray.origin.y, ray.origin.z, 1.0f);
    Vec3 localOrigin(localOrigin4.x / localOrigin4.w, localOrigin4.y / localOrigin4.w, localOrigin4.z / localOrigin4.w);

    Vec4 localDir4 = invWorldTransform * Vec4(ray.direction.x, ray.direction.y, ray.direction.z, 0.0f);
    Vec3 localDir(localDir4.x, localDir4.y, localDir4.z);
    localDir = localDir.Normalized();

    math::Ray localRay(localOrigin, localDir);

    const auto& meshes = m_ModelAsset->GetMeshes();
    float closestHit = std::numeric_limits<float>::infinity();
    bool hitAny = false;

    for (const auto& mesh : meshes) {
        const auto& vertices = mesh.vertices;
        const auto& indices = mesh.indices;

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const Vec3& v0 = vertices[indices[i]].position;
            const Vec3& v1 = vertices[indices[i + 1]].position;
            const Vec3& v2 = vertices[indices[i + 2]].position;

            float hitDist = 0.0f;
            if (localRay.IntersectTriangle(v0, v1, v2, hitDist)) {
                if (hitDist < closestHit) {
                    closestHit = hitDist;
                    hitAny = true;
                }
            }
        }
    }

    if (hitAny) {

        Vec3 localHitPoint = localRay.GetPoint(closestHit);

        Vec4 worldHitPoint4 = worldTransform * Vec4(localHitPoint.x, localHitPoint.y, localHitPoint.z, 1.0f);
        Vec3 worldHitPoint(worldHitPoint4.x / worldHitPoint4.w,
                          worldHitPoint4.y / worldHitPoint4.w,
                          worldHitPoint4.z / worldHitPoint4.w);

        outDistance = (worldHitPoint - ray.origin).Length();
        return true;
    }

    return false;
}

math::OBB StaticMesh3D::getOrientedBounds() const {

    AABB localBounds = CalculateCombinedBounds();

    if (!m_Owner) {
        return math::OBB::FromAABB(localBounds);
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return math::OBB::FromAABB(localBounds);
    }

    Vec3 worldPos = node3D->GetGlobalPosition();
    Quat worldRot = node3D->GetGlobalRotation();
    Vec3 worldScale = node3D->GetGlobalScale();

    Vec3 localCenter = localBounds.GetCenter();
    Vec3 localHalfExtents = localBounds.GetExtents();

    Vec3 scaledCenter(
        localCenter.x * worldScale.x,
        localCenter.y * worldScale.y,
        localCenter.z * worldScale.z
    );
    Vec3 worldCenter = worldPos + worldRot * scaledCenter;

    Vec3 scaledHalfExtents(
        localHalfExtents.x * std::abs(worldScale.x),
        localHalfExtents.y * std::abs(worldScale.y),
        localHalfExtents.z * std::abs(worldScale.z)
    );

    return math::OBB(worldCenter, scaledHalfExtents, worldRot);
}

RenderLayer StaticMesh3D::getRenderLayer() const {

    if (m_ModelAsset.IsValid()) {
        const auto& materials = m_ModelAsset->GetMaterials();
        for (const auto& material : materials) {
            if (material.opacity < 1.0f) {
                return RenderLayer::Transparent;
            }
        }
    }

    for (const auto& slot : m_MaterialSlots) {
        if (slot.enableOverride && slot.albedoColor.a < 1.0f) {
            return RenderLayer::Transparent;
        }
    }

    return RenderLayer::Opaque;
}

std::string StaticMesh3D::GetModelPath() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("modelPath");
    if (prop) {
        return prop->GetValue<std::string>();
    }
    return std::string("");
}

void StaticMesh3D::SetModelPath(const std::string& path) {
    // Convert to res:// path if possible
    std::string resPath = path;
    if (!path.empty() && !(path.size() >= 6 && path.substr(0, 6) == "res://")) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(path);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }

    std::string currentPath = GetModelPath();
    if (currentPath != resPath) {
        SetPropertyValue<std::string>("modelPath", resPath);
        m_CurrentModelPath = resPath;

        if (!resPath.empty()) {
            LoadModel(resPath);
        }
    }
}

bool StaticMesh3D::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    auto& assetDb = asset::AssetDatabase::GetInstance();
    bool anyChanged = false;

    // Check if this is our model
    std::string currentModelPath = GetModelPath();
    if (!currentModelPath.empty()) {
        std::string resolvedModelPath;
        if (assetDb.IsInitialized()) {
            resolvedModelPath = assetDb.ResolveAsset(currentModelPath);
        }

        bool modelMatches = (currentModelPath == changedPath) ||
                       (!resolvedModelPath.empty() && !resolvedChangedPath.empty() &&
                        resolvedModelPath == resolvedChangedPath);

        if (modelMatches) {
            
            m_MeshHandles.clear();
            m_ModelAsset.Reset();
            m_CurrentModelPath.clear();
            m_MeshesNeedUpload = true;
            m_MaterialSlots.clear();
            return true;
        }
    }

    // Check if this is one of our material slot textures
    for (auto& slot : m_MaterialSlots) {
        auto checkTexture = [&](const std::string& texPath, TextureHandle& handle,
                                asset::AssetRef<asset::ImageAsset>& asset) -> bool {
            if (texPath.empty()) return false;
            std::string resolvedTexPath;
            if (assetDb.IsInitialized()) {
                resolvedTexPath = assetDb.ResolveAsset(texPath);
            }
            bool matches = (texPath == changedPath) ||
                           (!resolvedTexPath.empty() && !resolvedChangedPath.empty() &&
                            resolvedTexPath == resolvedChangedPath);
            if (matches) {
                
                {
                    std::lock_guard<std::mutex> lock(GetStaticMesh3DTextureCacheMutex());
                    auto& cache = GetStaticMesh3DTextureCache();
                    auto it = cache.find(texPath);
                    if (it != cache.end()) { cache.erase(it); }
                }
                handle = TextureHandle();
                asset.Reset();
                slot.texturesNeedUpload = true;
                return true;
            }
            return false;
        };

        if (checkTexture(slot.albedoTexturePath, slot.albedoTextureHandle, slot.albedoTextureAsset)) anyChanged = true;
        if (checkTexture(slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureHandle, slot.metallicRoughnessTextureAsset)) anyChanged = true;
        if (checkTexture(slot.normalTexturePath, slot.normalTextureHandle, slot.normalTextureAsset)) anyChanged = true;
        if (checkTexture(slot.emissiveTexturePath, slot.emissiveTextureHandle, slot.emissiveTextureAsset)) anyChanged = true;
    }

    return anyChanged;
}

bool StaticMesh3D::GetCastShadow() const {
    return GetPropertyValue<bool>("castShadow");
}

void StaticMesh3D::SetCastShadow(bool castShadow) {
    SetPropertyValue<bool>("castShadow", castShadow);
}

bool StaticMesh3D::GetReceiveShadow() const {
    return GetPropertyValue<bool>("receiveShadow");
}

void StaticMesh3D::SetReceiveShadow(bool receiveShadow) {
    SetPropertyValue<bool>("receiveShadow", receiveShadow);
}

bool StaticMesh3D::GetDoubleSided() const {
    return GetPropertyValue<bool>("doubleSided");
}

void StaticMesh3D::SetDoubleSided(bool doubleSided) {
    SetPropertyValue<bool>("doubleSided", doubleSided);
}

MaterialSlot* StaticMesh3D::GetMaterialSlot(uint32_t index) {
    if (index >= m_MaterialSlots.size()) {
        return nullptr;
    }
    return &m_MaterialSlots[index];
}

const MaterialSlot* StaticMesh3D::GetMaterialSlot(uint32_t index) const {
    if (index >= m_MaterialSlots.size()) {
        return nullptr;
    }
    return &m_MaterialSlots[index];
}

void StaticMesh3D::SetMaterialSlotOverrideEnabled(uint32_t slotIndex, bool enabled) {
    MaterialSlot* slot = GetMaterialSlot(slotIndex);
    if (slot) {
        slot->enableOverride = enabled;
    }
}

void StaticMesh3D::SetMaterialSlotAlbedoColor(uint32_t slotIndex, const Color& color) {
    MaterialSlot* slot = GetMaterialSlot(slotIndex);
    if (slot) {
        slot->albedoColor = color;
    }
}

void StaticMesh3D::SetMaterialSlotAlbedoTexture(uint32_t slotIndex, const std::string& texturePath) {
    MaterialSlot* slot = GetMaterialSlot(slotIndex);
    if (slot) {
        slot->albedoTexturePath = texturePath;
        if (!texturePath.empty()) {
            if (LoadTexture(texturePath, slot->albedoTextureAsset)) {
                slot->texturesNeedUpload = true;

                slot->albedoTextureHandle = TextureHandle();
            }
        } else {
            slot->albedoTextureAsset.Reset();
            slot->albedoTextureHandle = TextureHandle();
        }
    }
}

}
}

