/**
 * @file lc_center_container.h
 * @brief Lupine Engine C API - CenterContainer component
 *
 * This header provides a container that centers its children.
 * Features:
 * - Auto-fit children to container size
 * - Maintain aspect ratio option
 * - Stack children independently or as a group
 */

#ifndef LUPINE_CAPI_CENTER_CONTAINER_H
#define LUPINE_CAPI_CENTER_CONTAINER_H

#include "core/lc_core.h"
#include "core/lc_node.h"

/* ============================================================================
 * CenterContainer Creation
 * ============================================================================ */

/**
 * @brief Create a CenterContainer component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * CenterContainer Properties
 * ============================================================================ */

/**
 * @brief Get whether auto-fit child is enabled
 * @param component Component handle
 * @param out_auto_fit Output parameter for auto-fit state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_get_auto_fit_child(LCComponentHandle component, bool* out_auto_fit);

/**
 * @brief Set whether to auto-fit children to container size
 * @param component Component handle
 * @param auto_fit Enable auto-fitting
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_set_auto_fit_child(LCComponentHandle component, bool auto_fit);

/**
 * @brief Get whether aspect ratio is maintained
 * @param component Component handle
 * @param out_maintain Output parameter for maintain aspect ratio state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_get_maintain_aspect_ratio(LCComponentHandle component, bool* out_maintain);

/**
 * @brief Set whether to maintain aspect ratio when auto-fitting
 * @param component Component handle
 * @param maintain Enable aspect ratio preservation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_set_maintain_aspect_ratio(LCComponentHandle component, bool maintain);

/**
 * @brief Get whether children are stacked
 * @param component Component handle
 * @param out_stack Output parameter for stack children state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_get_stack_children(LCComponentHandle component, bool* out_stack);

/**
 * @brief Set whether to center each child independently or stack them
 * @param component Component handle
 * @param stack Enable stacking
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_set_stack_children(LCComponentHandle component, bool stack);

/* ============================================================================
 * Container Size (inherited from Container)
 * ============================================================================ */

/**
 * @brief Get container width
 * @param component Component handle
 * @param out_width Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_get_width(LCComponentHandle component, float* out_width);

/**
 * @brief Set container width
 * @param component Component handle
 * @param width Width in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_set_width(LCComponentHandle component, float width);

/**
 * @brief Get container height
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_get_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set container height
 * @param component Component handle
 * @param height Height in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_set_height(LCComponentHandle component, float height);

/* ============================================================================
 * Rendering Properties (inherited from Container)
 * ============================================================================ */

/**
 * @brief Get the render layer
 * @param component Component handle
 * @param out_layer Output parameter for layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_get_layer(LCComponentHandle component, int* out_layer);

/**
 * @brief Set the render layer
 * @param component Component handle
 * @param layer Render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_set_layer(LCComponentHandle component, int layer);

/**
 * @brief Get the sorting order
 * @param component Component handle
 * @param out_order Output parameter for sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_get_sorting_order(LCComponentHandle component, int* out_order);

/**
 * @brief Set the sorting order
 * @param component Component handle
 * @param order Sorting order within layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_set_sorting_order(LCComponentHandle component, int order);

/**
 * @brief Get whether UI space is used
 * @param component Component handle
 * @param out_use_ui_space Output parameter for use UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space);

/**
 * @brief Set whether to use UI space
 * @param component Component handle
 * @param use_ui_space Use UI space for rendering
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_center_container_set_use_ui_space(LCComponentHandle component, bool use_ui_space);

#endif /* LUPINE_CAPI_CENTER_CONTAINER_H */
