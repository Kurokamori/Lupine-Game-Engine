#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/asset/ModelAsset.hpp"
#include <vector>
#include <string>

namespace lupine {
namespace components {

using namespace math;

/**
 * Shadow casting mode for MultiMeshGeneric
 */
enum class ShadowCastingMode {
    Off = 0,
    On = 1,
    OnlyShadows = 2
};

/**
 * Instance data for a single mesh instance
 */
struct MeshInstance {
    Mat4 transform;           // Instance transform (position, rotation, scale)
    Color color;              // Instance color tint
    Vec4 customData;          // Custom per-instance data (user-defined)
    
    MeshInstance()
        : transform(Mat4::Identity())
        , color(Color::White())
        , customData(Vec4(0.0f, 0.0f, 0.0f, 0.0f))
    {}
};

/**
 * MultiMeshGeneric Component
 * 
 * GPU-instanced rendering of many copies of the same mesh with different transforms/colors/etc.
 * Used for grass, trees, debris, crowds, and other scenarios requiring many instances of the same mesh.
 * 
 * Features:
 * - Mesh loading from file (GLTF, GLB, FBX, OBJ)
 * - Material override support
 * - Per-instance transforms, colors, and custom data
 * - Shadow casting control (Off, On, Only Shadows)
 * - Per-instance culling support
 * - LOD group support (future)
 * - Editor preview modes
 */
class MultiMeshGeneric : public core::Component, public IRenderableComponent {
public:
    MultiMeshGeneric();
    explicit MultiMeshGeneric(const std::string& name);
    virtual ~MultiMeshGeneric();

    // ISerializable interface
    std::string GetTypeName() const override { return "MultiMeshGeneric"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;

    // IRenderableComponent interface
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World3D; }

    // Mesh management
    bool LoadMesh(const std::string& filepath);
    std::string GetMeshPath() const;
    void SetMeshPath(const std::string& path);

    // Material override
    std::string GetMaterialOverride() const;
    void SetMaterialOverride(const std::string& path);

    // Shadow settings
    ShadowCastingMode GetCastShadow() const;
    void SetCastShadow(ShadowCastingMode mode);
    bool GetReceiveShadow() const;
    void SetReceiveShadow(bool receive);

    // Instance management
    int GetInstanceCount() const;
    void SetInstanceCount(int count);
    
    void SetInstanceTransform(int index, const Mat4& transform);
    Mat4 GetInstanceTransform(int index) const;
    
    void SetInstanceColor(int index, const Color& color);
    Color GetInstanceColor(int index) const;
    
    void SetInstanceCustomData(int index, const Vec4& data);
    Vec4 GetInstanceCustomData(int index) const;

    // Culling settings
    bool GetCullPerInstance() const;
    void SetCullPerInstance(bool cull);
    float GetMaxDistance() const;
    void SetMaxDistance(float distance);

    // LOD settings
    std::string GetLodGroup() const;
    void SetLodGroup(const std::string& path);

    // Editor settings
    bool GetEditableInEditor() const;
    void SetEditableInEditor(bool editable);
    bool GetPreviewSingleInstance() const;
    void SetPreviewSingleInstance(bool preview);
    bool GetPreviewInEditor() const;
    void SetPreviewInEditor(bool preview);

    // Direct instance access
    std::vector<MeshInstance>& GetInstances() { return m_Instances; }
    const std::vector<MeshInstance>& GetInstances() const { return m_Instances; }

private:
    // Mesh asset
    asset::AssetRef<asset::ModelAsset> m_MeshAsset;
    std::string m_CurrentMeshPath;

    // GPU mesh handle
    MeshHandle m_MeshHandle;
    bool m_MeshNeedsUpload;

    // Instance data
    std::vector<MeshInstance> m_Instances;

    // Material override texture handles
    TextureHandle m_AlbedoTextureHandle;
    bool m_MaterialTexturesNeedUpload;

    /**
     * Upload mesh to GPU if needed
     */
    void UploadMeshToGPU(RenderContext& ctx);

    /**
     * Calculate combined bounding box from all instances
     */
    AABB CalculateCombinedBounds() const;

    /**
     * Check if instance is within culling distance
     */
    bool IsInstanceVisible(int index, const Vec3& cameraPos) const;
};

} // namespace components
} // namespace lupine

