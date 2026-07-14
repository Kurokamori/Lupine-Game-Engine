/**
 * @file lc_texture_button.cpp
 * @brief Implementation of TextureButton C API
 */

#include "ui/lc_texture_button.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>

#include <lupine/components/TextureButton.hpp>

#include <cstring>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

TextureButtonState ToEngineState(LCTextureButtonState state) {
    return static_cast<TextureButtonState>(state);
}

LCTextureButtonState FromEngineState(TextureButtonState state) {
    return static_cast<LCTextureButtonState>(state);
}

TextureButtonStyleMode ToEngineStyleMode(LCTextureButtonStyleMode mode) {
    return static_cast<TextureButtonStyleMode>(mode);
}

LCTextureButtonStyleMode FromEngineStyleMode(TextureButtonStyleMode mode) {
    return static_cast<LCTextureButtonStyleMode>(mode);
}

TextureButtonStretchMode ToEngineStretchMode(LCTextureButtonStretchMode mode) {
    return static_cast<TextureButtonStretchMode>(mode);
}

LCTextureButtonStretchMode FromEngineStretchMode(TextureButtonStretchMode mode) {
    return static_cast<LCTextureButtonStretchMode>(mode);
}

ClickMaskMode ToEngineClickMaskMode(LCClickMaskMode mode) {
    return static_cast<ClickMaskMode>(mode);
}

LCClickMaskMode FromEngineClickMaskMode(ClickMaskMode mode) {
    return static_cast<LCClickMaskMode>(mode);
}

MouseButtonType ToEngineMouseButton(LCMouseButton button) {
    return static_cast<MouseButtonType>(button);
}

LCMouseButton FromEngineMouseButton(MouseButtonType button) {
    return static_cast<LCMouseButton>(button);
}

TextureButton* GetTextureButton(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<TextureButton*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * TextureButton Component Functions
 * ============================================================================ */

LC_API LCResult lc_texture_button_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<TextureButton>();
        nodePtr->AddComponent(comp);
        comp->DefineProperties();
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create TextureButton component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_texture_button_get_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWidth = btn->GetWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_width(LCComponentHandle handle, float width) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetWidth(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_get_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outHeight = btn->GetHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_height(LCComponentHandle handle, float height) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetHeight(height);
    return LC_SUCCESS;
}

/* ----- Layer Properties ----- */

LC_API LCResult lc_texture_button_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outLayer = btn->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_layer(LCComponentHandle handle, int layer) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outOrder = btn->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_sorting_order(LCComponentHandle handle, int order) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetSortingOrder(order);
    return LC_SUCCESS;
}

/* ----- UI Space ----- */

LC_API LCResult lc_texture_button_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace) {
    if (!outUseUISpace) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outUseUISpace = btn->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_use_ui_space(LCComponentHandle handle, bool useUISpace) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetUseUISpace(useUISpace);
    return LC_SUCCESS;
}

/* ----- Button State ----- */

LC_API LCResult lc_texture_button_get_current_state(LCComponentHandle handle, LCTextureButtonState* outState) {
    if (!outState) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outState = FromEngineState(btn->GetCurrentState());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_enabled(LCComponentHandle handle, bool enabled) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_is_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outEnabled = btn->IsButtonEnabled();
    return LC_SUCCESS;
}

/* ----- Style Mode ----- */

LC_API LCResult lc_texture_button_get_style_mode(LCComponentHandle handle, LCTextureButtonStyleMode* outMode) {
    if (!outMode) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outMode = FromEngineStyleMode(btn->GetStyleMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_style_mode(LCComponentHandle handle, LCTextureButtonStyleMode mode) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStyleMode(ToEngineStyleMode(mode));
    return LC_SUCCESS;
}

/* ----- Stretch Mode ----- */

LC_API LCResult lc_texture_button_get_stretch_mode(LCComponentHandle handle, LCTextureButtonStretchMode* outMode) {
    if (!outMode) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outMode = FromEngineStretchMode(btn->GetStretchMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_stretch_mode(LCComponentHandle handle, LCTextureButtonStretchMode mode) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStretchMode(ToEngineStretchMode(mode));
    return LC_SUCCESS;
}

/* ----- Toggle Mode ----- */

LC_API LCResult lc_texture_button_get_is_toggle(LCComponentHandle handle, bool* outIsToggle) {
    if (!outIsToggle) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outIsToggle = btn->GetIsToggle();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_is_toggle(LCComponentHandle handle, bool isToggle) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetIsToggle(isToggle);
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_get_is_checked(LCComponentHandle handle, bool* outIsChecked) {
    if (!outIsChecked) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outIsChecked = btn->GetIsChecked();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_is_checked(LCComponentHandle handle, bool isChecked) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetIsChecked(isChecked);
    return LC_SUCCESS;
}

/* ----- Click Mask ----- */

LC_API LCResult lc_texture_button_get_click_mask_mode(LCComponentHandle handle, LCClickMaskMode* outMode) {
    if (!outMode) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outMode = FromEngineClickMaskMode(btn->GetClickMaskMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_click_mask_mode(LCComponentHandle handle, LCClickMaskMode mode) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetClickMaskMode(ToEngineClickMaskMode(mode));
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_get_alpha_threshold(LCComponentHandle handle, float* outThreshold) {
    if (!outThreshold) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outThreshold = btn->GetAlphaThreshold();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_alpha_threshold(LCComponentHandle handle, float threshold) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetAlphaThreshold(threshold);
    return LC_SUCCESS;
}

/* ----- Mouse Button ----- */

LC_API LCResult lc_texture_button_get_mouse_button(LCComponentHandle handle, LCMouseButton* outButton) {
    if (!outButton) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outButton = FromEngineMouseButton(btn->GetMouseButton());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_mouse_button(LCComponentHandle handle, LCMouseButton button) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetMouseButton(ToEngineMouseButton(button));
    return LC_SUCCESS;
}

/* ----- Texture Properties (Automatic Mode) ----- */

LC_API LCResult lc_texture_button_get_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string path = btn->GetTexturePath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetTexturePath(path);
    return LC_SUCCESS;
}

/* ----- Per-State Textures (Manual Mode) ----- */

LC_API LCResult lc_texture_button_get_state_texture_path(LCComponentHandle handle, LCTextureButtonState state, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string path = btn->GetStateTexturePath(ToEngineState(state));
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_state_texture_path(LCComponentHandle handle, LCTextureButtonState state, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStateTexturePath(ToEngineState(state), path);
    return LC_SUCCESS;
}

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_texture_button_get_state_modulation(LCComponentHandle handle, LCTextureButtonState state, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetStateModulation(ToEngineState(state)));
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_state_modulation(LCComponentHandle handle, LCTextureButtonState state, LCColor color) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStateModulation(ToEngineState(state), ToEngineColor(color));
    return LC_SUCCESS;
}

/* ----- Per-State Sounds ----- */

LC_API LCResult lc_texture_button_set_state_sound_path(LCComponentHandle handle, LCTextureButtonState state, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStateSoundPath(ToEngineState(state), path);
    return LC_SUCCESS;
}

/* ----- Text Properties ----- */

LC_API LCResult lc_texture_button_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength) {
    if (!outText) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string text = btn->GetText();
    CopyStringToBuffer(outText, maxLength, text.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_text(LCComponentHandle handle, const char* text) {
    if (!text) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetText(text);
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string path = btn->GetFontPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_font_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontPath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outSize = btn->GetFontSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_font_size(LCComponentHandle handle, float size) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontSize(size);
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetFontColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_texture_button_set_font_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetTextureButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontColor(ToEngineColor(color));
    return LC_SUCCESS;
}
