#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/components/StyleBox.hpp"
#include "lupine/math/Math.hpp"
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
class Container : public UIControl, public IRenderableComponent {
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

    /**
     * How a child is placed along a container's cross axis when the child's own size
     * flags express no preference. Expressed in canvas terms: Begin is the LOW edge of
     * the axis (left on X, bottom on Y) and End is the HIGH edge (right on X, top on Y).
     */
    enum class CrossAxisAlign {
        Begin,
        Center,
        End,
        Fill
    };

    /**
     * How free space along a box container's MAIN axis is distributed when nothing expanded
     * to consume it. The first four are the classic alignments; the last three are the
     * distributed-spacing modes (flexbox's space-between / space-around / space-evenly),
     * which no container offered at all.
     *
     * Expressed in READING order: Begin is the top of a VBox and the left of an HBox.
     */
    enum class MainAxisDistribution {
        Begin = 0,
        Center = 1,
        End = 2,
        Fill = 3,
        SpaceBetween = 4,   // no space at the ends; equal gaps between children
        SpaceAround = 5,    // each child gets equal space around it, so the end gaps are half
        SpaceEvenly = 6     // the end gaps and the inter-child gaps are all equal
    };

    /**
     * Resolve `freeSpace` into a leading offset and an extra gap to add between children.
     *
     * Free space is only ever positive when nothing expanded (an expanding child consumes it
     * all), and negative free space -- children overflowing -- yields no offset and no gap
     * rather than pulling them on top of each other.
     */
    static void DistributeFreeSpace(MainAxisDistribution mode, float freeSpace, size_t childCount,
                                    float& outLeading, float& outExtraGap);

    Container();
    explicit Container(const std::string& name);
    virtual ~Container();

    // ISerializable interface
    std::string GetTypeName() const override { return "Container"; }
    void DefineProperties() override;
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Resolve our own rect, then arrange our children into it.
    void PerformLayout() override;

    // Editor integration
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // IRenderableComponent interface
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    math::OBB getOrientedBounds() const override;
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    // UIControl overrides: a Container is a control that arranges its children.
    bool IsLayoutContainer() const override { return true; }
    // Re-run our own layout and propagate up the container chain: a descendant's
    // content size changing (e.g. a Label whose font finished loading) can change
    // this container's content size, which in turn affects an outer container (a
    // ScrollContainer's scroll range). Terminates at the first non-container ancestor.
    void OnChildLayoutChanged() override { InvalidateLayout(); InvalidateParentContainerLayout(); }

    // Clipping: when "clipChildren" is enabled the render traversal scissors all
    // descendants to the container's content rect.
    bool ClipsDescendants() const override { return GetClipChildren(); }
    math::Rect GetClipRect() const override { return GetContentRect(); }
    // The minimum size reported to a parent container is the size needed to fit children.
    math::Vec2 GetContentMinSize() const override;

    // ========================================
    // Size Management
    // ========================================

    float GetWidth() const override;
    void SetWidth(float width);

    float GetHeight() const override;
    void SetHeight(float height);

    math::Vec2 GetSize() const override;
    void SetSize(const math::Vec2& size);

    // Min/max size constraints. These deliberately do NOT redeclare the (virtual)
    // UIControl::GetMinSize()/GetMaxSize(): a container used to carry its own
    // minSize/maxSize properties that shadowed the base ones, so the same object
    // reported different limits through a Container* than through a UIControl*, and the
    // anchor solver (which reads the base) silently ignored them. There is now one
    // storage -- customMinSize/customMaxSize on UIControl -- and these setters write it.
    void SetMinSize(const math::Vec2& minSize) { SetCustomMinSize(minSize); InvalidateLayout(); }
    void SetMaxSize(const math::Vec2& maxSize) { SetCustomMaxSize(maxSize); InvalidateLayout(); }

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

    math::Color GetBackgroundColor() const;
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

    math::Color GetBorderColor() const;
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

    /**
     * Run CalculateLayout() until the arrangement stops re-dirtying itself (bounded).
     * An InvalidateLayout() raised from inside CalculateLayout() is honored rather than
     * being cleared away underneath it.
     */
    void RunLayout();

    /**
     * True for properties that only change how the container paints and can never move
     * or resize anything. Layout is not re-run for these, so e.g. a background-colour
     * tween no longer re-lays-out the whole subtree every frame.
     */
    static bool IsPurelyVisualProperty(const std::string& propertyName);

    // ========================================
    // Rendering Properties
    // ========================================

    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

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
    // Shared measure / arrange helpers
    // ========================================

    /**
     * MEASURE: the intrinsic size a child wants, independent of any rect this container
     * previously assigned it. For a UIControl child this is GetDesiredSize() (authored
     * width/height floored to its content minimum); for a plain node it falls back to
     * scanning components for width/height or size properties.
     *
     * This is the single implementation for the whole container family. It used to be
     * copy-pasted into nine containers, eight of which scanned components and returned
     * the FIRST one carrying width/height -- so a Label with width=0 (auto-size from
     * text) measured as zero, and a node carrying both a Panel and a Label measured
     * whichever component happened to be added first.
     */
    math::Vec2 GetChildSize(core::Node* child) const;

    /**
     * A child's OUTER margin, as (top, right, bottom, left) -- space this container reserves
     * around it, over and above its size. Only a container child can carry one (`margin` is a
     * Container property); everything else reports zero.
     *
     * GetChildSize() adds it to the child's measurement and SetChildRect() insets the arranged
     * slot by it, which is what makes `margin` mean something. It used to affect NOTHING:
     * GetOuterRect() had no callers in core, and the only code that read the margin was
     * getWorldBounds(), which inflated the editor's selection box by it -- so `margin = 20`
     * produced no visual change but made the selection rectangle 40px bigger than the panel
     * and picked up clicks outside it.
     */
    math::Vec4 GetChildMargin(core::Node* child) const;

    /**
     * MEASURE (height-for-width): the height the child needs when constrained to
     * `availableWidth`. Falls back to GetChildSize(child).y for controls whose height
     * does not depend on their width.
     */
    float GetChildHeightForWidth(core::Node* child, float availableWidth) const;

    /**
     * ARRANGE: give the child its final global rect. This is the ONLY way a container
     * may place a child.
     *
     * It routes through UIControl::SetRect(), which stores the rect as the child's
     * resolved rect and moves the owning Node2D -- but deliberately does not touch the
     * child's authored width/height properties. Containers used to write the arranged
     * size back into those properties, which made children ratchet larger every frame
     * (they could grow but never shrink) and serialized transient layout output into the
     * saved scene, destroying the authored design-time size.
     *
     * Children with no UIControl fall back to positioning the Node2D and, only then,
     * writing size properties -- they have nowhere else to store a resolved size.
     */
    void SetChildRect(core::Node* child, const math::Rect& globalRect) const;

    // ========================================
    // Per-child size-flag helpers (generic expand/fill support)
    // ========================================

    /**
     * True if the child wants to expand along the given axis (vertical=true for the
     * vertical axis). Honors UIControl size flags (SizeFlagBits::Expand) and the
     * legacy Spacer "expand" property.
     */
    bool ChildWantsExpand(core::Node* child, bool vertical) const;

    /**
     * True if the child wants to fill the container's cross axis (UIControl
     * SizeFlagBits::Fill on the given axis).
     */
    bool ChildWantsFill(core::Node* child, bool vertical) const;

    /**
     * Stretch ratio of a child for distributing expand space (UIControl
     * sizeFlagsStretchRatio); returns 1.0 when the child has no UIControl.
     */
    float ChildStretchRatio(core::Node* child) const;

    /**
     * Distribute `availableMain` along a box container's MAIN axis and return each
     * child's final extent, parallel to `children`.
     *
     * Matches Godot's BoxContainer model:
     *  - non-expanding children keep their desired extent;
     *  - expanding children share a pool made of (their own desired extents + all the
     *    free space) split by stretch ratio -- NOT merely the leftover. Two children with
     *    a desired 100 and ratios 1:3 in a 400px box therefore end up 100/300, where
     *    splitting only the leftover would give 150/250;
     *  - a child whose ratio share falls below its minimum, or rises above its maximum, is
     *    pinned there and dropped from the pool, and the remainder is redistributed among
     *    the rest (iterated to a fixed point).
     *
     * The returned extents are pure output -- callers must arrange with SetChildRect and
     * must never write them back into the children's width/height properties.
     */
    std::vector<float> DistributeMainAxis(const std::vector<core::Node*>& children,
                                          bool vertical,
                                          float availableMain,
                                          float separation) const;

    /**
     * Nine-way alignment of a `childSize` box inside `area`, returning the child's global
     * rect. Expressed in canvas (Y-up) terms, so Top really is the top edge.
     *
     * Shared by every container that offers a nine-way child alignment. Each used to
     * open-code it against `area.position.y` as though that were the top edge, so TopLeft
     * landed bottom-left and all six Top/Bottom cases came out swapped.
     */
    static math::Rect AlignRectIn(const math::Rect& area, const math::Vec2& childSize,
                                  CrossAxisAlign horizontal, CrossAxisAlign vertical);

    /**
     * The size a child should be given inside `available` when a container auto-fits it:
     * the whole area, or the largest aspect-preserving box that fits when
     * `maintainAspectRatio` is set. Returns the child's desired size unchanged when
     * auto-fitting is off.
     */
    math::Vec2 FitChildSize(core::Node* child, const math::Vec2& available,
                            bool autoFit, bool maintainAspectRatio) const;

    /**
     * Place a child along the container's CROSS axis (the axis the container does not
     * stack along), honoring that child's per-child size flags on that axis:
     *
     *   Fill        -> take the whole extent
     *   ShrinkBegin -> keep the desired extent, sit at the low edge  (left / bottom)
     *   ShrinkCenter-> keep the desired extent, centered
     *   ShrinkEnd   -> keep the desired extent, sit at the high edge (right / top)
     *
     * `axisBegin`/`axisExtent` describe the available span in MINIMUM-corner terms (so on
     * the vertical axis `axisBegin` is the BOTTOM edge). `fallback` is the container-wide
     * alignment used when the child expresses no opinion via its flags.
     *
     * Fill is honored per child here, which is what makes sizeFlagsHorizontal/Vertical
     * mean something in every container rather than only in HBox/VBox.
     */
    void PlaceChildOnCrossAxis(core::Node* child, bool vertical,
                               float axisBegin, float axisExtent, float desiredExtent,
                               CrossAxisAlign fallback,
                               float& outBegin, float& outExtent) const;

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
     * Apply size constraints (min/max) from customMinSize/customMaxSize, where a
     * maximum of 0 on an axis means "unbounded" and the minimum always wins.
     */
    math::Vec2 ApplySizeConstraints(const math::Vec2& size) const;

    /**
     * The size needed to contain every visible child, for SizeMode::FitChildren.
     *
     * Derived from each child's INTRINSIC size (GetChildSize) laid out by this
     * container's own rules, never from the global positions this container wrote on a
     * previous pass -- deriving it from placed positions is a feedback loop, and the old
     * implementation additionally hardcoded every child to 1x1 pixel.
     *
     * The base implementation returns the union of the children's desired sizes (correct
     * for overlay-style containers such as Stack/Center). Stacking containers override
     * GetMinimumSize(), which is what FitChildren actually consumes for them.
     */
    virtual math::Vec2 CalculateChildrenBounds() const;

    /**
     * Sync internal state from properties. Runs on every layout pass, and ends by calling
     * SyncDerivedProperties() so the derived container's own cache is refreshed too.
     */
    void SyncFromProperties();

    /**
     * Re-read the DERIVED container's cached properties (alignment enums, spacing, ...)
     * from the property registry.
     *
     * Every derived container used to cache these in members written only from OnAwake()
     * and OnPropertyChanged(). The editor never calls OnAwake(), so a scene saved with
     * e.g. Wrap.spacingX = 20 reopened and laid out with the CONSTRUCTOR default of 0
     * until the property was nudged. Overriding this hook -- which SyncFromProperties()
     * calls on every pass -- is the fix, and it is also the only place these members need
     * to be read now: OnAwake() and OnPropertyChanged() just delegate here.
     *
     * Implementations should use the SyncCached* helpers below, which invalidate layout
     * only when a value actually changed.
     */
    virtual void SyncDerivedProperties() {}

    // Re-read `propertyName` into `cached`. Returns true, and invalidates layout, only if
    // the value actually changed -- so calling these every pass is free when nothing moved.
    bool SyncCachedFloat(const std::string& propertyName, float& cached);
    bool SyncCachedInt(const std::string& propertyName, int& cached);
    bool SyncCachedBool(const std::string& propertyName, bool& cached);
    bool SyncCachedColor(const std::string& propertyName, math::Color& cached);

    template <typename EnumT>
    bool SyncCachedEnum(const std::string& propertyName, EnumT& cached) {
        const EnumT value = static_cast<EnumT>(GetPropertyValue<int>(propertyName));
        if (value == cached) {
            return false;
        }
        cached = value;
        InvalidateLayout();
        return true;
    }

    /**
     * Invalidate the cached minimum size of this container and of every ancestor
     * container, so the next measure query recomputes. Called whenever anything that can
     * change our content size changes (children, padding, separation, a descendant's
     * text).
     */
    void InvalidateMeasureCache() const;

    /**
     * Adopt the CURRENT visible-child list as the baseline that PerformLayout() compares
     * against, without invalidating.
     *
     * For a container whose own CalculateLayout() changes child visibility (TabContainer
     * hides the inactive pages). Without this, PerformLayout() sees the list it itself
     * just caused to change, treats it as an external edit, and re-invalidates -- a
     * guaranteed redundant re-layout on the frame after every tab switch.
     */
    void SyncChildListCache();

protected:
    // ========================================
    // Member Variables
    // ========================================

    // Layout state
    bool m_LayoutDirty;
    math::Vec2 m_CachedSize;
    math::Vec2 m_CachedPosition;
    size_t m_CachedChildCount;
    size_t m_CachedVisibleChildCount;

    // Identity+order of the visible children at the last layout. Comparing only the
    // COUNT (as this class used to) misses reordering and reparenting entirely: swapping
    // two children of a VBox left them drawn in their old slots.
    std::vector<core::Node*> m_CachedVisibleChildren;

    // Memoized GetMinimumSize(). Mutable because GetContentMinSize() is a const query on
    // the measure path; dropped by InvalidateMeasureCache().
    mutable math::Vec2 m_CachedMinimumSize{0.0f, 0.0f};
    mutable bool m_MeasureCacheValid = false;

    // Padding & Margin (stored as top, right, bottom, left)
    core::LinkedProperty4 m_Padding;
    core::LinkedProperty4 m_Margin;

    // Border & Corner Radius
    core::LinkedProperty4 m_BorderWidth;
    core::LinkedProperty4 m_CornerRadius;

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
