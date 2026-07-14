#include "lupine/components/ScrollContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

ScrollContainer::ScrollContainer()
    : Container("ScrollContainer")
{
}

ScrollContainer::ScrollContainer(const std::string& name)
    : Container(name)
{
}

ScrollContainer::~ScrollContainer() {
}

void ScrollContainer::DefineProperties() {
    Container::DefineProperties();

    DefineProperty(PROPERTY_DEFAULT_GROUP(hScrollEnabled, Bool, true, "Scroll"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(vScrollEnabled, Bool, true, "Scroll"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hScroll, 0.0f, 0.0f, 100000.0f, 1.0f, "Scroll"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(vScroll, 0.0f, 0.0f, 100000.0f, 1.0f, "Scroll"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scrollSpeed, 30.0f, 1.0f, 500.0f, 1.0f, "Scroll"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scrollBarWidth, 12.0f, 2.0f, 64.0f, 1.0f, "Scroll"));
    DefineProperty(PROPERTY_ENUM_GROUP(hScrollBarVisibility, 0, "Scroll", Auto, AlwaysShow, NeverShow));
    DefineProperty(PROPERTY_ENUM_GROUP(vScrollBarVisibility, 0, "Scroll", Auto, AlwaysShow, NeverShow));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scrollBarColor, Color, Color(0.6f, 0.6f, 0.6f, 1.0f), "Scroll"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scrollBarBackgroundColor, Color, Color(0.2f, 0.2f, 0.2f, 0.5f), "Scroll"));
}

void ScrollContainer::DefineSignals() {
    RegisterSignal({"scrolled",
                    {{"h", core::PropertyValueType::Float}, {"v", core::PropertyValueType::Float}},
                    "Emitted when the scroll position changes."});
}

void ScrollContainer::OnAwake() {
    Container::OnAwake();
    // Force clipping on so the underlying StyleBox/content respects the viewport.
    SetClipChildren(true);
}

void ScrollContainer::OnReady() {
    Container::OnReady();
    InvalidateLayout();
}

void ScrollContainer::OnInput(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }
    UpdateScrollInteraction(deltaTime);
}

// ========================================
// Accessors
// ========================================

float ScrollContainer::GetHScroll() const { return GetPropertyValue<float>("hScroll"); }

void ScrollContainer::SetHScroll(float value) {
    SetPropertyValue<float>("hScroll", value);
    ClampScroll();
    InvalidateLayout();
}

float ScrollContainer::GetVScroll() const { return GetPropertyValue<float>("vScroll"); }

void ScrollContainer::SetVScroll(float value) {
    SetPropertyValue<float>("vScroll", value);
    ClampScroll();
    InvalidateLayout();
}

bool ScrollContainer::GetHScrollEnabled() const { return GetPropertyValue<bool>("hScrollEnabled"); }
void ScrollContainer::SetHScrollEnabled(bool enabled) { SetPropertyValue<bool>("hScrollEnabled", enabled); InvalidateLayout(); }
bool ScrollContainer::GetVScrollEnabled() const { return GetPropertyValue<bool>("vScrollEnabled"); }
void ScrollContainer::SetVScrollEnabled(bool enabled) { SetPropertyValue<bool>("vScrollEnabled", enabled); InvalidateLayout(); }

float ScrollContainer::GetScrollSpeed() const { return GetPropertyValue<float>("scrollSpeed"); }
void ScrollContainer::SetScrollSpeed(float speed) { SetPropertyValue<float>("scrollSpeed", speed); }
float ScrollContainer::GetScrollBarWidth() const { return GetPropertyValue<float>("scrollBarWidth"); }
void ScrollContainer::SetScrollBarWidth(float width) { SetPropertyValue<float>("scrollBarWidth", width); }

ScrollContainer::ScrollBarVisibility ScrollContainer::GetHScrollBarVisibility() const {
    return static_cast<ScrollBarVisibility>(GetPropertyValue<int>("hScrollBarVisibility"));
}
void ScrollContainer::SetHScrollBarVisibility(ScrollBarVisibility v) {
    SetPropertyValue<int>("hScrollBarVisibility", static_cast<int>(v));
}
ScrollContainer::ScrollBarVisibility ScrollContainer::GetVScrollBarVisibility() const {
    return static_cast<ScrollBarVisibility>(GetPropertyValue<int>("vScrollBarVisibility"));
}
void ScrollContainer::SetVScrollBarVisibility(ScrollBarVisibility v) {
    SetPropertyValue<int>("vScrollBarVisibility", static_cast<int>(v));
}

const std::vector<UIControl::ThemeBinding>& ScrollContainer::GetThemeBindings() const {
    // Includes the inherited Container bindings (so background/border/corner-radius
    // stay themeable for ScrollContainer) plus the scrollbar-specific entries.
    static const std::vector<ThemeBinding> kBindings = {
        { "backgroundColor",          "background",           ThemeBinding::Kind::Color },
        { "borderColor",              "border_color",         ThemeBinding::Kind::Color },
        { "cornerRadius",             "corner_radius",        ThemeBinding::Kind::Constant },
        { "scrollBarColor",           "scrollbar_color",      ThemeBinding::Kind::Color },
        { "scrollBarBackgroundColor", "scrollbar_background", ThemeBinding::Kind::Color }
    };
    return kBindings;
}

Color ScrollContainer::GetScrollBarColor() const {
    return ResolveThemedColor("scrollBarColor", "scrollbar_color");
}
void ScrollContainer::SetScrollBarColor(const Color& color) { SetThemedProperty<Color>("scrollBarColor", color); }

Color ScrollContainer::GetScrollBarBackgroundColor() const {
    return ResolveThemedColor("scrollBarBackgroundColor", "scrollbar_background");
}
void ScrollContainer::SetScrollBarBackgroundColor(const Color& color) { SetThemedProperty<Color>("scrollBarBackgroundColor", color); }

// ========================================
// Measure
// ========================================

Vec2 ScrollContainer::GetChildContentSize(Node* child) const {
    if (!child) {
        return Vec2(0.0f, 0.0f);
    }

    // The shared measure impl (authored size floored to the child's content minimum). This
    // used to be a private GetChildIntrinsicSize() that scanned the child's components and
    // returned the FIRST one carrying width/height, so a Label with width = 0 (auto-size
    // from its text) measured as zero.
    Vec2 size = GetChildSize(child);

    if (std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>()) {
        // Grow to the child's resolved on-screen size (honors anchors / size flags)...
        const Vec2 bounds = uc->GetBoundsSize();
        size.x = std::max(size.x, bounds.x);
        size.y = std::max(size.y, bounds.y);

        // ...and, for a nested container, to the size needed to fit its own children. A
        // VerticalContainer whose children overflow its own rect is therefore measured by
        // what it actually lays out, so the scroll range reflects the real content height
        // rather than the container's (possibly smaller) requested size.
        const Vec2 contentMin = uc->GetContentMinSize();
        size.x = std::max(size.x, contentMin.x);
        size.y = std::max(size.y, contentMin.y);
    }

    return size;
}

Vec2 ScrollContainer::MeasureContent() const {
    const std::vector<Node*> children = GetVisibleChildren();

    float contentWidth = 0.0f;
    float contentHeight = 0.0f;
    for (Node* child : children) {
        const Vec2 size = GetChildContentSize(child);
        contentWidth = std::max(contentWidth, size.x);
        contentHeight += size.y;
    }
    if (!children.empty()) {
        contentHeight += GetSeparation() * static_cast<float>(children.size() - 1);
    }

    m_ContentSize = Vec2(contentWidth, contentHeight);
    return m_ContentSize;
}

Vec2 ScrollContainer::GetMinimumSize() const {
    const Vec2 floorSize = Container::GetMinimumSize();
    const Vec2 content = MeasureContent();
    const float barWidth = GetScrollBarWidth();

    float minX = GetPaddingLeft() + GetPaddingRight();
    float minY = GetPaddingTop() + GetPaddingBottom();

    // An axis that cannot scroll has to show its content outright, so it must be at least as
    // big as that content. An axis that CAN scroll only needs the author's floor -- being
    // smaller than the content is the whole point.
    if (!GetHScrollEnabled()) {
        minX += content.x;
    }
    if (!GetVScrollEnabled()) {
        minY += content.y;
    }

    // A bar pinned to AlwaysShow permanently reserves its strip, so the minimum has to
    // include it or the viewport is squeezed below the author's floor.
    if (GetVScrollEnabled() && GetVScrollBarVisibility() == ScrollBarVisibility::AlwaysShow) {
        minX += barWidth;
    }
    if (GetHScrollEnabled() && GetHScrollBarVisibility() == ScrollBarVisibility::AlwaysShow) {
        minY += barWidth;
    }

    return Vec2(std::max(floorSize.x, minX), std::max(floorSize.y, minY));
}

// ========================================
// Viewport
// ========================================

bool ScrollContainer::WantsVScrollBar(float contentHeight, float viewportHeight) const {
    if (!GetVScrollEnabled()) {
        return false;
    }
    switch (GetVScrollBarVisibility()) {
        case ScrollBarVisibility::AlwaysShow: return true;
        case ScrollBarVisibility::NeverShow:  return false;
        case ScrollBarVisibility::Auto:
        default:                              return contentHeight > viewportHeight + 0.5f;
    }
}

bool ScrollContainer::WantsHScrollBar(float contentWidth, float viewportWidth) const {
    if (!GetHScrollEnabled()) {
        return false;
    }
    switch (GetHScrollBarVisibility()) {
        case ScrollBarVisibility::AlwaysShow: return true;
        case ScrollBarVisibility::NeverShow:  return false;
        case ScrollBarVisibility::Auto:
        default:                              return contentWidth > viewportWidth + 0.5f;
    }
}

void ScrollContainer::ComputeViewport(Rect& outViewport, bool& outShowV, bool& outShowH) const {
    const Rect content = GetContentRect();
    const Vec2 contentSize = MeasureContent();
    const float barWidth = GetScrollBarWidth();

    outShowV = false;
    outShowH = false;

    // Two passes, because the two bars are mutually dependent: reserving the right-hand
    // strip for a vertical bar narrows the viewport, which can push the horizontal axis into
    // overflow and require a horizontal bar -- which in turn shortens the viewport. A second
    // pass settles it; a third could not change the outcome, since a bar once shown is never
    // withdrawn by the space the other one takes.
    for (int pass = 0; pass < 2; ++pass) {
        const float viewportWidth = std::max(0.0f, content.size.x - (outShowV ? barWidth : 0.0f));
        const float viewportHeight = std::max(0.0f, content.size.y - (outShowH ? barWidth : 0.0f));

        outShowV = WantsVScrollBar(contentSize.y, viewportHeight);
        outShowH = WantsHScrollBar(contentSize.x, viewportWidth);
    }

    // The vertical bar takes a strip off the RIGHT edge, the horizontal one off the BOTTOM.
    outViewport = Rect::FromEdges(
        content.GetLeftEdge(),
        content.GetTopEdge(),
        content.GetRightEdge() - (outShowV ? barWidth : 0.0f),
        content.GetBottomEdge() + (outShowH ? barWidth : 0.0f)
    );
}

Rect ScrollContainer::GetViewportRect() const {
    Rect viewport;
    bool showV = false;
    bool showH = false;
    ComputeViewport(viewport, showV, showH);
    return viewport;
}

// ========================================
// Layout
// ========================================

void ScrollContainer::CalculateLayout() {
    if (!m_Owner) {
        return;
    }

    Rect viewport;
    bool showV = false;
    bool showH = false;
    ComputeViewport(viewport, showV, showH);

    const Vec2 contentSize = MeasureContent();

    ClampScroll();

    const float hScroll = GetHScrollEnabled() ? GetHScroll() : 0.0f;
    const float vScroll = GetVScrollEnabled() ? GetVScroll() : 0.0f;

    const float separation = GetSeparation();

    // Children are laid out across the viewport, and only as wide as the scrollable content
    // when that is wider (so a horizontally-scrolled row keeps its full width rather than
    // being clamped to the visible strip).
    const float crossExtent = std::max(viewport.size.x, contentSize.x);
    const float crossBegin = viewport.GetLeftEdge() - hScroll;

    // Y-up canvas: stack downward from the viewport's TOP edge, so the first child is at the
    // top. A positive vScroll raises the content above the viewport top, bringing items
    // further down the list into view.
    float currentTop = viewport.GetTopEdge() + vScroll;

    for (Node* child : GetVisibleChildren()) {
        // The child is given the full extent it needs, not merely its authored height, so a
        // nested container whose own children overflow is not cut off mid-list.
        const float childHeight = GetChildContentSize(child).y;

        float childX = 0.0f;
        float childWidth = 0.0f;
        PlaceChildOnCrossAxis(child, false,
                              crossBegin, crossExtent, GetChildSize(child).x,
                              CrossAxisAlign::Fill,
                              childX, childWidth);

        SetChildRect(child, Rect(childX, currentTop - childHeight, childWidth, childHeight));

        currentTop -= childHeight + separation;
    }
}

void ScrollContainer::ClampScroll() {
    Rect viewport;
    bool showV = false;
    bool showH = false;
    ComputeViewport(viewport, showV, showH);

    // Measured here and now, never read out of a cache the layout pass happens to have
    // filled in: a script that appends rows and immediately scrolls to the bottom must be
    // clamped against the content it just created.
    const Vec2 contentSize = MeasureContent();

    const float maxH = std::max(0.0f, contentSize.x - viewport.size.x);
    const float maxV = std::max(0.0f, contentSize.y - viewport.size.y);

    const float hScroll = GetHScrollEnabled() ? std::clamp(GetHScroll(), 0.0f, maxH) : 0.0f;
    const float vScroll = GetVScrollEnabled() ? std::clamp(GetVScroll(), 0.0f, maxV) : 0.0f;

    if (hScroll != GetHScroll()) {
        SetPropertyValue<float>("hScroll", hScroll);
    }
    if (vScroll != GetVScroll()) {
        SetPropertyValue<float>("vScroll", vScroll);
    }
}

// ========================================
// Scrollbars
// ========================================

bool ScrollContainer::ShouldShowVScrollBar() const {
    Rect viewport;
    bool showV = false;
    bool showH = false;
    ComputeViewport(viewport, showV, showH);
    return showV;
}

bool ScrollContainer::ShouldShowHScrollBar() const {
    Rect viewport;
    bool showV = false;
    bool showH = false;
    ComputeViewport(viewport, showV, showH);
    return showH;
}

bool ScrollContainer::ComputeVScrollBar(Rect& outTrack, Rect& outThumb) const {
    Rect viewport;
    bool showV = false;
    bool showH = false;
    ComputeViewport(viewport, showV, showH);

    const Rect content = GetContentRect();
    const Vec2 contentSize = MeasureContent();
    const float barWidth = GetScrollBarWidth();

    if (viewport.size.y <= 0.0f || contentSize.y <= 0.0f) {
        return false;
    }

    const float maxV = std::max(0.0f, contentSize.y - viewport.size.y);

    // The track occupies exactly the strip the viewport gave up on the right, and spans the
    // viewport's height (not the content rect's, so it does not run under a horizontal bar).
    outTrack.position = Vec2(content.GetRightEdge() - barWidth, viewport.GetBottomEdge());
    outTrack.size = Vec2(barWidth, viewport.size.y);

    float thumbHeight = viewport.size.y * (viewport.size.y / contentSize.y);
    thumbHeight = std::clamp(thumbHeight, std::min(barWidth * 2.0f, viewport.size.y), viewport.size.y);

    const float travel = viewport.size.y - thumbHeight;
    const float t = (maxV > 0.0f) ? (GetVScroll() / maxV) : 0.0f;

    // Y-up: the thumb sits at the TOP of the track when vScroll is 0 (t = 0) and slides down
    // to the bottom as vScroll reaches its maximum (t = 1).
    outThumb.position = Vec2(outTrack.position.x, viewport.GetBottomEdge() + travel * (1.0f - t));
    outThumb.size = Vec2(barWidth, thumbHeight);
    return true;
}

bool ScrollContainer::ComputeHScrollBar(Rect& outTrack, Rect& outThumb) const {
    Rect viewport;
    bool showV = false;
    bool showH = false;
    ComputeViewport(viewport, showV, showH);

    const Rect content = GetContentRect();
    const Vec2 contentSize = MeasureContent();
    const float barWidth = GetScrollBarWidth();

    if (viewport.size.x <= 0.0f || contentSize.x <= 0.0f) {
        return false;
    }

    const float maxH = std::max(0.0f, contentSize.x - viewport.size.x);

    // The horizontal bar sits along the bottom edge of the content rect -- the strip the
    // viewport gave up -- and spans the viewport's width.
    outTrack.position = Vec2(viewport.GetLeftEdge(), content.GetBottomEdge());
    outTrack.size = Vec2(viewport.size.x, barWidth);

    float thumbWidth = viewport.size.x * (viewport.size.x / contentSize.x);
    thumbWidth = std::clamp(thumbWidth, std::min(barWidth * 2.0f, viewport.size.x), viewport.size.x);

    const float travel = viewport.size.x - thumbWidth;
    const float t = (maxH > 0.0f) ? (GetHScroll() / maxH) : 0.0f;

    outThumb.position = Vec2(viewport.GetLeftEdge() + t * travel, outTrack.position.y);
    outThumb.size = Vec2(thumbWidth, barWidth);
    return true;
}

void ScrollContainer::RenderScrollBars(RenderContext& ctx) {
    const Color barColor = GetScrollBarColor();
    const Color bgColor = GetScrollBarBackgroundColor();
    const float radius = GetScrollBarWidth() * 0.5f;

    // The render traversal records a node's own draws BEFORE descending into its children,
    // so at an equal Z-index the scrolled content would draw over the scrollbars. Raise the
    // Z-index for the bars (within this canvas layer) so they sort above the container's
    // descendants, which share the container's Z-index by default.
    const int savedZIndex = ctx.getZIndex();
    ctx.setZIndex(savedZIndex + 4096);

    if (ShouldShowVScrollBar()) {
        Rect track, thumb;
        if (ComputeVScrollBar(track, thumb)) {
            ctx.drawRoundedRect(track.position, track.size, radius, bgColor, 0);
            ctx.drawRoundedRect(thumb.position, thumb.size, radius, barColor, 0);
        }
    }
    if (ShouldShowHScrollBar()) {
        Rect track, thumb;
        if (ComputeHScrollBar(track, thumb)) {
            ctx.drawRoundedRect(track.position, track.size, radius, bgColor, 0);
            ctx.drawRoundedRect(thumb.position, thumb.size, radius, barColor, 0);
        }
    }

    ctx.setZIndex(savedZIndex);
}

// ========================================
// Interaction
// ========================================

void ScrollContainer::UpdateScrollInteraction(float deltaTime) {
    (void)deltaTime;
    input::InputManager& inputMgr = input::InputManager::Get();

    const Vec2 mouse = GetCanvasMousePosition();
    const Rect viewport = GetViewportRect();
    const Vec2 contentSize = MeasureContent();

    const float maxV = std::max(0.0f, contentSize.y - viewport.size.y);
    const float maxH = std::max(0.0f, contentSize.x - viewport.size.x);

    const bool leftDown = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);
    const bool leftJustPressed = inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left);

    // Begin / continue / end dragging. Clicking anywhere on the track (not only the thumb)
    // jumps the content to that position and begins dragging, so the thin thumb does not
    // have to be grabbed precisely.
    if (leftJustPressed && !m_DraggingV && !m_DraggingH) {
        Rect vTrack, vThumb, hTrack, hThumb;
        const bool vShown = ShouldShowVScrollBar() && ComputeVScrollBar(vTrack, vThumb);
        const bool hShown = ShouldShowHScrollBar() && ComputeHScrollBar(hTrack, hThumb);

        if (vShown && vTrack.Contains(mouse)) {
            m_DraggingV = true;
            if (!vThumb.Contains(mouse)) {
                const float travel = viewport.size.y - vThumb.size.y;
                if (travel > 0.0f && maxV > 0.0f) {
                    const float oneMinusT = (mouse.y - viewport.GetBottomEdge() - vThumb.size.y * 0.5f) / travel;
                    SetVScroll(std::clamp(1.0f - oneMinusT, 0.0f, 1.0f) * maxV);
                }
            }
            m_DragStartMouse = mouse.y;
            m_DragStartScroll = GetVScroll();
        } else if (hShown && hTrack.Contains(mouse)) {
            m_DraggingH = true;
            if (!hThumb.Contains(mouse)) {
                const float travel = viewport.size.x - hThumb.size.x;
                if (travel > 0.0f && maxH > 0.0f) {
                    const float t = (mouse.x - viewport.GetLeftEdge() - hThumb.size.x * 0.5f) / travel;
                    SetHScroll(std::clamp(t, 0.0f, 1.0f) * maxH);
                }
            }
            m_DragStartMouse = mouse.x;
            m_DragStartScroll = GetHScroll();
        }
    }

    if (m_DraggingV) {
        if (!leftDown) {
            m_DraggingV = false;
        } else {
            Rect track, thumb;
            if (ComputeVScrollBar(track, thumb)) {
                const float travel = viewport.size.y - thumb.size.y;
                if (travel > 0.0f && maxV > 0.0f) {
                    // Y-up: dragging the thumb down (decreasing mouse Y) increases scroll.
                    const float newScroll = m_DragStartScroll - (mouse.y - m_DragStartMouse) * (maxV / travel);
                    SetVScroll(newScroll);
                    Emit("scrolled", { GetHScroll(), GetVScroll() });
                }
            }
        }
        return;
    }

    if (m_DraggingH) {
        if (!leftDown) {
            m_DraggingH = false;
        } else {
            Rect track, thumb;
            if (ComputeHScrollBar(track, thumb)) {
                const float travel = viewport.size.x - thumb.size.x;
                if (travel > 0.0f && maxH > 0.0f) {
                    const float newScroll = m_DragStartScroll + (mouse.x - m_DragStartMouse) * (maxH / travel);
                    SetHScroll(newScroll);
                    Emit("scrolled", { GetHScroll(), GetVScroll() });
                }
            }
        }
        return;
    }

    // Wheel scrolling when the pointer is over the viewport.
    if (viewport.Contains(mouse)) {
        const glm::vec2 wheel = inputMgr.GetMouseScrollDelta();
        if (wheel.x != 0.0f || wheel.y != 0.0f) {
            const float speed = GetScrollSpeed();
            const bool scrollHorizontally = inputMgr.IsShiftPressed() || (wheel.y == 0.0f && wheel.x != 0.0f);

            bool changed = false;
            if (scrollHorizontally && GetHScrollEnabled()) {
                SetPropertyValue<float>("hScroll", GetHScroll() - wheel.y * speed - wheel.x * speed);
                changed = true;
            } else if (GetVScrollEnabled()) {
                SetPropertyValue<float>("vScroll", GetVScroll() - wheel.y * speed);
                changed = true;
            }

            if (changed) {
                ClampScroll();
                InvalidateLayout();
                Emit("scrolled", { GetHScroll(), GetVScroll() });
            }
        }
    }
}

void ScrollContainer::buildDrawCommands(RenderContext& ctx) {
    // The base renders the background/border. Scrollbars are drawn afterward so they sit
    // above the content and are not affected by the content clip (a control does not clip
    // its own draws, only its descendants).
    Container::buildDrawCommands(ctx);
    RenderScrollBars(ctx);
}

} // namespace components
} // namespace lupine
