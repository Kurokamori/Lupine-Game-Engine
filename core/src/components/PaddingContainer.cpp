#include "lupine/components/PaddingContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

PaddingContainer::PaddingContainer()
    : Container("PaddingContainer")
    , m_AutoFitChildren(true)
    , m_MaintainAspectRatio(true)
    , m_ChildAlignment(Alignment::TopLeft)
{
}

PaddingContainer::PaddingContainer(const std::string& name)
    : Container(name)
    , m_AutoFitChildren(true)
    , m_MaintainAspectRatio(true)
    , m_ChildAlignment(Alignment::TopLeft)
{
}

PaddingContainer::~PaddingContainer() {
}

void PaddingContainer::DefineProperties() {
    // Call parent to define base container properties
    Container::DefineProperties();

    // Define PaddingContainer-specific properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoFitChildren, Bool, true, "PaddingContainer"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(maintainAspectRatio, Bool, true, "PaddingContainer"));
    DefineProperty(PROPERTY_ENUM_GROUP(childAlignment, 0, "PaddingContainer",
        TopLeft, TopCenter, TopRight,
        CenterLeft, Center, CenterRight,
        BottomLeft, BottomCenter, BottomRight
    ));
}

void PaddingContainer::OnAwake() {
    Container::OnAwake();

    SyncDerivedProperties();
}

void PaddingContainer::OnReady() {
    Container::OnReady();

    // Force initial layout calculation
    InvalidateLayout();
}

void PaddingContainer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // The base runs SyncFromProperties() -> SyncDerivedProperties(), which refreshes every
    // cached member below -- so there is no parallel if/else chain here to fall out of sync
    // with (and, unlike that chain, it also runs on the editor's load path).
    Container::OnPropertyChanged(propertyName, newValue);
}

void PaddingContainer::SyncDerivedProperties() {
    SyncCachedBool("autoFitChildren", m_AutoFitChildren);
    SyncCachedBool("maintainAspectRatio", m_MaintainAspectRatio);
    SyncCachedEnum("childAlignment", m_ChildAlignment);
}

// ========================================
// Padding Container Specific Properties
// ========================================

bool PaddingContainer::GetAutoFitChildren() const {
    return m_AutoFitChildren;
}

void PaddingContainer::SetAutoFitChildren(bool autoFit) {
    m_AutoFitChildren = autoFit;
    SetPropertyValue<bool>("autoFitChildren", autoFit);
    InvalidateLayout();
}

bool PaddingContainer::GetMaintainAspectRatio() const {
    return m_MaintainAspectRatio;
}

void PaddingContainer::SetMaintainAspectRatio(bool maintain) {
    m_MaintainAspectRatio = maintain;
    SetPropertyValue<bool>("maintainAspectRatio", maintain);
    InvalidateLayout();
}

PaddingContainer::Alignment PaddingContainer::GetChildAlignment() const {
    return m_ChildAlignment;
}

void PaddingContainer::SetChildAlignment(Alignment alignment) {
    m_ChildAlignment = alignment;
    SetPropertyValue<int>("childAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

// ========================================
// Container Virtual Method Overrides
// ========================================

void PaddingContainer::CalculateLayout() {
    if (!m_Owner) {
        return;
    }

    // Get the padded content area
    Rect paddedArea = GetContentRect();

    // Get all visible children
    std::vector<Node*> children = GetVisibleChildren();

    if (children.empty()) {
        return;
    }

    CrossAxisAlign horizontal = CrossAxisAlign::Begin;
    CrossAxisAlign vertical = CrossAxisAlign::Begin;
    SplitAlignment(m_ChildAlignment, horizontal, vertical);

    for (Node* child : children) {
        if (!child) {
            continue;
        }

        // Fit is recomputed from the child's DESIRED size every pass, so it is idempotent.
        // The result is handed to SetChildRect as pure layout output; it is never written
        // back into the child's width/height, so shrinking the container lets the child
        // shrink again instead of staying inflated forever.
        const Vec2 fitted = FitChildSize(child, paddedArea.size, m_AutoFitChildren, m_MaintainAspectRatio);
        SetChildRect(child, AlignRectIn(paddedArea, fitted, horizontal, vertical));
    }
}

void PaddingContainer::SplitAlignment(Alignment alignment,
                                      CrossAxisAlign& outHorizontal,
                                      CrossAxisAlign& outVertical) {
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
        default:                      outHorizontal = CrossAxisAlign::Begin;  outVertical = CrossAxisAlign::End;    break;
    }
    // NOTE the vertical mapping: on this Y-up canvas the TOP edge is the axis's HIGH end,
    // so Top maps to CrossAxisAlign::End and Bottom to ::Begin. Getting this backwards is
    // exactly what made TopLeft land in the bottom-left corner.
}

Vec2 PaddingContainer::GetMinimumSize() const {
    // customMinSize is a floor on the whole container, not a value padding is added to.
    const Vec2 floorSize = Container::GetMinimumSize();

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    Vec2 content(0.0f, 0.0f);
    for (Node* child : GetVisibleChildren()) {
        Vec2 childSize = GetChildSize(child);
        content.x = std::max(content.x, childSize.x);
        content.y = std::max(content.y, childSize.y);
    }

    return Vec2(
        std::max(floorSize.x, content.x + padX),
        std::max(floorSize.y, content.y + padY)
    );
}

void PaddingContainer::OnLayoutInvalidated() {
    // Base implementation
    Container::OnLayoutInvalidated();
}

} // namespace components
} // namespace lupine
