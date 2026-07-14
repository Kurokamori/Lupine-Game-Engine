/**
 * @file lc_checkbox.cpp
 * @brief Implementation of Checkbox and CheckList C API
 */

#include "ui/lc_checkbox.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>

#include <lupine/components/Checkbox.hpp>
#include <lupine/components/CheckList.hpp>

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

CheckboxState ToEngineState(LCCheckboxState state) {
    return static_cast<CheckboxState>(state);
}

LCCheckboxState FromEngineState(CheckboxState state) {
    return static_cast<LCCheckboxState>(state);
}

CheckListOrientation ToEngineOrientation(LCCheckListOrientation orientation) {
    return static_cast<CheckListOrientation>(orientation);
}

LCCheckListOrientation FromEngineOrientation(CheckListOrientation orientation) {
    return static_cast<LCCheckListOrientation>(orientation);
}

Checkbox* GetCheckbox(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<Checkbox*>(comp.get());
}

CheckList* GetCheckList(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<CheckList*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * Checkbox Component Functions
 * ============================================================================ */

LC_API LCResult lc_checkbox_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<Checkbox>();
        nodePtr->AddComponent(comp);
        comp->DefineProperties();
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create Checkbox component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_checkbox_get_box_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outSize = cb->GetBoxSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_box_size(LCComponentHandle handle, float size) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetBoxSize(size);
    return LC_SUCCESS;
}

/* ----- Layer Properties ----- */

LC_API LCResult lc_checkbox_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outLayer = cb->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_layer(LCComponentHandle handle, int layer) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outOrder = cb->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_sorting_order(LCComponentHandle handle, int order) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetSortingOrder(order);
    return LC_SUCCESS;
}

/* ----- UI Space ----- */

LC_API LCResult lc_checkbox_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace) {
    if (!outUseUISpace) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outUseUISpace = cb->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_use_ui_space(LCComponentHandle handle, bool useUISpace) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetUseUISpace(useUISpace);
    return LC_SUCCESS;
}

/* ----- Button State ----- */

LC_API LCResult lc_checkbox_get_current_state(LCComponentHandle handle, LCCheckboxState* outState) {
    if (!outState) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outState = FromEngineState(cb->GetCurrentState());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_enabled(LCComponentHandle handle, bool enabled) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_is_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outEnabled = cb->IsButtonEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_is_checked(LCComponentHandle handle, bool* outChecked) {
    if (!outChecked) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outChecked = cb->IsChecked();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_checked(LCComponentHandle handle, bool checked) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetChecked(checked);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_toggle(LCComponentHandle handle) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->Toggle();
    return LC_SUCCESS;
}

/* ----- Group Management ----- */

LC_API LCResult lc_checkbox_get_group_name(LCComponentHandle handle, char* outName, uint32_t maxLength) {
    if (!outName) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string name = cb->GetGroupName();
    CopyStringToBuffer(outName, maxLength, name.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_group_name(LCComponentHandle handle, const char* groupName) {
    if (!groupName) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetGroupName(groupName);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_checkbox_value(LCComponentHandle handle, int* outValue) {
    if (!outValue) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outValue = cb->GetCheckboxValue();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_checkbox_value(LCComponentHandle handle, int value) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetCheckboxValue(value);
    return LC_SUCCESS;
}

/* ----- Box Properties ----- */

LC_API LCResult lc_checkbox_get_border_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(cb->GetBorderColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_border_color(LCComponentHandle handle, LCColor color) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetBorderColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(cb->GetBackgroundColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_background_color(LCComponentHandle handle, LCColor color) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetBackgroundColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_checkmark_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(cb->GetCheckmarkColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_checkmark_color(LCComponentHandle handle, LCColor color) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetCheckmarkColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_corner_radius(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outRadius = cb->GetCornerRadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_corner_radius(LCComponentHandle handle, float radius) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetCornerRadius(radius);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_border_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outWidth = cb->GetBorderWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_border_width(LCComponentHandle handle, float width) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetBorderWidth(width);
    return LC_SUCCESS;
}

/* ----- Texture Properties ----- */

LC_API LCResult lc_checkbox_get_unchecked_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string path = cb->GetUncheckedTexturePath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_unchecked_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetUncheckedTexturePath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_checked_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string path = cb->GetCheckedTexturePath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_checked_texture_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetCheckedTexturePath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_use_textures(LCComponentHandle handle, bool* outUse) {
    if (!outUse) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outUse = cb->GetUseTextures();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_use_textures(LCComponentHandle handle, bool use) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetUseTextures(use);
    return LC_SUCCESS;
}

/* ----- Text Properties ----- */

LC_API LCResult lc_checkbox_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength) {
    if (!outText) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string text = cb->GetText();
    CopyStringToBuffer(outText, maxLength, text.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_text(LCComponentHandle handle, const char* text) {
    if (!text) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetText(text);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string path = cb->GetFontPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_font_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetFontPath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outSize = cb->GetFontSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_font_size(LCComponentHandle handle, float size) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetFontSize(size);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(cb->GetFontColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_font_color(LCComponentHandle handle, LCColor color) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetFontColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_text_offset(LCComponentHandle handle, float* outOffset) {
    if (!outOffset) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outOffset = cb->GetTextOffset();
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_text_offset(LCComponentHandle handle, float offset) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetTextOffset(offset);
    return LC_SUCCESS;
}

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_checkbox_get_state_modulation(LCComponentHandle handle, LCCheckboxState state, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(cb->GetStateModulation(ToEngineState(state)));
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_state_modulation(LCComponentHandle handle, LCCheckboxState state, LCColor color) {
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetStateModulation(ToEngineState(state), ToEngineColor(color));
    return LC_SUCCESS;
}

/* ----- Audio ----- */

LC_API LCResult lc_checkbox_get_check_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string path = cb->GetCheckSoundPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_check_sound_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetCheckSoundPath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_uncheck_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string path = cb->GetUncheckSoundPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_uncheck_sound_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetUncheckSoundPath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_hover_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string path = cb->GetHoverSoundPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_hover_sound_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetHoverSoundPath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_get_press_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    std::string path = cb->GetPressSoundPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checkbox_set_press_sound_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto cb = GetCheckbox(handle);
    if (!cb) return LC_ERROR_INVALID_HANDLE;
    cb->SetPressSoundPath(path);
    return LC_SUCCESS;
}

/* ============================================================================
 * CheckList Component Functions
 * ============================================================================ */

LC_API LCResult lc_check_list_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<CheckList>();
        nodePtr->AddComponent(comp);
        comp->DefineProperties();
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create CheckList component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Group Management ----- */

LC_API LCResult lc_check_list_get_group_name(LCComponentHandle handle, char* outName, uint32_t maxLength) {
    if (!outName) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    std::string name = list->GetGroupName();
    CopyStringToBuffer(outName, maxLength, name.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_set_group_name(LCComponentHandle handle, const char* groupName) {
    if (!groupName) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetGroupName(groupName);
    return LC_SUCCESS;
}

/* ----- Layout Properties ----- */

LC_API LCResult lc_check_list_get_orientation(LCComponentHandle handle, LCCheckListOrientation* outOrientation) {
    if (!outOrientation) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outOrientation = FromEngineOrientation(list->GetOrientation());
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_set_orientation(LCComponentHandle handle, LCCheckListOrientation orientation) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetOrientation(ToEngineOrientation(orientation));
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_get_spacing(LCComponentHandle handle, float* outSpacing) {
    if (!outSpacing) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outSpacing = list->GetSpacing();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_set_spacing(LCComponentHandle handle, float spacing) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetSpacing(spacing);
    return LC_SUCCESS;
}

/* ----- Rendering Properties ----- */

LC_API LCResult lc_check_list_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outLayer = list->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_set_layer(LCComponentHandle handle, int layer) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outOrder = list->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_set_sorting_order(LCComponentHandle handle, int order) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetSortingOrder(order);
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace) {
    if (!outUseUISpace) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outUseUISpace = list->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_set_use_ui_space(LCComponentHandle handle, bool useUISpace) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetUseUISpace(useUISpace);
    return LC_SUCCESS;
}

/* ----- Group Operations ----- */

LC_API LCResult lc_check_list_check_all(LCComponentHandle handle) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->CheckAll();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_uncheck_all(LCComponentHandle handle) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->UncheckAll();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_toggle_all(LCComponentHandle handle) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->ToggleAll();
    return LC_SUCCESS;
}

/* ----- State Queries ----- */

LC_API LCResult lc_check_list_are_all_checked(LCComponentHandle handle, bool* outAllChecked) {
    if (!outAllChecked) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outAllChecked = list->AreAllChecked();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_are_all_unchecked(LCComponentHandle handle, bool* outAllUnchecked) {
    if (!outAllUnchecked) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outAllUnchecked = list->AreAllUnchecked();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_is_any_checked(LCComponentHandle handle, bool* outAnyChecked) {
    if (!outAnyChecked) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outAnyChecked = list->IsAnyChecked();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_get_checked_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outCount = list->GetCheckedCount();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_get_total_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outCount = list->GetTotalCount();
    return LC_SUCCESS;
}

/* ----- Item Management ----- */

LC_API LCResult lc_check_list_add_item(LCComponentHandle handle, const char* text, int value) {
    if (!text) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->AddItem(text, value);
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_remove_item(LCComponentHandle handle, int index) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->RemoveItem(index);
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_clear_items(LCComponentHandle handle) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->ClearItems();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_get_item_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outCount = list->GetItemCount();
    return LC_SUCCESS;
}

/* ----- Auto-Create ----- */

LC_API LCResult lc_check_list_get_auto_create_checkboxes(LCComponentHandle handle, bool* outAutoCreate) {
    if (!outAutoCreate) return LC_ERROR_NULL_POINTER;
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outAutoCreate = list->GetAutoCreateCheckboxes();
    return LC_SUCCESS;
}

LC_API LCResult lc_check_list_set_auto_create_checkboxes(LCComponentHandle handle, bool autoCreate) {
    auto list = GetCheckList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetAutoCreateCheckboxes(autoCreate);
    return LC_SUCCESS;
}
