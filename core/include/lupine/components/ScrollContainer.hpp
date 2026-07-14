#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * ScrollContainer
 *
 * A container that arranges its children as a vertical stack of content larger
 * than its own rect, clips them to its content area (always), and scrolls them
 * with the mouse wheel or draggable scrollbars.
 *
 * Features:
 * - Horizontal and vertical scrolling (independently toggleable)
 * - Mouse-wheel scrolling (Shift+wheel scrolls horizontally)
 * - Draggable scrollbar thumbs
 * - Scrollbar visibility per axis (Auto/Always/Never)
 * - Content clipped to the viewport via the engine scissor clip mechanism
 */
class ScrollContainer : public Container {
public:
    enum class ScrollBarVisibility {
        Auto = 0,      // Show only when content overflows
        AlwaysShow = 1,
        NeverShow = 2
    };

    ScrollContainer();
    explicit ScrollContainer(const std::string& name);
    virtual ~ScrollContainer();

    std::string GetTypeName() const override { return "ScrollContainer"; }
    void DefineProperties() override;
    void DefineSignals() override;
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnAwake() override;
    void OnReady() override;
    void OnInput(float deltaTime) override;

    // ScrollContainer always clips its descendants -- to the VIEWPORT, which is the content
    // rect minus the strips reserved by whichever scrollbars are visible. Clipping to the
    // full content rect would let the content draw under the bars.
    bool ClipsDescendants() const override { return true; }
    math::Rect GetClipRect() const override { return GetViewportRect(); }

    /**
     * The area children are actually laid out in and clipped to: the content rect minus the
     * strip each visible scrollbar reserves (right edge for the vertical bar, bottom edge
     * for the horizontal one).
     *
     * The bars used to be drawn OVER the content -- ComputeVScrollBar put the track inside
     * GetContentRect() while CalculateLayout laid children across the full content width --
     * so the rightmost scrollBarWidth pixels of every row were permanently covered.
     */
    math::Rect GetViewportRect() const;

    // ===== Scroll position =====
    float GetHScroll() const;
    void SetHScroll(float value);
    float GetVScroll() const;
    void SetVScroll(float value);

    // The full scrollable extent of the children. Measured on demand, never a stale cache.
    math::Vec2 GetContentSize() const { return MeasureContent(); }

    // ===== Enable flags =====
    bool GetHScrollEnabled() const;
    void SetHScrollEnabled(bool enabled);
    bool GetVScrollEnabled() const;
    void SetVScrollEnabled(bool enabled);

    // ===== Appearance / behavior =====
    float GetScrollSpeed() const;
    void SetScrollSpeed(float speed);
    float GetScrollBarWidth() const;
    void SetScrollBarWidth(float width);

    ScrollBarVisibility GetHScrollBarVisibility() const;
    void SetHScrollBarVisibility(ScrollBarVisibility v);
    ScrollBarVisibility GetVScrollBarVisibility() const;
    void SetVScrollBarVisibility(ScrollBarVisibility v);

    math::Color GetScrollBarColor() const;
    void SetScrollBarColor(const math::Color& color);
    math::Color GetScrollBarBackgroundColor() const;
    void SetScrollBarBackgroundColor(const math::Color& color);

protected:
    void CalculateLayout() override;
    void buildDrawCommands(RenderContext& ctx) override;

    /**
     * A ScrollContainer does NOT report its content as its minimum size -- overflowing is
     * the entire point of it. It reports the author's floor plus padding, plus the extent
     * of any axis that CANNOT scroll (that axis has to fit outright), plus the strip of any
     * bar pinned to AlwaysShow.
     *
     * It previously did not override this at all and so reported (0,0): a ScrollContainer in
     * a VerticalContainer with verticalSizeMode = Minimum collapsed to zero height.
     */
    math::Vec2 GetMinimumSize() const override;

private:
    /**
     * Full scrollable extent of a child: its intrinsic size grown to its resolved bounds
     * and, for a nested container, to the size needed to fit that container's own children,
     * so content overflowing a child container is measured and the scroll range reflects the
     * true content size.
     */
    math::Vec2 GetChildContentSize(core::Node* child) const;

    /**
     * The scrollable extent of all children stacked. Recomputed on every call (and cached
     * into m_ContentSize purely for diagnostics).
     *
     * ClampScroll() used to read a m_ContentSize written only by CalculateLayout(), so a
     * script that added twenty rows and immediately called set_v_scroll(9999) was clamped
     * against the PREVIOUS frame's content size.
     */
    math::Vec2 MeasureContent() const;

    /**
     * Resolve the viewport and which bars are visible together -- they are mutually
     * dependent, since reserving a strip for one bar shrinks the other axis and can push it
     * into overflow.
     */
    void ComputeViewport(math::Rect& outViewport, bool& outShowV, bool& outShowH) const;

    // Overflow tests against explicit extents, so they can be used while the viewport is
    // still being resolved (calling the public ShouldShow* here would recurse).
    bool WantsVScrollBar(float contentHeight, float viewportHeight) const;
    bool WantsHScrollBar(float contentWidth, float viewportWidth) const;

    void ClampScroll();
    void UpdateScrollInteraction(float deltaTime);

    // Scrollbar geometry, in global logical coordinates (minimum-corner rects).
    bool ComputeVScrollBar(math::Rect& outTrack, math::Rect& outThumb) const;
    bool ComputeHScrollBar(math::Rect& outTrack, math::Rect& outThumb) const;
    bool ShouldShowVScrollBar() const;
    bool ShouldShowHScrollBar() const;

    void RenderScrollBars(RenderContext& ctx);

    // Diagnostic cache of the last measured content size. Mutable because measuring happens
    // on the const query path; nothing may READ this as an authority -- call MeasureContent().
    mutable math::Vec2 m_ContentSize{0.0f, 0.0f};

    bool m_DraggingV{false};
    bool m_DraggingH{false};
    float m_DragStartMouse{0.0f};
    float m_DragStartScroll{0.0f};
};

} // namespace components
} // namespace lupine
