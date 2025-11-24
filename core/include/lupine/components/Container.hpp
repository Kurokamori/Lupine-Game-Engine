#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/StyleBox.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/math/LinkedValue.hpp"
#include <memory>
#include <vector>

namespace lupine {
namespace components {

/**
 * Container Component - Base class for all UI container components
 *
 * Provides foundational container behaviors including:
 * - Size management (width, height, min/max constraints)
 * - Padding and margin control
 * - Background rendering with borders and corner radius
 * - Child layout system with invalidation
 * - Clipping support
 * - Mouse interaction area calculation
 *
 * This is an abstract base class. Derived classes should override:
 * - CalculateLayout() to implement specific layout algorithms
 * - GetMinimumSize() to calculate minimum required size
 *
 * Usage:
 *   class VBoxContainer : public Container {
 *       void CalculateLayout() override { ... }
 *   };
 */
class Container : public core::Component, public IRenderableComponent {
public:
    enum class SizeMode {
        Fixed,          // Use explicit width/height
        FitChildren,    // Size to fit all children
        Expand,         // Expand to fill available space
        Minimum         // Use minimum required size
    };

    enum class LayoutDirection {
        Horizontal,
        Vertical
    };

    Container();
    explicit Container(const std::string& name);
    virtual ~Container();

    // ISerializable interface
    std::string GetTypeName() const override { return "Container"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;

    // Editor integration
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // IRenderableComponent interface
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    math::OBB getOrientedBounds() const override;
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    // ========================================
    // Size Management
    // ========================================

    float GetWidth() const;
    void SetWidth(float width);

    float GetHeight() const;
    void SetHeight(float height);

    math::Vec2 GetSize() const;
    void SetSize(const math::Vec2& size);

    math::Vec2 GetMinSize() const;
    void SetMinSize(const math::Vec2& minSize);

    math::Vec2 GetMaxSize() const;
    void SetMaxSize(const math::Vec2& maxSize);

    SizeMode GetHorizontalSizeMode() const;
    void SetHorizontalSizeMode(SizeMode mode);

    SizeMode GetVerticalSizeMode() const;
    void SetVerticalSizeMode(SizeMode mode);

    // ========================================
    // Padding & Margin
    // ========================================

    bool GetPaddingLinked() const;
    void SetPaddingLinked(bool linked);

    float GetPaddingLeft() const;
    void SetPaddingLeft(float padding);

    float GetPaddingRight() const;
    void SetPaddingRight(float padding);

    float GetPaddingTop() const;
    void SetPaddingTop(float padding);

    float GetPaddingBottom() const;
    void SetPaddingBottom(float padding);

    math::Vec4 GetPadding() const;
    void SetPadding(const math::Vec4& padding);

    bool GetMarginLinked() const;
    void SetMarginLinked(bool linked);

    float GetMarginLeft() const;
    void SetMarginLeft(float margin);

    float GetMarginRight() const;
    void SetMarginRight(float margin);

    float GetMarginTop() const;
    void SetMarginTop(float margin);

    float GetMarginBottom() const;
    void SetMarginBottom(float margin);

    math::Vec4 GetMargin() const;
    void SetMargin(const math::Vec4& margin);

    // ========================================
    // Visual Appearance
    // ========================================

    const math::Color& GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);

    float GetOpacity() const;
    void SetOpacity(float opacity);

    bool GetDrawBackground() const;
    void SetDrawBackground(bool draw);

    bool GetBorderEnabled() const;
    void SetBorderEnabled(bool enabled);

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

    const math::Color& GetBorderColor() const;
    void SetBorderColor(const math::Color& color);

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

    // ========================================
    // Layout System
    // ========================================

    bool GetClipChildren() const;
    void SetClipChildren(bool clip);

    float GetSeparation() const;
    void SetSeparation(float separation);

    bool GetLayoutDirty() const { return m_LayoutDirty; }
    void InvalidateLayout();
    void ForceLayoutUpdate();

    // ========================================
    // Rendering Properties
    // ========================================

    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    bool GetUseUISpace() const;
    void SetUseUISpace(bool useUISpace);

    // ========================================
    // Child Management
    // ========================================

    /**
     * Get all child nodes of the container's owner
     */
    std::vector<core::Node*> GetChildren() const;

    /**
     * Get all visible child nodes
     */
    std::vector<core::Node*> GetVisibleChildren() const;

    /**
     * Get number of children
     */
    int GetChildCount() const;

    /**
     * Get content rect (available space for children after padding)
     */
    math::Rect GetContentRect() const;

    /**
     * Get outer rect (total space including margin)
     */
    math::Rect GetOuterRect() const;

protected:
    // ========================================
    // Virtual Layout Methods (Override in Derived Classes)
    // ========================================

    /**
     * Calculate and apply layout to children
     * Override this in derived classes to implement specific layout algorithms
     * (e.g., VBox, HBox, Grid, etc.)
     */
    virtual void CalculateLayout();

    /**
     * Calculate minimum size required by this container
     * Override to implement custom minimum size calculation
     */
    virtual math::Vec2 GetMinimumSize() const;

    /**
     * Called when layout needs to be recalculated
     * Override to add custom behavior on layout invalidation
     */
    virtual void OnLayoutInvalidated();

    // ========================================
    // Internal Rendering Methods
    // ========================================

    void RenderBackground(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);
    void RenderBorder(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    // ========================================
    // Internal Helper Methods
    // ========================================

    /**
     * Calculate final size based on size mode, constraints, and children
     */
    math::Vec2 CalculateFinalSize() const;

    /**
     * Apply size constraints (min/max)
     */
    math::Vec2 ApplySizeConstraints(const math::Vec2& size) const;

    /**
     * Calculate total size of all children (for FitChildren mode)
     */
    math::Vec2 CalculateChildrenBounds() const;

    /**
     * Check if a point is within container bounds (for mouse interaction)
     */
    bool IsPointInside(const math::Vec2& point) const;

    /**
     * Sync internal state from properties
     */
    void SyncFromProperties();

protected:
    // ========================================
    // Member Variables
    // ========================================

    // Layout state
    bool m_LayoutDirty;
    math::Vec2 m_CachedSize;
    math::Vec2 m_CachedPosition;

    // Padding & Margin (stored as top, right, bottom, left)
    math::LinkedValue<float> m_Padding;
    math::LinkedValue<float> m_Margin;

    // Border & Corner Radius
    math::LinkedValue<float> m_BorderWidth;
    math::LinkedValue<float> m_CornerRadius;

    // Style
    std::shared_ptr<StyleBoxFlat> m_StyleBox;

    // Cached values for performance
    SizeMode m_HorizontalSizeMode;
    SizeMode m_VerticalSizeMode;
    bool m_ClipChildren;
    float m_Separation;

    // Rendering state
    bool m_MeshNeedsRegeneration;
};

} // namespace components
} // namespace lupine
