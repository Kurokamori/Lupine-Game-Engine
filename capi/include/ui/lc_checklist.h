/**
 * @file lc_checklist.h
 * @brief Lupine Engine C API - CheckList component
 *
 * This header provides a container component that manages a group of Checkbox components.
 * Features:
 * - Group management (check all, uncheck all, toggle all)
 * - Layout management (vertical/horizontal orientation)
 * - State queries (all checked, all unchecked, any checked)
 * - Auto-creation of checkboxes from item list
 */

#ifndef LUPINE_CAPI_CHECKLIST_H
#define LUPINE_CAPI_CHECKLIST_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "ui/lc_checkbox.h"  /* For LCCheckListOrientation enum */

/* Note: LCCheckListOrientation enum is defined in lc_checkbox.h
 * Values: LC_CHECK_LIST_VERTICAL, LC_CHECK_LIST_HORIZONTAL
 */

/* ============================================================================
 * CheckList Creation
 * ============================================================================ */

/**
 * @brief Create a CheckList component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_create(const char* name, LCComponentHandle* out_component);

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
LC_API LCResult lc_checklist_get_group_name(LCComponentHandle component, char* out_buffer, size_t buffer_size);

/**
 * @brief Set the group name
 * @param component Component handle
 * @param group_name Group name for matching checkboxes
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_set_group_name(LCComponentHandle component, const char* group_name);

/* ============================================================================
 * Layout Properties
 * ============================================================================ */

/**
 * @brief Get the layout orientation
 * @param component Component handle
 * @param out_orientation Output parameter for orientation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_orientation(LCComponentHandle component, LCCheckListOrientation* out_orientation);

/**
 * @brief Set the layout orientation
 * @param component Component handle
 * @param orientation Layout orientation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_set_orientation(LCComponentHandle component, LCCheckListOrientation orientation);

/**
 * @brief Get the spacing between checkboxes
 * @param component Component handle
 * @param out_spacing Output parameter for spacing
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_spacing(LCComponentHandle component, float* out_spacing);

/**
 * @brief Set the spacing between checkboxes
 * @param component Component handle
 * @param spacing Spacing in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_set_spacing(LCComponentHandle component, float spacing);

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

/**
 * @brief Get the render layer
 * @param component Component handle
 * @param out_layer Output parameter for layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_layer(LCComponentHandle component, int* out_layer);

/**
 * @brief Set the render layer
 * @param component Component handle
 * @param layer Render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_set_layer(LCComponentHandle component, int layer);

/**
 * @brief Get the sorting order
 * @param component Component handle
 * @param out_order Output parameter for sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_sorting_order(LCComponentHandle component, int* out_order);

/**
 * @brief Set the sorting order
 * @param component Component handle
 * @param order Sorting order within layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_set_sorting_order(LCComponentHandle component, int order);

/**
 * @brief Get whether UI space is used
 * @param component Component handle
 * @param out_use_ui_space Output parameter for use UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space);

/**
 * @brief Set whether to use UI space
 * @param component Component handle
 * @param use_ui_space Use UI space for rendering
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_set_use_ui_space(LCComponentHandle component, bool use_ui_space);

/* ============================================================================
 * Group Operations
 * ============================================================================ */

/**
 * @brief Check all checkboxes in the group
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_check_all(LCComponentHandle component);

/**
 * @brief Uncheck all checkboxes in the group
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_uncheck_all(LCComponentHandle component);

/**
 * @brief Toggle all checkboxes in the group
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_toggle_all(LCComponentHandle component);

/* ============================================================================
 * State Queries
 * ============================================================================ */

/**
 * @brief Check if all checkboxes are checked
 * @param component Component handle
 * @param out_all_checked Output parameter for result
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_are_all_checked(LCComponentHandle component, bool* out_all_checked);

/**
 * @brief Check if all checkboxes are unchecked
 * @param component Component handle
 * @param out_all_unchecked Output parameter for result
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_are_all_unchecked(LCComponentHandle component, bool* out_all_unchecked);

/**
 * @brief Check if any checkbox is checked
 * @param component Component handle
 * @param out_any_checked Output parameter for result
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_is_any_checked(LCComponentHandle component, bool* out_any_checked);

/**
 * @brief Get the count of checked checkboxes
 * @param component Component handle
 * @param out_count Output parameter for count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_checked_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get the total count of checkboxes
 * @param component Component handle
 * @param out_count Output parameter for count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_total_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get the values of checked checkboxes
 * @param component Component handle
 * @param out_values Output array for values (caller provides buffer)
 * @param max_values Maximum values to retrieve
 * @param out_count Output parameter for actual count retrieved
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_checked_values(LCComponentHandle component, int* out_values, int max_values, int* out_count);

/* ============================================================================
 * Item Management
 * ============================================================================ */

/**
 * @brief Add an item to the checklist
 * @param component Component handle
 * @param text Item text
 * @param value Item value (-1 for auto-assigned)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_add_item(LCComponentHandle component, const char* text, int value);

/**
 * @brief Remove an item by index
 * @param component Component handle
 * @param index Index of item to remove
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_remove_item(LCComponentHandle component, int index);

/**
 * @brief Clear all items
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_clear_items(LCComponentHandle component);

/**
 * @brief Get the item count
 * @param component Component handle
 * @param out_count Output parameter for count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_item_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get whether auto-create checkboxes is enabled
 * @param component Component handle
 * @param out_auto_create Output parameter for auto-create flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_get_auto_create_checkboxes(LCComponentHandle component, bool* out_auto_create);

/**
 * @brief Set whether to auto-create checkboxes from items
 * @param component Component handle
 * @param auto_create Enable auto-creation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_checklist_set_auto_create_checkboxes(LCComponentHandle component, bool auto_create);

#endif /* LUPINE_CAPI_CHECKLIST_H */
