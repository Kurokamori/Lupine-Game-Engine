#include "lupine/components/Stack.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Stack::Stack()
    : Container("Stack")
    , m_DefaultAlignment(Alignment::Center)
    , m_SortByZIndex(true)
{
}

Stack::Stack(const std::string& name)
    : Container(name)
    , m_DefaultAlignment(Alignment::Center)
    , m_SortByZIndex(true)
{
}

Stack::~Stack() {
}

void Stack::DefineProperties() {
    // Call parent to define base container properties
    Container::DefineProperties();

    // Define Stack-specific properties
    DefineProperty(PROPERTY_ENUM_GROUP(defaultAlignment, 4, "Stack", 
        TopLeft, TopCenter, TopRight, CenterLeft, Center, CenterRight, BottomLeft, BottomCenter, BottomRight));
    DefineProperty(PROPERTY_DEFAULT_GROUP(sortByZIndex, Bool, true, "Stack"));
}

void Stack::OnAwake() {
    Container::OnAwake();

    SyncDerivedProperties();
}

void Stack::OnReady() {
    Container::OnReady();

    // Force initial layout calculation
    InvalidateLayout();
}

void Stack::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // The base runs SyncFromProperties() -> SyncDerivedProperties(), which refreshes every
    // cached member below -- so there is no parallel if/else chain here to fall out of sync
    // with (and, unlike that chain, it also runs on the editor's load path).
    Container::OnPropertyChanged(propertyName, newValue);
}

void Stack::SyncDerivedProperties() {
    SyncCachedEnum("defaultAlignment", m_DefaultAlignment);
    SyncCachedBool("sortByZIndex", m_SortByZIndex);
}

// ========================================
// Stack Specific Properties
// ========================================

Stack::Alignment Stack::GetDefaultAlignment() const {
    return m_DefaultAlignment;
}

void Stack::SetDefaultAlignment(Alignment alignment) {
    m_DefaultAlignment = alignment;
    SetPropertyValue<int>("defaultAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

bool Stack::GetSortByZIndex() const {
    return m_SortByZIndex;
}

void Stack::SetSortByZIndex(bool sort) {
    m_SortByZIndex = sort;
    SetPropertyValue<bool>("sortByZIndex", sort);
    InvalidateLayout();
}

// ========================================
// Container Virtual Method Overrides
// ========================================

void Stack::CalculateLayout() {
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

    // Sort children by z-index when enabled. This used to sort by GetChildZIndex(), which
    // returned a hardcoded 0 for every child -- an all-equal key, so std::sort was a no-op
    // and sortByZIndex did nothing. The key now comes from each child's LayoutSlot.
    //
    // std::stable_sort, so children that share a z-index keep their scene-tree order
    // instead of being permuted arbitrarily.
    if (m_SortByZIndex) {
        std::stable_sort(children.begin(), children.end(), [this](Node* a, Node* b) {
            return GetChildZIndex(a) < GetChildZIndex(b);
        });
    }

    for (Node* child : children) {
        if (!child) continue;

        // Skip if child should ignore layout
        if (GetChildIgnoreLayout(child)) {
            continue;
        }

        // matchParent stretches the child across the whole content rect; otherwise it keeps
        // its desired size and is aligned within it.
        if (GetChildMatchParent(child)) {
            SetChildRect(child, contentArea);
            continue;
        }

        CrossAxisAlign horizontal = CrossAxisAlign::Begin;
        CrossAxisAlign vertical = CrossAxisAlign::Begin;
        SplitAlignment(GetChildAlignment(child), horizontal, vertical);

        SetChildRect(child, AlignRectIn(contentArea, GetChildSize(child), horizontal, vertical));
    }
}

void Stack::SplitAlignment(Alignment alignment,
                           CrossAxisAlign& outHorizontal,
                           CrossAxisAlign& outVertical) {
    // Y-up: the TOP edge is the axis's HIGH end, so Top -> End and Bottom -> Begin. The old
    // per-case position math treated contentArea.position.y as the top edge, so all six
    // Top/Bottom cases came out swapped.
    switch (alignment) {
        case Alignment::TopLeft:      outHorizontal = CrossAxisAlign::Begin;  outVertical = CrossAxisAlign::End;    break;
        case Alignment::TopCenter:    outHorizontal = CrossAxisAlign::Center; outVertical = CrossAxisAlign::End;    break;
        case Alignment::TopRight:     outHorizontal = CrossAxisAlign::End;    outVertical = CrossAxisAlign::End;    break;
        case Alignment::CenterLeft:   outHorizontal = CrossAxisAlign::Begin;  outVertical = CrossAxisAlign::Center; break;
        case Alignment::Center:       outHorizontal = CrossAxisAlign::Center; outVertical = CrossAxisAlign::Center; break;
        case Alignment::CenterRight:  outHorizontal = CrossAxisAlign::End;    outVertical = CrossAxisAlign::Center; break;
        case Alignment::BottomLeft:   outHorizontal = CrossAxisAlign::Begin;  outVertical = CrossAxisAlign::Begin;  break;
        case Alignment::BottomCenter: outHorizontal = CrossAxisAlign::Center; outVertical = CrossAxisAlign::Begin;  break;
        case Alignment::BottomRight:  outHorizontal = CrossAxisAlign::End;    outVertical = CrossAxisAlign::Begin;  break;
        default:                      outHorizontal = CrossAxisAlign::Center; outVertical = CrossAxisAlign::Center; break;
    }
}

math::Vec2 Stack::GetMinimumSize() const {
    // customMinSize is a floor on the whole container, not a value padding is added to.
    const Vec2 floorSize = Container::GetMinimumSize();

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    // A stack overlays its children, so its content is the largest of them.
    Vec2 content(0.0f, 0.0f);
    for (Node* child : GetVisibleChildren()) {
        if (GetChildIgnoreLayout(child)) {
            continue;
        }
        Vec2 childSize = GetChildSize(child);
        content.x = std::max(content.x, childSize.x);
        content.y = std::max(content.y, childSize.y);
    }

    return Vec2(
        std::max(floorSize.x, content.x + padX),
        std::max(floorSize.y, content.y + padY)
    );
}

void Stack::OnLayoutInvalidated() {
    // Base implementation
    Container::OnLayoutInvalidated();
}

// ========================================
// Internal Helper Methods
// ========================================

// Per-child settings live on the child as a LayoutSlot (the engine's attached-property
// mechanism). All four of these used to ignore the child and return a constant, which is
// why sortByZIndex sorted an all-zero key, matchParent was never true (making the
// match-parent path unreachable), and per-child alignment did not exist at all.
//
// A child with no LayoutSlot falls back to the container-wide default, so attaching one is
// optional and existing scenes are unaffected.

Stack::Alignment Stack::GetChildAlignment(Node* child) const {
    if (std::shared_ptr<LayoutSlot> slot = LayoutSlot::For(child)) {
        return ToAlignment(slot->GetAlignment());
    }
    return m_DefaultAlignment;
}

int Stack::GetChildZIndex(Node* child) const {
    if (std::shared_ptr<LayoutSlot> slot = LayoutSlot::For(child)) {
        return slot->GetZIndex();
    }
    return 0;
}

bool Stack::GetChildIgnoreLayout(Node* child) const {
    if (std::shared_ptr<LayoutSlot> slot = LayoutSlot::For(child)) {
        return slot->GetIgnoreLayout();
    }
    return false;
}

bool Stack::GetChildMatchParent(Node* child) const {
    if (std::shared_ptr<LayoutSlot> slot = LayoutSlot::For(child)) {
        return slot->GetMatchParent();
    }
    return false;
}

Stack::Alignment Stack::ToAlignment(LayoutSlot::Alignment alignment) {
    switch (alignment) {
        case LayoutSlot::Alignment::TopLeft:      return Alignment::TopLeft;
        case LayoutSlot::Alignment::TopCenter:    return Alignment::TopCenter;
        case LayoutSlot::Alignment::TopRight:     return Alignment::TopRight;
        case LayoutSlot::Alignment::CenterLeft:   return Alignment::CenterLeft;
        case LayoutSlot::Alignment::CenterRight:  return Alignment::CenterRight;
        case LayoutSlot::Alignment::BottomLeft:   return Alignment::BottomLeft;
        case LayoutSlot::Alignment::BottomCenter: return Alignment::BottomCenter;
        case LayoutSlot::Alignment::BottomRight:  return Alignment::BottomRight;
        case LayoutSlot::Alignment::Center:
        default:                                  return Alignment::Center;
    }
}

std::string Stack::AlignmentToString(Alignment alignment) {
    switch (alignment) {
        case Alignment::TopLeft: return "top_left";
        case Alignment::TopCenter: return "top_center";
        case Alignment::TopRight: return "top_right";
        case Alignment::CenterLeft: return "center_left";
        case Alignment::Center: return "center";
        case Alignment::CenterRight: return "center_right";
        case Alignment::BottomLeft: return "bottom_left";
        case Alignment::BottomCenter: return "bottom_center";
        case Alignment::BottomRight: return "bottom_right";
        default: return "center";
    }
}

Stack::Alignment Stack::StringToAlignment(const std::string& str) {
    if (str == "top_left") return Alignment::TopLeft;
    if (str == "top_center") return Alignment::TopCenter;
    if (str == "top_right") return Alignment::TopRight;
    if (str == "center_left") return Alignment::CenterLeft;
    if (str == "center") return Alignment::Center;
    if (str == "center_right") return Alignment::CenterRight;
    if (str == "bottom_left") return Alignment::BottomLeft;
    if (str == "bottom_center") return Alignment::BottomCenter;
    if (str == "bottom_right") return Alignment::BottomRight;
    return Alignment::Center;
}

} // namespace components
} // namespace lupine

