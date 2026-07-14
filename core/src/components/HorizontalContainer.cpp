#include "lupine/components/HorizontalContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

HorizontalContainer::HorizontalContainer()
    : Container("HorizontalContainer")
    , m_VerticalAlignment(VerticalAlignment::Top)
    , m_HorizontalAlignment(HorizontalAlignment::Begin)
{
}

HorizontalContainer::HorizontalContainer(const std::string& name)
    : Container(name)
    , m_VerticalAlignment(VerticalAlignment::Top)
    , m_HorizontalAlignment(HorizontalAlignment::Begin)
{
}

HorizontalContainer::~HorizontalContainer() {
}

void HorizontalContainer::DefineProperties() {
    // Call parent to define base container properties
    Container::DefineProperties();

    // Define HorizontalContainer-specific properties
    DefineProperty(PROPERTY_ENUM_GROUP(verticalAlignment, 0, "HorizontalContainer", Top, Center, Bottom, Fill));
    DefineProperty(PROPERTY_ENUM_GROUP(horizontalAlignment, 0, "HorizontalContainer",
        Begin, Center, End, Fill, SpaceBetween, SpaceAround, SpaceEvenly));
}

void HorizontalContainer::OnAwake() {
    Container::OnAwake();

    SyncDerivedProperties();
}

void HorizontalContainer::OnReady() {
    Container::OnReady();

    // Force initial layout calculation
    InvalidateLayout();
}

void HorizontalContainer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // The base runs SyncFromProperties() -> SyncDerivedProperties(), which refreshes every
    // cached member below -- so there is no parallel if/else chain here to fall out of sync
    // with (and, unlike that chain, it also runs on the editor's load path).
    Container::OnPropertyChanged(propertyName, newValue);
}

void HorizontalContainer::SyncDerivedProperties() {
    SyncCachedEnum("verticalAlignment", m_VerticalAlignment);
    SyncCachedEnum("horizontalAlignment", m_HorizontalAlignment);
}

// ========================================
// HorizontalContainer Specific Properties
// ========================================

HorizontalContainer::VerticalAlignment HorizontalContainer::GetVerticalAlignment() const {
    return m_VerticalAlignment;
}

void HorizontalContainer::SetVerticalAlignment(VerticalAlignment alignment) {
    m_VerticalAlignment = alignment;
    SetPropertyValue<int>("verticalAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

HorizontalContainer::HorizontalAlignment HorizontalContainer::GetHorizontalAlignment() const {
    return m_HorizontalAlignment;
}

void HorizontalContainer::SetHorizontalAlignment(HorizontalAlignment alignment) {
    m_HorizontalAlignment = alignment;
    SetPropertyValue<int>("horizontalAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

// ========================================
// Container Virtual Method Overrides
// ========================================

void HorizontalContainer::CalculateLayout() {
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

    // Main axis (horizontal): same Godot-style distribution as VerticalContainer, via the
    // shared helper. Results are output only -- never written back into child properties.
    const std::vector<float> widths = DistributeMainAxis(children, false, availableWidth, separation);

    float usedWidth = separation * static_cast<float>(children.size() - 1);
    for (float w : widths) {
        usedWidth += w;
    }

    // Free space exists only when nothing expanded to consume it. The old code computed
    // `availableWidth - totalChildWidth - extraSpace` where extraSpace was itself
    // max(0, availableWidth - totalChildWidth), so the bracket was identically ZERO and
    // Center/End were silent no-ops -- two 100px buttons in a 500px HBox stayed flush left.
    const float freeSpace = std::max(0.0f, availableWidth - usedWidth);

    // Distributed per the container-wide horizontal alignment, which now also offers the
    // flexbox-style space-between / space-around / space-evenly modes.
    float leading = 0.0f;
    float extraGap = 0.0f;
    DistributeFreeSpace(static_cast<MainAxisDistribution>(m_HorizontalAlignment),
                        freeSpace, children.size(), leading, extraGap);

    float currentX = contentArea.GetLeftEdge() + leading;

    const CrossAxisAlign fallback = ToCrossAxisAlign(m_VerticalAlignment);

    for (size_t i = 0; i < children.size(); ++i) {
        Node* child = children[i];
        const float childWidth = widths[i];

        // Cross axis (vertical). contentArea.position.y is the BOTTOM edge in this Y-up
        // canvas, so passing it as the axis origin makes CrossAxisAlign::Begin mean bottom
        // and ::End mean top -- which is what ToCrossAxisAlign maps Bottom/Top onto. The
        // old code treated position.y as the TOP edge, so `Top` placed children at the
        // bottom of the row and `Bottom` at the top.
        float childY = 0.0f;
        float childHeight = 0.0f;
        PlaceChildOnCrossAxis(child, true,
                              contentArea.GetBottomEdge(), availableHeight,
                              GetChildSize(child).y, fallback,
                              childY, childHeight);

        SetChildRect(child, Rect(currentX, childY, childWidth, childHeight));

        currentX += childWidth + separation + extraGap;
    }
}

Container::CrossAxisAlign HorizontalContainer::ToCrossAxisAlign(VerticalAlignment alignment) {
    // Begin/End are LOW/HIGH edges of the axis. On the Y-up canvas the low edge is the
    // BOTTOM, so Bottom -> Begin and Top -> End.
    switch (alignment) {
        case VerticalAlignment::Center: return CrossAxisAlign::Center;
        case VerticalAlignment::Top:    return CrossAxisAlign::End;
        case VerticalAlignment::Fill:   return CrossAxisAlign::Fill;
        case VerticalAlignment::Bottom:
        default:                        return CrossAxisAlign::Begin;
    }
}

Vec2 HorizontalContainer::GetMinimumSize() const {
    // customMinSize is a FLOOR on the whole container, not a value padding is added on
    // top of (see VerticalContainer::GetMinimumSize).
    const Vec2 floorSize = Container::GetMinimumSize();

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    Vec2 content(0.0f, 0.0f);
    std::vector<Node*> children = GetVisibleChildren();
    if (!children.empty()) {
        for (Node* child : children) {
            Vec2 childSize = GetChildSize(child);
            content.x += childSize.x;
            content.y = std::max(content.y, childSize.y);
        }
        content.x += GetSeparation() * static_cast<float>(children.size() - 1);
    }

    return Vec2(
        std::max(floorSize.x, content.x + padX),
        std::max(floorSize.y, content.y + padY)
    );
}

void HorizontalContainer::OnLayoutInvalidated() {
    // Base implementation
    Container::OnLayoutInvalidated();
}

} // namespace components
} // namespace lupine

