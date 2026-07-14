/**
 * @file lc_toggle_button.cpp
 * @brief Implementation of ToggleButton C API
 */

#include "ui/lc_toggle_button.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>

#include <lupine/components/ToggleButton.hpp>

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

ToggleButtonState ToEngineState(LCToggleButtonState state) {
    return static_cast<ToggleButtonState>(state);
}

LCToggleButtonState FromEngineState(ToggleButtonState state) {
    return static_cast<LCToggleButtonState>(state);
}

ToggleButtonStyleMode ToEngineStyleMode(LCToggleButtonStyleMode mode) {
    return static_cast<ToggleButtonStyleMode>(mode);
}

LCToggleButtonStyleMode FromEngineStyleMode(ToggleButtonStyleMode mode) {
    return static_cast<LCToggleButtonStyleMode>(mode);
}

ToggleButton* GetToggleButton(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<ToggleButton*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * ToggleButton Component Functions
 * ============================================================================ */

LC_API LCResult lc_toggle_button_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<ToggleButton>();
        nodePtr->AddComponent(comp);
        comp->DefineProperties();
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create ToggleButton component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_toggle_button_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWidth = btn->GetWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_width(LCComponentHandle handle, float width) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetWidth(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outHeight = btn->GetHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_height(LCComponentHandle handle, float height) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetHeight(height);
    return LC_SUCCESS;
}

/* ----- Layer Properties ----- */

LC_API LCResult lc_toggle_button_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outLayer = btn->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_layer(LCComponentHandle handle, int layer) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outOrder = btn->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_sorting_order(LCComponentHandle handle, int order) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetSortingOrder(order);
    return LC_SUCCESS;
}

/* ----- UI Space ----- */

LC_API LCResult lc_toggle_button_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace) {
    if (!outUseUISpace) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outUseUISpace = btn->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_use_ui_space(LCComponentHandle handle, bool useUISpace) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetUseUISpace(useUISpace);
    return LC_SUCCESS;
}

/* ----- Button State ----- */

LC_API LCResult lc_toggle_button_set_enabled(LCComponentHandle handle, bool enabled) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_is_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outEnabled = btn->IsButtonEnabled();
    return LC_SUCCESS;
}

/* ----- Style Mode ----- */

LC_API LCResult lc_toggle_button_get_style_mode(LCComponentHandle handle, LCToggleButtonStyleMode* outMode) {
    if (!outMode) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outMode = FromEngineStyleMode(btn->GetStyleMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_style_mode(LCComponentHandle handle, LCToggleButtonStyleMode mode) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStyleMode(ToEngineStyleMode(mode));
    return LC_SUCCESS;
}

/* ----- Toggle State ----- */

LC_API LCResult lc_toggle_button_is_toggled(LCComponentHandle handle, bool* outToggled) {
    if (!outToggled) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outToggled = btn->IsToggled();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_toggled(LCComponentHandle handle, bool toggled) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetToggled(toggled);
    return LC_SUCCESS;
}

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_toggle_button_get_state_modulation(LCComponentHandle handle, LCToggleButtonState state, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetStateModulation(ToEngineState(state)));
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_state_modulation(LCComponentHandle handle, LCToggleButtonState state, LCColor color) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStateModulation(ToEngineState(state), ToEngineColor(color));
    return LC_SUCCESS;
}

/* ----- Per-State Sounds ----- */

LC_API LCResult lc_toggle_button_set_state_sound_path(LCComponentHandle handle, LCToggleButtonState state, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStateSoundPath(ToEngineState(state), path);
    return LC_SUCCESS;
}

/* ----- Text Properties ----- */

LC_API LCResult lc_toggle_button_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength) {
    if (!outText) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string text = btn->GetText();
    CopyStringToBuffer(outText, maxLength, text.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_text(LCComponentHandle handle, const char* text) {
    if (!text) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetText(text);
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string path = btn->GetFontPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_font_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontPath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outSize = btn->GetFontSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_font_size(LCComponentHandle handle, float size) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontSize(size);
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetFontColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_font_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontColor(ToEngineColor(color));
    return LC_SUCCESS;
}

/* ----- StyleBox Properties ----- */

LC_API LCResult lc_toggle_button_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetBackgroundColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_background_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBackgroundColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_get_opacity(LCComponentHandle handle, float* outOpacity) {
    if (!outOpacity) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outOpacity = btn->GetOpacity();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_opacity(LCComponentHandle handle, float opacity) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetOpacity(opacity);
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_get_border_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outEnabled = btn->GetBorderEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_border_enabled(LCComponentHandle handle, bool enabled) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetBorderColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_toggle_button_set_border_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetToggleButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderColor(ToEngineColor(color));
    return LC_SUCCESS;
}
