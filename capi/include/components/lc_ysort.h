/**
 * @file lc_ysort.h
 * @brief Lupine Engine C API - YSort component
 *
 * This header provides Y-axis depth sorting for 2D scenes.
 * Automatically sorts child nodes based on their Y (or X) position
 * to create believable depth in 2D space.
 */

#ifndef LUPINE_CAPI_YSORT_H
#define LUPINE_CAPI_YSORT_H

#include "core/lc_core.h"
#include "core/lc_node.h"

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Axis to use for sorting
 */
typedef enum LCYSortAxis {
    LC_YSORT_AXIS_Y = 0,  /**< Sort by Y position (default) */
    LC_YSORT_AXIS_X = 1   /**< Sort by X position */
} LCYSortAxis;

/**
 * @brief Update mode for automatic sorting
 */
typedef enum LCYSortUpdateMode {
    LC_YSORT_UPDATE_EVERY_FRAME = 0,       /**< Sort every frame */
    LC_YSORT_UPDATE_ON_TRANSFORM_CHANGE = 1, /**< Sort when transforms change */
    LC_YSORT_UPDATE_MANUAL = 2             /**< Only sort when Sort() is called */
} LCYSortUpdateMode;

/* ============================================================================
 * YSort Creation
 * ============================================================================ */

/**
 * @brief Create a YSort component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

/**
 * @brief Get whether YSort is enabled
 * @param component Component handle
 * @param out_enabled Output parameter for enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_get_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether YSort is enabled
 * @param component Component handle
 * @param enabled Enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_set_enabled(LCComponentHandle component, bool enabled);

/* ============================================================================
 * Sort Axis
 * ============================================================================ */

/**
 * @brief Get the sort axis
 * @param component Component handle
 * @param out_axis Output parameter for sort axis
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_get_sort_axis(LCComponentHandle component, LCYSortAxis* out_axis);

/**
 * @brief Set the sort axis
 * @param component Component handle
 * @param axis Sort axis (Y or X)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_set_sort_axis(LCComponentHandle component, LCYSortAxis axis);

/* ============================================================================
 * Invert
 * ============================================================================ */

/**
 * @brief Get whether sort order is inverted
 * @param component Component handle
 * @param out_invert Output parameter for invert state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_get_invert(LCComponentHandle component, bool* out_invert);

/**
 * @brief Set whether to invert sort order
 * @param component Component handle
 * @param invert Invert state (true = higher values drawn first)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_set_invert(LCComponentHandle component, bool invert);

/* ============================================================================
 * Update Mode
 * ============================================================================ */

/**
 * @brief Get the update mode
 * @param component Component handle
 * @param out_mode Output parameter for update mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_get_update_mode(LCComponentHandle component, LCYSortUpdateMode* out_mode);

/**
 * @brief Set the update mode
 * @param component Component handle
 * @param mode Update mode (EveryFrame, OnTransformChange, or Manual)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_set_update_mode(LCComponentHandle component, LCYSortUpdateMode mode);

/* ============================================================================
 * Z Offset
 * ============================================================================ */

/**
 * @brief Get the Z offset
 * @param component Component handle
 * @param out_offset Output parameter for Z offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_get_z_offset(LCComponentHandle component, int* out_offset);

/**
 * @brief Set the Z offset
 * @param component Component handle
 * @param offset Base Z offset for sorted nodes
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_set_z_offset(LCComponentHandle component, int offset);

/* ============================================================================
 * Sprite Children Filter
 * ============================================================================ */

/**
 * @brief Get whether only sprite children are affected
 * @param component Component handle
 * @param out_sprite_only Output parameter for sprite-only state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_get_affect_only_sprite_children(LCComponentHandle component, bool* out_sprite_only);

/**
 * @brief Set whether to only affect sprite children
 * @param component Component handle
 * @param sprite_only When true, only nodes with sprite components are sorted
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_set_affect_only_sprite_children(LCComponentHandle component, bool sprite_only);

/* ============================================================================
 * Manual Sorting
 * ============================================================================ */

/**
 * @brief Trigger a manual sort
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @note Only needed when using LC_YSORT_UPDATE_MANUAL mode
 */
LC_API LCResult lc_ysort_sort(LCComponentHandle component);

/**
 * @brief Force immediate sort and reset dirty flags
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_ysort_force_sort(LCComponentHandle component);

#endif /* LUPINE_CAPI_YSORT_H */
