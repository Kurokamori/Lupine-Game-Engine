/**
 * @file lc_radiolist.h
 * @brief Lupine Engine C API - RadioList component
 *
 * This header provides a container component that manages a group of RadioButton components.
 * Features:
 * - Automatic mutual exclusion (only one radio button selected at a time)
 * - Selection tracking by index or value
 * - Layout management (vertical/horizontal orientation)
 * - Auto-creation of radio buttons from item list
 */

#ifndef LUPINE_CAPI_RADIOLIST_H
#define LUPINE_CAPI_RADIOLIST_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "ui/lc_radio_button.h"  /* For LCRadioListOrientation enum */

/* Note: LCRadioListOrientation enum is defined in lc_radio_button.h
 * Values: LC_RADIO_LIST_VERTICAL, LC_RADIO_LIST_HORIZONTAL
 */

/* ============================================================================
 * RadioList Creation
 * ============================================================================ */

/**
 * @brief Create a RadioList component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Group Properties
 * ============================================================================ */

/**
 * @brief Get the group name
 * @param component Component handle
 * @param out_buffer Output buffer for group name
 * @param buffer_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_get_group_name(LCComponentHandle component, char* out_buffer, size_t buffer_size);

/**
 * @brief Set the group name
 * @param component Component handle
 * @param group_name Group name for matching radio buttons
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_set_group_name(LCComponentHandle component, const char* group_name);

/* ============================================================================
 * Selection Management
 * ============================================================================ */

/**
 * @brief Get the selected index
 * @param component Component handle
 * @param out_index Output parameter for selected index (-1 if none)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_get_selected_index(LCComponentHandle component, int* out_index);

/**
 * @brief Set the selected index
 * @param component Component handle
 * @param index Index to select (-1 to deselect all)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_set_selected_index(LCComponentHandle component, int index);

/**
 * @brief Get the selected value
 * @param component Component handle
 * @param out_value Output parameter for selected value (-1 if none)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_get_selected_value(LCComponentHandle component, int* out_value);

/**
 * @brief Set selection by value
 * @param component Component handle
 * @param value Value to select
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_set_selected_value(LCComponentHandle component, int value);

/* ============================================================================
 * Layout Properties
 * ============================================================================ */

/**
 * @brief Get the layout orientation
 * @param component Component handle
 * @param out_orientation Output parameter for orientation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_get_orientation(LCComponentHandle component, LCRadioListOrientation* out_orientation);

/**
 * @brief Set the layout orientation
 * @param component Component handle
 * @param orientation Layout orientation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_set_orientation(LCComponentHandle component, LCRadioListOrientation orientation);

/**
 * @brief Get the spacing between radio buttons
 * @param component Component handle
 * @param out_spacing Output parameter for spacing
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_get_spacing(LCComponentHandle component, float* out_spacing);

/**
 * @brief Set the spacing between radio buttons
 * @param component Component handle
 * @param spacing Spacing in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_set_spacing(LCComponentHandle component, float spacing);

/* ============================================================================
 * Item Management
 * ============================================================================ */

/**
 * @brief Add an item to the radio list
 * @param component Component handle
 * @param text Item text
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_add_item(LCComponentHandle component, const char* text);

/**
 * @brief Remove an item by index
 * @param component Component handle
 * @param index Index of item to remove
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_remove_item(LCComponentHandle component, int index);

/**
 * @brief Clear all items
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_clear_items(LCComponentHandle component);

/**
 * @brief Get the item count
 * @param component Component handle
 * @param out_count Output parameter for count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_get_item_count(LCComponentHandle component, int* out_count);

/* ============================================================================
 * Auto-Create Settings
 * ============================================================================ */

/**
 * @brief Get whether auto-create buttons is enabled
 * @param component Component handle
 * @param out_auto_create Output parameter for auto-create flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_get_auto_create_buttons(LCComponentHandle component, bool* out_auto_create);

/**
 * @brief Set whether to auto-create radio buttons from items
 * @param component Component handle
 * @param auto_create Enable auto-creation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_set_auto_create_buttons(LCComponentHandle component, bool auto_create);

/**
 * @brief Get whether layout management is enabled
 * @param component Component handle
 * @param out_manage Output parameter for manage layout flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_get_manage_layout(LCComponentHandle component, bool* out_manage);

/**
 * @brief Set whether to manage layout of radio buttons
 * @param component Component handle
 * @param manage Enable layout management
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_set_manage_layout(LCComponentHandle component, bool manage);

/**
 * @brief Regenerate radio buttons from items
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_radiolist_regenerate_buttons(LCComponentHandle component);

#endif /* LUPINE_CAPI_RADIOLIST_H */
