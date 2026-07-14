#include "lupine/components/Spacer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Spacer::Spacer()
    : UIControl("Spacer")
{
}

Spacer::Spacer(const std::string& name)
    : UIControl(name)
{
}

Spacer::~Spacer() {
}

void Spacer::DefineProperties() {
    // Shared UIControl layout properties (width/height/anchors/size-flags/uiSpace).
    DefineUIControlProperties(50.0f, 50.0f, "uiSpace", "Size");

    // Size constraints come from UIControl (customMinSize / customMaxSize). Spacer used to
    // define its own minSize/maxSize here, which shadowed the base accessors the solver
    // reads -- two fields with the same name and meaning, only one of which was obeyed.

    // Layout properties (legacy expand flag; also honored via UIControl size flags)
    DefineProperty(PROPERTY_DEFAULT_GROUP(expand, Bool, false, "Layout"));
}

void Spacer::OnAwake() {
    // No initialization needed - spacer is purely for layout
}

// ========================================
// Size Management
// ========================================

Vec2 Spacer::GetContentMinSize() const {
    // A spacer has no content of its own, so it contributes nothing here. Its floor is
    // customMinSize, which UIControl::GetMinSize() already folds in -- returning it from
    // here as well would be harmless but redundant.
    return Vec2(0.0f, 0.0f);
}

bool Spacer::GetExpand() const {
    return GetPropertyValue<bool>("expand");
}

void Spacer::SetExpand(bool expand) {
    SetPropertyValue<bool>("expand", expand);
}

} // namespace components
} // namespace lupine

