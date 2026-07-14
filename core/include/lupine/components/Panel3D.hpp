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
 * Billboard mode for Panel3D
 */
enum class Panel3DBillboardMode {
    Disabled,       // No billboarding, panel faces its local forward direction
    Enabled,        // Full billboarding, always faces camera
    YAxisOnly       // Billboard only around Y axis
};

/**
 * Panel3D Component
 *
 * A 3D UI component that renders a styled panel in 3D space using StyleBox.
 * Combines all Panel features with 3D rendering and billboard modes from Sprite3D.
 *
 * Features:
 * - All Panel features (StyleBox, borders, corners, shadows, etc.)
 * - Rendered in 3D space with proper depth testing
 * - Billboard mode support (face camera, Y-axis only, or disabled)
 * - Shadow casting and receiving
 * - Double-sided rendering option
 * - Can be used for 3D UI, floating panels, holographic displays, etc.
 */
class Panel3D : public core::Component, public IRenderableComponent {
public:
    Panel3D();
    explicit Panel3D(const std::string& name);
    virtual ~Panel3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Panel3D"; }
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

    // ===== 3D Rendering Properties =====
    
    // Billboard mode
    Panel3DBillboardMode GetBillboardMode() const;
    void SetBillboardMode(Panel3DBillboardMode mode);
    
    // Double-sided rendering
    bool GetDoubleSided() const;
    void SetDoubleSided(bool doubleSided);

    // Shadow casting
    bool GetCastShadow() const;
    void SetCastShadow(bool castShadow);

    // Shadow receiving
    bool GetReceiveShadow() const;
    void SetReceiveShadow(bool receiveShadow);

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

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

    // Background
    math::Color GetBackgroundColor() const;
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
    math::Color GetBorderColor() const;
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

    math::Color GetShadowColor() const;
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
    SpatialType getSpatialType() const override { return SpatialType::World3D; }

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
     * Render the filled rectangle in 3D space
     */
    void RenderFill(RenderContext& ctx, const math::Mat4& transform, const math::Vec2& size, bool isBackFace = false);

    /**
     * Render the border in 3D space
     */
    void RenderBorder(RenderContext& ctx, const math::Mat4& transform, const math::Vec2& size, bool isBackFace = false);

    /**
     * Calculate billboard transform matrix
     */
    math::Mat4 CalculateBillboardTransform(const math::Mat4& viewMatrix) const;

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

