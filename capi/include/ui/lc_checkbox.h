/**
 * @file lc_checkbox.h
 * @brief Lupine Engine C API - Checkbox and CheckList Components
 *
 * Checkbox: Box indicator with checkmark, group management
 * CheckList: Container for managing groups of checkboxes
 */

#ifndef LUPINE_CAPI_CHECKBOX_H
#define LUPINE_CAPI_CHECKBOX_H

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
 * @brief Checkbox mouse states
 */
typedef enum LCCheckboxState {
    LC_CHECKBOX_STATE_NORMAL = 0,
    LC_CHECKBOX_STATE_HOVER = 1,
    LC_CHECKBOX_STATE_PRESSED = 2,
    LC_CHECKBOX_STATE_CHECKED = 3,
    LC_CHECKBOX_STATE_CHECKED_HOVER = 4,
    LC_CHECKBOX_STATE_DISABLED = 5
} LCCheckboxState;

/**
 * @brief CheckList orientation
 */
typedef enum LCCheckListOrientation {
    LC_CHECK_LIST_VERTICAL = 0,
    LC_CHECK_LIST_HORIZONTAL = 1
} LCCheckListOrientation;

/* ============================================================================
 * Checkbox Component Functions
 * ============================================================================ */

/**
 * @brief Create a Checkbox component on a node
 */
LC_API LCResult lc_checkbox_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_checkbox_get_box_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_checkbox_set_box_size(LCComponentHandle handle, float size);

/* ----- Layer Properties ----- */

LC_API LCResult lc_checkbox_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_checkbox_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_checkbox_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_checkbox_set_sorting_order(LCComponentHandle handle, int order);

/* ----- UI Space ----- */

LC_API LCResult lc_checkbox_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace);
LC_API LCResult lc_checkbox_set_use_ui_space(LCComponentHandle handle, bool useUISpace);

/* ----- Button State ----- */

LC_API LCResult lc_checkbox_get_current_state(LCComponentHandle handle, LCCheckboxState* outState);
LC_API LCResult lc_checkbox_set_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_checkbox_is_enabled(LCComponentHandle handle, bool* outEnabled);

LC_API LCResult lc_checkbox_is_checked(LCComponentHandle handle, bool* outChecked);
LC_API LCResult lc_checkbox_set_checked(LCComponentHandle handle, bool checked);
LC_API LCResult lc_checkbox_toggle(LCComponentHandle handle);

/* ----- Group Management ----- */

LC_API LCResult lc_checkbox_get_group_name(LCComponentHandle handle, char* outName, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_group_name(LCComponentHandle handle, const char* groupName);
LC_API LCResult lc_checkbox_get_checkbox_value(LCComponentHandle handle, int* outValue);
LC_API LCResult lc_checkbox_set_checkbox_value(LCComponentHandle handle, int value);

/* ----- Box Properties ----- */

LC_API LCResult lc_checkbox_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_checkbox_set_border_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_checkbox_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_checkbox_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_checkbox_get_checkmark_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_checkbox_set_checkmark_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_checkbox_get_corner_radius(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_checkbox_set_corner_radius(LCComponentHandle handle, float radius);
LC_API LCResult lc_checkbox_get_border_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_checkbox_set_border_width(LCComponentHandle handle, float width);

/* ----- Texture Properties ----- */

LC_API LCResult lc_checkbox_get_unchecked_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_unchecked_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_checkbox_get_checked_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_checked_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_checkbox_get_use_textures(LCComponentHandle handle, bool* outUse);
LC_API LCResult lc_checkbox_set_use_textures(LCComponentHandle handle, bool use);

/* ----- Text Properties ----- */

LC_API LCResult lc_checkbox_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_text(LCComponentHandle handle, const char* text);
LC_API LCResult lc_checkbox_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_checkbox_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_checkbox_set_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_checkbox_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_checkbox_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_checkbox_get_text_offset(LCComponentHandle handle, float* outOffset);
LC_API LCResult lc_checkbox_set_text_offset(LCComponentHandle handle, float offset);

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_checkbox_get_state_modulation(LCComponentHandle handle, LCCheckboxState state, LCColor* outColor);
LC_API LCResult lc_checkbox_set_state_modulation(LCComponentHandle handle, LCCheckboxState state, LCColor color);

/* ----- Audio ----- */

LC_API LCResult lc_checkbox_get_check_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_check_sound_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_checkbox_get_uncheck_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_uncheck_sound_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_checkbox_get_hover_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_hover_sound_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_checkbox_get_press_sound_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_checkbox_set_press_sound_path(LCComponentHandle handle, const char* path);

/* ============================================================================
 * CheckList Component Functions
 * ============================================================================ */

/**
 * @brief Create a CheckList component on a node
 */
LC_API LCResult lc_check_list_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Group Management ----- */

LC_API LCResult lc_check_list_get_group_name(LCComponentHandle handle, char* outName, uint32_t maxLength);
LC_API LCResult lc_check_list_set_group_name(LCComponentHandle handle, const char* groupName);

/* ----- Layout Properties ----- */

LC_API LCResult lc_check_list_get_orientation(LCComponentHandle handle, LCCheckListOrientation* outOrientation);
LC_API LCResult lc_check_list_set_orientation(LCComponentHandle handle, LCCheckListOrientation orientation);
LC_API LCResult lc_check_list_get_spacing(LCComponentHandle handle, float* outSpacing);
LC_API LCResult lc_check_list_set_spacing(LCComponentHandle handle, float spacing);

/* ----- Rendering Properties ----- */

LC_API LCResult lc_check_list_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_check_list_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_check_list_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_check_list_set_sorting_order(LCComponentHandle handle, int order);
LC_API LCResult lc_check_list_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace);
LC_API LCResult lc_check_list_set_use_ui_space(LCComponentHandle handle, bool useUISpace);

/* ----- Group Operations ----- */

LC_API LCResult lc_check_list_check_all(LCComponentHandle handle);
LC_API LCResult lc_check_list_uncheck_all(LCComponentHandle handle);
LC_API LCResult lc_check_list_toggle_all(LCComponentHandle handle);

/* ----- State Queries ----- */

LC_API LCResult lc_check_list_are_all_checked(LCComponentHandle handle, bool* outAllChecked);
LC_API LCResult lc_check_list_are_all_unchecked(LCComponentHandle handle, bool* outAllUnchecked);
LC_API LCResult lc_check_list_is_any_checked(LCComponentHandle handle, bool* outAnyChecked);
LC_API LCResult lc_check_list_get_checked_count(LCComponentHandle handle, int* outCount);
LC_API LCResult lc_check_list_get_total_count(LCComponentHandle handle, int* outCount);

/* ----- Item Management ----- */

LC_API LCResult lc_check_list_add_item(LCComponentHandle handle, const char* text, int value);
LC_API LCResult lc_check_list_remove_item(LCComponentHandle handle, int index);
LC_API LCResult lc_check_list_clear_items(LCComponentHandle handle);
LC_API LCResult lc_check_list_get_item_count(LCComponentHandle handle, int* outCount);

/* ----- Auto-Create ----- */

LC_API LCResult lc_check_list_get_auto_create_checkboxes(LCComponentHandle handle, bool* outAutoCreate);
LC_API LCResult lc_check_list_set_auto_create_checkboxes(LCComponentHandle handle, bool autoCreate);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_CHECKBOX_H */
