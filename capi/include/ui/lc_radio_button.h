/**
 * @file lc_radio_button.h
 * @brief Lupine Engine C API - RadioButton and RadioList Components
 *
 * RadioButton: Radio button with circular indicator, group management
 * RadioList: Container for managing groups of radio buttons
 */

#ifndef LUPINE_CAPI_RADIO_BUTTON_H
#define LUPINE_CAPI_RADIO_BUTTON_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief RadioButton mouse states
 */
typedef enum LCRadioButtonState {
    LC_RADIO_BUTTON_STATE_NORMAL = 0,
    LC_RADIO_BUTTON_STATE_HOVER = 1,
    LC_RADIO_BUTTON_STATE_PRESSED = 2,
    LC_RADIO_BUTTON_STATE_SELECTED = 3,
    LC_RADIO_BUTTON_STATE_SELECTED_HOVER = 4,
    LC_RADIO_BUTTON_STATE_DISABLED = 5
} LCRadioButtonState;

/**
 * @brief RadioList orientation
 */
typedef enum LCRadioListOrientation {
    LC_RADIO_LIST_VERTICAL = 0,
    LC_RADIO_LIST_HORIZONTAL = 1
} LCRadioListOrientation;

/* ============================================================================
 * RadioButton Component Functions
 * ============================================================================ */

/**
 * @brief Create a RadioButton component on a node
 */
LC_API LCResult lc_radio_button_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_radio_button_get_indicator_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_radio_button_set_indicator_size(LCComponentHandle handle, float size);

/* ----- Layer Properties ----- */

LC_API LCResult lc_radio_button_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_radio_button_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_radio_button_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_radio_button_set_sorting_order(LCComponentHandle handle, int order);

/* ----- UI Space ----- */

LC_API LCResult lc_radio_button_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace);
LC_API LCResult lc_radio_button_set_use_ui_space(LCComponentHandle handle, bool useUISpace);

/* ----- Button State ----- */

LC_API LCResult lc_radio_button_get_current_state(LCComponentHandle handle, LCRadioButtonState* outState);
LC_API LCResult lc_radio_button_set_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_radio_button_is_enabled(LCComponentHandle handle, bool* outEnabled);

LC_API LCResult lc_radio_button_is_selected(LCComponentHandle handle, bool* outSelected);
LC_API LCResult lc_radio_button_set_selected(LCComponentHandle handle, bool selected);

/* ----- Group Management ----- */

LC_API LCResult lc_radio_button_get_group_name(LCComponentHandle handle, char* outName, uint32_t maxLength);
LC_API LCResult lc_radio_button_set_group_name(LCComponentHandle handle, const char* groupName);
LC_API LCResult lc_radio_button_get_radio_value(LCComponentHandle handle, int* outValue);
LC_API LCResult lc_radio_button_set_radio_value(LCComponentHandle handle, int value);

/* ----- Indicator Properties ----- */

LC_API LCResult lc_radio_button_get_outer_circle_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_radio_button_set_outer_circle_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_radio_button_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_radio_button_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_radio_button_get_inner_circle_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_radio_button_set_inner_circle_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_radio_button_get_inner_circle_scale(LCComponentHandle handle, float* outScale);
LC_API LCResult lc_radio_button_set_inner_circle_scale(LCComponentHandle handle, float scale);
LC_API LCResult lc_radio_button_get_border_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_radio_button_set_border_width(LCComponentHandle handle, float width);

/* ----- Text Properties ----- */

LC_API LCResult lc_radio_button_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength);
LC_API LCResult lc_radio_button_set_text(LCComponentHandle handle, const char* text);
LC_API LCResult lc_radio_button_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_radio_button_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_radio_button_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_radio_button_set_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_radio_button_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_radio_button_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_radio_button_get_text_offset(LCComponentHandle handle, float* outOffset);
LC_API LCResult lc_radio_button_set_text_offset(LCComponentHandle handle, float offset);

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_radio_button_get_state_modulation(LCComponentHandle handle, LCRadioButtonState state, LCColor* outColor);
LC_API LCResult lc_radio_button_set_state_modulation(LCComponentHandle handle, LCRadioButtonState state, LCColor color);

/* ----- Audio ----- */

LC_API LCResult lc_radio_button_get_select_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_radio_button_set_select_sound_path(LCComponentHandle handle, const char* path);

/* ============================================================================
 * RadioList Component Functions
 * ============================================================================ */

/**
 * @brief Create a RadioList component on a node
 */
LC_API LCResult lc_radio_list_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Group Management ----- */

LC_API LCResult lc_radio_list_get_group_name(LCComponentHandle handle, char* outName, uint32_t maxLength);
LC_API LCResult lc_radio_list_set_group_name(LCComponentHandle handle, const char* groupName);

/* ----- Selection Management ----- */

LC_API LCResult lc_radio_list_get_selected_index(LCComponentHandle handle, int* outIndex);
LC_API LCResult lc_radio_list_set_selected_index(LCComponentHandle handle, int index);
LC_API LCResult lc_radio_list_get_selected_value(LCComponentHandle handle, int* outValue);
LC_API LCResult lc_radio_list_set_selected_value(LCComponentHandle handle, int value);

/* ----- Layout Properties ----- */

LC_API LCResult lc_radio_list_get_orientation(LCComponentHandle handle, LCRadioListOrientation* outOrientation);
LC_API LCResult lc_radio_list_set_orientation(LCComponentHandle handle, LCRadioListOrientation orientation);
LC_API LCResult lc_radio_list_get_spacing(LCComponentHandle handle, float* outSpacing);
LC_API LCResult lc_radio_list_set_spacing(LCComponentHandle handle, float spacing);

/* ----- Item Management ----- */

LC_API LCResult lc_radio_list_add_item(LCComponentHandle handle, const char* item);
LC_API LCResult lc_radio_list_remove_item(LCComponentHandle handle, int index);
LC_API LCResult lc_radio_list_clear_items(LCComponentHandle handle);
LC_API LCResult lc_radio_list_get_item_count(LCComponentHandle handle, int* outCount);

/* ----- Auto-Create ----- */

LC_API LCResult lc_radio_list_get_auto_create_buttons(LCComponentHandle handle, bool* outAutoCreate);
LC_API LCResult lc_radio_list_set_auto_create_buttons(LCComponentHandle handle, bool autoCreate);
LC_API LCResult lc_radio_list_get_manage_layout(LCComponentHandle handle, bool* outManage);
LC_API LCResult lc_radio_list_set_manage_layout(LCComponentHandle handle, bool manage);
LC_API LCResult lc_radio_list_regenerate_buttons(LCComponentHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_RADIO_BUTTON_H */
