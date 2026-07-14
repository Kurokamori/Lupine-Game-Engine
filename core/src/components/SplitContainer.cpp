#include "lupine/components/SplitContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/input/InputManager.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

SplitContainer::SplitContainer()
    : Container("SplitContainer")
    , m_Orientation(Orientation::Horizontal)
    , m_SplitOffset(0.0f)
    , m_SplitterWidth(6.0f)
    , m_Draggable(true)
{
}

SplitContainer::SplitContainer(const std::string& name)
    : Container(name)
    , m_Orientation(Orientation::Horizontal)
    , m_SplitOffset(0.0f)
    , m_SplitterWidth(6.0f)
    , m_Draggable(true)
{
}

SplitContainer::~SplitContainer() {
}

void SplitContainer::DefineProperties() {
    Container::DefineProperties();

    DefineProperty(PROPERTY_ENUM_GROUP(orientation, 0, "Split", Horizontal, Vertical));
    // 0 means "centered": the split starts halfway along the axis. Any other value is the
    // distance in pixels from the START edge (left when horizontal, TOP when vertical).
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(splitOffset, 0.0f, 0.0f, 10000.0f, 1.0f, "Split"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(splitterWidth, 6.0f, 1.0f, 64.0f, 1.0f, "Split"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(draggable, Bool, true, "Split"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(splitterColor, Color, Color(0.35f, 0.35f, 0.4f, 1.0f), "Split"));
}

void SplitContainer::DefineSignals() {
    RegisterSignal({"dragged",
                    {{"offset", core::PropertyValueType::Float}},
                    "Emitted while the splitter is being dragged."});
}

const std::vector<UIControl::ThemeBinding>& SplitContainer::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "backgroundColor", "background",     ThemeBinding::Kind::Color },
        { "borderColor",     "border_color",   ThemeBinding::Kind::Color },
        { "cornerRadius",    "corner_radius",  ThemeBinding::Kind::Constant },
        { "splitterColor",   "splitter_color", ThemeBinding::Kind::Color }
    };
    return kBindings;
}

void SplitContainer::OnAwake() {
    Container::OnAwake();
    SyncDerivedProperties();
}

void SplitContainer::SyncDerivedProperties() {
    SyncCachedEnum("orientation", m_Orientation);
    SyncCachedFloat("splitOffset", m_SplitOffset);
    SyncCachedFloat("splitterWidth", m_SplitterWidth);
    SyncCachedBool("draggable", m_Draggable);
}

void SplitContainer::SetOrientation(Orientation orientation) {
    m_Orientation = orientation;
    SetPropertyValue<int>("orientation", static_cast<int>(orientation));
    InvalidateLayout();
}

void SplitContainer::SetSplitOffset(float offset) {
    m_SplitOffset = std::max(0.0f, offset);
    SetPropertyValue<float>("splitOffset", m_SplitOffset);
    InvalidateLayout();
}

void SplitContainer::SetSplitterWidth(float width) {
    m_SplitterWidth = std::max(1.0f, width);
    SetPropertyValue<float>("splitterWidth", m_SplitterWidth);
    InvalidateLayout();
}

void SplitContainer::SetDraggable(bool draggable) {
    m_Draggable = draggable;
    SetPropertyValue<bool>("draggable", draggable);
}

Color SplitContainer::GetSplitterColor() const {
    return ResolveThemedColor("splitterColor", "splitter_color");
}

void SplitContainer::SetSplitterColor(const Color& color) {
    SetThemedProperty<Color>("splitterColor", color);
}

// ========================================
// Geometry
// ========================================

float SplitContainer::ResolveSplitOffset() const {
    const Rect content = GetContentRect();
    const bool vertical = (m_Orientation == Orientation::Vertical);

    const float axisLength = vertical ? content.size.y : content.size.x;
    const float usable = std::max(0.0f, axisLength - m_SplitterWidth);

    // A zero offset means "centered", which is the only sensible default for a fresh split.
    float offset = (m_SplitOffset > 0.0f) ? m_SplitOffset : (usable * 0.5f);

    // Neither pane may be squeezed below its own minimum.
    const std::vector<Node*> children = GetVisibleChildren();
    float firstMin = 0.0f;
    float secondMin = 0.0f;
    if (children.size() >= 1 && children[0]) {
        const Vec2 minSize = GetChildSize(children[0]);
        firstMin = vertical ? minSize.y : minSize.x;
    }
    if (children.size() >= 2 && children[1]) {
        const Vec2 minSize = GetChildSize(children[1]);
        secondMin = vertical ? minSize.y : minSize.x;
    }

    const float low = std::min(firstMin, usable);
    const float high = std::max(low, usable - secondMin);

    return std::clamp(offset, low, high);
}

Rect SplitContainer::GetSplitterRect() const {
    const Rect content = GetContentRect();
    const float offset = ResolveSplitOffset();

    if (m_Orientation == Orientation::Vertical) {
        // Vertical split: the panes stack, and the bar runs across. The offset is measured
        // DOWN from the top edge, which on this Y-up canvas means subtracting from it.
        const float top = content.GetTopEdge() - offset;
        return Rect::FromEdges(content.GetLeftEdge(), top,
                               content.GetRightEdge(), top - m_SplitterWidth);
    }

    const float left = content.GetLeftEdge() + offset;
    return Rect::FromEdges(left, content.GetTopEdge(),
                           left + m_SplitterWidth, content.GetBottomEdge());
}

// ========================================
// Layout
// ========================================

void SplitContainer::CalculateLayout() {
    if (!m_Owner) {
        return;
    }

    const std::vector<Node*> children = GetVisibleChildren();
    if (children.empty()) {
        return;
    }

    const Rect content = GetContentRect();
    const Rect splitter = GetSplitterRect();
    const bool vertical = (m_Orientation == Orientation::Vertical);

    // First pane: everything before the splitter.
    Rect first;
    if (vertical) {
        first = Rect::FromEdges(content.GetLeftEdge(), content.GetTopEdge(),
                                content.GetRightEdge(), splitter.GetTopEdge());
    } else {
        first = Rect::FromEdges(content.GetLeftEdge(), content.GetTopEdge(),
                                splitter.GetLeftEdge(), content.GetBottomEdge());
    }
    SetChildRect(children[0], first);

    // Second pane: everything after it. A SplitContainer has exactly two sides, so any
    // further children are collapsed rather than left drawn at a stale position.
    if (children.size() >= 2) {
        Rect second;
        if (vertical) {
            second = Rect::FromEdges(content.GetLeftEdge(), splitter.GetBottomEdge(),
                                     content.GetRightEdge(), content.GetBottomEdge());
        } else {
            second = Rect::FromEdges(splitter.GetRightEdge(), content.GetTopEdge(),
                                     content.GetRightEdge(), content.GetBottomEdge());
        }
        SetChildRect(children[1], second);
    }

    for (size_t i = 2; i < children.size(); ++i) {
        SetChildRect(children[i], Rect(content.GetTopLeft(), Vec2(0.0f, 0.0f)));
    }
}

Vec2 SplitContainer::GetMinimumSize() const {
    const Vec2 floorSize = Container::GetMinimumSize();

    const std::vector<Node*> children = GetVisibleChildren();
    const bool vertical = (m_Orientation == Orientation::Vertical);

    // Along the split axis the two panes' minimums add up, plus the bar between them. Across
    // it, both panes must fit, so the larger minimum wins.
    float along = 0.0f;
    float across = 0.0f;
    const size_t paneCount = std::min<size_t>(2, children.size());
    for (size_t i = 0; i < paneCount; ++i) {
        const Vec2 childSize = GetChildSize(children[i]);
        along += vertical ? childSize.y : childSize.x;
        across = std::max(across, vertical ? childSize.x : childSize.y);
    }
    if (paneCount == 2) {
        along += m_SplitterWidth;
    }

    const Vec2 content = vertical ? Vec2(across, along) : Vec2(along, across);

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    return Vec2(std::max(floorSize.x, content.x + padX),
                std::max(floorSize.y, content.y + padY));
}

// ========================================
// Interaction
// ========================================

void SplitContainer::OnInput(float deltaTime) {
    (void)deltaTime;
    if (!IsEnabled() || !m_Draggable) {
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();
    const Vec2 mouse = GetCanvasMousePosition();
    const bool vertical = (m_Orientation == Orientation::Vertical);

    if (inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left) && !m_Dragging) {
        if (GetSplitterRect().Contains(mouse)) {
            m_Dragging = true;
            m_DragStartMouse = vertical ? mouse.y : mouse.x;
            m_DragStartOffset = ResolveSplitOffset();
        }
        return;
    }

    if (!m_Dragging) {
        return;
    }

    if (!inputMgr.IsMouseButtonPressed(input::MouseButton::Left)) {
        m_Dragging = false;
        return;
    }

    // The offset grows toward the axis's END. On X that is +mouse.x; on Y the offset is
    // measured DOWNWARD from the top edge while the canvas is Y-up, so it grows as the mouse
    // moves toward -y.
    const float current = vertical ? mouse.y : mouse.x;
    const float delta = vertical ? (m_DragStartMouse - current) : (current - m_DragStartMouse);

    SetSplitOffset(m_DragStartOffset + delta);
    Emit("dragged", { GetSplitOffset() });
}

void SplitContainer::buildDrawCommands(RenderContext& ctx) {
    // The base paints the background/border; the splitter bar goes on top of it. Drawn before
    // descending into the panes would be fine too -- they do not overlap the bar -- but the
    // raised Z keeps it above any child that overflows its pane.
    Container::buildDrawCommands(ctx);

    if (!IsEnabled() || GetVisibleChildren().size() < 2) {
        return;
    }

    const Rect splitter = GetSplitterRect();
    if (splitter.size.x <= 0.0f || splitter.size.y <= 0.0f) {
        return;
    }

    const int savedZIndex = ctx.getZIndex();
    ctx.setZIndex(savedZIndex + 4096);
    ctx.drawRoundedRect(splitter.position, splitter.size, 0.0f, GetSplitterColor(), 0);
    ctx.setZIndex(savedZIndex);
}

} // namespace components
} // namespace lupine
