#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace components {

/**
 * Billboard mode for Sprite3D
 */
enum class BillboardMode {
    Disabled,       // No billboarding, sprite faces its local forward direction
    Enabled,        // Full billboarding, always faces camera
    YAxisOnly       // Billboard only around Y axis (useful for trees, characters, etc.)
};

/**
 * Sprite3D Component
 *
 * Renders a 2D sprite in 3D space with texture support, modulation, sprite sheet slicing,
 * rotation, scale, alpha settings, and alpha cutting.
 *
 * Features:
 * - All Sprite2D features (texture, modulation, UV rect, alpha, flip, etc.)
 * - Rendered in 3D space
 * - Billboard mode support (face camera, Y-axis only, or disabled)
 * - Proper depth testing and sorting
 * - Can be used for particles, decals, foliage, etc.
 */
class Sprite3D : public core::Component, public IRenderableComponent {
public:
    Sprite3D();
    explicit Sprite3D(const std::string& name);
    virtual ~Sprite3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Sprite3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // ===== Texture Management =====
    
    /**
     * Load texture from file path
     */
    bool LoadTexture(const std::string& filepath);
    
    /**
     * Set texture from asset
     */
    void SetTexture(const asset::AssetRef<asset::ImageAsset>& texture);
    
    /**
     * Get current texture asset
     */
    const asset::AssetRef<asset::ImageAsset>& GetTextureAsset() const { return m_TextureAsset; }
    
    /**
     * Get GPU texture handle
     */
    TextureHandle GetTextureHandle() const { return m_TextureHandle; }

    // ===== Property Accessors =====
    
    // Texture path
    const std::string& GetTexturePath() const;
    void SetTexturePath(const std::string& path);
    
    // Modulation color (tint)
    const math::Color& GetModulate() const;
    void SetModulate(const math::Color& color);
    
    // Size in world units
    const math::Vec2& GetSize() const;
    void SetSize(const math::Vec2& size);
    
    // UV rectangle for sprite sheet slicing (normalized 0-1)
    const math::Vec4& GetUVRect() const;
    void SetUVRect(const math::Vec4& uvRect);
    
    // Alpha cutoff threshold (0-1)
    float GetAlphaCutoff() const;
    void SetAlphaCutoff(float cutoff);
    
    // Flip flags
    bool GetFlipH() const;
    void SetFlipH(bool flip);
    
    bool GetFlipV() const;
    void SetFlipV(bool flip);
    
    // Billboard mode
    BillboardMode GetBillboardMode() const;
    void SetBillboardMode(BillboardMode mode);
    
    // Pixel size (scale sprite to maintain pixel-perfect size at distance)
    float GetPixelSize() const;
    void SetPixelSize(float size);
    
    // Double-sided rendering
    bool GetDoubleSided() const;
    void SetDoubleSided(bool doubleSided);

    // Shadow casting
    bool GetCastShadow() const;
    void SetCastShadow(bool castShadow);

    // Shadow receiving
    bool GetReceiveShadow() const;
    void SetReceiveShadow(bool receiveShadow);

    // ===== Sprite Sheet Methods =====

    // Sprite sheet enabled flag
    bool GetSpriteSheetEnabled() const;
    void SetSpriteSheetEnabled(bool enabled);

    // Sprite size in pixels (for sprite sheet slicing)
    const math::Vec2& GetSpriteSize() const;
    void SetSpriteSize(const math::Vec2& size);

    // Current frame index
    int GetCurrentFrame() const;
    void SetCurrentFrame(int frame);

    // Get total number of frames in the sprite sheet
    int GetFrameCount() const;

    // Get frames per row/column
    int GetFramesPerRow() const;
    int GetFramesPerColumn() const;

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World3D; }
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;
    math::OBB getOrientedBounds() const override;

private:
    // Texture asset and GPU handle
    asset::AssetRef<asset::ImageAsset> m_TextureAsset;
    TextureHandle m_TextureHandle;

    // Flag to track if texture needs to be uploaded to GPU
    bool m_TextureNeedsUpload;

    // Track current texture path to detect changes
    std::string m_CurrentTexturePath;
    
    /**
     * Upload texture to GPU if needed
     */
    void UploadTextureToGPU();
    
    /**
     * Calculate actual render size based on texture and size property
     */
    math::Vec2 CalculateRenderSize() const;
    
    /**
     * Calculate UV coordinates based on flip flags and UV rect
     */
    void CalculateUVCoordinates(math::Vec2& uvMin, math::Vec2& uvMax) const;
    
    /**
     * Calculate billboard transform matrix
     */
    math::Mat4 CalculateBillboardTransform(const math::Mat4& viewMatrix) const;

    /**
     * Calculate UV rect for a specific frame in sprite sheet mode
     */
    math::Vec4 CalculateFrameUVRect(int frameIndex) const;
};

} // namespace components
} // namespace lupine

