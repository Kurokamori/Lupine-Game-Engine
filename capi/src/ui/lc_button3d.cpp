/**
 * @file lc_button3d.cpp
 * @brief Implementation of Button3D C API
 */

#include "ui/lc_button3d.h"
#include "../core/lc_internal.h"
#include <lupine/core/Node.hpp>

#include <lupine/components/Button3D.hpp>

#include <cstring>

using namespace lupine;
using namespace lupine::components;

namespace {

lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);
}

LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

Button3DState ToEngineState(LCButton3DState state) {
    return static_cast<Button3DState>(state);
}

LCButton3DState FromEngineState(Button3DState state) {
    return static_cast<LCButton3DState>(state);
}

Button3DStyleMode ToEngineStyleMode(LCButton3DStyleMode mode) {
    return static_cast<Button3DStyleMode>(mode);
}

LCButton3DStyleMode FromEngineStyleMode(Button3DStyleMode mode) {
    return static_cast<LCButton3DStyleMode>(mode);
}

Button3DScaleMode ToEngineScaleMode(LCButton3DScaleMode mode) {
    return static_cast<Button3DScaleMode>(mode);
}

LCButton3DScaleMode FromEngineScaleMode(Button3DScaleMode mode) {
    return static_cast<LCButton3DScaleMode>(mode);
}

Button3DBillboardMode ToEngineBillboardMode(LCButton3DBillboardMode mode) {
    return static_cast<Button3DBillboardMode>(mode);
}

LCButton3DBillboardMode FromEngineBillboardMode(Button3DBillboardMode mode) {
    return static_cast<LCButton3DBillboardMode>(mode);
}

Button3D* GetButton3D(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<Button3D*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * Button3D Component Functions
 * ============================================================================ */

LC_API LCResult lc_button3d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto comp = std::make_shared<Button3D>();
        nodePtr->AddComponent(comp);
        comp->DefineProperties();
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create Button3D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_button3d_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWidth = btn->GetWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_width(LCComponentHandle handle, float width) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetWidth(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outHeight = btn->GetHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_height(LCComponentHandle handle, float height) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetHeight(height);
    return LC_SUCCESS;
}

/* ----- 3D Rendering Properties ----- */

LC_API LCResult lc_button3d_get_billboard_mode(LCComponentHandle handle, LCButton3DBillboardMode* outMode) {
    if (!outMode) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outMode = FromEngineBillboardMode(btn->GetBillboardMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_billboard_mode(LCComponentHandle handle, LCButton3DBillboardMode mode) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBillboardMode(ToEngineBillboardMode(mode));
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_double_sided(LCComponentHandle handle, bool* outDoubleSided) {
    if (!outDoubleSided) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outDoubleSided = btn->GetDoubleSided();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_double_sided(LCComponentHandle handle, bool doubleSided) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetDoubleSided(doubleSided);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_cast_shadow(LCComponentHandle handle, bool* outCastShadow) {
    if (!outCastShadow) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outCastShadow = btn->GetCastShadow();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_cast_shadow(LCComponentHandle handle, bool castShadow) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetCastShadow(castShadow);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_receive_shadow(LCComponentHandle handle, bool* outReceiveShadow) {
    if (!outReceiveShadow) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outReceiveShadow = btn->GetReceiveShadow();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_receive_shadow(LCComponentHandle handle, bool receiveShadow) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetReceiveShadow(receiveShadow);
    return LC_SUCCESS;
}

/* ----- Layer Properties ----- */

LC_API LCResult lc_button3d_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outLayer = btn->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_layer(LCComponentHandle handle, int layer) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outOrder = btn->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_sorting_order(LCComponentHandle handle, int order) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetSortingOrder(order);
    return LC_SUCCESS;
}

/* ----- Button State ----- */

LC_API LCResult lc_button3d_get_current_state(LCComponentHandle handle, LCButton3DState* outState) {
    if (!outState) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outState = FromEngineState(btn->GetCurrentState());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_enabled(LCComponentHandle handle, bool enabled) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_is_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outEnabled = btn->IsButtonEnabled();
    return LC_SUCCESS;
}

/* ----- Style Mode ----- */

LC_API LCResult lc_button3d_get_style_mode(LCComponentHandle handle, LCButton3DStyleMode* outMode) {
    if (!outMode) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outMode = FromEngineStyleMode(btn->GetStyleMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_style_mode(LCComponentHandle handle, LCButton3DStyleMode mode) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStyleMode(ToEngineStyleMode(mode));
    return LC_SUCCESS;
}

/* ----- Scale Mode ----- */

LC_API LCResult lc_button3d_get_scale_mode(LCComponentHandle handle, LCButton3DScaleMode* outMode) {
    if (!outMode) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outMode = FromEngineScaleMode(btn->GetScaleMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_scale_mode(LCComponentHandle handle, LCButton3DScaleMode mode) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetScaleMode(ToEngineScaleMode(mode));
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_text_padding(LCComponentHandle handle, LCVec2* outPadding) {
    if (!outPadding) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outPadding = FromEngineVec2(btn->GetTextPadding());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_text_padding(LCComponentHandle handle, LCVec2 padding) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetTextPadding(ToEngineVec2(padding));
    return LC_SUCCESS;
}

/* ----- Background (Base Style) ----- */

LC_API LCResult lc_button3d_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetBackgroundColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_background_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBackgroundColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_opacity(LCComponentHandle handle, float* outOpacity) {
    if (!outOpacity) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outOpacity = btn->GetOpacity();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_opacity(LCComponentHandle handle, float opacity) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetOpacity(opacity);
    return LC_SUCCESS;
}

/* ----- Border ----- */

LC_API LCResult lc_button3d_get_border_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outEnabled = btn->GetBorderEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_border_enabled(LCComponentHandle handle, bool enabled) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetBorderColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_border_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_border_width_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outLinked = btn->GetBorderWidthLinked();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_border_width_linked(LCComponentHandle handle, bool linked) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderWidthLinked(linked);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_border_width_left(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWidth = btn->GetBorderWidthLeft();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_border_width_left(LCComponentHandle handle, float width) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderWidthLeft(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_border_width_right(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWidth = btn->GetBorderWidthRight();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_border_width_right(LCComponentHandle handle, float width) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderWidthRight(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_border_width_top(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWidth = btn->GetBorderWidthTop();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_border_width_top(LCComponentHandle handle, float width) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderWidthTop(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_border_width_bottom(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWidth = btn->GetBorderWidthBottom();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_border_width_bottom(LCComponentHandle handle, float width) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderWidthBottom(width);
    return LC_SUCCESS;
}

/* ----- Corner Radius ----- */

LC_API LCResult lc_button3d_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked) {
    if (!outLinked) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outLinked = btn->GetCornerRadiusLinked();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_corner_radius_linked(LCComponentHandle handle, bool linked) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetCornerRadiusLinked(linked);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_corner_radius_top_left(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outRadius = btn->GetCornerRadiusTopLeft();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_corner_radius_top_left(LCComponentHandle handle, float radius) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetCornerRadiusTopLeft(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_corner_radius_top_right(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outRadius = btn->GetCornerRadiusTopRight();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_corner_radius_top_right(LCComponentHandle handle, float radius) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetCornerRadiusTopRight(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_corner_radius_bottom_left(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outRadius = btn->GetCornerRadiusBottomLeft();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_corner_radius_bottom_left(LCComponentHandle handle, float radius) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetCornerRadiusBottomLeft(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_corner_radius_bottom_right(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outRadius = btn->GetCornerRadiusBottomRight();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_corner_radius_bottom_right(LCComponentHandle handle, float radius) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetCornerRadiusBottomRight(radius);
    return LC_SUCCESS;
}

/* ----- Text Properties ----- */

LC_API LCResult lc_button3d_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength) {
    if (!outText) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string text = btn->GetText();
    CopyStringToBuffer(outText, maxLength, text.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_text(LCComponentHandle handle, const char* text) {
    if (!text) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetText(text);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string path = btn->GetFontPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_font_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontPath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outSize = btn->GetFontSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_font_size(LCComponentHandle handle, float size) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontSize(size);
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetFontColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_font_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_get_word_wrap(LCComponentHandle handle, bool* outWrap) {
    if (!outWrap) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWrap = btn->GetWordWrap();
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_word_wrap(LCComponentHandle handle, bool wrap) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetWordWrap(wrap);
    return LC_SUCCESS;
}

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_button3d_get_state_modulation(LCComponentHandle handle, LCButton3DState state, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetStateModulation(ToEngineState(state)));
    return LC_SUCCESS;
}

LC_API LCResult lc_button3d_set_state_modulation(LCComponentHandle handle, LCButton3DState state, LCColor color) {
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStateModulation(ToEngineState(state), ToEngineColor(color));
    return LC_SUCCESS;
}

/* ----- Per-State Sounds ----- */

LC_API LCResult lc_button3d_set_state_sound_path(LCComponentHandle handle, LCButton3DState state, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetButton3D(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStateSoundPath(ToEngineState(state), path);
    return LC_SUCCESS;
}
