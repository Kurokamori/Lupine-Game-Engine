#include "lupine/components/LayoutSlot.hpp"
#include "lupine/core/Node.hpp"

namespace lupine {
namespace components {

using namespace core;

LayoutSlot::LayoutSlot()
    : Component("LayoutSlot")
{
}

LayoutSlot::LayoutSlot(const std::string& name)
    : Component(name)
{
}

LayoutSlot::~LayoutSlot() {
}

void LayoutSlot::DefineProperties() {
    DefineProperty(PROPERTY_ENUM_GROUP(dockSide, 0, "Dock", Left, Right, Top, Bottom, Fill));

    DefineProperty(PROPERTY_ENUM_GROUP(alignment, 4, "Stack",
                                       TopLeft, TopCenter, TopRight,
                                       CenterLeft, Center, CenterRight,
                                       BottomLeft, BottomCenter, BottomRight));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(zIndex, 0, -1000, 1000, 1, "Stack"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(matchParent, Bool, false, "Stack"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(ignoreLayout, Bool, false, "Layout"));
}

std::shared_ptr<LayoutSlot> LayoutSlot::For(core::Node* node) {
    if (!node) {
        return nullptr;
    }
    return node->GetComponent<LayoutSlot>();
}

LayoutSlot::DockSide LayoutSlot::GetDockSide() const {
    return static_cast<DockSide>(GetPropertyValue<int>("dockSide"));
}

void LayoutSlot::SetDockSide(DockSide side) {
    SetPropertyValue<int>("dockSide", static_cast<int>(side));
}

LayoutSlot::Alignment LayoutSlot::GetAlignment() const {
    return static_cast<Alignment>(GetPropertyValue<int>("alignment"));
}

void LayoutSlot::SetAlignment(Alignment alignment) {
    SetPropertyValue<int>("alignment", static_cast<int>(alignment));
}

int LayoutSlot::GetZIndex() const {
    return GetPropertyValue<int>("zIndex");
}

void LayoutSlot::SetZIndex(int zIndex) {
    SetPropertyValue<int>("zIndex", zIndex);
}

bool LayoutSlot::GetIgnoreLayout() const {
    return GetPropertyValue<bool>("ignoreLayout");
}

void LayoutSlot::SetIgnoreLayout(bool ignore) {
    SetPropertyValue<bool>("ignoreLayout", ignore);
}

bool LayoutSlot::GetMatchParent() const {
    return GetPropertyValue<bool>("matchParent");
}

void LayoutSlot::SetMatchParent(bool matchParent) {
    SetPropertyValue<bool>("matchParent", matchParent);
}

} // namespace components
} // namespace lupine
