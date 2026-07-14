#include "lupine/components/DockContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

DockContainer::DockContainer()
    : Container("DockContainer")
    , m_DockSpacing(0.0f)
    , m_DefaultDockSide(DockSide::Center)
{
}

DockContainer::DockContainer(const std::string& name)
    : Container(name)
    , m_DockSpacing(0.0f)
    , m_DefaultDockSide(DockSide::Center)
{
}

DockContainer::~DockContainer() {
}

void DockContainer::DefineProperties() {
    // Call parent to define base container properties
    Container::DefineProperties();

    // Define DockContainer-specific properties
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(dockSpacing, 0.0f, 0.0f, 100.0f, 1.0f, "DockContainer"));
    DefineProperty(PROPERTY_ENUM_GROUP(defaultDockSide, 4, "DockContainer", Top, Bottom, Left, Right, Center));
}

void DockContainer::OnAwake() {
    Container::OnAwake();

    SyncDerivedProperties();
}

void DockContainer::OnReady() {
    Container::OnReady();

    // Force initial layout calculation
    InvalidateLayout();
}

void DockContainer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // The base runs SyncFromProperties() -> SyncDerivedProperties(), which refreshes every
    // cached member below -- so there is no parallel if/else chain here to fall out of sync
    // with (and, unlike that chain, it also runs on the editor's load path).
    Container::OnPropertyChanged(propertyName, newValue);
}

void DockContainer::SyncDerivedProperties() {
    SyncCachedFloat("dockSpacing", m_DockSpacing);
    SyncCachedEnum("defaultDockSide", m_DefaultDockSide);
}

// ========================================
// DockContainer Specific Properties
// ========================================

float DockContainer::GetDockSpacing() const {
    return m_DockSpacing;
}

void DockContainer::SetDockSpacing(float spacing) {
    m_DockSpacing = spacing;
    SetPropertyValue<float>("dockSpacing", spacing);
    InvalidateLayout();
}

DockContainer::DockSide DockContainer::GetDefaultDockSide() const {
    return m_DefaultDockSide;
}

void DockContainer::SetDefaultDockSide(DockSide side) {
    m_DefaultDockSide = side;
    SetPropertyValue<int>("defaultDockSide", static_cast<int>(side));
    InvalidateLayout();
}

// ========================================
// Container Virtual Method Overrides
// ========================================

void DockContainer::CalculateLayout() {
    if (!m_Owner) {
        return;
    }

    // Get the content area (container size minus padding)
    Rect contentArea = GetContentRect();

    // Get all visible children
    std::vector<Node*> children = GetVisibleChildren();

    if (children.empty()) {
        return;
    }

    // Available rect starts as the full content area and shrinks as each child claims an
    // edge. The LAST Fill child consumes whatever is left.
    Rect availableRect = contentArea;

    for (Node* child : children) {
        if (!child) continue;

        DockChild(child, GetChildDockSide(child), availableRect);
    }
}

math::Vec2 DockContainer::GetMinimumSize() const {
    // The old implementation ignored the children entirely and returned minSize + padding,
    // so a DockContainer nested in a VBox was allocated nothing and spilled out of it.
    //
    // A dock layout's true minimum: the left/right children sit side by side (their widths
    // add, their heights are independent), the top/bottom children stack (their heights
    // add), and the Fill children must fit in whatever is left. Summing the docked extents
    // per axis and taking the max against the fill/cross contributions gives a minimum that
    // is always large enough to place every child.
    const Vec2 floorSize = Container::GetMinimumSize();

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    float horizontalRun = 0.0f;   // widths of Left/Right children, which consume x
    float verticalRun = 0.0f;     // heights of Top/Bottom children, which consume y
    float maxCrossHeight = 0.0f;  // tallest Left/Right child
    float maxCrossWidth = 0.0f;   // widest Top/Bottom child
    Vec2 fillSize(0.0f, 0.0f);

    std::vector<Node*> children = GetVisibleChildren();
    int dockedCount = 0;

    for (Node* child : children) {
        const Vec2 childSize = GetChildSize(child);

        switch (GetChildDockSide(child)) {
            case DockSide::Left:
            case DockSide::Right:
                horizontalRun += childSize.x;
                maxCrossHeight = std::max(maxCrossHeight, childSize.y);
                ++dockedCount;
                break;
            case DockSide::Top:
            case DockSide::Bottom:
                verticalRun += childSize.y;
                maxCrossWidth = std::max(maxCrossWidth, childSize.x);
                ++dockedCount;
                break;
            case DockSide::Center:
            default:
                fillSize.x = std::max(fillSize.x, childSize.x);
                fillSize.y = std::max(fillSize.y, childSize.y);
                break;
        }
    }

    const float spacingRun = m_DockSpacing * static_cast<float>(std::max(0, dockedCount));

    const Vec2 content(
        std::max(horizontalRun + fillSize.x, maxCrossWidth) + spacingRun,
        std::max(verticalRun + fillSize.y, maxCrossHeight) + spacingRun
    );

    return Vec2(
        std::max(floorSize.x, content.x + padX),
        std::max(floorSize.y, content.y + padY)
    );
}

void DockContainer::OnLayoutInvalidated() {
    // Base implementation
    Container::OnLayoutInvalidated();
}

// ========================================
// Internal Helper Methods
// ========================================

DockContainer::DockSide DockContainer::GetChildDockSide(Node* child) const {
    // Per-child dock side, stored on the child as a LayoutSlot (the engine's attached-
    // property mechanism). This used to ignore the child and return m_DefaultDockSide for
    // everyone, so EVERY child docked to the same side -- and with the default side being
    // the fill side, the first child consumed the entire rect and every subsequent child
    // was arranged into a 0x0 rect at the collapsed point.
    //
    // Children with no LayoutSlot fall back to the container-wide default, so existing
    // scenes keep working and a slot only ever needs adding where a child differs.
    if (std::shared_ptr<LayoutSlot> slot = LayoutSlot::For(child)) {
        return ToDockSide(slot->GetDockSide());
    }
    return m_DefaultDockSide;
}

DockContainer::DockSide DockContainer::ToDockSide(LayoutSlot::DockSide side) {
    switch (side) {
        case LayoutSlot::DockSide::Left:   return DockSide::Left;
        case LayoutSlot::DockSide::Right:  return DockSide::Right;
        case LayoutSlot::DockSide::Top:    return DockSide::Top;
        case LayoutSlot::DockSide::Bottom: return DockSide::Bottom;
        case LayoutSlot::DockSide::Fill:
        default:                           return DockSide::Center;
    }
}

void DockContainer::DockChild(Node* child, DockSide side, Rect& availableRect) {
    if (!child) {
        return;
    }

    // Nothing left to dock into. Without this the available rect went NEGATIVE on
    // overflow and negative widths/heights were written straight into child properties.
    if (availableRect.size.x <= 0.0f || availableRect.size.y <= 0.0f) {
        SetChildRect(child, Rect(availableRect.position, Vec2(0.0f, 0.0f)));
        return;
    }

    const Vec2 childSize = GetChildSize(child);

    // A docked child never takes more than the rect it is docking into.
    const float bandWidth = std::min(childSize.x, availableRect.size.x);
    const float bandHeight = std::min(childSize.y, availableRect.size.y);

    Rect childRect;

    switch (side) {
        case DockSide::Top:
            // Y-up: the TOP edge is the MAXIMUM y, so the band hangs BELOW the top edge and
            // the remaining rect keeps its min-y corner and simply gets shorter. The old
            // code docked along availableRect.position.y -- the BOTTOM edge -- and shrank
            // from the bottom, so DockSide::Top docked to the bottom.
            childRect = Rect::FromEdges(availableRect.GetLeftEdge(),
                                        availableRect.GetTopEdge(),
                                        availableRect.GetRightEdge(),
                                        availableRect.GetTopEdge() - bandHeight);
            availableRect = Rect::FromEdges(availableRect.GetLeftEdge(),
                                            availableRect.GetTopEdge() - bandHeight - m_DockSpacing,
                                            availableRect.GetRightEdge(),
                                            availableRect.GetBottomEdge());
            break;

        case DockSide::Bottom:
            childRect = Rect::FromEdges(availableRect.GetLeftEdge(),
                                        availableRect.GetBottomEdge() + bandHeight,
                                        availableRect.GetRightEdge(),
                                        availableRect.GetBottomEdge());
            availableRect = Rect::FromEdges(availableRect.GetLeftEdge(),
                                            availableRect.GetTopEdge(),
                                            availableRect.GetRightEdge(),
                                            availableRect.GetBottomEdge() + bandHeight + m_DockSpacing);
            break;

        case DockSide::Left:
            childRect = Rect::FromEdges(availableRect.GetLeftEdge(),
                                        availableRect.GetTopEdge(),
                                        availableRect.GetLeftEdge() + bandWidth,
                                        availableRect.GetBottomEdge());
            availableRect = Rect::FromEdges(availableRect.GetLeftEdge() + bandWidth + m_DockSpacing,
                                            availableRect.GetTopEdge(),
                                            availableRect.GetRightEdge(),
                                            availableRect.GetBottomEdge());
            break;

        case DockSide::Right:
            childRect = Rect::FromEdges(availableRect.GetRightEdge() - bandWidth,
                                        availableRect.GetTopEdge(),
                                        availableRect.GetRightEdge(),
                                        availableRect.GetBottomEdge());
            availableRect = Rect::FromEdges(availableRect.GetLeftEdge(),
                                            availableRect.GetTopEdge(),
                                            availableRect.GetRightEdge() - bandWidth - m_DockSpacing,
                                            availableRect.GetBottomEdge());
            break;

        case DockSide::Center:
        default:
            // Fill: take everything that is left.
            childRect = availableRect;
            availableRect = Rect(availableRect.position, Vec2(0.0f, 0.0f));
            break;
    }

    // Rect::FromEdges clamps an inverted rect to zero extent, so the available rect can no
    // longer go negative no matter how far the children overflow.
    SetChildRect(child, childRect);
}

std::string DockContainer::DockSideToString(DockSide side) {
    switch (side) {
        case DockSide::Top: return "top";
        case DockSide::Bottom: return "bottom";
        case DockSide::Left: return "left";
        case DockSide::Right: return "right";
        case DockSide::Center: return "center";
        default: return "center";
    }
}

DockContainer::DockSide DockContainer::StringToDockSide(const std::string& str) {
    if (str == "top") return DockSide::Top;
    if (str == "bottom") return DockSide::Bottom;
    if (str == "left") return DockSide::Left;
    if (str == "right") return DockSide::Right;
    if (str == "center") return DockSide::Center;
    return DockSide::Center;
}

} // namespace components
} // namespace lupine

