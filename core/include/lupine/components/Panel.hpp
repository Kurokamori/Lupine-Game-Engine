#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/components/StyleBox.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace components {

/**
 * Panel Component
 *
 * A UI component that renders a styled panel using StyleBox.
 * Similar to Godot's Panel, provides a background for UI layouts.
 *
 * Features:
 * - StyleBox support (flat, textured, etc.)
 * - Load/save StyleBox configurations from .style files
 * - Width and height properties
 * - Layer ordering
 * - UI space selection (Camera2D or CameraUI)
 * - All StyleBox features:
 *   - Background color with opacity
 *   - Per-border width/color (linked or independent)
 *   - Per-corner radius (linked or independent)
 *   - Corner detail and anti-aliasing
 *   - Shadows
 *   - Custom shader support
 */
class Panel : public core::Component, public IRenderableComponent {
public:
    Panel();
    explicit Panel(const std::string& name);
    virtual ~Panel();

    // ISerializable interface
    std::string GetTypeName() const override { return "Panel"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Size Properties =====
    
    float GetWidth() const;
    void SetWidth(float width);

    float GetHeight() const;
    void SetHeight(float height);

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== UI Space =====
    
    bool GetUseUISpace() const;
    void SetUseUISpace(bool useUISpace);

    // ===== StyleBox Management =====
    
    /**
     * Get the current StyleBox
     */
    std::shared_ptr<StyleBox> GetStyleBox() const { return m_StyleBox; }

    /**
     * Set a new StyleBox
     */
    void SetStyleBox(std::shared_ptr<StyleBox> styleBox);

    /**
     * Load StyleBox from .style file
     */
    bool LoadStyleFromFile(const std::string& filepath);

    /**
     * Save current StyleBox to .style file
     */
    bool SaveStyleToFile(const std::string& filepath) const;

    /**
     * Get style file path property
     */
    const std::string& GetStylePath() const;
    void SetStylePath(const std::string& path);

    // ===== Direct StyleBox Property Access =====
    // These provide convenient access to the StyleBoxFlat properties
    // for the inspector panel

    // Background
    const math::Color& GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);

    float GetOpacity() const;
    void SetOpacity(float opacity);

    // Border width
    bool GetBorderWidthLinked() const;
    void SetBorderWidthLinked(bool linked);

    float GetBorderWidthLeft() const;
    void SetBorderWidthLeft(float width);

    float GetBorderWidthRight() const;
    void SetBorderWidthRight(float width);

    float GetBorderWidthTop() const;
    void SetBorderWidthTop(float width);

    float GetBorderWidthBottom() const;
    void SetBorderWidthBottom(float width);

    // Border enabled
    bool GetBorderEnabled() const;
    void SetBorderEnabled(bool enabled);

    // Border color
    const math::Color& GetBorderColor() const;
    void SetBorderColor(const math::Color& color);

    // Corner radius
    bool GetCornerRadiusLinked() const;
    void SetCornerRadiusLinked(bool linked);

    float GetCornerRadiusTopLeft() const;
    void SetCornerRadiusTopLeft(float radius);

    float GetCornerRadiusTopRight() const;
    void SetCornerRadiusTopRight(float radius);

    float GetCornerRadiusBottomLeft() const;
    void SetCornerRadiusBottomLeft(float radius);

    float GetCornerRadiusBottomRight() const;
    void SetCornerRadiusBottomRight(float radius);

    // Corner detail
    int GetCornerDetail() const;
    void SetCornerDetail(int detail);

    bool GetAntiAliasing() const;
    void SetAntiAliasing(bool aa);

    float GetAntiAliasingSize() const;
    void SetAntiAliasingSize(float size);

    // Shadow
    bool GetShadowEnabled() const;
    void SetShadowEnabled(bool enabled);

    const math::Color& GetShadowColor() const;
    void SetShadowColor(const math::Color& color);

    float GetShadowSize() const;
    void SetShadowSize(float size);

    const math::Vec2& GetShadowOffset() const;
    void SetShadowOffset(const math::Vec2& offset);

    // Custom shader
    const std::string& GetCustomShaderPath() const;
    void SetCustomShaderPath(const std::string& path);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    // Override to provide oriented bounding box that rotates with panel
    math::OBB getOrientedBounds() const override;

    // Override for precise ray intersection on rotated panels
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

private:
    /**
     * Ensure we have a valid StyleBox (create default if needed)
     */
    void EnsureStyleBox();

    /**
     * Get StyleBoxFlat (cast from base StyleBox)
     */
    std::shared_ptr<StyleBoxFlat> GetStyleBoxFlat() const;

    /**
     * Render the filled rectangle
     */
    void RenderFill(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    /**
     * Render the border
     */
    void RenderBorder(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    // The StyleBox that defines the visual appearance
    std::shared_ptr<StyleBox> m_StyleBox;

    // Linked properties for editor control
    core::LinkedProperty4 m_CornerRadius;
    core::LinkedProperty4 m_BorderWidth;

    // Flag for mesh regeneration
    bool m_MeshNeedsRegeneration;
};

} // namespace components
} // namespace lupine
