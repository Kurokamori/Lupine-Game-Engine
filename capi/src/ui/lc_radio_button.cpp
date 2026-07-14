/**
 * @file lc_radio_button.cpp
 * @brief Implementation of RadioButton and RadioList C API
 */

#include "ui/lc_radio_button.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>

#include <lupine/components/RadioButton.hpp>
#include <lupine/components/RadioList.hpp>

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

RadioButtonState ToEngineState(LCRadioButtonState state) {
    return static_cast<RadioButtonState>(state);
}

LCRadioButtonState FromEngineState(RadioButtonState state) {
    return static_cast<LCRadioButtonState>(state);
}

RadioList::Orientation ToEngineOrientation(LCRadioListOrientation orientation) {
    return static_cast<RadioList::Orientation>(orientation);
}

LCRadioListOrientation FromEngineOrientation(RadioList::Orientation orientation) {
    return static_cast<LCRadioListOrientation>(orientation);
}

RadioButton* GetRadioButton(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<RadioButton*>(comp.get());
}

RadioList* GetRadioList(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<RadioList*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * RadioButton Component Functions
 * ============================================================================ */

LC_API LCResult lc_radio_button_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<RadioButton>();
        nodePtr->AddComponent(comp);
        comp->DefineProperties();
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create RadioButton component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Size Properties ----- */

LC_API LCResult lc_radio_button_get_indicator_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outSize = btn->GetIndicatorSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_indicator_size(LCComponentHandle handle, float size) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetIndicatorSize(size);
    return LC_SUCCESS;
}

/* ----- Layer Properties ----- */

LC_API LCResult lc_radio_button_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outLayer = btn->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_layer(LCComponentHandle handle, int layer) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outOrder = btn->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_sorting_order(LCComponentHandle handle, int order) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetSortingOrder(order);
    return LC_SUCCESS;
}

/* ----- UI Space ----- */

LC_API LCResult lc_radio_button_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace) {
    if (!outUseUISpace) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outUseUISpace = btn->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_use_ui_space(LCComponentHandle handle, bool useUISpace) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetUseUISpace(useUISpace);
    return LC_SUCCESS;
}

/* ----- Button State ----- */

LC_API LCResult lc_radio_button_get_current_state(LCComponentHandle handle, LCRadioButtonState* outState) {
    if (!outState) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outState = FromEngineState(btn->GetCurrentState());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_enabled(LCComponentHandle handle, bool enabled) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_is_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outEnabled = btn->IsButtonEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_is_selected(LCComponentHandle handle, bool* outSelected) {
    if (!outSelected) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outSelected = btn->IsSelected();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_selected(LCComponentHandle handle, bool selected) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetSelected(selected);
    return LC_SUCCESS;
}

/* ----- Group Management ----- */

LC_API LCResult lc_radio_button_get_group_name(LCComponentHandle handle, char* outName, uint32_t maxLength) {
    if (!outName) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string name = btn->GetGroupName();
    CopyStringToBuffer(outName, maxLength, name.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_group_name(LCComponentHandle handle, const char* groupName) {
    if (!groupName) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetGroupName(groupName);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_radio_value(LCComponentHandle handle, int* outValue) {
    if (!outValue) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outValue = btn->GetRadioValue();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_radio_value(LCComponentHandle handle, int value) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetRadioValue(value);
    return LC_SUCCESS;
}

/* ----- Indicator Properties ----- */

LC_API LCResult lc_radio_button_get_outer_circle_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetOuterCircleColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_outer_circle_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetOuterCircleColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetBackgroundColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_background_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBackgroundColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_inner_circle_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetInnerCircleColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_inner_circle_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetInnerCircleColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_inner_circle_scale(LCComponentHandle handle, float* outScale) {
    if (!outScale) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outScale = btn->GetInnerCircleScale();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_inner_circle_scale(LCComponentHandle handle, float scale) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetInnerCircleScale(scale);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_border_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outWidth = btn->GetBorderWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_border_width(LCComponentHandle handle, float width) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetBorderWidth(width);
    return LC_SUCCESS;
}

/* ----- Text Properties ----- */

LC_API LCResult lc_radio_button_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength) {
    if (!outText) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string text = btn->GetText();
    CopyStringToBuffer(outText, maxLength, text.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_text(LCComponentHandle handle, const char* text) {
    if (!text) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetText(text);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string path = btn->GetFontPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_font_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontPath(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outSize = btn->GetFontSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_font_size(LCComponentHandle handle, float size) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontSize(size);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetFontColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_font_color(LCComponentHandle handle, LCColor color) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetFontColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_get_text_offset(LCComponentHandle handle, float* outOffset) {
    if (!outOffset) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outOffset = btn->GetTextOffset();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_text_offset(LCComponentHandle handle, float offset) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetTextOffset(offset);
    return LC_SUCCESS;
}

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_radio_button_get_state_modulation(LCComponentHandle handle, LCRadioButtonState state, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(btn->GetStateModulation(ToEngineState(state)));
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_state_modulation(LCComponentHandle handle, LCRadioButtonState state, LCColor color) {
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetStateModulation(ToEngineState(state), ToEngineColor(color));
    return LC_SUCCESS;
}

/* ----- Audio ----- */

LC_API LCResult lc_radio_button_get_select_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    std::string path = btn->GetSelectSoundPath();
    CopyStringToBuffer(outPath, maxLength, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_button_set_select_sound_path(LCComponentHandle handle, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto btn = GetRadioButton(handle);
    if (!btn) return LC_ERROR_INVALID_HANDLE;
    btn->SetSelectSoundPath(path);
    return LC_SUCCESS;
}

/* ============================================================================
 * RadioList Component Functions
 * ============================================================================ */

LC_API LCResult lc_radio_list_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<RadioList>();
        nodePtr->AddComponent(comp);
        comp->DefineProperties();
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create RadioList component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Group Management ----- */

LC_API LCResult lc_radio_list_get_group_name(LCComponentHandle handle, char* outName, uint32_t maxLength) {
    if (!outName) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    std::string name = list->GetGroupName();
    CopyStringToBuffer(outName, maxLength, name.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_set_group_name(LCComponentHandle handle, const char* groupName) {
    if (!groupName) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetGroupName(groupName);
    return LC_SUCCESS;
}

/* ----- Selection Management ----- */

LC_API LCResult lc_radio_list_get_selected_index(LCComponentHandle handle, int* outIndex) {
    if (!outIndex) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outIndex = list->GetSelectedIndex();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_set_selected_index(LCComponentHandle handle, int index) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetSelectedIndex(index);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_get_selected_value(LCComponentHandle handle, int* outValue) {
    if (!outValue) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outValue = list->GetSelectedValue();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_set_selected_value(LCComponentHandle handle, int value) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetSelectedValue(value);
    return LC_SUCCESS;
}

/* ----- Layout Properties ----- */

LC_API LCResult lc_radio_list_get_orientation(LCComponentHandle handle, LCRadioListOrientation* outOrientation) {
    if (!outOrientation) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outOrientation = FromEngineOrientation(list->GetOrientation());
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_set_orientation(LCComponentHandle handle, LCRadioListOrientation orientation) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetOrientation(ToEngineOrientation(orientation));
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_get_spacing(LCComponentHandle handle, float* outSpacing) {
    if (!outSpacing) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outSpacing = list->GetSpacing();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_set_spacing(LCComponentHandle handle, float spacing) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetSpacing(spacing);
    return LC_SUCCESS;
}

/* ----- Item Management ----- */

LC_API LCResult lc_radio_list_add_item(LCComponentHandle handle, const char* item) {
    if (!item) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->AddItem(item);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_remove_item(LCComponentHandle handle, int index) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->RemoveItem(index);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_clear_items(LCComponentHandle handle) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->ClearItems();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_get_item_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outCount = list->GetItemCount();
    return LC_SUCCESS;
}

/* ----- Auto-Create ----- */

LC_API LCResult lc_radio_list_get_auto_create_buttons(LCComponentHandle handle, bool* outAutoCreate) {
    if (!outAutoCreate) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outAutoCreate = list->GetAutoCreateButtons();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_set_auto_create_buttons(LCComponentHandle handle, bool autoCreate) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetAutoCreateButtons(autoCreate);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_get_manage_layout(LCComponentHandle handle, bool* outManage) {
    if (!outManage) return LC_ERROR_NULL_POINTER;
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    *outManage = list->GetManageLayout();
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_set_manage_layout(LCComponentHandle handle, bool manage) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->SetManageLayout(manage);
    return LC_SUCCESS;
}

LC_API LCResult lc_radio_list_regenerate_buttons(LCComponentHandle handle) {
    auto list = GetRadioList(handle);
    if (!list) return LC_ERROR_INVALID_HANDLE;
    list->RegenerateRadioButtons();
    return LC_SUCCESS;
}
