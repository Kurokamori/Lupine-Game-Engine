/**
 * @file lc_uicontrol.cpp
 * @brief Implementation of Lupine Engine C API - UIControl (shared UI layout base)
 */

#include "ui/lc_uicontrol.h"
#include "../core/lc_internal.h"

#include <lupine/components/UIControl.hpp>
#include <lupine/core/Node.hpp>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetUIControlError(LCResult code, const char* message) {
    ::SetError(code, message);
}

UIControl* GetUIControl(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<UIControl*>(component.get());
}

LCVec2 FromEngineVec2(const Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

Vec2 ToEngineVec2(const LCVec2& vec) {
    return Vec2(vec.x, vec.y);
}

} // anonymous namespace

/* ----- Size ----- */

LC_API LCResult lc_uicontrol_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetUIControlError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = uc->GetWidth(); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_width(LCComponentHandle handle, float width) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetWidth(width); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetUIControlError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = uc->GetHeight(); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_height(LCComponentHandle handle, float height) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetHeight(height); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetUIControlError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(uc->GetSize()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_size(LCComponentHandle handle, LCVec2 size) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetSize(ToEngineVec2(size)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_custom_min_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetUIControlError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(uc->GetCustomMinSize()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetCustomMinSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_custom_min_size(LCComponentHandle handle, LCVec2 size) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetCustomMinSize(ToEngineVec2(size)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetCustomMinSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_custom_max_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetUIControlError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(uc->GetCustomMaxSize()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetCustomMaxSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_custom_max_size(LCComponentHandle handle, LCVec2 size) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetCustomMaxSize(ToEngineVec2(size)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetCustomMaxSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_min_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetUIControlError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(uc->GetMinSize()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetMinSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_max_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) { SetUIControlError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = FromEngineVec2(uc->GetMaxSize()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetMaxSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Anchors / offsets ----- */

LC_API LCResult lc_uicontrol_get_anchor_min(LCComponentHandle handle, LCVec2* outAnchor) {
    if (!outAnchor) { SetUIControlError(LC_ERROR_NULL_POINTER, "outAnchor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAnchor = FromEngineVec2(uc->GetAnchorMin()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetAnchorMin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_anchor_min(LCComponentHandle handle, LCVec2 anchor) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetAnchorMin(ToEngineVec2(anchor)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetAnchorMin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_anchor_max(LCComponentHandle handle, LCVec2* outAnchor) {
    if (!outAnchor) { SetUIControlError(LC_ERROR_NULL_POINTER, "outAnchor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outAnchor = FromEngineVec2(uc->GetAnchorMax()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetAnchorMax failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_anchor_max(LCComponentHandle handle, LCVec2 anchor) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetAnchorMax(ToEngineVec2(anchor)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetAnchorMax failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_offset_min(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) { SetUIControlError(LC_ERROR_NULL_POINTER, "outOffset is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOffset = FromEngineVec2(uc->GetOffsetMin()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetOffsetMin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_offset_min(LCComponentHandle handle, LCVec2 offset) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetOffsetMin(ToEngineVec2(offset)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetOffsetMin failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_offset_max(LCComponentHandle handle, LCVec2* outOffset) {
    if (!outOffset) { SetUIControlError(LC_ERROR_NULL_POINTER, "outOffset is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOffset = FromEngineVec2(uc->GetOffsetMax()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetOffsetMax failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_offset_max(LCComponentHandle handle, LCVec2 offset) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetOffsetMax(ToEngineVec2(offset)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetOffsetMax failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_anchor_preset(LCComponentHandle handle, int* outPreset) {
    if (!outPreset) { SetUIControlError(LC_ERROR_NULL_POINTER, "outPreset is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPreset = static_cast<int>(uc->GetAnchorPreset()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetAnchorPreset failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_apply_anchor_preset(LCComponentHandle handle, int preset) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->ApplyAnchorPreset(static_cast<AnchorPreset>(preset), false); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "ApplyAnchorPreset failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Grow direction / layout mode ----- */

/* lc_uicontrol_{get,set}_pivot were removed: the underlying "pivot" property was never read
 * by the layout solver (every control is center-pivoted by WriteRectToNode), so the pair
 * wrote to and read back a field with no effect on anything.
 * See UIControl::DefineUIControlProperties. */

LC_API LCResult lc_uicontrol_get_layout_mode(LCComponentHandle handle, int* outMode) {
    if (!outMode) { SetUIControlError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMode = static_cast<int>(uc->GetLayoutMode()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetLayoutMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_layout_mode(LCComponentHandle handle, int mode) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetLayoutMode(static_cast<LayoutMode>(mode)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetLayoutMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_grow_direction_h(LCComponentHandle handle, int* outDir) {
    if (!outDir) { SetUIControlError(LC_ERROR_NULL_POINTER, "outDir is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDir = static_cast<int>(uc->GetGrowDirectionH()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetGrowDirectionH failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_grow_direction_h(LCComponentHandle handle, int dir) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetGrowDirectionH(static_cast<GrowDirection>(dir)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetGrowDirectionH failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_grow_direction_v(LCComponentHandle handle, int* outDir) {
    if (!outDir) { SetUIControlError(LC_ERROR_NULL_POINTER, "outDir is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDir = static_cast<int>(uc->GetGrowDirectionV()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetGrowDirectionV failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_grow_direction_v(LCComponentHandle handle, int dir) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetGrowDirectionV(static_cast<GrowDirection>(dir)); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetGrowDirectionV failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Container size flags ----- */

LC_API LCResult lc_uicontrol_get_size_flags_horizontal(LCComponentHandle handle, int* outFlags) {
    if (!outFlags) { SetUIControlError(LC_ERROR_NULL_POINTER, "outFlags is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outFlags = uc->GetSizeFlagsHorizontal(); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetSizeFlagsHorizontal failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_size_flags_horizontal(LCComponentHandle handle, int flags) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetSizeFlagsHorizontal(flags); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetSizeFlagsHorizontal failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_size_flags_vertical(LCComponentHandle handle, int* outFlags) {
    if (!outFlags) { SetUIControlError(LC_ERROR_NULL_POINTER, "outFlags is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outFlags = uc->GetSizeFlagsVertical(); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetSizeFlagsVertical failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_size_flags_vertical(LCComponentHandle handle, int flags) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetSizeFlagsVertical(flags); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetSizeFlagsVertical failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_size_flags_stretch_ratio(LCComponentHandle handle, float* outRatio) {
    if (!outRatio) { SetUIControlError(LC_ERROR_NULL_POINTER, "outRatio is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRatio = uc->GetSizeFlagsStretchRatio(); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetSizeFlagsStretchRatio failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_size_flags_stretch_ratio(LCComponentHandle handle, float ratio) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetSizeFlagsStretchRatio(ratio); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetSizeFlagsStretchRatio failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- UI space / resolved rect ----- */

LC_API LCResult lc_uicontrol_get_ui_space(LCComponentHandle handle, bool* outUISpace) {
    if (!outUISpace) { SetUIControlError(LC_ERROR_NULL_POINTER, "outUISpace is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outUISpace = uc->GetUISpace(); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_ui_space(LCComponentHandle handle, bool uiSpace) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { uc->SetUISpace(uiSpace); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetUISpace failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_get_rect(LCComponentHandle handle, LCRect* outRect) {
    if (!outRect) { SetUIControlError(LC_ERROR_NULL_POINTER, "outRect is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        Rect rect = uc->GetRect();
        outRect->position = FromEngineVec2(rect.position);
        outRect->size = FromEngineVec2(rect.size);
        return LC_SUCCESS;
    }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetRect failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Mouse filter / mouse state ----- */

LC_API LCResult lc_uicontrol_get_mouse_filter(LCComponentHandle handle, int* outFilter) {
    if (!outFilter) { SetUIControlError(LC_ERROR_NULL_POINTER, "outFilter is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outFilter = static_cast<int>(uc->GetMouseFilter()); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "GetMouseFilter failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_set_mouse_filter(LCComponentHandle handle, int filter) {
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    if (filter < static_cast<int>(MouseFilter::Stop) || filter > static_cast<int>(MouseFilter::Ignore)) {
        SetUIControlError(LC_ERROR_INVALID_PARAMETER, "filter is not a valid LCMouseFilter");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        if (!uc->GetPropertyRegistry().HasProperty("mouseFilter")) {
            SetUIControlError(LC_ERROR_INVALID_PARAMETER, "Component does not expose a mouse filter");
            return LC_ERROR_INVALID_PARAMETER;
        }
        uc->SetMouseFilter(static_cast<MouseFilter>(filter));
        return LC_SUCCESS;
    }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "SetMouseFilter failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_is_mouse_hovered(LCComponentHandle handle, bool* outHovered) {
    if (!outHovered) { SetUIControlError(LC_ERROR_NULL_POINTER, "outHovered is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHovered = uc->IsMouseHovered(); return LC_SUCCESS; }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "IsMouseHovered failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_uicontrol_is_mouse_pressed(LCComponentHandle handle, int button, bool* outPressed) {
    if (!outPressed) { SetUIControlError(LC_ERROR_NULL_POINTER, "outPressed is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* uc = GetUIControl(handle);
    if (!uc) { SetUIControlError(LC_ERROR_INVALID_HANDLE, "Invalid UIControl handle"); return LC_ERROR_INVALID_HANDLE; }
    if (button < static_cast<int>(lupine::input::MouseButton::Left) ||
        button > static_cast<int>(lupine::input::MouseButton::Middle)) {
        SetUIControlError(LC_ERROR_INVALID_PARAMETER, "button is not a valid LCUIMouseButton");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        *outPressed = uc->IsMousePressed(static_cast<lupine::input::MouseButton>(button));
        return LC_SUCCESS;
    }
    catch (...) { SetUIControlError(LC_ERROR_INTERNAL_ERROR, "IsMousePressed failed"); return LC_ERROR_INTERNAL_ERROR; }
}
