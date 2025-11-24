#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace components {

/**
 * Aspect mode for Image2D
 * Determines how the image fits within its rect
 */
enum class AspectMode {
    Fit,        // Show entire image, may have letterboxing
    Letterbox,  // Same as Fit, explicit name
    Fill,       // Fill rect, may crop image
    Stretch     // Ignore aspect ratio, stretch to fill
};

/**
 * Blend mode for Image2D
 */
enum class BlendMode {
    Alpha,      // Standard alpha blending
    Additive,   // Additive blending
    Multiply,   // Multiplicative blending
    Opaque,     // No blending
    Overlay     // Overlay blending
};

/**
 * Mouse interaction behavior
 */
enum class MouseBehaviour {
    Ignore,         // Pass through, don't respond to mouse
    PropagateUp,    // Handle and propagate to parent
    Stop            // Handle and stop propagation
};

/**
 * Image2D Component
 *
 * A UI image component for displaying textures in UI space.
 * Similar to Sprite2D but designed specifically for UI layouts.
 *
 * Features:
 * - Texture loading and display
 * - Material override for custom shaders
 * - Color tint with alpha
 * - Anchor-based positioning (anchor_min/max, offset_min/max)
 * - Aspect ratio preservation with multiple fit modes
 * - Flip horizontal/vertical
 * - Multiple blend modes
 * - Mouse interaction control
 * - Rounded corners (with link toggle)
 * - Border rendering
 * - UI space toggle
 */
class Image2D : public core::Component, public IRenderableComponent {
public:
    Image2D();
    explicit Image2D(const std::string& name);
    virtual ~Image2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Image2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Property Accessors =====
    math::Vec2 CalculateSize() const;

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
    
    /**
     * Get/Set texture path
     */
    const std::string& GetTexturePath() const;
    void SetTexturePath(const std::string& path);

    // ===== Material Override =====
    
    /**
     * Get/Set material override path
     */
    const std::string& GetMaterialOverride() const;
    void SetMaterialOverride(const std::string& materialPath);

    // ===== Color/Tint =====
    
    /**
     * Get/Set color tint (includes alpha)
     */
    const math::Color& GetColor() const;
    void SetColor(const math::Color& color);

    // ===== Anchor Properties =====
    
    /**
     * Get/Set anchor min (normalized 0-1 within parent)
     */
    const math::Vec2& GetAnchorMin() const;
    void SetAnchorMin(const math::Vec2& anchor);
    
    /**
     * Get/Set anchor max (normalized 0-1 within parent)
     */
    const math::Vec2& GetAnchorMax() const;
    void SetAnchorMax(const math::Vec2& anchor);
    
    /**
     * Get/Set offset min (pixel offset from anchor min)
     */
    const math::Vec2& GetOffsetMin() const;
    void SetOffsetMin(const math::Vec2& offset);
    
    /**
     * Get/Set offset max (pixel offset from anchor max)
     */
    const math::Vec2& GetOffsetMax() const;
    void SetOffsetMax(const math::Vec2& offset);

    // ===== Aspect Ratio =====
    
    /**
     * Get/Set preserve aspect flag
     */
    bool GetPreserveAspect() const;
    void SetPreserveAspect(bool preserve);
    
    /**
     * Get/Set aspect mode
     */
    AspectMode GetAspectMode() const;
    void SetAspectMode(AspectMode mode);

    // ===== Flip Properties =====
    
    /**
     * Get/Set flip horizontal
     */
    bool GetFlipH() const;
    void SetFlipH(bool flip);
    
    /**
     * Get/Set flip vertical
     */
    bool GetFlipV() const;
    void SetFlipV(bool flip);

    // ===== Blend Mode =====
    
    /**
     * Get/Set blend mode
     */
    BlendMode GetBlendMode() const;
    void SetBlendMode(BlendMode mode);

    // ===== Mouse Behaviour =====
    
    /**
     * Get/Set mouse behaviour
     */
    MouseBehaviour GetMouseBehaviour() const;
    void SetMouseBehaviour(MouseBehaviour behaviour);

    // ===== Corner Radius Properties =====
    
    /**
     * Get the corner radius linked property
     * Order: [top-left, top-right, bottom-right, bottom-left]
     */
    const core::LinkedProperty4& GetCornerRadius() const;
    void SetCornerRadius(const core::LinkedProperty4& radius);
    
    /**
     * Set all corner radii to the same value
     */
    void SetCornerRadiusAll(float radius);
    
    /**
     * Set individual corner radius
     */
    void SetCornerRadiusIndividual(size_t index, float radius);
    
    /**
     * Get/Set corner radius linked state
     */
    bool IsCornerRadiusLinked() const;
    void SetCornerRadiusLinked(bool linked);

    // ===== Border Properties =====
    
    /**
     * Get/Set border enabled state
     */
    bool GetBorderEnabled() const;
    void SetBorderEnabled(bool enabled);
    
    /**
     * Get/Set border color
     */
    const math::Color& GetBorderColor() const;
    void SetBorderColor(const math::Color& color);
    
    /**
     * Get the border width linked property
     * Order: [top, right, bottom, left]
     */
    const core::LinkedProperty4& GetBorderWidth() const;
    void SetBorderWidth(const core::LinkedProperty4& width);
    
    /**
     * Set all border widths to the same value
     */
    void SetBorderWidthAll(float width);
    
    /**
     * Set individual border width
     */
    void SetBorderWidthIndividual(size_t index, float width);
    
    /**
     * Get/Set border width linked state
     */
    bool IsBorderWidthLinked() const;
    void SetBorderWidthLinked(bool linked);

    // ===== UI Space Properties =====
    
    /**
     * Get/Set UI space flag
     * true = renders in CameraUI space
     * false = renders in Camera2D space
     */
    bool GetUISpace() const;
    void SetUISpace(bool uiSpace);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    // Texture asset and GPU handle
    asset::AssetRef<asset::ImageAsset> m_TextureAsset;
    TextureHandle m_TextureHandle;
    bool m_TextureNeedsUpload;
    std::string m_CurrentTexturePath;
    
    // Linked properties
    core::LinkedProperty4 m_CornerRadius;
    core::LinkedProperty4 m_BorderWidth;
    
    /**
     * Upload texture to GPU if needed
     */
    void UploadTextureToGPU();
    
    /**
     * Calculate final rect based on anchors and offsets
     */
    math::Vec2 CalculatePosition() const;
    
    /**
     * Calculate UV coordinates based on flip flags
     */
    void CalculateUVCoordinates(math::Vec2& uvMin, math::Vec2& uvMax) const;
    
    /**
     * Apply aspect ratio calculations
     */
    void ApplyAspectRatio(math::Vec2& position, math::Vec2& size) const;
    
    /**
     * Render the image
     */
    void RenderImage(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    /**
     * Render the border
     */
    void RenderBorder(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);
    
    /**
     * Convert enum to int for serialization
     */
    int AspectModeToInt(AspectMode mode) const;
    AspectMode IntToAspectMode(int value) const;
    
    int BlendModeToInt(BlendMode mode) const;
    BlendMode IntToBlendMode(int value) const;
    
    int MouseBehaviourToInt(MouseBehaviour behaviour) const;
    MouseBehaviour IntToMouseBehaviour(int value) const;
};

} // namespace components
} // namespace lupine
