#include "lupine/components/StaticMesh3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/platform/FileSystem.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

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

    if (texturePath[0] == '*') {
        return texturePath;
    }

    std::string filename = platform::Path::GetFilename(texturePath);

    if (platform::Path::IsAbsolute(texturePath)) {

        if (platform::FileSystem::Exists(texturePath)) {
            return texturePath;
        }

    }

    std::string resolvedPath = platform::Path::Join(modelDir, texturePath);

    if (platform::FileSystem::Exists(resolvedPath)) {
        return resolvedPath;
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

            return testPath;
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
        TextureDesc desc;
        desc.width = slot.albedoTextureAsset->GetWidth();
        desc.height = slot.albedoTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_SRGB;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = slot.albedoTextureAsset->GetData();
        slot.albedoTextureHandle = device->createTexture(desc);

    }

    if (slot.metallicRoughnessTextureAsset && slot.metallicRoughnessTextureAsset->IsLoaded() && !slot.metallicRoughnessTextureHandle.isValid()) {
        TextureDesc desc;
        desc.width = slot.metallicRoughnessTextureAsset->GetWidth();
        desc.height = slot.metallicRoughnessTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = slot.metallicRoughnessTextureAsset->GetData();
        slot.metallicRoughnessTextureHandle = device->createTexture(desc);

    }

    if (slot.normalTextureAsset && slot.normalTextureAsset->IsLoaded() && !slot.normalTextureHandle.isValid()) {
        TextureDesc desc;
        desc.width = slot.normalTextureAsset->GetWidth();
        desc.height = slot.normalTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = slot.normalTextureAsset->GetData();
        slot.normalTextureHandle = device->createTexture(desc);

    }

    if (slot.emissiveTextureAsset && slot.emissiveTextureAsset->IsLoaded() && !slot.emissiveTextureHandle.isValid()) {
        TextureDesc desc;
        desc.width = slot.emissiveTextureAsset->GetWidth();
        desc.height = slot.emissiveTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_SRGB;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = slot.emissiveTextureAsset->GetData();
        slot.emissiveTextureHandle = device->createTexture(desc);

    }

    slot.texturesNeedUpload = false;
}

void StaticMesh3D::LoadAndUploadMaterialTextures(RenderContext& ctx, MaterialSlot& slot) {
    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    if (!slot.albedoTexturePath.empty() && !slot.albedoTextureHandle.isValid()) {
        if (!slot.albedoTextureAsset.IsValid()) {

            if (LoadTexture(slot.albedoTexturePath, slot.albedoTextureAsset)) {

            }
        }

        if (slot.albedoTextureAsset.IsValid() && slot.albedoTextureAsset->IsLoaded()) {
            TextureDesc desc;
            desc.width = slot.albedoTextureAsset->GetWidth();
            desc.height = slot.albedoTextureAsset->GetHeight();
            desc.format = TextureFormat::RGBA8_SRGB;
            desc.usage = TextureUsage::Sampled;
            desc.initialData = slot.albedoTextureAsset->GetData();
            slot.albedoTextureHandle = device->createTexture(desc);

        }
    }

    if (!slot.metallicRoughnessTexturePath.empty() && !slot.metallicRoughnessTextureHandle.isValid()) {
        if (!slot.metallicRoughnessTextureAsset.IsValid()) {
            if (LoadTexture(slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureAsset)) {

            }
        }

        if (slot.metallicRoughnessTextureAsset.IsValid() && slot.metallicRoughnessTextureAsset->IsLoaded()) {
            TextureDesc desc;
            desc.width = slot.metallicRoughnessTextureAsset->GetWidth();
            desc.height = slot.metallicRoughnessTextureAsset->GetHeight();
            desc.format = TextureFormat::RGBA8_UNORM;
            desc.usage = TextureUsage::Sampled;
            desc.initialData = slot.metallicRoughnessTextureAsset->GetData();
            slot.metallicRoughnessTextureHandle = device->createTexture(desc);

        }
    }

    if (!slot.normalTexturePath.empty() && !slot.normalTextureHandle.isValid()) {
        if (!slot.normalTextureAsset.IsValid()) {
            if (LoadTexture(slot.normalTexturePath, slot.normalTextureAsset)) {

            }
        }

        if (slot.normalTextureAsset.IsValid() && slot.normalTextureAsset->IsLoaded()) {
            TextureDesc desc;
            desc.width = slot.normalTextureAsset->GetWidth();
            desc.height = slot.normalTextureAsset->GetHeight();
            desc.format = TextureFormat::RGBA8_UNORM;
            desc.usage = TextureUsage::Sampled;
            desc.initialData = slot.normalTextureAsset->GetData();
            slot.normalTextureHandle = device->createTexture(desc);

        }
    }

    if (!slot.emissiveTexturePath.empty() && !slot.emissiveTextureHandle.isValid()) {
        if (!slot.emissiveTextureAsset.IsValid()) {
            if (LoadTexture(slot.emissiveTexturePath, slot.emissiveTextureAsset)) {

            }
        }

        if (slot.emissiveTextureAsset.IsValid() && slot.emissiveTextureAsset->IsLoaded()) {
            TextureDesc desc;
            desc.width = slot.emissiveTextureAsset->GetWidth();
            desc.height = slot.emissiveTextureAsset->GetHeight();
            desc.format = TextureFormat::RGBA8_SRGB;
            desc.usage = TextureUsage::Sampled;
            desc.initialData = slot.emissiveTextureAsset->GetData();
            slot.emissiveTextureHandle = device->createTexture(desc);

        }
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

    MaterialHandle defaultMaterial = ctx.getDefaultPBRMaterial();
    if (!defaultMaterial.isValid()) {

        return;
    }

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

        if (slot && slot->enableOverride) {

            useOverride = true;

            UploadMaterialSlotTextures(ctx, *slot);

            overrides.setColor("u_AlbedoColor", slot->albedoColor);
            overrides.setVec4("u_MaterialParams1", Vec4(slot->metallic, slot->roughness, slot->normalScale, slot->emissiveStrength));
            overrides.setColor("u_EmissiveColor", slot->emissiveColor);
            overrides.setFloat("u_AlphaCutoff", slot->alphaCutoff);

            Vec4 textureFlags(
                slot->albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->normalTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );
            overrides.setVec4("u_TextureFlags", textureFlags);

            if (slot->albedoTextureHandle.isValid()) {
                overrides.setTexture("u_AlbedoTexture", slot->albedoTextureHandle);
            }
            if (slot->metallicRoughnessTextureHandle.isValid()) {
                overrides.setTexture("u_MetallicRoughnessTexture", slot->metallicRoughnessTextureHandle);
            }
            if (slot->normalTextureHandle.isValid()) {
                overrides.setTexture("u_NormalTexture", slot->normalTextureHandle);
            }
            if (slot->emissiveTextureHandle.isValid()) {
                overrides.setTexture("u_EmissiveTexture", slot->emissiveTextureHandle);
            }

        } else if (materialIndex < materials.size() && slot) {

            const auto& material = materials[materialIndex];

            overrides.setColor("u_AlbedoColor", Color(material.albedo.x, material.albedo.y, material.albedo.z, material.opacity));
            overrides.setVec4("u_MaterialParams1", Vec4(material.metallic, material.roughness, 1.0f, 1.0f));
            overrides.setColor("u_EmissiveColor", Color(material.emissive.x, material.emissive.y, material.emissive.z, 1.0f));
            overrides.setFloat("u_AlphaCutoff", 0.5f);

            LoadAndUploadMaterialTextures(ctx, *slot);

            Vec4 textureFlags(
                slot->albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->normalTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );
            overrides.setVec4("u_TextureFlags", textureFlags);

            if (slot->albedoTextureHandle.isValid()) {
                overrides.setTexture("u_AlbedoTexture", slot->albedoTextureHandle);

            }
            if (slot->metallicRoughnessTextureHandle.isValid()) {
                overrides.setTexture("u_MetallicRoughnessTexture", slot->metallicRoughnessTextureHandle);

            }
            if (slot->normalTextureHandle.isValid()) {
                overrides.setTexture("u_NormalTexture", slot->normalTextureHandle);

            }
            if (slot->emissiveTextureHandle.isValid()) {
                overrides.setTexture("u_EmissiveTexture", slot->emissiveTextureHandle);

            }

        }

        ctx.drawMesh(meshHandle, defaultMaterial, transform, overrides, meshIdx, GetCastShadow(), GetReceiveShadow());
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
    std::string currentPath = GetModelPath();
    if (currentPath != path) {
        SetPropertyValue<std::string>("modelPath", path);
        m_CurrentModelPath = path;

        if (!path.empty()) {
            LoadModel(path);
        }
    }
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

