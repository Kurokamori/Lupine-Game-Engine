/**
 * @file lc_progress_bar.cpp
 * @brief Implementation of Lupine Engine C API - ProgressBar and ProgressBar3D Components
 */

#include "ui/lc_progress_bar.h"
#include "../core/lc_internal.h"

#include <lupine/components/ProgressBar.hpp>
#include <lupine/components/ProgressBar3D.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetProgressBarError(LCResult code, const char* message) {
    ::SetError(code, message);
}

ProgressBar* GetProgressBar(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<ProgressBar*>(component.get());
}

ProgressBar3D* GetProgressBar3D(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<ProgressBar3D*>(component.get());
}

LCColor FromEngineColor(const Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

Color ToEngineColor(const LCColor& color) {
    return Color(color.r, color.g, color.b, color.a);
}

} // anonymous namespace

/* ============================================================================
 * ProgressBar Component
 * ============================================================================ */

LC_API LCResult lc_progress_bar_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetProgressBarError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<ProgressBar>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "Failed to create ProgressBar component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Value Properties ----- */

LC_API LCResult lc_progress_bar_get_min_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = pb->GetMinValue(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetMinValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_min_value(LCComponentHandle handle, float value) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetMinValue(value); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetMinValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_max_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = pb->GetMaxValue(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetMaxValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_max_value(LCComponentHandle handle, float value) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetMaxValue(value); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetMaxValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = pb->GetValue(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_value(LCComponentHandle handle, float value) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetValue(value); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_step(LCComponentHandle handle, float* outStep) {
    if (!outStep) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outStep is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outStep = pb->GetStep(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetStep failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_step(LCComponentHandle handle, float step) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetStep(step); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetStep failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Smooth Properties ----- */

LC_API LCResult lc_progress_bar_get_smooth(LCComponentHandle handle, bool* outSmooth) {
    if (!outSmooth) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outSmooth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSmooth = pb->GetSmooth(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetSmooth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_smooth(LCComponentHandle handle, bool smooth) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetSmooth(smooth); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetSmooth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_smooth_speed(LCComponentHandle handle, float* outSpeed) {
    if (!outSpeed) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outSpeed is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSpeed = pb->GetSmoothSpeed(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetSmoothSpeed failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_smooth_speed(LCComponentHandle handle, float speed) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetSmoothSpeed(speed); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetSmoothSpeed failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_progress_bar_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = pb->GetWidth(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_width(LCComponentHandle handle, float width) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetWidth(width); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = pb->GetHeight(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_height(LCComponentHandle handle, float height) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetHeight(height); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Orientation & Fill Direction ----- */

LC_API LCResult lc_progress_bar_get_orientation(LCComponentHandle handle, LCProgressBarOrientation* outOrientation) {
    if (!outOrientation) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outOrientation is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOrientation = static_cast<LCProgressBarOrientation>(pb->GetOrientation()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetOrientation failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_orientation(LCComponentHandle handle, LCProgressBarOrientation orientation) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetOrientation(static_cast<int>(orientation)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetOrientation failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_fill_direction(LCComponentHandle handle, LCProgressBarFillDirection* outDirection) {
    if (!outDirection) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outDirection is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDirection = static_cast<LCProgressBarFillDirection>(pb->GetFillDirection()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetFillDirection failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_fill_direction(LCComponentHandle handle, LCProgressBarFillDirection direction) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetFillDirection(static_cast<int>(direction)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetFillDirection failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Background Texture ----- */

LC_API LCResult lc_progress_bar_get_background_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = pb->GetBackgroundTexturePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_background_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetProgressBarError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBackgroundTexturePath(path); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(pb->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Fill Texture ----- */

LC_API LCResult lc_progress_bar_get_fill_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = pb->GetFillTexturePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetFillTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_fill_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetProgressBarError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetFillTexturePath(path); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetFillTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_fill_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(pb->GetFillColor()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetFillColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_fill_color(LCComponentHandle handle, LCColor color) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetFillColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetFillColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Texture ----- */

LC_API LCResult lc_progress_bar_get_border_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = pb->GetBorderTexturePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBorderTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_border_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetProgressBarError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBorderTexturePath(path); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(pb->GetBorderColor()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_border_color(LCComponentHandle handle, LCColor color) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBorderColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Value Display ----- */

LC_API LCResult lc_progress_bar_get_show_value(LCComponentHandle handle, bool* outShow) {
    if (!outShow) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outShow is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outShow = pb->GetShowValue(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetShowValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_show_value(LCComponentHandle handle, bool show) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetShowValue(show); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetShowValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_value_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = pb->GetValueFontPath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetValueFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_value_font_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetProgressBarError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetValueFontPath(path); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetValueFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_value_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = pb->GetValueFontSize(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetValueFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_value_font_size(LCComponentHandle handle, float size) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetValueFontSize(size); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetValueFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_value_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(pb->GetValueColor()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetValueColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_value_color(LCComponentHandle handle, LCColor color) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetValueColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetValueColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Radius ----- */

LC_API LCResult lc_progress_bar_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = pb->IsCornerRadiusLinked(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "IsCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_corner_radius_linked(LCComponentHandle handle, bool linked) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetCornerRadiusLinked(linked); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_corner_radius_all(LCComponentHandle handle, float radius) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetCornerRadiusAll(radius); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_corner_radius_individual(LCComponentHandle handle, int index, float radius) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    if (index < 0 || index > 3) { SetProgressBarError(LC_ERROR_INVALID_PARAMETER, "Index must be 0-3"); return LC_ERROR_INVALID_PARAMETER; }
    try { pb->SetCornerRadiusIndividual(static_cast<size_t>(index), radius); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusIndividual failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_corner_radius(LCComponentHandle handle, int index, float* outRadius) {
    if (!outRadius) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    if (index < 0 || index > 3) { SetProgressBarError(LC_ERROR_INVALID_PARAMETER, "Index must be 0-3"); return LC_ERROR_INVALID_PARAMETER; }
    try {
        const auto& cornerRadius = pb->GetCornerRadius();
        *outRadius = cornerRadius.Get(static_cast<size_t>(index));
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadius failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Width ----- */

LC_API LCResult lc_progress_bar_get_border_width_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = pb->IsBorderWidthLinked(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "IsBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_border_width_linked(LCComponentHandle handle, bool linked) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBorderWidthLinked(linked); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_border_width_all(LCComponentHandle handle, float width) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBorderWidthAll(width); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_set_border_width_individual(LCComponentHandle handle, int index, float width) {
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    if (index < 0 || index > 3) { SetProgressBarError(LC_ERROR_INVALID_PARAMETER, "Index must be 0-3"); return LC_ERROR_INVALID_PARAMETER; }
    try { pb->SetBorderWidthIndividual(static_cast<size_t>(index), width); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthIndividual failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar_get_border_width(LCComponentHandle handle, int index, float* outWidth) {
    if (!outWidth) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar handle"); return LC_ERROR_INVALID_HANDLE; }
    if (index < 0 || index > 3) { SetProgressBarError(LC_ERROR_INVALID_PARAMETER, "Index must be 0-3"); return LC_ERROR_INVALID_PARAMETER; }
    try {
        const auto& borderWidth = pb->GetBorderWidth();
        *outWidth = borderWidth.Get(static_cast<size_t>(index));
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ============================================================================
 * ProgressBar3D Component
 * ============================================================================ */

LC_API LCResult lc_progress_bar3d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetProgressBarError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<ProgressBar3D>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "Failed to create ProgressBar3D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Value Properties ----- */

LC_API LCResult lc_progress_bar3d_get_min_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = pb->GetMinValue(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetMinValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_min_value(LCComponentHandle handle, float value) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetMinValue(value); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetMinValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_max_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = pb->GetMaxValue(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetMaxValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_max_value(LCComponentHandle handle, float value) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetMaxValue(value); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetMaxValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = pb->GetValue(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_value(LCComponentHandle handle, float value) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetValue(value); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_step(LCComponentHandle handle, float* outStep) {
    if (!outStep) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outStep is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outStep = pb->GetStep(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetStep failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_step(LCComponentHandle handle, float step) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetStep(step); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetStep failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Smooth Properties ----- */

LC_API LCResult lc_progress_bar3d_get_smooth(LCComponentHandle handle, bool* outSmooth) {
    if (!outSmooth) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outSmooth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSmooth = pb->GetSmooth(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetSmooth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_smooth(LCComponentHandle handle, bool smooth) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetSmooth(smooth); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetSmooth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_smooth_speed(LCComponentHandle handle, float* outSpeed) {
    if (!outSpeed) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outSpeed is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSpeed = pb->GetSmoothSpeed(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetSmoothSpeed failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_smooth_speed(LCComponentHandle handle, float speed) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetSmoothSpeed(speed); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetSmoothSpeed failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_progress_bar3d_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = pb->GetWidth(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_width(LCComponentHandle handle, float width) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetWidth(width); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = pb->GetHeight(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_height(LCComponentHandle handle, float height) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetHeight(height); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Orientation & Fill Direction ----- */

LC_API LCResult lc_progress_bar3d_get_orientation(LCComponentHandle handle, LCProgressBarOrientation* outOrientation) {
    if (!outOrientation) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outOrientation is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOrientation = static_cast<LCProgressBarOrientation>(pb->GetOrientation()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetOrientation failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_orientation(LCComponentHandle handle, LCProgressBarOrientation orientation) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetOrientation(static_cast<int>(orientation)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetOrientation failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_fill_direction(LCComponentHandle handle, LCProgressBarFillDirection* outDirection) {
    if (!outDirection) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outDirection is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDirection = static_cast<LCProgressBarFillDirection>(pb->GetFillDirection()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetFillDirection failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_fill_direction(LCComponentHandle handle, LCProgressBarFillDirection direction) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetFillDirection(static_cast<int>(direction)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetFillDirection failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- 3D Rendering Properties ----- */

LC_API LCResult lc_progress_bar3d_get_billboard_mode(LCComponentHandle handle, LCProgressBar3DBillboardMode* outMode) {
    if (!outMode) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outMode is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        auto mode = pb->GetBillboardMode();
        *outMode = static_cast<LCProgressBar3DBillboardMode>(static_cast<int>(mode));
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBillboardMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_billboard_mode(LCComponentHandle handle, LCProgressBar3DBillboardMode mode) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        pb->SetBillboardMode(static_cast<ProgressBar3DBillboardMode>(static_cast<int>(mode)));
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBillboardMode failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_double_sided(LCComponentHandle handle, bool* outDoubleSided) {
    if (!outDoubleSided) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outDoubleSided is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDoubleSided = pb->GetDoubleSided(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetDoubleSided failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_double_sided(LCComponentHandle handle, bool doubleSided) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetDoubleSided(doubleSided); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetDoubleSided failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_cast_shadow(LCComponentHandle handle, bool* outCastShadow) {
    if (!outCastShadow) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outCastShadow is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCastShadow = pb->GetCastShadow(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetCastShadow failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_cast_shadow(LCComponentHandle handle, bool castShadow) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetCastShadow(castShadow); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetCastShadow failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_receive_shadow(LCComponentHandle handle, bool* outReceiveShadow) {
    if (!outReceiveShadow) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outReceiveShadow is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outReceiveShadow = pb->GetReceiveShadow(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetReceiveShadow failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_receive_shadow(LCComponentHandle handle, bool receiveShadow) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetReceiveShadow(receiveShadow); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetReceiveShadow failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Background Texture ----- */

LC_API LCResult lc_progress_bar3d_get_background_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = pb->GetBackgroundTexturePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_background_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetProgressBarError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBackgroundTexturePath(path); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(pb->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Fill Texture ----- */

LC_API LCResult lc_progress_bar3d_get_fill_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = pb->GetFillTexturePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetFillTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_fill_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetProgressBarError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetFillTexturePath(path); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetFillTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_fill_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(pb->GetFillColor()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetFillColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_fill_color(LCComponentHandle handle, LCColor color) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetFillColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetFillColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Texture ----- */

LC_API LCResult lc_progress_bar3d_get_border_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = pb->GetBorderTexturePath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBorderTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_border_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetProgressBarError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBorderTexturePath(path); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderTexturePath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(pb->GetBorderColor()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_border_color(LCComponentHandle handle, LCColor color) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBorderColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Value Display ----- */

LC_API LCResult lc_progress_bar3d_get_show_value(LCComponentHandle handle, bool* outShow) {
    if (!outShow) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outShow is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outShow = pb->GetShowValue(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetShowValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_show_value(LCComponentHandle handle, bool show) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetShowValue(show); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetShowValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_value_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outPath is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        const std::string& path = pb->GetValueFontPath();
        CopyStringToBuffer(outPath, maxLength, path.c_str());
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetValueFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_value_font_path(LCComponentHandle handle, const char* path) {
    if (!path) { SetProgressBarError(LC_ERROR_NULL_POINTER, "path is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetValueFontPath(path); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetValueFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_value_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = pb->GetValueFontSize(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetValueFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_value_font_size(LCComponentHandle handle, float size) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetValueFontSize(size); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetValueFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_value_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(pb->GetValueColor()); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetValueColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_value_color(LCComponentHandle handle, LCColor color) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetValueColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetValueColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Corner Radius ----- */

LC_API LCResult lc_progress_bar3d_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = pb->IsCornerRadiusLinked(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "IsCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_corner_radius_linked(LCComponentHandle handle, bool linked) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetCornerRadiusLinked(linked); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_corner_radius_all(LCComponentHandle handle, float radius) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetCornerRadiusAll(radius); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_corner_radius_individual(LCComponentHandle handle, int index, float radius) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    if (index < 0 || index > 3) { SetProgressBarError(LC_ERROR_INVALID_PARAMETER, "Index must be 0-3"); return LC_ERROR_INVALID_PARAMETER; }
    try { pb->SetCornerRadiusIndividual(static_cast<size_t>(index), radius); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetCornerRadiusIndividual failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_corner_radius(LCComponentHandle handle, int index, float* outRadius) {
    if (!outRadius) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outRadius is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    if (index < 0 || index > 3) { SetProgressBarError(LC_ERROR_INVALID_PARAMETER, "Index must be 0-3"); return LC_ERROR_INVALID_PARAMETER; }
    try {
        const auto& cornerRadius = pb->GetCornerRadius();
        *outRadius = cornerRadius.Get(static_cast<size_t>(index));
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetCornerRadius failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Border Width ----- */

LC_API LCResult lc_progress_bar3d_get_border_width_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outLinked is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outLinked = pb->IsBorderWidthLinked(); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "IsBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_border_width_linked(LCComponentHandle handle, bool linked) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBorderWidthLinked(linked); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthLinked failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_border_width_all(LCComponentHandle handle, float width) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    try { pb->SetBorderWidthAll(width); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_set_border_width_individual(LCComponentHandle handle, int index, float width) {
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    if (index < 0 || index > 3) { SetProgressBarError(LC_ERROR_INVALID_PARAMETER, "Index must be 0-3"); return LC_ERROR_INVALID_PARAMETER; }
    try { pb->SetBorderWidthIndividual(static_cast<size_t>(index), width); return LC_SUCCESS; }
    catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "SetBorderWidthIndividual failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_progress_bar3d_get_border_width(LCComponentHandle handle, int index, float* outWidth) {
    if (!outWidth) { SetProgressBarError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* pb = GetProgressBar3D(handle);
    if (!pb) { SetProgressBarError(LC_ERROR_INVALID_HANDLE, "Invalid ProgressBar3D handle"); return LC_ERROR_INVALID_HANDLE; }
    if (index < 0 || index > 3) { SetProgressBarError(LC_ERROR_INVALID_PARAMETER, "Index must be 0-3"); return LC_ERROR_INVALID_PARAMETER; }
    try {
        const auto& borderWidth = pb->GetBorderWidth();
        *outWidth = borderWidth.Get(static_cast<size_t>(index));
        return LC_SUCCESS;
    } catch (...) { SetProgressBarError(LC_ERROR_INTERNAL_ERROR, "GetBorderWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}
