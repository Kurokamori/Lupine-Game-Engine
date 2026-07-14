#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/PBRMaterial.hpp"
#include "lupine/rendering/Material.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/AABB.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/ModelAsset.hpp"
#include <optional>
#include <vector>
#include <string>
#include <unordered_map>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

// Use ShaderType from PBRMaterial.hpp
using lupine::ShaderType;

/**
 * Material slot for SkeletalMesh3D
 * Allows per-material overrides for each material in the model
 */
struct SkeletalMaterialSlot {
    std::string name;                           // Material name from model
    uint32_t materialIndex;                     // Index in model's material array
    bool enableOverride = false;                // Whether to use override or model's material

    // Shader selection
    ShaderType shaderType = ShaderType::PBR;
    std::string customVertShaderPath;
    std::string customFragShaderPath;
    std::string customLshShaderPath;            // Custom .lsh shader (skeletal layout)

    // Toon shader specific parameters
    float shadowBands = 3.0f;
    float shadowThreshold = 0.5f;
    float shadowSoftness = 0.02f;
    float specularBands = 2.0f;
    float specularPower = 32.0f;
    float rimIntensity = 0.0f;
    float rimPower = 3.0f;

    // Stylized shader specific parameters (legacy - kept for backward compatibility)
    float stylizedShadowSoftness = 0.3f;        // How soft the shadow transitions are
    float stylizedSpecularSoftness = 0.15f;     // How soft the specular edge is
    float stylizedShadowBrightness = 0.4f;      // How bright shadows are (0-1)
    float stylizedShadowWarmth = 0.5f;          // Warm/cool color shift in shadows (0-1)
    float stylizedSpecularIntensity = 0.5f;     // Specular highlight intensity
    float stylizedHalfLambertPower = 1.5f;      // Controls wrap-around lighting softness

    // Generic shader parameters - uniform name to value mapping
    // This allows any shader to define custom parameters without C++ code changes
    // Keys are uniform names (e.g., "u_GlowParams", "u_StylizedParams")
    // Values can be float, Vec2, Vec3, Vec4, Color, int, bool
    std::unordered_map<std::string, MaterialPropertyValue> shaderParams;

    // PBR Material override properties
    Color albedoColor = Color::White();
    std::string albedoTexturePath;
    asset::AssetRef<asset::ImageAsset> albedoTextureAsset;
    TextureHandle albedoTextureHandle;

    float metallic = 0.0f;
    float roughness = 0.5f;
    std::string metallicRoughnessTexturePath;
    asset::AssetRef<asset::ImageAsset> metallicRoughnessTextureAsset;
    TextureHandle metallicRoughnessTextureHandle;

    std::string normalTexturePath;
    asset::AssetRef<asset::ImageAsset> normalTextureAsset;
    TextureHandle normalTextureHandle;
    float normalScale = 1.0f;

    Color emissiveColor = Color::Black();
    std::string emissiveTexturePath;
    asset::AssetRef<asset::ImageAsset> emissiveTextureAsset;
    TextureHandle emissiveTextureHandle;
    float emissiveStrength = 1.0f;

    float alphaCutoff = 0.5f;

    bool texturesNeedUpload = false;
};

/**
 * SkeletalMesh3D Component
 *
 * Renders animated 3D models with skeletal animation support.
 * Supports FBX and GLTF models with skeletons and animations.
 *
 * Features:
 * - Load rigged models from FBX, GLTF formats via Assimp
 * - Skeletal animation playback with blending
 * - Support for multiple animations per model
 * - GPU skinning for performance
 * - Root motion support
 * - Multiple meshes and materials per model
 * - Per-material slot overrides
 * - LOD support
 * - Shadow casting and receiving
 * - Editor skeleton visualization
 */
class SkeletalMesh3D : public core::Component, public IRenderableComponent {
public:
    SkeletalMesh3D();
    explicit SkeletalMesh3D(const std::string& name);
    virtual ~SkeletalMesh3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "SkeletalMesh3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Model Loading =====

    /**
     * Load model from file path
     */
    bool LoadModel(const std::string& filepath);

    /**
     * Get current model asset
     */
    const asset::AssetRef<asset::ModelAsset>& GetModelAsset() const { return m_ModelAsset; }

    // ===== Animation Control =====

    /**
     * Play animation by name
     */
    void PlayAnimation(const std::string& animationName, bool loop = true);

    /**
     * Stop current animation
     */
    void StopAnimation();

    /**
     * Get list of available animation names
     */
    std::vector<std::string> GetAnimationNames() const;

    /**
     * Check if animation is currently playing
     */
    bool IsPlaying() const { return m_IsPlaying; }

    /**
     * Get current animation time
     */
    float GetAnimationTime() const { return m_CurrentTime; }

    /**
     * Set animation time
     */
    void SetAnimationTime(float time);

    // ===== Property Accessors =====

    // Model path
    std::string GetModelPath() const;
    void SetModelPath(const std::string& path);

    // Animation properties
    std::string GetDefaultAnimation() const;
    void SetDefaultAnimation(const std::string& animName);

    std::string GetCurrentAnimation() const;
    void SetCurrentAnimation(const std::string& animName);

    float GetPlaybackSpeed() const;
    void SetPlaybackSpeed(float speed);

    bool GetLoop() const;
    void SetLoop(bool loop);

    bool GetAutoPlay() const;
    void SetAutoPlay(bool autoPlay);

    bool GetPlaying() const;
    void SetPlaying(bool playing);

    bool GetGPUSkinning() const;
    void SetGPUSkinning(bool gpuSkinning);

    // Root motion
    bool GetRootMotionEnabled() const;
    void SetRootMotionEnabled(bool enabled);

    std::string GetRootBoneName() const;
    void SetRootBoneName(const std::string& boneName);

    // Rendering properties
    bool GetCastShadow() const;
    void SetCastShadow(bool castShadow);

    bool GetReceiveShadow() const;
    void SetReceiveShadow(bool receiveShadow);

    bool GetShowSkeletonInEditor() const;
    void SetShowSkeletonInEditor(bool show);

    // ===== Material Slot Management =====

    /**
     * Get number of material slots
     */
    uint32_t GetMaterialSlotCount() const { return static_cast<uint32_t>(m_MaterialSlots.size()); }

    /**
     * Get material slot by index
     */
    SkeletalMaterialSlot* GetMaterialSlot(uint32_t index);
    const SkeletalMaterialSlot* GetMaterialSlot(uint32_t index) const;

    /**
     * Enable/disable material override for a slot
     */
    void SetMaterialSlotOverrideEnabled(uint32_t slotIndex, bool enabled);

    /**
     * Set albedo color for a material slot
     */
    void SetMaterialSlotAlbedoColor(uint32_t slotIndex, const Color& color);

    /**
     * Set albedo texture for a material slot
     */
    void SetMaterialSlotAlbedoTexture(uint32_t slotIndex, const std::string& texturePath);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World3D; }
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;
    math::OBB getOrientedBounds() const override;
    void prepareGPUResources(IGfxDevice* device) override;

private:
    // Model asset
    asset::AssetRef<asset::ModelAsset> m_ModelAsset;
    std::string m_CurrentModelPath;

    // GPU mesh handles (one per mesh in the model)
    std::vector<MeshHandle> m_MeshHandles;

    // Material slots (one per material in the model)
    std::vector<SkeletalMaterialSlot> m_MaterialSlots;

    // Animation state
    std::string m_DefaultAnimation;
    std::string m_CurrentAnimationName;
    int m_CurrentAnimationIndex;
    float m_CurrentTime;
    float m_PlaybackSpeed;
    bool m_Loop;
    bool m_AutoPlay;
    bool m_IsPlaying;

    // Skeletal animation data
    std::vector<math::Mat4> m_BoneTransforms;  // Final bone transforms for rendering
    std::vector<math::Mat4> m_BoneLocalTransforms;  // Local transforms for each bone
    bool m_GPUSkinning;

    // Root motion
    bool m_RootMotionEnabled;
    std::string m_RootBoneName;
    int m_RootBoneIndex;
    math::Vec3 m_LastRootPosition;
    math::Quat m_LastRootRotation;

    // Rendering properties
    bool m_CastShadow;
    bool m_ReceiveShadow;
    bool m_ShowSkeletonInEditor;

    // Flag to track if meshes need to be uploaded to GPU
    bool m_MeshesNeedUpload;

    /**
     * Upload meshes to GPU if needed (using RenderContext)
     */
    void UploadMeshesToGPU(RenderContext& ctx);

    /**
     * Upload meshes to GPU if needed (using device directly)
     * Used for pre-upload optimization to avoid first-frame stutter
     */
    void UploadMeshesToGPU(IGfxDevice* device);

    /**
     * Upload material textures to GPU for a specific slot
     */
    void UploadMaterialSlotTextures(RenderContext& ctx, SkeletalMaterialSlot& slot);

    /**
     * Load a texture from file path or embedded texture
     */
    bool LoadTexture(const std::string& filepath, asset::AssetRef<asset::ImageAsset>& outAsset);

    /**
     * Load an embedded texture from the model
     */
    bool LoadEmbeddedTexture(const std::string& textureName, asset::AssetRef<asset::ImageAsset>& outAsset);

    /**
     * Resolve texture path relative to model directory
     */
    std::string ResolveTexturePath(const std::string& texturePath, const std::string& modelDir);

    /**
     * Load and upload textures from model material for a specific slot
     */
    void LoadAndUploadMaterialTextures(RenderContext& ctx, SkeletalMaterialSlot& slot);

    /**
     * Create material slots from loaded model
     */
    void CreateMaterialSlotsFromModel();

    /**
     * Calculate combined bounding box from all meshes
     */
    AABB CalculateCombinedBounds() const;

    /**
     * Update animation and calculate bone transforms
     */
    void UpdateAnimation(float deltaTime);

    /**
     * Calculate bone transforms for current animation state
     */
    void CalculateBoneTransforms();

    /**
     * Calculate bind pose bone transforms (used when no animation is playing)
     */
    void CalculateBindPose();

    /**
     * Recursively calculate bind pose bone transform hierarchy
     */
    void CalculateBindPoseBone(int boneIndex, const math::Mat4& parentTransform);

    /**
     * Recursively calculate bone transform hierarchy
     */
    void CalculateBoneTransform(int boneIndex, const math::Mat4& parentTransform);

    /**
     * Interpolate between keyframes
     */
    math::Vec3 InterpolatePosition(const asset::AnimationChannel& channel, float time);
    math::Quat InterpolateRotation(const asset::AnimationChannel& channel, float time);
    math::Vec3 InterpolateScale(const asset::AnimationChannel& channel, float time);

    /**
     * Find keyframe index for given time
     */
    int FindPositionKeyframe(const asset::AnimationChannel& channel, float time);
    int FindRotationKeyframe(const asset::AnimationChannel& channel, float time);
    int FindScaleKeyframe(const asset::AnimationChannel& channel, float time);
};

} // namespace components
} // namespace lupine

