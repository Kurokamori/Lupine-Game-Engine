#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/asset/ModelAsset.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include <vector>
#include <string>
#include <array>

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
    // Instances and base/LOD settings all advance the render epoch when edited (the
    // instance mutators call NotifyRenderStateChanged), so the view is safe to cache
    // once the base mesh is uploaded. While the base level has no GPU meshes yet it
    // reports dynamic so the first gather (which performs the upload) is never cached.
    bool isRenderContentDynamic() const override { return m_Lods[0].meshHandles.empty(); }
    SpatialType getSpatialType() const override { return SpatialType::World3D; }
    void prepareGPUResources(IGfxDevice* device) override;

    // Mesh management
    bool LoadMesh(const std::string& filepath);
    std::string GetMeshPath() const;
    void SetMeshPath(const std::string& path);

    // Material override
    std::string GetMaterialOverride() const;
    void SetMaterialOverride(const std::string& path);
    // Custom instanced .lsh shader (translated at runtime, honors #render_mode; takes
    // precedence over the default PBR instanced material).
    std::string GetCustomLshShaderPath() const;
    void SetCustomLshShaderPath(const std::string& path);

    // Shadow settings
    ShadowCastingMode GetCastShadow() const;
    void SetCastShadow(ShadowCastingMode mode);
    bool GetReceiveShadow() const;
    void SetReceiveShadow(bool receive);

    // Base transform applied to every instance before per-instance variation.
    // Base rotation is stored in degrees (Euler XYZ); base scale multiplies the
    // mesh uniformly per axis. These are useful for correcting an imported mesh's
    // orientation/size for all instances at once.
    Vec3 GetBaseRotation() const;
    void SetBaseRotation(const Vec3& eulerDegrees);
    Vec3 GetBaseScale() const;
    void SetBaseScale(const Vec3& scale);

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

    // Discrete LOD levels. LOD 0 is the base mesh (GetMeshPath()); LOD 1-3 are
    // optional lower-detail meshes selected by camera distance. Each level has an
    // upper distance threshold: an instance closer than lod1Distance draws LOD 0,
    // closer than lod2Distance draws LOD 1, and so on. Instances beyond
    // GetMaxDistance() are culled entirely. Empty LOD mesh paths fall back to the
    // nearest lower (more detailed) populated level.
    static constexpr int kLodLevelCount = 4;

    std::string GetLodMeshPath(int level) const;
    void SetLodMeshPath(int level, const std::string& path);
    float GetLodDistance(int level) const;
    void SetLodDistance(int level, float distance);

    // Automatic LOD generation: when enabled, any LOD level (1..3) that has no
    // author-supplied mesh path is filled by decimating the base mesh. Each
    // successive level keeps autoLodReduction^level of the base triangle count
    // (e.g. reduction 0.5 yields 50% / 25% / 12.5%). Author-supplied LOD meshes
    // always take precedence over generated ones.
    bool GetAutoGenerateLods() const;
    void SetAutoGenerateLods(bool enable);
    float GetAutoLodReduction() const;
    void SetAutoLodReduction(float reduction);

    // Editor settings
    bool GetEditableInEditor() const;
    void SetEditableInEditor(bool editable);
    bool GetPreviewSingleInstance() const;
    void SetPreviewSingleInstance(bool preview);
    bool GetPreviewInEditor() const;
    void SetPreviewInEditor(bool preview);

    // Direct instance access
    std::vector<MeshInstance>& GetInstances() { m_InstanceBuildDirty = true; NotifyRenderStateChanged(); return m_Instances; }
    const std::vector<MeshInstance>& GetInstances() const { return m_Instances; }

protected:
    // Per-mesh material data
    struct MeshMaterial {
        // Material properties
        Color albedoColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
        Color emissiveColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
        float metallic = 0.0f;
        float roughness = 0.5f;

        // Texture assets
        asset::AssetRef<asset::ImageAsset> albedoTextureAsset;
        asset::AssetRef<asset::ImageAsset> normalTextureAsset;
        asset::AssetRef<asset::ImageAsset> metallicRoughnessTextureAsset;
        asset::AssetRef<asset::ImageAsset> emissiveTextureAsset;

        // GPU texture handles
        TextureHandle albedoTextureHandle;
        TextureHandle normalTextureHandle;
        TextureHandle metallicRoughnessTextureHandle;
        TextureHandle emissiveTextureHandle;

        // Texture paths
        std::string albedoTexturePath;
        std::string normalTexturePath;
        std::string metallicRoughnessTexturePath;
        std::string emissiveTexturePath;
    };

    // One discrete level of detail: its own model asset, GPU meshes, per-submesh
    // materials, and a persistent per-instance GPU vertex buffer that is refilled
    // each frame with the instances bucketed to this level by camera distance.
    struct LODLevel {
        std::string currentPath;                        // path currently loaded
        asset::AssetRef<asset::ModelAsset> asset;       // model for this level
        std::vector<MeshHandle> meshHandles;            // one per submesh
        std::vector<MeshMaterial> materials;            // one per submesh
        bool needsUpload = false;                       // mesh awaits GPU upload
        BufferHandle instanceBuffer;                    // per-instance vertex buffer
        uint32_t instanceCapacity = 0;                  // capacity in instances
        std::vector<InstanceVertexData> instanceData;   // bucketed instances, rebuilt only when dirty
        AABB cachedWorldBounds;                         // world AABB of the bucketed instances, cached across frames

        // True when this level's meshes were produced by automatic decimation
        // rather than loaded from a path. Such levels own their decimated mesh
        // handles but only borrow the base level's texture handles, so the
        // textures must not be destroyed when releasing the level.
        bool autoGenerated = false;
        bool borrowsTextures = false;
    };

    // LOD 0 is the base mesh (meshPath); 1..3 are optional lower-detail meshes.
    std::array<LODLevel, kLodLevelCount> m_Lods;

    // Instance data (shared across all LOD levels; bucketed per frame)
    std::vector<MeshInstance> m_Instances;

    // Cached state used to skip rebuilding + re-uploading the per-instance GPU
    // buffers when nothing that affects them changed. The buffers are only refilled
    // when an instance/base-transform/count is edited (m_InstanceBuildDirty), the
    // owning node moves, or - for distance-dependent LOD/cull - the camera moves
    // beyond a small threshold. A static multimesh under a static camera uploads
    // exactly once.
    bool m_InstanceBuildDirty = true;
    bool m_HasBuiltInstances = false;
    Mat4 m_LastBuildNodeTransform = Mat4::Identity();
    Vec3 m_LastBuildBaseRotation = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 m_LastBuildBaseScale = Vec3(1.0f, 1.0f, 1.0f);
    int m_LastBuildRenderCount = -1;
    Vec3 m_LastBuildCameraPos = Vec3(0.0f, 0.0f, 0.0f);

    // Cached state used to decide when automatically generated LODs must be
    // rebuilt (base mesh changed, toggle flipped, or reduction ratio changed).
    std::string m_AutoLodSourcePath;
    float m_AutoLodReduction = -1.0f;
    bool m_AutoLodEnabled = false;
    bool m_AutoLodsBuilt = false;

    /**
     * Pull each level's mesh path from its property; when it changed, release the
     * old GPU resources and (re)load the model. LOD 0 uses meshPath, LOD 1..3 use
     * lodNMeshPath.
     */
    void SyncLevelPaths(IGfxDevice* device);

    /**
     * Upload a level's meshes to GPU if needed.
     */
    void UploadLevelMeshes(IGfxDevice* device, LODLevel& level);

    /**
     * Load and upload a level's material textures.
     */
    void LoadLevelMaterialTextures(IGfxDevice* device, LODLevel& level);

    /**
     * Destroy a level's GPU mesh/texture/instance-buffer resources.
     */
    void ReleaseLevelGPU(IGfxDevice* device, LODLevel& level);

    /**
     * Synchronize automatically generated LOD levels (1..3) with the current
     * autoGenerateLods / autoLodReduction settings and the base mesh. Levels with
     * an author-supplied mesh path are left untouched; empty levels are filled by
     * decimating the base mesh when enabled, or released when disabled.
     */
    void SyncAutoLods(IGfxDevice* device);

    /**
     * Decimate the base level's meshes to `ratio` of their triangle count and
     * upload the result into `level`, borrowing the base level's materials and
     * texture handles. Returns false if generation was not possible.
     */
    bool GenerateDecimatedLevel(IGfxDevice* device, LODLevel& level, float ratio);

    /**
     * Select the LOD index (0..kLodLevelCount-1) for a camera distance, falling
     * back to the nearest populated lower-detail level; returns -1 if the
     * instance is beyond the cull distance.
     */
    int SelectLodForDistance(float distance) const;

    /**
     * Ensure a level's instance buffer can hold at least `count` instances,
     * (re)creating it if necessary; returns false on failure.
     */
    bool EnsureInstanceBuffer(IGfxDevice* device, LODLevel& level, uint32_t count);

    /**
     * Load a texture from file or embedded data (embedded uses meshAsset).
     */
    bool LoadTexture(const asset::AssetRef<asset::ModelAsset>& meshAsset,
                     const std::string& filepath, asset::AssetRef<asset::ImageAsset>& outAsset);

    /**
     * Load embedded texture from a model.
     */
    bool LoadEmbeddedTexture(const asset::AssetRef<asset::ModelAsset>& meshAsset,
                             const std::string& textureName, asset::AssetRef<asset::ImageAsset>& outAsset);

    /**
     * Resolve texture path relative to model directory
     */
    std::string ResolveTexturePath(const std::string& texturePath, const std::string& modelDir);

    /**
     * Calculate combined bounding box from all instances (using the base mesh).
     */
    AABB CalculateCombinedBounds() const;

    /**
     * Combined world bounds of a set of instance transforms given mesh bounds.
     */
    AABB CombinedInstanceBounds(const AABB& meshBounds,
                                const std::vector<InstanceVertexData>& instances) const;

    /**
     * Check if instance is within culling distance
     */
    bool IsInstanceVisible(int index, const Vec3& cameraPos) const;
};

} // namespace components
} // namespace lupine

