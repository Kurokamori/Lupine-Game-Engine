#include "lupine/components/MultiMeshGeneric.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
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

MultiMeshGeneric::MultiMeshGeneric()
    : Component("MultiMeshGeneric")
    , m_MeshNeedsUpload(false)
    , m_MaterialTexturesNeedUpload(false)
{
}

MultiMeshGeneric::MultiMeshGeneric(const std::string& name)
    : Component(name)
    , m_MeshNeedsUpload(false)
    , m_MaterialTexturesNeedUpload(false)
{
}

MultiMeshGeneric::~MultiMeshGeneric() {
}

void MultiMeshGeneric::DefineProperties() {
    // Mesh group
    DefineProperty(PROPERTY_FILE_GROUP(meshPath, std::string(""), "*.gltf,*.glb,*.fbx,*.obj", "Mesh"));
    DefineProperty(PROPERTY_FILE_GROUP(materialOverride, std::string(""), "*.mat,*.material", "Mesh"));

    // Shadow settings group
    DefineProperty(PROPERTY_ENUM_GROUP(castShadow, 1, "Shadows", Off, On, OnlyShadows));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Shadows"));

    // Instance settings group
    DefineProperty(PROPERTY_INT_RANGE_GROUP(instanceCount, 0, 0, 100000, 1, "Instances"));

    // Culling settings group
    DefineProperty(PROPERTY_DEFAULT_GROUP(cullPerInstance, Bool, true, "Culling"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxDistance, 1000.0f, 0.0f, 10000.0f, 10.0f, "Culling"));

    // LOD settings group
    DefineProperty(PROPERTY_FILE_GROUP(lodGroup, std::string(""), "*.lod", "LOD"));

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

    // Initialize instances based on instance count
    int count = GetInstanceCount();
    m_Instances.resize(count);
}

void MultiMeshGeneric::OnReady() {
}

bool MultiMeshGeneric::LoadMesh(const std::string& filepath) {
    if (filepath.empty()) {
        return false;
    }

    m_MeshAsset = asset::AssetRef<asset::ModelAsset>(new asset::ModelAsset());

    bool loaded = m_MeshAsset->LoadFromFile(filepath, true);

    if (!loaded) {
        m_MeshAsset.Reset();
        return false;
    }

    m_MeshNeedsUpload = true;
    SetMeshPath(filepath);

    return true;
}

void MultiMeshGeneric::UploadMeshToGPU(RenderContext& ctx) {
    if (!m_MeshNeedsUpload || !m_MeshAsset.IsValid()) {
        return;
    }

    // Destroy old mesh if it exists
    if (m_MeshHandle.isValid() && ctx.getDevice()) {
        ctx.getDevice()->destroyMesh(m_MeshHandle);
        m_MeshHandle = MeshHandle();
    }

    const auto& meshes = m_MeshAsset->GetMeshes();
    if (meshes.empty()) {
        m_MeshNeedsUpload = false;
        return;
    }

    // Use the first mesh from the model
    const auto& assetMesh = meshes[0];

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
        m_MeshHandle = ctx.getDevice()->createMesh(meshData);
    }

    m_MeshNeedsUpload = false;
}

void MultiMeshGeneric::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return;
    }

    // Check if mesh path changed
    std::string currentPath = GetMeshPath();
    if (currentPath != m_CurrentMeshPath) {
        if (m_MeshHandle.isValid()) {
            IGfxDevice* device = ctx.getDevice();
            if (device) {
                device->destroyMesh(m_MeshHandle);
                m_MeshHandle = MeshHandle();
            }
        }

        m_MeshAsset.Reset();

        if (!currentPath.empty()) {
            m_MeshAsset = asset::AssetRef<asset::ModelAsset>(new asset::ModelAsset());
            bool loaded = m_MeshAsset->LoadFromFile(currentPath, true);

            if (!loaded) {
                m_MeshAsset.Reset();
            } else {
                m_MeshNeedsUpload = true;
            }
        }

        m_CurrentMeshPath = currentPath;
    }

    if (!m_MeshAsset.IsValid()) {
        return;
    }

    // Upload mesh to GPU if needed
    UploadMeshToGPU(ctx);

    if (!m_MeshHandle.isValid()) {
        return;
    }

    // Get node's global transform
    Mat4 nodeTransform = node3D->GetGlobalTransformMatrix();

    // Get material
    MaterialHandle material = ctx.getDefaultPBRMaterial();
    if (!material.isValid()) {
        return;
    }

    // Get shadow settings
    ShadowCastingMode castShadowMode = GetCastShadow();
    bool receiveShadow = GetReceiveShadow();
    bool castShadow = (castShadowMode == ShadowCastingMode::On || castShadowMode == ShadowCastingMode::OnlyShadows);

    // Get culling settings
    bool cullPerInstance = GetCullPerInstance();
    float maxDistance = GetMaxDistance();
    Vec3 cameraPos = ctx.getViewMatrix().Inverse().TransformPoint(Vec3(0.0f, 0.0f, 0.0f));

    // Get editor preview settings
    bool previewInEditor = GetPreviewInEditor();
    bool previewSingleInstance = GetPreviewSingleInstance();

    // Determine how many instances to render
    int instanceCount = GetInstanceCount();
    int renderCount = instanceCount;

    // In editor preview mode, optionally render only one instance
    if (previewSingleInstance && renderCount > 0) {
        renderCount = 1;
    }

    // Render each instance
    // NOTE: This is a simple implementation that submits one draw call per instance.
    // For true GPU instancing, we would need to extend RenderContext with a drawMeshInstanced() method
    // that accepts instance buffers. This is a TODO for future optimization.
    for (int i = 0; i < renderCount; ++i) {
        if (i >= static_cast<int>(m_Instances.size())) {
            break;
        }

        const MeshInstance& instance = m_Instances[i];

        // Per-instance culling
        if (cullPerInstance) {
            Vec3 instancePos = instance.transform.TransformPoint(Vec3(0.0f, 0.0f, 0.0f));
            Vec3 worldPos = nodeTransform.TransformPoint(instancePos);
            float distance = (worldPos - cameraPos).Length();

            if (distance > maxDistance) {
                continue; // Skip this instance
            }
        }

        // Combine node transform with instance transform
        Mat4 finalTransform = nodeTransform * instance.transform;

        // Set up material property overrides
        MaterialPropertyBlock overrides;
        overrides.setColor("u_AlbedoColor", instance.color);
        overrides.setVec4("u_MaterialParams1", Vec4(0.0f, 0.5f, 1.0f, 1.0f)); // Default metallic/roughness
        overrides.setColor("u_EmissiveColor", Color::Black());
        overrides.setFloat("u_AlphaCutoff", 0.5f);

        // Use custom data for additional material parameters if needed
        // overrides.setVec4("u_CustomData", instance.customData);

        // Submit draw command
        ctx.drawMesh(m_MeshHandle, material, finalTransform, overrides, 0, castShadow, receiveShadow);
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
    if (m_Instances.empty() || !m_MeshAsset.IsValid()) {
        return AABB();
    }

    const auto& meshes = m_MeshAsset->GetMeshes();
    if (meshes.empty()) {
        return AABB();
    }

    // Get mesh bounds
    const auto& mesh = meshes[0];
    AABB meshBounds(mesh.boundsMin, mesh.boundsMax);

    // Calculate combined bounds from all instances
    Vec3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    Mat4 nodeTransform = node3D ? node3D->GetGlobalTransformMatrix() : Mat4::Identity();

    for (const auto& instance : m_Instances) {
        Mat4 finalTransform = nodeTransform * instance.transform;

        // Transform mesh bounds corners
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

        for (const auto& corner : corners) {
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

RenderLayer MultiMeshGeneric::getRenderLayer() const {
    // MultiMesh instances are typically opaque
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
}

std::string MultiMeshGeneric::GetMaterialOverride() const {
    return GetPropertyValue<std::string>("materialOverride");
}

void MultiMeshGeneric::SetMaterialOverride(const std::string& path) {
    SetPropertyValue<std::string>("materialOverride", path);
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
}

void MultiMeshGeneric::SetInstanceTransform(int index, const Mat4& transform) {
    if (index >= 0 && index < static_cast<int>(m_Instances.size())) {
        m_Instances[index].transform = transform;
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
}

float MultiMeshGeneric::GetMaxDistance() const {
    return GetPropertyValue<float>("maxDistance");
}

void MultiMeshGeneric::SetMaxDistance(float distance) {
    SetPropertyValue<float>("maxDistance", distance);
}

std::string MultiMeshGeneric::GetLodGroup() const {
    return GetPropertyValue<std::string>("lodGroup");
}

void MultiMeshGeneric::SetLodGroup(const std::string& path) {
    SetPropertyValue<std::string>("lodGroup", path);
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

} // namespace components
} // namespace lupine

