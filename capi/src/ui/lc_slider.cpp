/**
 * @file lc_slider.cpp
 * @brief Implementation of Lupine Engine C API - Slider component
 */

#include "ui/lc_slider.h"
#include "../core/lc_internal.h"

#include <lupine/components/Slider.hpp>
#include <lupine/core/Node.hpp>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetSliderError(LCResult code, const char* message) {
    ::SetError(code, message);
}

Slider* GetSlider(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<Slider*>(component.get());
}

LCColor FromEngineColor(const Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

Color ToEngineColor(const LCColor& color) {
    return Color(color.r, color.g, color.b, color.a);
}

} // anonymous namespace

LC_API LCResult lc_slider_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetSliderError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        auto component = std::make_shared<Slider>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetSliderError(LC_ERROR_INTERNAL_ERROR, "Failed to create Slider component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Value ----- */

LC_API LCResult lc_slider_get_min_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetSliderError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = s->GetMinValue(); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetMinValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_min_value(LCComponentHandle handle, float value) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetMinValue(value); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetMinValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_max_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetSliderError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = s->GetMaxValue(); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetMaxValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_max_value(LCComponentHandle handle, float value) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetMaxValue(value); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetMaxValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetSliderError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = s->GetValue(); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_value(LCComponentHandle handle, float value) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetValue(value); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_step(LCComponentHandle handle, float* outStep) {
    if (!outStep) { SetSliderError(LC_ERROR_NULL_POINTER, "outStep is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outStep = s->GetStep(); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetStep failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_step(LCComponentHandle handle, float step) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetStep(step); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetStep failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_ratio(LCComponentHandle handle, float* outRatio) {
    if (!outRatio) { SetSliderError(LC_ERROR_NULL_POINTER, "outRatio is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRatio = s->GetRatio(); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetRatio failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_orientation(LCComponentHandle handle, LCSliderOrientation* outOrientation) {
    if (!outOrientation) { SetSliderError(LC_ERROR_NULL_POINTER, "outOrientation is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOrientation = static_cast<LCSliderOrientation>(s->GetOrientation()); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetOrientation failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_orientation(LCComponentHandle handle, LCSliderOrientation orientation) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetOrientation(static_cast<Slider::Orientation>(orientation)); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetOrientation failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_editable(LCComponentHandle handle, bool* outEditable) {
    if (!outEditable) { SetSliderError(LC_ERROR_NULL_POINTER, "outEditable is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEditable = s->GetEditable(); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetEditable failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_editable(LCComponentHandle handle, bool editable) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetEditable(editable); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetEditable failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Appearance ----- */

LC_API LCResult lc_slider_get_track_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetSliderError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(s->GetTrackColor()); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetTrackColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_track_color(LCComponentHandle handle, LCColor color) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetTrackColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetTrackColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_fill_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetSliderError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(s->GetFillColor()); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetFillColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_fill_color(LCComponentHandle handle, LCColor color) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetFillColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetFillColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_handle_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetSliderError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(s->GetHandleColor()); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetHandleColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_handle_color(LCComponentHandle handle, LCColor color) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetHandleColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetHandleColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_track_thickness(LCComponentHandle handle, float* outThickness) {
    if (!outThickness) { SetSliderError(LC_ERROR_NULL_POINTER, "outThickness is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outThickness = s->GetTrackThickness(); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetTrackThickness failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_track_thickness(LCComponentHandle handle, float thickness) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetTrackThickness(thickness); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetTrackThickness failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_get_handle_radius(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) { SetSliderError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outRadius = s->GetHandleRadius(); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "GetHandleRadius failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_slider_set_handle_radius(LCComponentHandle handle, float radius) {
    auto* s = GetSlider(handle);
    if (!s) { SetSliderError(LC_ERROR_INVALID_HANDLE, "Invalid Slider handle"); return LC_ERROR_INVALID_HANDLE; }
    try { s->SetHandleRadius(radius); return LC_SUCCESS; }
    catch (...) { SetSliderError(LC_ERROR_INTERNAL_ERROR, "SetHandleRadius failed"); return LC_ERROR_INTERNAL_ERROR; }
}
