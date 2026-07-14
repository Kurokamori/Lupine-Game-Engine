#include "lupine/components/MultiMeshGeneric.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/MeshSimplifier.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/platform/FileSystem.hpp"
#include <algorithm>
#include <cctype>
#include <cfloat>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

MultiMeshGeneric::MultiMeshGeneric()
    : Component("MultiMeshGeneric")
{
}

MultiMeshGeneric::MultiMeshGeneric(const std::string& name)
    : Component(name)
{
}

MultiMeshGeneric::~MultiMeshGeneric() {
}

void MultiMeshGeneric::DefineProperties() {
    // Mesh group
    DefineProperty(PROPERTY_FILE_GROUP(meshPath, std::string(""), "*.gltf,*.glb,*.fbx,*.obj", "Mesh"));
    DefineProperty(PROPERTY_FILE_GROUP(materialOverride, std::string(""), "*.mat,*.material", "Mesh"));
    // Custom instanced .lsh shader (translated at runtime, honors #render_mode). Must
    // declare the per-instance inputs a_InstanceModel0..3/Color/Custom (locations 4-9),
    // as in the built-in pbr_instanced.lsh. Takes precedence over the default material.
    DefineProperty(PROPERTY_FILE_GROUP(customLshShaderPath, std::string(""), "*.lsh", "Mesh"));

    // Shadow settings group
    DefineProperty(PROPERTY_ENUM_GROUP(castShadow, 1, "Shadows", Off, On, OnlyShadows));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Shadows"));

    // Base transform applied to every instance before per-instance variation.
    DefineProperty(PROPERTY_DEFAULT_GROUP(baseRotation, Vec3, Vec3(0.0f, 0.0f, 0.0f), "Base Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(baseScale, Vec3, Vec3(1.0f, 1.0f, 1.0f), "Base Transform"));

    // Instance settings group
    DefineProperty(PROPERTY_INT_RANGE_GROUP(instanceCount, 0, 0, 100000, 1, "Instances"));

    // Culling settings group
    DefineProperty(PROPERTY_DEFAULT_GROUP(cullPerInstance, Bool, true, "Culling"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxDistance, 1000.0f, 0.0f, 100000.0f, 10.0f, "Culling"));

    // LOD settings group: optional lower-detail meshes + the camera distance at
    // which each takes over from the more detailed level above it.
    DefineProperty(PROPERTY_FILE_GROUP(lodGroup, std::string(""), "*.lod", "LOD"));
    DefineProperty(PROPERTY_FILE_GROUP(lod1MeshPath, std::string(""), "*.gltf,*.glb,*.fbx,*.obj", "LOD"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lod1Distance, 30.0f, 0.0f, 100000.0f, 5.0f, "LOD"));
    DefineProperty(PROPERTY_FILE_GROUP(lod2MeshPath, std::string(""), "*.gltf,*.glb,*.fbx,*.obj", "LOD"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lod2Distance, 80.0f, 0.0f, 100000.0f, 5.0f, "LOD"));
    DefineProperty(PROPERTY_FILE_GROUP(lod3MeshPath, std::string(""), "*.gltf,*.glb,*.fbx,*.obj", "LOD"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lod3Distance, 200.0f, 0.0f, 100000.0f, 5.0f, "LOD"));

    // Automatic LOD generation: decimate the base mesh to fill empty LOD slots.
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoGenerateLods, Bool, false, "LOD"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(autoLodReduction, 0.5f, 0.05f, 0.95f, 0.05f, "LOD"));

    // Editor settings group
    DefineProperty(PROPERTY_DEFAULT_GROUP(editableInEditor, Bool, true, "Editor"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(previewSingleInstance, Bool, false, "Editor"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(previewInEditor, Bool, true, "Editor"));
}

void MultiMeshGeneric::OnAwake() {
    std::string meshPath = GetMeshPath();
    if (!meshPath.empty()) {
        LoadMesh(meshPath);
    }

    int count = GetInstanceCount();
    m_Instances.resize(std::max(0, count));
}

void MultiMeshGeneric::OnReady() {
}

bool MultiMeshGeneric::LoadMesh(const std::string& filepath) {
    if (filepath.empty()) {
        return false;
    }

    LODLevel& level = m_Lods[0];
    level.asset = asset::AssetRef<asset::ModelAsset>(new asset::ModelAsset());

    bool loaded = level.asset->LoadFromFile(filepath, true);
    if (!loaded) {
        level.asset.Reset();
        return false;
    }

    level.needsUpload = true;
    level.currentPath = filepath;
    SetMeshPath(filepath);

    return true;
}

void MultiMeshGeneric::SyncLevelPaths(IGfxDevice* device) {
    for (int lvl = 0; lvl < kLodLevelCount; ++lvl) {
        LODLevel& level = m_Lods[lvl];
        std::string path = GetLodMeshPath(lvl);

        if (path == level.currentPath) {
            continue;
        }

        // Path changed: free the old GPU resources and reload the model.
        if (device) {
            ReleaseLevelGPU(device, level);
        }
        level.materials.clear();
        level.asset.Reset();
        level.currentPath = path;
        level.needsUpload = false;

        if (!path.empty()) {
            level.asset = asset::AssetRef<asset::ModelAsset>(new asset::ModelAsset());
            if (level.asset->LoadFromFile(path, true)) {
                level.needsUpload = true;
            } else {
                level.asset.Reset();
            }
        }
    }
}

void MultiMeshGeneric::UploadLevelMeshes(IGfxDevice* device, LODLevel& level) {
    if (!device || !level.needsUpload || !level.asset.IsValid()) {
        return;
    }

    if (!level.meshHandles.empty()) {
        for (MeshHandle& handle : level.meshHandles) {
            if (handle.isValid()) {
                device->destroyMesh(handle);
            }
        }
        level.meshHandles.clear();
    }

    const std::vector<asset::Mesh>& meshes = level.asset->GetMeshes();
    const std::vector<asset::Material>& materials = level.asset->GetMaterials();

    if (meshes.empty()) {
        level.needsUpload = false;
        return;
    }

    level.meshHandles.reserve(meshes.size());
    for (const asset::Mesh& assetMesh : meshes) {
        MeshData meshData;
        meshData.vertices.reserve(assetMesh.vertices.size());

        for (const asset::Vertex& assetVertex : assetMesh.vertices) {
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
        level.meshHandles.push_back(handle);
    }

    level.materials.clear();
    level.materials.resize(meshes.size());

    std::string modelDir = platform::Path::GetDirectory(level.currentPath);

    for (size_t i = 0; i < meshes.size(); ++i) {
        const asset::Mesh& assetMesh = meshes[i];
        MeshMaterial& meshMat = level.materials[i];

        if (assetMesh.materialIndex < materials.size()) {
            const asset::Material& mat = materials[assetMesh.materialIndex];

            meshMat.albedoColor = Color(mat.albedo.x, mat.albedo.y, mat.albedo.z, mat.opacity);
            meshMat.metallic = mat.metallic;
            meshMat.roughness = mat.roughness;
            meshMat.emissiveColor = Color(mat.emissive.x, mat.emissive.y, mat.emissive.z, 1.0f);

            if (!mat.albedoMap.empty()) {
                meshMat.albedoTexturePath = ResolveTexturePath(mat.albedoMap, modelDir);
            }
            if (!mat.normalMap.empty()) {
                meshMat.normalTexturePath = ResolveTexturePath(mat.normalMap, modelDir);
            }
            if (!mat.metallicMap.empty()) {
                meshMat.metallicRoughnessTexturePath = ResolveTexturePath(mat.metallicMap, modelDir);
            }
            if (!mat.emissiveMap.empty()) {
                meshMat.emissiveTexturePath = ResolveTexturePath(mat.emissiveMap, modelDir);
            }
        }
    }

    level.needsUpload = false;
}

void MultiMeshGeneric::LoadLevelMaterialTextures(IGfxDevice* device, LODLevel& level) {
    if (!device) {
        return;
    }

    for (MeshMaterial& meshMat : level.materials) {
        if (!meshMat.albedoTexturePath.empty() && !meshMat.albedoTextureHandle.isValid()) {
            if (!meshMat.albedoTextureAsset.IsValid()) {
                LoadTexture(level.asset, meshMat.albedoTexturePath, meshMat.albedoTextureAsset);
            }
            if (meshMat.albedoTextureAsset.IsValid() && meshMat.albedoTextureAsset->IsLoaded()) {
                meshMat.albedoTextureHandle = lupine::CreateTexture2DFromImage(device, *meshMat.albedoTextureAsset, TextureFormat::RGBA8_SRGB);
            }
        }

        if (!meshMat.normalTexturePath.empty() && !meshMat.normalTextureHandle.isValid()) {
            if (!meshMat.normalTextureAsset.IsValid()) {
                LoadTexture(level.asset, meshMat.normalTexturePath, meshMat.normalTextureAsset);
            }
            if (meshMat.normalTextureAsset.IsValid() && meshMat.normalTextureAsset->IsLoaded()) {
                meshMat.normalTextureHandle = lupine::CreateTexture2DFromImage(device, *meshMat.normalTextureAsset, TextureFormat::RGBA8_UNORM);
            }
        }

        if (!meshMat.metallicRoughnessTexturePath.empty() && !meshMat.metallicRoughnessTextureHandle.isValid()) {
            if (!meshMat.metallicRoughnessTextureAsset.IsValid()) {
                LoadTexture(level.asset, meshMat.metallicRoughnessTexturePath, meshMat.metallicRoughnessTextureAsset);
            }
            if (meshMat.metallicRoughnessTextureAsset.IsValid() && meshMat.metallicRoughnessTextureAsset->IsLoaded()) {
                meshMat.metallicRoughnessTextureHandle = lupine::CreateTexture2DFromImage(device, *meshMat.metallicRoughnessTextureAsset, TextureFormat::RGBA8_UNORM);
            }
        }

        if (!meshMat.emissiveTexturePath.empty() && !meshMat.emissiveTextureHandle.isValid()) {
            if (!meshMat.emissiveTextureAsset.IsValid()) {
                LoadTexture(level.asset, meshMat.emissiveTexturePath, meshMat.emissiveTextureAsset);
            }
            if (meshMat.emissiveTextureAsset.IsValid() && meshMat.emissiveTextureAsset->IsLoaded()) {
                meshMat.emissiveTextureHandle = lupine::CreateTexture2DFromImage(device, *meshMat.emissiveTextureAsset, TextureFormat::RGBA8_SRGB);
            }
        }
    }
}

void MultiMeshGeneric::ReleaseLevelGPU(IGfxDevice* device, LODLevel& level) {
    if (!device) {
        return;
    }

    for (MeshHandle& handle : level.meshHandles) {
        if (handle.isValid()) {
            device->destroyMesh(handle);
        }
    }
    level.meshHandles.clear();

    // Auto-generated levels borrow the base level's textures; only the owning
    // (base/manual) level may destroy them.
    if (!level.borrowsTextures) {
    for (MeshMaterial& meshMat : level.materials) {
        if (meshMat.albedoTextureHandle.isValid()) {
            device->destroyTexture(meshMat.albedoTextureHandle);
            meshMat.albedoTextureHandle = TextureHandle();
        }
        if (meshMat.normalTextureHandle.isValid()) {
            device->destroyTexture(meshMat.normalTextureHandle);
            meshMat.normalTextureHandle = TextureHandle();
        }
        if (meshMat.metallicRoughnessTextureHandle.isValid()) {
            device->destroyTexture(meshMat.metallicRoughnessTextureHandle);
            meshMat.metallicRoughnessTextureHandle = TextureHandle();
        }
        if (meshMat.emissiveTextureHandle.isValid()) {
            device->destroyTexture(meshMat.emissiveTextureHandle);
            meshMat.emissiveTextureHandle = TextureHandle();
        }
    }
    }
    level.materials.clear();
    level.borrowsTextures = false;
    level.autoGenerated = false;

    if (level.instanceBuffer.isValid()) {
        device->destroyBuffer(level.instanceBuffer);
        level.instanceBuffer = BufferHandle();
    }
    level.instanceCapacity = 0;
}

bool MultiMeshGeneric::GenerateDecimatedLevel(IGfxDevice* device, LODLevel& level, float ratio) {
    if (!device) {
        return false;
    }

    LODLevel& base = m_Lods[0];
    if (!base.asset.IsValid() || base.meshHandles.empty()) {
        return false;
    }

    const std::vector<asset::Mesh>& meshes = base.asset->GetMeshes();
    if (meshes.empty()) {
        return false;
    }

    // Discard any prior GPU state for this level (also clears flags/materials).
    ReleaseLevelGPU(device, level);

    level.meshHandles.reserve(meshes.size());
    for (const asset::Mesh& assetMesh : meshes) {
        MeshData meshData;
        meshData.vertices.reserve(assetMesh.vertices.size());
        for (const asset::Vertex& assetVertex : assetMesh.vertices) {
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

        MeshData simplified = MeshSimplifier::Simplify(meshData, ratio);
        MeshHandle handle = device->createMesh(simplified);
        level.meshHandles.push_back(handle);
    }

    // Borrow the base level's per-submesh materials (including texture handles);
    // the auto level must not destroy those shared textures.
    level.materials = base.materials;
    level.borrowsTextures = true;
    level.autoGenerated = true;
    level.needsUpload = false;
    level.currentPath.clear();

    return true;
}

void MultiMeshGeneric::SyncAutoLods(IGfxDevice* device) {
    if (!device) {
        return;
    }

    bool enabled = GetAutoGenerateLods();
    float reduction = GetAutoLodReduction();
    const std::string& basePath = m_Lods[0].currentPath;

    bool baseReady = m_Lods[0].asset.IsValid() && !m_Lods[0].meshHandles.empty();

    // Nothing to do if the relevant inputs are unchanged since the last build.
    if (m_AutoLodsBuilt &&
        enabled == m_AutoLodEnabled &&
        reduction == m_AutoLodReduction &&
        basePath == m_AutoLodSourcePath) {
        return;
    }

    for (int lvl = 1; lvl < kLodLevelCount; ++lvl) {
        LODLevel& level = m_Lods[lvl];

        // Author-supplied LOD meshes always win; never overwrite them.
        if (!GetLodMeshPath(lvl).empty()) {
            continue;
        }

        if (enabled && baseReady) {
            // ratio shrinks with each level: reduction^lvl.
            float ratio = 1.0f;
            for (int r = 0; r < lvl; ++r) {
                ratio *= reduction;
            }
            GenerateDecimatedLevel(device, level, ratio);
        } else if (level.autoGenerated) {
            // Auto generation was turned off (or base unavailable): drop it.
            ReleaseLevelGPU(device, level);
        }
    }

    m_AutoLodEnabled = enabled;
    m_AutoLodReduction = reduction;
    m_AutoLodSourcePath = basePath;
    m_AutoLodsBuilt = baseReady || !enabled;
}

int MultiMeshGeneric::SelectLodForDistance(float distance) const {
    // Walk from the lowest-detail level down to the base, choosing the first
    // level whose distance threshold the instance is past and whose mesh is
    // populated. Distances thresholds: lod1Distance < lod2Distance < lod3Distance.
    for (int lvl = kLodLevelCount - 1; lvl >= 1; --lvl) {
        if (distance >= GetLodDistance(lvl) && !m_Lods[lvl].meshHandles.empty()) {
            return lvl;
        }
    }
    return 0;
}

bool MultiMeshGeneric::EnsureInstanceBuffer(IGfxDevice* device, LODLevel& level, uint32_t count) {
    if (!device || count == 0) {
        return false;
    }

    if (level.instanceBuffer.isValid() && level.instanceCapacity >= count) {
        return true;
    }

    if (level.instanceBuffer.isValid()) {
        device->destroyBuffer(level.instanceBuffer);
        level.instanceBuffer = BufferHandle();
    }

    uint32_t capacity = level.instanceCapacity > 0 ? level.instanceCapacity : 64u;
    while (capacity < count) {
        capacity *= 2u;
    }

    BufferDesc desc;
    desc.size = static_cast<uint64_t>(capacity) * sizeof(InstanceVertexData);
    desc.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
    desc.initialData = nullptr;
    // Per-instance data is refilled whenever instances change, so keep it host-visible
    // and persistently mapped (no staging-copy GPU stall on update, notably on Vulkan).
    desc.dynamic = true;

    level.instanceBuffer = device->createBuffer(desc);
    if (!level.instanceBuffer.isValid()) {
        level.instanceCapacity = 0;
        return false;
    }

    level.instanceCapacity = capacity;
    return true;
}

void MultiMeshGeneric::prepareGPUResources(IGfxDevice* device) {
    if (!device) {
        return;
    }

    SyncLevelPaths(device);
    for (LODLevel& level : m_Lods) {
        if (level.asset.IsValid()) {
            UploadLevelMeshes(device, level);
            LoadLevelMaterialTextures(device, level);
        }
    }
}

void MultiMeshGeneric::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return;
    }

    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    // Keep each LOD level's loaded model in sync with its path property, then
    // upload meshes and material textures for every populated level.
    SyncLevelPaths(device);
    for (LODLevel& level : m_Lods) {
        if (!level.asset.IsValid()) {
            continue;
        }
        UploadLevelMeshes(device, level);
        LoadLevelMaterialTextures(device, level);
    }

    // The base level must be present to render anything.
    if (m_Lods[0].meshHandles.empty()) {
        return;
    }

    // Fill any empty LOD slots by decimating the base mesh when auto-LOD is on.
    SyncAutoLods(device);

    // A custom instanced .lsh shader takes precedence over the default PBR instanced
    // material; translated at runtime with the instanced vertex layout (per-instance
    // buffer at binding 1) and its optional #render_mode driving blend/cull/depth.
    MaterialHandle material;
    const std::string lshPath = GetCustomLshShaderPath();
    if (!lshPath.empty()) {
        material = ctx.getOrCreateLshMaterial(lshPath, 3 /*opaque default*/,
                                              LshMaterialLayout::InstancedMesh3D);
    }
    if (!material.isValid()) {
        material = ctx.getDefaultPBRInstancedMaterial();
    }
    if (!material.isValid()) {
        return;
    }

    Mat4 nodeTransform = node3D->GetGlobalTransformMatrix();

    ShadowCastingMode castShadowMode = GetCastShadow();
    bool receiveShadow = GetReceiveShadow();
    bool castShadow = (castShadowMode == ShadowCastingMode::On || castShadowMode == ShadowCastingMode::OnlyShadows);

    bool cullPerInstance = GetCullPerInstance();
    float maxDistance = GetMaxDistance();
    Vec3 cameraPos = ctx.getViewMatrix().Inverse().TransformPoint(Vec3(0.0f, 0.0f, 0.0f));

    // Keep the instance storage in sync with the instanceCount property. The
    // inspector (and scripting) set the property directly without resizing the
    // backing vector, so without this the new instances would never render.
    int instanceCount = GetInstanceCount();
    if (instanceCount < 0) {
        instanceCount = 0;
    }
    if (static_cast<int>(m_Instances.size()) != instanceCount) {
        m_Instances.resize(static_cast<size_t>(instanceCount));
    }

    int renderCount = std::min(instanceCount, static_cast<int>(m_Instances.size()));

    if (GetPreviewSingleInstance() && renderCount > 0) {
        renderCount = 1;
    }

    // Base transform (rotation in degrees + scale) applied to every instance in
    // mesh-local space, before the per-instance transform/variation.
    constexpr float kDegToRad = 3.14159265358979f / 180.0f;
    Vec3 baseRotation = GetBaseRotation();
    Vec3 baseScale = GetBaseScale();
    Mat4 baseTransform = Mat4::TRS(
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(baseRotation.x * kDegToRad, baseRotation.y * kDegToRad, baseRotation.z * kDegToRad),
        baseScale
    );

    // Decide whether the per-instance buckets and GPU buffers must be rebuilt this
    // frame. For a static multimesh under a static camera this is false after the
    // first build, so the per-instance matrix math and the GPU upload are skipped
    // entirely and the previously uploaded buffers + cached bounds are reused.
    int populatedLevels = 0;
    for (const LODLevel& level : m_Lods) {
        if (!level.meshHandles.empty()) {
            ++populatedLevels;
        }
    }
    bool cameraDependent = cullPerInstance || populatedLevels > 1;

    bool needsRebuild =
        m_InstanceBuildDirty ||
        !m_HasBuiltInstances ||
        renderCount != m_LastBuildRenderCount ||
        nodeTransform != m_LastBuildNodeTransform ||
        baseRotation != m_LastBuildBaseRotation ||
        baseScale != m_LastBuildBaseScale;

    if (!needsRebuild && cameraDependent) {
        constexpr float kCameraRebuildThreshold = 0.01f;
        if ((cameraPos - m_LastBuildCameraPos).Length() > kCameraRebuildThreshold) {
            needsRebuild = true;
        }
    }

    if (needsRebuild) {
        // Reset this frame's per-level instance buckets.
        for (LODLevel& level : m_Lods) {
            level.instanceData.clear();
        }

        // Bucket every (visible) instance into the LOD level chosen by its distance.
        for (int i = 0; i < renderCount; ++i) {
            const MeshInstance& instance = m_Instances[i];
            Mat4 finalTransform = nodeTransform * instance.transform * baseTransform;
            Vec3 worldPos = finalTransform.TransformPoint(Vec3(0.0f, 0.0f, 0.0f));
            float distance = (worldPos - cameraPos).Length();

            if (cullPerInstance && distance > maxDistance) {
                continue;
            }

            int lod = SelectLodForDistance(distance);
            if (lod < 0) {
                continue;
            }

            InstanceVertexData data;
            data.setModel(finalTransform);
            data.color = instance.color;
            data.customData = instance.customData;
            m_Lods[lod].instanceData.push_back(data);
        }
    }

    MeshMaterial defaultMeshMat;

    // Emit one instanced draw per (populated LOD level, submesh).
    for (LODLevel& level : m_Lods) {
        if (level.instanceData.empty() || level.meshHandles.empty()) {
            continue;
        }

        uint32_t count = static_cast<uint32_t>(level.instanceData.size());

        if (needsRebuild) {
            if (!EnsureInstanceBuffer(device, level, count)) {
                continue;
            }

            device->updateBuffer(
                level.instanceBuffer,
                level.instanceData.data(),
                static_cast<uint64_t>(count) * sizeof(InstanceVertexData),
                0
            );

            // Combined mesh bounds for this level, expanded over all its instances.
            Vec3 meshMin(FLT_MAX, FLT_MAX, FLT_MAX);
            Vec3 meshMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            if (level.asset.IsValid()) {
                for (const asset::Mesh& mesh : level.asset->GetMeshes()) {
                    meshMin.x = std::min(meshMin.x, mesh.boundsMin.x);
                    meshMin.y = std::min(meshMin.y, mesh.boundsMin.y);
                    meshMin.z = std::min(meshMin.z, mesh.boundsMin.z);
                    meshMax.x = std::max(meshMax.x, mesh.boundsMax.x);
                    meshMax.y = std::max(meshMax.y, mesh.boundsMax.y);
                    meshMax.z = std::max(meshMax.z, mesh.boundsMax.z);
                }
            }
            level.cachedWorldBounds = CombinedInstanceBounds(AABB(meshMin, meshMax), level.instanceData);
        }

        if (!level.instanceBuffer.isValid()) {
            continue;
        }
        const AABB& worldBounds = level.cachedWorldBounds;

        for (size_t meshIdx = 0; meshIdx < level.meshHandles.size(); ++meshIdx) {
            const MeshHandle& meshHandle = level.meshHandles[meshIdx];
            if (!meshHandle.isValid()) {
                continue;
            }

            const MeshMaterial& meshMat = (meshIdx < level.materials.size())
                ? level.materials[meshIdx]
                : defaultMeshMat;

            // Per-submesh material data is shared across the instanced batch; the
            // per-instance albedo tint is supplied through the instance buffer.
            Vec4 matParams1(meshMat.metallic, meshMat.roughness, 1.0f, 1.0f);
            Vec4 matParams2(0.5f, 1.0f, 1.0f, 0.0f);
            Vec4 textureFlags(
                meshMat.albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                meshMat.metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                meshMat.normalTextureHandle.isValid() ? 1.0f : 0.0f,
                meshMat.emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );

            MaterialPropertyBlock overrides;
            overrides.setVec4("u_MaterialParams1", matParams1);
            overrides.setVec4("u_MaterialParams2", matParams2);
            overrides.setVec4("u_TextureFlags", textureFlags);
            overrides.setColor("u_EmissiveColor", meshMat.emissiveColor);
            overrides.setColor("u_AlbedoColor", meshMat.albedoColor);
            overrides.setTexture("u_AlbedoTexture", meshMat.albedoTextureHandle);
            overrides.setTexture("u_MetallicRoughnessTexture", meshMat.metallicRoughnessTextureHandle);
            overrides.setTexture("u_NormalTexture", meshMat.normalTextureHandle);
            overrides.setTexture("u_EmissiveTexture", meshMat.emissiveTextureHandle);

            ctx.drawMeshInstanced(
                meshHandle,
                material,
                level.instanceBuffer,
                count,
                worldBounds,
                overrides,
                static_cast<uint32_t>(meshIdx),
                castShadow,
                receiveShadow
            );
        }
    }

    if (needsRebuild) {
        m_LastBuildNodeTransform = nodeTransform;
        m_LastBuildBaseRotation = baseRotation;
        m_LastBuildBaseScale = baseScale;
        m_LastBuildRenderCount = renderCount;
        m_LastBuildCameraPos = cameraPos;
        m_HasBuiltInstances = true;
        m_InstanceBuildDirty = false;
    }
}

AABB MultiMeshGeneric::getWorldBounds() const {
    if (!m_Owner || m_Instances.empty()) {
        return AABB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return AABB();
    }

    return CalculateCombinedBounds();
}

AABB MultiMeshGeneric::CalculateCombinedBounds() const {
    const LODLevel& base = m_Lods[0];
    if (m_Instances.empty() || !base.asset.IsValid()) {
        return AABB();
    }

    const std::vector<asset::Mesh>& meshes = base.asset->GetMeshes();
    if (meshes.empty()) {
        return AABB();
    }

    Vec3 combinedMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 combinedMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const asset::Mesh& mesh : meshes) {
        combinedMin.x = std::min(combinedMin.x, mesh.boundsMin.x);
        combinedMin.y = std::min(combinedMin.y, mesh.boundsMin.y);
        combinedMin.z = std::min(combinedMin.z, mesh.boundsMin.z);
        combinedMax.x = std::max(combinedMax.x, mesh.boundsMax.x);
        combinedMax.y = std::max(combinedMax.y, mesh.boundsMax.y);
        combinedMax.z = std::max(combinedMax.z, mesh.boundsMax.z);
    }

    AABB meshBounds(combinedMin, combinedMax);

    Vec3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    Mat4 nodeTransform = node3D ? node3D->GetGlobalTransformMatrix() : Mat4::Identity();

    // Mirror the base transform applied per-instance in buildDrawCommands so the
    // component bounds enclose the drawn geometry.
    constexpr float kDegToRad = 3.14159265358979f / 180.0f;
    Vec3 baseRotation = GetBaseRotation();
    Vec3 baseScale = GetBaseScale();
    Mat4 baseTransform = Mat4::TRS(
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(baseRotation.x * kDegToRad, baseRotation.y * kDegToRad, baseRotation.z * kDegToRad),
        baseScale
    );

    Vec3 corners[8] = {
        Vec3(meshBounds.min.x, meshBounds.min.y, meshBounds.min.z),
        Vec3(meshBounds.max.x, meshBounds.min.y, meshBounds.min.z),
        Vec3(meshBounds.min.x, meshBounds.max.y, meshBounds.min.z),
        Vec3(meshBounds.max.x, meshBounds.max.y, meshBounds.min.z),
        Vec3(meshBounds.min.x, meshBounds.min.y, meshBounds.max.z),
        Vec3(meshBounds.max.x, meshBounds.min.y, meshBounds.max.z),
        Vec3(meshBounds.min.x, meshBounds.max.y, meshBounds.max.z),
        Vec3(meshBounds.max.x, meshBounds.max.y, meshBounds.max.z)
    };

    for (const MeshInstance& instance : m_Instances) {
        Mat4 finalTransform = nodeTransform * instance.transform * baseTransform;

        for (const Vec3& corner : corners) {
            Vec3 transformedCorner = finalTransform.TransformPoint(corner);
            minBounds.x = std::min(minBounds.x, transformedCorner.x);
            minBounds.y = std::min(minBounds.y, transformedCorner.y);
            minBounds.z = std::min(minBounds.z, transformedCorner.z);
            maxBounds.x = std::max(maxBounds.x, transformedCorner.x);
            maxBounds.y = std::max(maxBounds.y, transformedCorner.y);
            maxBounds.z = std::max(maxBounds.z, transformedCorner.z);
        }
    }

    return AABB(minBounds, maxBounds);
}

AABB MultiMeshGeneric::CombinedInstanceBounds(const AABB& meshBounds,
                                              const std::vector<InstanceVertexData>& instances) const {
    if (instances.empty()) {
        return AABB();
    }

    Vec3 corners[8] = {
        Vec3(meshBounds.min.x, meshBounds.min.y, meshBounds.min.z),
        Vec3(meshBounds.max.x, meshBounds.min.y, meshBounds.min.z),
        Vec3(meshBounds.min.x, meshBounds.max.y, meshBounds.min.z),
        Vec3(meshBounds.max.x, meshBounds.max.y, meshBounds.min.z),
        Vec3(meshBounds.min.x, meshBounds.min.y, meshBounds.max.z),
        Vec3(meshBounds.max.x, meshBounds.min.y, meshBounds.max.z),
        Vec3(meshBounds.min.x, meshBounds.max.y, meshBounds.max.z),
        Vec3(meshBounds.max.x, meshBounds.max.y, meshBounds.max.z)
    };

    Vec3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const InstanceVertexData& inst : instances) {
        for (const Vec3& corner : corners) {
            // worldPos = col0*x + col1*y + col2*z + col3 (matches the shader).
            Vec3 wp(
                inst.modelCol0.x * corner.x + inst.modelCol1.x * corner.y + inst.modelCol2.x * corner.z + inst.modelCol3.x,
                inst.modelCol0.y * corner.x + inst.modelCol1.y * corner.y + inst.modelCol2.y * corner.z + inst.modelCol3.y,
                inst.modelCol0.z * corner.x + inst.modelCol1.z * corner.y + inst.modelCol2.z * corner.z + inst.modelCol3.z
            );
            minBounds.x = std::min(minBounds.x, wp.x);
            minBounds.y = std::min(minBounds.y, wp.y);
            minBounds.z = std::min(minBounds.z, wp.z);
            maxBounds.x = std::max(maxBounds.x, wp.x);
            maxBounds.y = std::max(maxBounds.y, wp.y);
            maxBounds.z = std::max(maxBounds.z, wp.z);
        }
    }

    return AABB(minBounds, maxBounds);
}

RenderLayer MultiMeshGeneric::getRenderLayer() const {
    return RenderLayer::Opaque;
}

bool MultiMeshGeneric::IsInstanceVisible(int index, const Vec3& cameraPos) const {
    if (index < 0 || index >= static_cast<int>(m_Instances.size())) {
        return false;
    }

    if (!GetCullPerInstance()) {
        return true;
    }

    const MeshInstance& instance = m_Instances[index];
    Vec3 instancePos = instance.transform.TransformPoint(Vec3(0.0f, 0.0f, 0.0f));

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (node3D) {
        Mat4 nodeTransform = node3D->GetGlobalTransformMatrix();
        instancePos = nodeTransform.TransformPoint(instancePos);
    }

    float distance = (instancePos - cameraPos).Length();
    return distance <= GetMaxDistance();
}

// Property accessors

std::string MultiMeshGeneric::GetMeshPath() const {
    return GetPropertyValue<std::string>("meshPath");
}

void MultiMeshGeneric::SetMeshPath(const std::string& path) {
    SetPropertyValue<std::string>("meshPath", path);
    m_InstanceBuildDirty = true;
}

std::string MultiMeshGeneric::GetMaterialOverride() const {
    return GetPropertyValue<std::string>("materialOverride");
}

void MultiMeshGeneric::SetMaterialOverride(const std::string& path) {
    SetPropertyValue<std::string>("materialOverride", path);
}

std::string MultiMeshGeneric::GetCustomLshShaderPath() const {
    return GetPropertyValue<std::string>("customLshShaderPath");
}

void MultiMeshGeneric::SetCustomLshShaderPath(const std::string& path) {
    SetPropertyValue<std::string>("customLshShaderPath", path);
}

ShadowCastingMode MultiMeshGeneric::GetCastShadow() const {
    int mode = GetPropertyValue<int>("castShadow");
    return static_cast<ShadowCastingMode>(mode);
}

void MultiMeshGeneric::SetCastShadow(ShadowCastingMode mode) {
    SetPropertyValue<int>("castShadow", static_cast<int>(mode));
}

bool MultiMeshGeneric::GetReceiveShadow() const {
    return GetPropertyValue<bool>("receiveShadow");
}

void MultiMeshGeneric::SetReceiveShadow(bool receive) {
    SetPropertyValue<bool>("receiveShadow", receive);
}

int MultiMeshGeneric::GetInstanceCount() const {
    return GetPropertyValue<int>("instanceCount");
}

void MultiMeshGeneric::SetInstanceCount(int count) {
    SetPropertyValue<int>("instanceCount", count);
    m_Instances.resize(std::max(0, count));
    m_InstanceBuildDirty = true;
}

void MultiMeshGeneric::SetInstanceTransform(int index, const Mat4& transform) {
    if (index >= 0 && index < static_cast<int>(m_Instances.size())) {
        m_Instances[index].transform = transform;
        m_InstanceBuildDirty = true;
        NotifyRenderStateChanged();
    }
}

Mat4 MultiMeshGeneric::GetInstanceTransform(int index) const {
    if (index >= 0 && index < static_cast<int>(m_Instances.size())) {
        return m_Instances[index].transform;
    }
    return Mat4::Identity();
}

void MultiMeshGeneric::SetInstanceColor(int index, const Color& color) {
    if (index >= 0 && index < static_cast<int>(m_Instances.size())) {
        m_Instances[index].color = color;
        m_InstanceBuildDirty = true;
        NotifyRenderStateChanged();
    }
}

Color MultiMeshGeneric::GetInstanceColor(int index) const {
    if (index >= 0 && index < static_cast<int>(m_Instances.size())) {
        return m_Instances[index].color;
    }
    return Color::White();
}

void MultiMeshGeneric::SetInstanceCustomData(int index, const Vec4& data) {
    if (index >= 0 && index < static_cast<int>(m_Instances.size())) {
        m_Instances[index].customData = data;
        m_InstanceBuildDirty = true;
        NotifyRenderStateChanged();
    }
}

Vec4 MultiMeshGeneric::GetInstanceCustomData(int index) const {
    if (index >= 0 && index < static_cast<int>(m_Instances.size())) {
        return m_Instances[index].customData;
    }
    return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

bool MultiMeshGeneric::GetCullPerInstance() const {
    return GetPropertyValue<bool>("cullPerInstance");
}

void MultiMeshGeneric::SetCullPerInstance(bool cull) {
    SetPropertyValue<bool>("cullPerInstance", cull);
    m_InstanceBuildDirty = true;
}

float MultiMeshGeneric::GetMaxDistance() const {
    return GetPropertyValue<float>("maxDistance");
}

void MultiMeshGeneric::SetMaxDistance(float distance) {
    SetPropertyValue<float>("maxDistance", distance);
    m_InstanceBuildDirty = true;
}

std::string MultiMeshGeneric::GetLodGroup() const {
    return GetPropertyValue<std::string>("lodGroup");
}

void MultiMeshGeneric::SetLodGroup(const std::string& path) {
    SetPropertyValue<std::string>("lodGroup", path);
    m_InstanceBuildDirty = true;
}

std::string MultiMeshGeneric::GetLodMeshPath(int level) const {
    switch (level) {
        case 0: return GetMeshPath();
        case 1: return GetPropertyValue<std::string>("lod1MeshPath");
        case 2: return GetPropertyValue<std::string>("lod2MeshPath");
        case 3: return GetPropertyValue<std::string>("lod3MeshPath");
        default: return std::string();
    }
}

void MultiMeshGeneric::SetLodMeshPath(int level, const std::string& path) {
    switch (level) {
        case 0: SetMeshPath(path); break;
        case 1: SetPropertyValue<std::string>("lod1MeshPath", path); break;
        case 2: SetPropertyValue<std::string>("lod2MeshPath", path); break;
        case 3: SetPropertyValue<std::string>("lod3MeshPath", path); break;
        default: break;
    }
    m_InstanceBuildDirty = true;
}

float MultiMeshGeneric::GetLodDistance(int level) const {
    switch (level) {
        case 1: return GetPropertyValue<float>("lod1Distance");
        case 2: return GetPropertyValue<float>("lod2Distance");
        case 3: return GetPropertyValue<float>("lod3Distance");
        default: return 0.0f;
    }
}

void MultiMeshGeneric::SetLodDistance(int level, float distance) {
    switch (level) {
        case 1: SetPropertyValue<float>("lod1Distance", distance); break;
        case 2: SetPropertyValue<float>("lod2Distance", distance); break;
        case 3: SetPropertyValue<float>("lod3Distance", distance); break;
        default: break;
    }
    m_InstanceBuildDirty = true;
}

bool MultiMeshGeneric::GetAutoGenerateLods() const {
    return GetPropertyValue<bool>("autoGenerateLods");
}

void MultiMeshGeneric::SetAutoGenerateLods(bool enable) {
    SetPropertyValue<bool>("autoGenerateLods", enable);
    m_InstanceBuildDirty = true;
}

float MultiMeshGeneric::GetAutoLodReduction() const {
    return GetPropertyValue<float>("autoLodReduction");
}

void MultiMeshGeneric::SetAutoLodReduction(float reduction) {
    SetPropertyValue<float>("autoLodReduction", reduction);
    m_InstanceBuildDirty = true;
}

Vec3 MultiMeshGeneric::GetBaseRotation() const {
    return GetPropertyValue<Vec3>("baseRotation");
}

void MultiMeshGeneric::SetBaseRotation(const Vec3& eulerDegrees) {
    SetPropertyValue<Vec3>("baseRotation", eulerDegrees);
}

Vec3 MultiMeshGeneric::GetBaseScale() const {
    return GetPropertyValue<Vec3>("baseScale");
}

void MultiMeshGeneric::SetBaseScale(const Vec3& scale) {
    SetPropertyValue<Vec3>("baseScale", scale);
}

bool MultiMeshGeneric::GetEditableInEditor() const {
    return GetPropertyValue<bool>("editableInEditor");
}

void MultiMeshGeneric::SetEditableInEditor(bool editable) {
    SetPropertyValue<bool>("editableInEditor", editable);
}

bool MultiMeshGeneric::GetPreviewSingleInstance() const {
    return GetPropertyValue<bool>("previewSingleInstance");
}

void MultiMeshGeneric::SetPreviewSingleInstance(bool preview) {
    SetPropertyValue<bool>("previewSingleInstance", preview);
}

bool MultiMeshGeneric::GetPreviewInEditor() const {
    return GetPropertyValue<bool>("previewInEditor");
}

void MultiMeshGeneric::SetPreviewInEditor(bool preview) {
    SetPropertyValue<bool>("previewInEditor", preview);
}

bool MultiMeshGeneric::LoadTexture(const asset::AssetRef<asset::ModelAsset>& meshAsset,
                                   const std::string& filepath, asset::AssetRef<asset::ImageAsset>& outAsset) {
    if (filepath.empty()) {
        return false;
    }

    // Embedded textures start with '*' and are resolved against the model.
    if (filepath[0] == '*' && meshAsset.IsValid()) {
        return LoadEmbeddedTexture(meshAsset, filepath, outAsset);
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

bool MultiMeshGeneric::LoadEmbeddedTexture(const asset::AssetRef<asset::ModelAsset>& meshAsset,
                                           const std::string& textureName, asset::AssetRef<asset::ImageAsset>& outAsset) {
    if (!meshAsset.IsValid()) {
        return false;
    }

    const asset::EmbeddedTexture* embeddedTex = meshAsset->GetEmbeddedTexture(textureName);
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

std::string MultiMeshGeneric::ResolveTexturePath(const std::string& texturePath, const std::string& modelDir) {
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

    for (const std::string& subdir : searchDirs) {
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

} // namespace components
} // namespace lupine
