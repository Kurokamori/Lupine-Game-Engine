#include "lupine/components/AspectRatioContainer.hpp"
#include "lupine/core/Node.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

AspectRatioContainer::AspectRatioContainer()
    : Container("AspectRatioContainer")
    , m_Ratio(1.0f)
    , m_StretchMode(StretchMode::Fit)
    , m_HorizontalAlignment(CrossAxisAlign::Center)
    , m_VerticalAlignment(CrossAxisAlign::Center)
{
}

AspectRatioContainer::AspectRatioContainer(const std::string& name)
    : Container(name)
    , m_Ratio(1.0f)
    , m_StretchMode(StretchMode::Fit)
    , m_HorizontalAlignment(CrossAxisAlign::Center)
    , m_VerticalAlignment(CrossAxisAlign::Center)
{
}

AspectRatioContainer::~AspectRatioContainer() {
}

void AspectRatioContainer::DefineProperties() {
    Container::DefineProperties();

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(ratio, 1.0f, 0.01f, 100.0f, 0.01f, "Aspect"));
    DefineProperty(PROPERTY_ENUM_GROUP(stretchMode, 0, "Aspect",
        Fit, Cover, WidthControlsHeight, HeightControlsWidth));
    // The alignment enums are the container-wide CrossAxisAlign, named for the canvas axis:
    // Begin is the LOW edge (left on X, BOTTOM on Y) and End is the high edge.
    DefineProperty(PROPERTY_ENUM_GROUP(horizontalAlignment, 1, "Aspect", Begin, Center, End, Fill));
    DefineProperty(PROPERTY_ENUM_GROUP(verticalAlignment, 1, "Aspect", Begin, Center, End, Fill));
}

void AspectRatioContainer::OnAwake() {
    Container::OnAwake();
    SyncDerivedProperties();
}

void AspectRatioContainer::SyncDerivedProperties() {
    SyncCachedFloat("ratio", m_Ratio);
    SyncCachedEnum("stretchMode", m_StretchMode);
    SyncCachedEnum("horizontalAlignment", m_HorizontalAlignment);
    SyncCachedEnum("verticalAlignment", m_VerticalAlignment);

    if (!(m_Ratio > 0.0f)) {
        m_Ratio = 1.0f;
    }
}

void AspectRatioContainer::SetRatio(float ratio) {
    m_Ratio = (ratio > 0.0f) ? ratio : 1.0f;
    SetPropertyValue<float>("ratio", m_Ratio);
    InvalidateLayout();
}

void AspectRatioContainer::SetStretchMode(StretchMode mode) {
    m_StretchMode = mode;
    SetPropertyValue<int>("stretchMode", static_cast<int>(mode));
    InvalidateLayout();
}

void AspectRatioContainer::SetHorizontalAlignment(CrossAxisAlign alignment) {
    m_HorizontalAlignment = alignment;
    SetPropertyValue<int>("horizontalAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

void AspectRatioContainer::SetVerticalAlignment(CrossAxisAlign alignment) {
    m_VerticalAlignment = alignment;
    SetPropertyValue<int>("verticalAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

Vec2 AspectRatioContainer::ResolveChildSize(const Vec2& available) const {
    const float ratio = (m_Ratio > 0.0f) ? m_Ratio : 1.0f;

    switch (m_StretchMode) {
        case StretchMode::WidthControlsHeight:
            return Vec2(available.x, available.x / ratio);

        case StretchMode::HeightControlsWidth:
            return Vec2(available.y * ratio, available.y);

        case StretchMode::Cover: {
            // The larger of the two candidate boxes: the child covers the area and overflows
            // on whichever axis does not fit (the container clips it when clipChildren is on).
            const Vec2 byWidth(available.x, available.x / ratio);
            const Vec2 byHeight(available.y * ratio, available.y);
            return (byWidth.y >= available.y) ? byWidth : byHeight;
        }

        case StretchMode::Fit:
        default: {
            // The largest ratio-correct box that fits ENTIRELY inside the area.
            const Vec2 byWidth(available.x, available.x / ratio);
            if (byWidth.y <= available.y) {
                return byWidth;
            }
            return Vec2(available.y * ratio, available.y);
        }
    }
}

void AspectRatioContainer::CalculateLayout() {
    if (!m_Owner) {
        return;
    }

    const std::vector<Node*> children = GetVisibleChildren();
    if (children.empty()) {
        return;
    }

    const Rect content = GetContentRect();
    if (content.size.x <= 0.0f || content.size.y <= 0.0f) {
        return;
    }

    const Vec2 childSize = ResolveChildSize(content.size);

    // Every child gets the same ratio-correct box, aligned in the content rect. AlignRectIn
    // is canvas-oriented, so a vertical End really is the top edge.
    //
    // Fill is deliberately NOT honored on either axis here: filling would mean ignoring the
    // ratio, which is the entire point of this container.
    for (Node* child : children) {
        const CrossAxisAlign horizontal =
            (m_HorizontalAlignment == CrossAxisAlign::Fill) ? CrossAxisAlign::Center
                                                            : m_HorizontalAlignment;
        const CrossAxisAlign vertical =
            (m_VerticalAlignment == CrossAxisAlign::Fill) ? CrossAxisAlign::Center
                                                          : m_VerticalAlignment;

        // AlignRectIn clamps the box to the area, which would silently defeat Cover. Place a
        // Cover box by hand so it is allowed to overflow.
        if (m_StretchMode == StretchMode::Cover) {
            const float x = content.GetLeftEdge() + (content.size.x - childSize.x) * 0.5f;
            const float y = content.GetBottomEdge() + (content.size.y - childSize.y) * 0.5f;
            SetChildRect(child, Rect(x, y, childSize.x, childSize.y));
            continue;
        }

        SetChildRect(child, AlignRectIn(content, childSize, horizontal, vertical));
    }
}

Vec2 AspectRatioContainer::CalculateChildrenBounds() const {
    const std::vector<Node*> children = GetVisibleChildren();
    if (children.empty()) {
        return Vec2(0.0f, 0.0f);
    }

    // The smallest ratio-correct box that contains every child's own minimum.
    const float ratio = (m_Ratio > 0.0f) ? m_Ratio : 1.0f;

    Vec2 needed(0.0f, 0.0f);
    for (Node* child : children) {
        const Vec2 childSize = GetChildSize(child);
        needed.x = std::max(needed.x, childSize.x);
        needed.y = std::max(needed.y, childSize.y);
    }

    // Grow whichever axis is short of the ratio, never shrink the other -- shrinking would
    // push a child below its own minimum.
    return Vec2(std::max(needed.x, needed.y * ratio),
                std::max(needed.y, needed.x / ratio));
}

Vec2 AspectRatioContainer::GetMinimumSize() const {
    const Vec2 floorSize = Container::GetMinimumSize();
    const Vec2 content = CalculateChildrenBounds();

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    return Vec2(std::max(floorSize.x, content.x + padX),
                std::max(floorSize.y, content.y + padY));
}

} // namespace components
} // namespace lupine
