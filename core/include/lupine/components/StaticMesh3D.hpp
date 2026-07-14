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
 * Material slot for StaticMesh3D
 * Allows per-material overrides for each material in the model
 */
struct MaterialSlot {
    std::string name;                           // Material name from model
    uint32_t materialIndex;                     // Index in model's material array
    bool enableOverride = false;                // Whether to use override or model's material

    // Shader selection
    ShaderType shaderType = ShaderType::PBR;    // Which shader to use
    std::string customVertShaderPath;           // Custom vertex shader path
    std::string customFragShaderPath;           // Custom fragment shader path
    std::string customLshShaderPath;            // Custom .lsh shader (translated at runtime,
                                                // honors #render_mode; takes precedence)

    // Toon shader specific parameters
    float shadowBands = 3.0f;                   // Number of shadow bands for toon shading
    float shadowThreshold = 0.5f;               // Shadow cutoff threshold
    float shadowSoftness = 0.02f;               // Softness of shadow band edges
    float specularBands = 2.0f;                 // Number of specular bands
    float specularPower = 32.0f;                // Specular highlight sharpness
    float rimIntensity = 0.0f;                  // Rim lighting intensity
    float rimPower = 3.0f;                      // Rim lighting falloff

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
 * StaticMesh3D Component
 *
 * Renders static 3D models loaded from files (GLTF, FBX, OBJ).
 * Supports multiple meshes, materials, and textures per model.
 *
 * Features:
 * - Load models from GLTF, FBX, OBJ formats via Assimp
 * - Support for multiple meshes in a single model
 * - Support for multiple materials with PBR textures
 * - Per-material slot overrides
 * - Alpha cutting/clipping support
 * - Graceful handling of unsupported material features
 * - Proper PBR rendering with lighting
 */
class StaticMesh3D : public core::Component, public IRenderableComponent {
public:
    StaticMesh3D();
    explicit StaticMesh3D(const std::string& name);
    virtual ~StaticMesh3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "StaticMesh3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // ===== Model Loading =====
    
    /**
     * Load model from file path
     */
    bool LoadModel(const std::string& filepath);
    
    /**
     * Get current model asset
     */
    const asset::AssetRef<asset::ModelAsset>& GetModelAsset() const { return m_ModelAsset; }

    /**
     * Called when an asset file changes on disk.
     * Override to properly invalidate cached mesh and texture handles.
     */
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Property Accessors =====

    // Model path
    std::string GetModelPath() const;
    void SetModelPath(const std::string& path);
    
    // Cast shadows
    bool GetCastShadow() const;
    void SetCastShadow(bool castShadow);
    
    // Receive shadows
    bool GetReceiveShadow() const;
    void SetReceiveShadow(bool receiveShadow);
    
    // Double-sided rendering
    bool GetDoubleSided() const;
    void SetDoubleSided(bool doubleSided);
    
    // ===== Material Slot Management =====
    
    /**
     * Get number of material slots
     */
    uint32_t GetMaterialSlotCount() const { return static_cast<uint32_t>(m_MaterialSlots.size()); }
    
    /**
     * Get material slot by index
     */
    MaterialSlot* GetMaterialSlot(uint32_t index);
    const MaterialSlot* GetMaterialSlot(uint32_t index) const;
    
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
    // Static geometry: once the mesh is GPU-ready its content only changes through the
    // (epoch-tracked) mesh/material properties and the node transform, so its view is
    // safe to cache. While the mesh is still (async) loading - m_MeshHandles empty - it
    // reports dynamic so the renderer keeps re-gathering until the mesh appears.
    bool isRenderContentDynamic() const override { return m_MeshHandles.empty(); }
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
    std::vector<MaterialSlot> m_MaterialSlots;

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
    void UploadMaterialSlotTextures(RenderContext& ctx, MaterialSlot& slot);

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
    void LoadAndUploadMaterialTextures(RenderContext& ctx, MaterialSlot& slot);

    /**
     * Create material slots from loaded model
     */
    void CreateMaterialSlotsFromModel();

    /**
     * Calculate combined bounding box from all meshes
     */
    AABB CalculateCombinedBounds() const;
};

} // namespace components
} // namespace lupine

