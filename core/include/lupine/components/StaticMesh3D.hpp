#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/PBRMaterial.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/AABB.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/ModelAsset.hpp"
#include <optional>
#include <vector>
#include <string>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

/**
 * Material slot for StaticMesh3D
 * Allows per-material overrides for each material in the model
 */
struct MaterialSlot {
    std::string name;                           // Material name from model
    uint32_t materialIndex;                     // Index in model's material array
    bool enableOverride = false;                // Whether to use override or model's material
    
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
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World3D; }
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;
    math::OBB getOrientedBounds() const override;

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
     * Upload meshes to GPU if needed
     */
    void UploadMeshesToGPU(RenderContext& ctx);

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

