/**
 * @file lc_dock_container.h
 * @brief Lupine Engine C API - DockContainer component
 *
 * This header provides a docking layout container.
 * Features:
 * - Dock children to top, bottom, left, right, or center
 * - Configurable spacing between docked elements
 * - Automatic layout management
 */

#ifndef LUPINE_CAPI_DOCK_CONTAINER_H
#define LUPINE_CAPI_DOCK_CONTAINER_H

#include "core/lc_core.h"
#include "core/lc_node.h"

/* ============================================================================
 * Enums
 * ============================================================================ */

/**
 * @brief Dock side options for children
 */
typedef enum LCDockSide {
    LC_DOCK_TOP = 0,
    LC_DOCK_BOTTOM = 1,
    LC_DOCK_LEFT = 2,
    LC_DOCK_RIGHT = 3,
    LC_DOCK_CENTER = 4
} LCDockSide;

/* ============================================================================
 * DockContainer Creation
 * ============================================================================ */

/**
 * @brief Create a DockContainer component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * DockContainer Properties
 * ============================================================================ */

/**
 * @brief Get dock spacing
 * @param component Component handle
 * @param out_spacing Output parameter for spacing
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_get_dock_spacing(LCComponentHandle component, float* out_spacing);

/**
 * @brief Set dock spacing
 * @param component Component handle
 * @param spacing Spacing between docked elements in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_set_dock_spacing(LCComponentHandle component, float spacing);

/**
 * @brief Get default dock side
 * @param component Component handle
 * @param out_side Output parameter for dock side
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_get_default_dock_side(LCComponentHandle component, LCDockSide* out_side);

/**
 * @brief Set default dock side for new children
 * @param component Component handle
 * @param side Default dock side
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_set_default_dock_side(LCComponentHandle component, LCDockSide side);

/* ============================================================================
 * Container Size (inherited from Container)
 * ============================================================================ */

/**
 * @brief Get container width
 * @param component Component handle
 * @param out_width Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_get_width(LCComponentHandle component, float* out_width);

/**
 * @brief Set container width
 * @param component Component handle
 * @param width Width in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_set_width(LCComponentHandle component, float width);

/**
 * @brief Get container height
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_get_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set container height
 * @param component Component handle
 * @param height Height in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_set_height(LCComponentHandle component, float height);

/* ============================================================================
 * Rendering Properties (inherited from Container)
 * ============================================================================ */

/**
 * @brief Get the render layer
 * @param component Component handle
 * @param out_layer Output parameter for layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_get_layer(LCComponentHandle component, int* out_layer);

/**
 * @brief Set the render layer
 * @param component Component handle
 * @param layer Render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_set_layer(LCComponentHandle component, int layer);

/**
 * @brief Get the sorting order
 * @param component Component handle
 * @param out_order Output parameter for sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_get_sorting_order(LCComponentHandle component, int* out_order);

/**
 * @brief Set the sorting order
 * @param component Component handle
 * @param order Sorting order within layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_set_sorting_order(LCComponentHandle component, int order);

/**
 * @brief Get whether UI space is used
 * @param component Component handle
 * @param out_use_ui_space Output parameter for use UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space);

/**
 * @brief Set whether to use UI space
 * @param component Component handle
 * @param use_ui_space Use UI space for rendering
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_dock_container_set_use_ui_space(LCComponentHandle component, bool use_ui_space);

#endif /* LUPINE_CAPI_DOCK_CONTAINER_H */
