#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/PBRMaterial.hpp"
#include "lupine/rendering/Material.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/AABB.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include <optional>
#include <unordered_map>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

/**
 * Primitive shape types for PrimitiveMesh3D
 */
enum class PrimitiveShape {
    Cube = 0,
    Sphere,
    Cylinder,
    Cone,
    Pyramid,
    Torus,
    Capsule
};

/**
 * PrimitiveMesh3D Component
 *
 * Renders a primitive 3D mesh shape with configurable geometry.
 * Supports vertex color tinting and material overrides.
 *
 * Features:
 * - Multiple primitive shapes (cube, sphere, cylinder, cone, pyramid, torus, capsule)
 * - Vertex color support
 * - Automatic mesh generation based on shape parameters
 * - Transform inherited from Node3D owner (position, rotation, scale)
 * - Material override support (placeholder for future implementation)
 */
class PrimitiveMesh3D : public core::Component, public IRenderableComponent {
public:
    PrimitiveMesh3D();
    explicit PrimitiveMesh3D(const std::string& name);
    virtual ~PrimitiveMesh3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "PrimitiveMesh3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // ===== Shape Configuration =====

    /**
     * Get/Set the primitive shape type
     */
    PrimitiveShape GetShape() const;
    void SetShape(PrimitiveShape shape);

    /**
     * Get/Set vertex color (tint)
     */
    Color GetColor() const;
    void SetColor(const Color& color);

    /**
     * Get/Set shape-specific size parameter
     * For cube/pyramid: base size
     * For sphere/cone/cylinder/capsule: radius
     * For torus: major radius
     */
    float GetSize() const;
    void SetSize(float size);

    /**
     * Get/Set height (for cylinder, cone, pyramid, capsule)
     */
    float GetHeight() const;
    void SetHeight(float height);

    /**
     * Get/Set detail level (segments/rings for curved shapes)
     */
    int GetDetail() const;
    void SetDetail(int detail);

    /**
     * Get/Set minor radius (for torus)
     */
    float GetMinorRadius() const;
    void SetMinorRadius(float radius);

    /**
     * Get/Set whether this mesh casts shadows
     */
    bool GetCastShadow() const;
    void SetCastShadow(bool castShadow);

    /**
     * Get/Set whether this mesh receives shadows
     */
    bool GetReceiveShadow() const;
    void SetReceiveShadow(bool receiveShadow);

    /**
     * Get/Set double-sided rendering (disables backface culling)
     */
    bool GetDoubleSided() const;
    void SetDoubleSided(bool doubleSided);

    // ===== Material Override =====

    /**
     * Check if material override is enabled
     */
    bool HasMaterialOverride() const;

    /**
     * Enable/disable material override
     */
    void SetMaterialOverrideEnabled(bool enabled);

    /**
     * Get material override properties (read-only)
     */
    const PBRMaterialProperties& GetMaterialProperties() const;

    /**
     * Set albedo color
     */
    void SetAlbedoColor(const Color& color);
    Color GetAlbedoColor() const;

    /**
     * Set albedo texture
     */
    void SetAlbedoTexture(TextureHandle texture);
    TextureHandle GetAlbedoTexture() const;

    /**
     * Set metallic value (0.0 = dielectric, 1.0 = metal)
     */
    void SetMetallic(float metallic);
    float GetMetallic() const;

    /**
     * Set roughness value (0.0 = smooth, 1.0 = rough)
     */
    void SetRoughness(float roughness);
    float GetRoughness() const;

    /**
     * Set metallic-roughness texture (glTF convention: G=roughness, B=metallic)
     */
    void SetMetallicRoughnessTexture(TextureHandle texture);
    TextureHandle GetMetallicRoughnessTexture() const;

    /**
     * Set normal map texture
     */
    void SetNormalTexture(TextureHandle texture);
    TextureHandle GetNormalTexture() const;

    /**
     * Set normal map scale
     */
    void SetNormalScale(float scale);
    float GetNormalScale() const;

    /**
     * Set emissive color
     */
    void SetEmissiveColor(const Color& color);
    Color GetEmissiveColor() const;

    /**
     * Set emissive texture
     */
    void SetEmissiveTexture(TextureHandle texture);
    TextureHandle GetEmissiveTexture() const;

    /**
     * Set emissive strength multiplier
     */
    void SetEmissiveStrength(float strength);
    float GetEmissiveStrength() const;

    // ===== Shader Selection =====

    /**
     * Get/Set shader type
     */
    ShaderType GetShaderType() const;
    void SetShaderType(ShaderType type);

    /**
     * Custom shader paths (used when ShaderType::Custom is selected)
     */
    std::string GetCustomVertShaderPath() const;
    void SetCustomVertShaderPath(const std::string& path);
    std::string GetCustomFragShaderPath() const;
    void SetCustomFragShaderPath(const std::string& path);
    // Custom .lsh shader (translated at runtime, honors #render_mode; takes precedence).
    std::string GetCustomLshShaderPath() const;
    void SetCustomLshShaderPath(const std::string& path);

    /**
     * Toon shader parameters
     */
    float GetShadowBands() const;
    void SetShadowBands(float bands);
    float GetShadowThreshold() const;
    void SetShadowThreshold(float threshold);
    float GetShadowSoftness() const;
    void SetShadowSoftness(float softness);
    float GetSpecularBands() const;
    void SetSpecularBands(float bands);
    float GetSpecularPower() const;
    void SetSpecularPower(float power);
    float GetRimIntensity() const;
    void SetRimIntensity(float intensity);
    float GetRimPower() const;
    void SetRimPower(float power);

    // ===== Generic Shader Parameters =====

    /**
     * Get shader parameters map (for iteration)
     */
    const std::unordered_map<std::string, MaterialPropertyValue>& GetShaderParams() const { return m_ShaderParams; }

    /**
     * Set a shader parameter by uniform name
     */
    void SetShaderParam(const std::string& uniformName, const MaterialPropertyValue& value) { m_ShaderParams[uniformName] = value; }

    /**
     * Clear all shader parameters
     */
    void ClearShaderParams() { m_ShaderParams.clear(); }

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    // Procedural mesh generated from (epoch-tracked) properties; static once generated.
    // Reports dynamic until the mesh handle is valid so the first gather is never cached
    // as empty.
    bool isRenderContentDynamic() const override { return !m_MeshHandle.isValid(); }
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World3D; }
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;
    math::OBB getOrientedBounds() const override;

private:
    // Cached mesh handle (regenerated when shape parameters change)
    MeshHandle m_MeshHandle;
    bool m_MeshNeedsRegeneration;

    // Material override
    std::optional<PBRMaterialProperties> m_MaterialOverride;

    // Material texture assets and handles
    asset::AssetRef<asset::ImageAsset> m_AlbedoTextureAsset;
    asset::AssetRef<asset::ImageAsset> m_MetallicRoughnessTextureAsset;
    asset::AssetRef<asset::ImageAsset> m_NormalTextureAsset;
    asset::AssetRef<asset::ImageAsset> m_EmissiveTextureAsset;

    TextureHandle m_AlbedoTextureHandle;
    TextureHandle m_MetallicRoughnessTextureHandle;
    TextureHandle m_NormalTextureHandle;
    TextureHandle m_EmissiveTextureHandle;

    bool m_TexturesNeedUpload;

    // Generic shader parameters - uniform name to value mapping
    std::unordered_map<std::string, MaterialPropertyValue> m_ShaderParams;

    // Cached shape parameters to detect changes
    mutable PrimitiveShape m_CachedShape;
    mutable float m_CachedSize;
    mutable float m_CachedHeight;
    mutable int m_CachedDetail;
    mutable float m_CachedMinorRadius;

    /**
     * Check if shape parameters have changed since last mesh generation
     */
    bool HasShapeParametersChanged() const;

    /**
     * Update cached shape parameters to current values
     */
    void UpdateCachedParameters() const;

    /**
     * Regenerate the mesh based on current shape parameters
     */
    void RegenerateMesh(RenderContext& ctx);

    /**
     * Get the appropriate mesh data for the current shape
     */
    MeshData GenerateShapeMesh() const;

    /**
     * Load a texture from file path
     */
    bool LoadTexture(const std::string& filepath, asset::AssetRef<asset::ImageAsset>& outAsset);

    /**
     * Upload material textures to GPU if needed
     */
    void UploadMaterialTextures(RenderContext& ctx);
};

} // namespace components
} // namespace lupine

