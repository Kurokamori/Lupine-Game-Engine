#include "lupine/components/VerticalContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

VerticalContainer::VerticalContainer()
    : Container("VerticalContainer")
    , m_HorizontalAlignment(HorizontalAlignment::Left)
    , m_VerticalAlignment(VerticalAlignment::Begin)
{
}

VerticalContainer::VerticalContainer(const std::string& name)
    : Container(name)
    , m_HorizontalAlignment(HorizontalAlignment::Left)
    , m_VerticalAlignment(VerticalAlignment::Begin)
{
}

VerticalContainer::~VerticalContainer() {
}

void VerticalContainer::DefineProperties() {
    // Call parent to define base container properties
    Container::DefineProperties();

    // Define VerticalContainer-specific properties
    DefineProperty(PROPERTY_ENUM_GROUP(horizontalAlignment, 0, "VerticalContainer", Left, Center, Right, Fill));
    DefineProperty(PROPERTY_ENUM_GROUP(verticalAlignment, 0, "VerticalContainer",
        Begin, Center, End, Fill, SpaceBetween, SpaceAround, SpaceEvenly));
}

void VerticalContainer::OnAwake() {
    Container::OnAwake();

    SyncDerivedProperties();
}

void VerticalContainer::OnReady() {
    Container::OnReady();

    // Force initial layout calculation
    InvalidateLayout();
}

void VerticalContainer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // The base runs SyncFromProperties() -> SyncDerivedProperties(), which refreshes every
    // cached member below -- so there is no parallel if/else chain here to fall out of sync
    // with (and, unlike that chain, it also runs on the editor's load path).
    Container::OnPropertyChanged(propertyName, newValue);
}

void VerticalContainer::SyncDerivedProperties() {
    SyncCachedEnum("horizontalAlignment", m_HorizontalAlignment);
    SyncCachedEnum("verticalAlignment", m_VerticalAlignment);
}

// ========================================
// VerticalContainer Specific Properties
// ========================================

VerticalContainer::HorizontalAlignment VerticalContainer::GetHorizontalAlignment() const {
    return m_HorizontalAlignment;
}

void VerticalContainer::SetHorizontalAlignment(HorizontalAlignment alignment) {
    m_HorizontalAlignment = alignment;
    SetPropertyValue<int>("horizontalAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

VerticalContainer::VerticalAlignment VerticalContainer::GetVerticalAlignment() const {
    return m_VerticalAlignment;
}

void VerticalContainer::SetVerticalAlignment(VerticalAlignment alignment) {
    m_VerticalAlignment = alignment;
    SetPropertyValue<int>("verticalAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

// ========================================
// Container Virtual Method Overrides
// ========================================

void VerticalContainer::CalculateLayout() {
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

    const float separation = GetSeparation();
    const float availableWidth = contentArea.size.x;
    const float availableHeight = contentArea.size.y;

    // Main axis (vertical): Godot-style distribution. Expanding children split the pool
    // of (their own desired heights + the free space) by stretch ratio, and every child is
    // clamped to its own min/max. The results are pure output -- nothing is written back
    // into the children's height properties, so they can shrink again next frame.
    const std::vector<float> heights = DistributeMainAxis(children, true, availableHeight, separation);

    float usedHeight = separation * static_cast<float>(children.size() - 1);
    for (float h : heights) {
        usedHeight += h;
    }

    // Free space only exists when nothing expanded to consume it; it is then distributed per
    // the container-wide vertical alignment -- which now includes the flexbox-style
    // space-between / space-around / space-evenly modes.
    const float freeSpace = std::max(0.0f, availableHeight - usedHeight);

    float leading = 0.0f;
    float extraGap = 0.0f;
    DistributeFreeSpace(static_cast<MainAxisDistribution>(m_VerticalAlignment),
                        freeSpace, children.size(), leading, extraGap);

    // Y-up canvas: the content area's TOP edge is its MAXIMUM y. Stack downward from it so
    // the first child is at the top.
    float currentTop = contentArea.GetTopEdge() - leading;

    const CrossAxisAlign fallback = ToCrossAxisAlign(m_HorizontalAlignment);

    for (size_t i = 0; i < children.size(); ++i) {
        Node* child = children[i];
        const float childHeight = heights[i];

        // Cross axis (horizontal): the child's own size flags decide, falling back to the
        // container-wide horizontal alignment for children with no UIControl.
        float childX = 0.0f;
        float childWidth = 0.0f;
        PlaceChildOnCrossAxis(child, false,
                              contentArea.GetLeftEdge(), availableWidth,
                              GetChildSize(child).x, fallback,
                              childX, childWidth);

        // The rect's position is its MINIMUM corner, so the child's bottom edge sits one
        // height below the running top edge.
        SetChildRect(child, Rect(childX, currentTop - childHeight, childWidth, childHeight));

        currentTop -= childHeight + separation + extraGap;
    }
}

Container::CrossAxisAlign VerticalContainer::ToCrossAxisAlign(HorizontalAlignment alignment) {
    switch (alignment) {
        case HorizontalAlignment::Center: return CrossAxisAlign::Center;
        case HorizontalAlignment::Right:  return CrossAxisAlign::End;
        case HorizontalAlignment::Fill:   return CrossAxisAlign::Fill;
        case HorizontalAlignment::Left:
        default:                          return CrossAxisAlign::Begin;
    }
}

Vec2 VerticalContainer::GetMinimumSize() const {
    // The authored customMinSize is a FLOOR on the whole container, not something padding
    // is added on top of -- adding padding to it (as this used to) inflates a container
    // that declared an exact minimum.
    const Vec2 floorSize = Container::GetMinimumSize();

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    Vec2 content(0.0f, 0.0f);
    std::vector<Node*> children = GetVisibleChildren();
    if (!children.empty()) {
        for (Node* child : children) {
            Vec2 childSize = GetChildSize(child);
            content.x = std::max(content.x, childSize.x);
            content.y += childSize.y;
        }
        content.y += GetSeparation() * static_cast<float>(children.size() - 1);
    }

    return Vec2(
        std::max(floorSize.x, content.x + padX),
        std::max(floorSize.y, content.y + padY)
    );
}

void VerticalContainer::OnLayoutInvalidated() {
    // Base implementation
    Container::OnLayoutInvalidated();
}

} // namespace components
} // namespace lupine

