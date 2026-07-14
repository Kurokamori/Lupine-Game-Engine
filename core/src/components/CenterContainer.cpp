#include "lupine/components/CenterContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

CenterContainer::CenterContainer()
    : Container("CenterContainer")
    , m_AutoFitChild(false)
    , m_MaintainAspectRatio(true)
    , m_StackChildren(false)
{
}

CenterContainer::CenterContainer(const std::string& name)
    : Container(name)
    , m_AutoFitChild(false)
    , m_MaintainAspectRatio(true)
    , m_StackChildren(false)
{
}

CenterContainer::~CenterContainer() {
}

void CenterContainer::DefineProperties() {
    // Call parent to define base container properties
    Container::DefineProperties();

    // Define CenterContainer-specific properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoFitChild, Bool, false, "CenterContainer"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(maintainAspectRatio, Bool, true, "CenterContainer"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(stackChildren, Bool, false, "CenterContainer"));
}

void CenterContainer::OnAwake() {
    Container::OnAwake();

    SyncDerivedProperties();
}

void CenterContainer::OnReady() {
    Container::OnReady();

    // Force initial layout calculation
    InvalidateLayout();
}

void CenterContainer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // The base runs SyncFromProperties() -> SyncDerivedProperties(), which refreshes every
    // cached member below -- so there is no parallel if/else chain here to fall out of sync
    // with (and, unlike that chain, it also runs on the editor's load path).
    Container::OnPropertyChanged(propertyName, newValue);
}

void CenterContainer::SyncDerivedProperties() {
    SyncCachedBool("autoFitChild", m_AutoFitChild);
    SyncCachedBool("maintainAspectRatio", m_MaintainAspectRatio);
    SyncCachedBool("stackChildren", m_StackChildren);
}

// ========================================
// CenterContainer Specific Properties
// ========================================

bool CenterContainer::GetAutoFitChild() const {
    return m_AutoFitChild;
}

void CenterContainer::SetAutoFitChild(bool autoFit) {
    m_AutoFitChild = autoFit;
    SetPropertyValue<bool>("autoFitChild", autoFit);
    InvalidateLayout();
}

bool CenterContainer::GetMaintainAspectRatio() const {
    return m_MaintainAspectRatio;
}

void CenterContainer::SetMaintainAspectRatio(bool maintain) {
    m_MaintainAspectRatio = maintain;
    SetPropertyValue<bool>("maintainAspectRatio", maintain);
    InvalidateLayout();
}

bool CenterContainer::GetStackChildren() const {
    return m_StackChildren;
}

void CenterContainer::SetStackChildren(bool stack) {
    m_StackChildren = stack;
    SetPropertyValue<bool>("stackChildren", stack);
    InvalidateLayout();
}

// ========================================
// Container Virtual Method Overrides
// ========================================

void CenterContainer::CalculateLayout() {
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

    // Stack mode centers every child (all overlapping); otherwise only the first.
    const size_t count = m_StackChildren ? children.size() : 1u;

    for (size_t i = 0; i < count; ++i) {
        Node* child = children[i];
        if (!child) {
            continue;
        }

        const Vec2 fitted = FitChildSize(child, contentArea.size, m_AutoFitChild, m_MaintainAspectRatio);
        SetChildRect(child, AlignRectIn(contentArea, fitted,
                                        CrossAxisAlign::Center, CrossAxisAlign::Center));
    }
}

Vec2 CenterContainer::GetMinimumSize() const {
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

void CenterContainer::OnLayoutInvalidated() {
    // Base implementation
    Container::OnLayoutInvalidated();
}

} // namespace components
} // namespace lupine
