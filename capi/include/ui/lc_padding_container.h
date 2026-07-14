/**
 * @file lc_padding_container.h
 * @brief Lupine Engine C API - PaddingContainer component
 *
 * This header provides a container that applies padding to its children.
 * Features:
 * - Configurable padding on all sides
 * - Child alignment within padded area
 * - Auto-fit children option
 * - Maintain aspect ratio option
 */

#ifndef LUPINE_CAPI_PADDING_CONTAINER_H
#define LUPINE_CAPI_PADDING_CONTAINER_H

#include "core/lc_core.h"
#include "core/lc_node.h"

/* ============================================================================
 * Enums
 * ============================================================================ */

/**
 * @brief Alignment options for children within the padded area
 */
typedef enum LCPaddingAlignment {
    LC_PADDING_ALIGN_TOP_LEFT = 0,
    LC_PADDING_ALIGN_TOP_CENTER = 1,
    LC_PADDING_ALIGN_TOP_RIGHT = 2,
    LC_PADDING_ALIGN_CENTER_LEFT = 3,
    LC_PADDING_ALIGN_CENTER = 4,
    LC_PADDING_ALIGN_CENTER_RIGHT = 5,
    LC_PADDING_ALIGN_BOTTOM_LEFT = 6,
    LC_PADDING_ALIGN_BOTTOM_CENTER = 7,
    LC_PADDING_ALIGN_BOTTOM_RIGHT = 8
} LCPaddingAlignment;

/* ============================================================================
 * PaddingContainer Creation
 * ============================================================================ */

/**
 * @brief Create a PaddingContainer component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * PaddingContainer Properties
 * ============================================================================ */

/**
 * @brief Get whether auto-fit children is enabled
 * @param component Component handle
 * @param out_auto_fit Output parameter for auto-fit state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_auto_fit_children(LCComponentHandle component, bool* out_auto_fit);

/**
 * @brief Set whether to auto-fit children
 * @param component Component handle
 * @param auto_fit Enable auto-fitting
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_auto_fit_children(LCComponentHandle component, bool auto_fit);

/**
 * @brief Get whether aspect ratio is maintained
 * @param component Component handle
 * @param out_maintain Output parameter for maintain aspect ratio state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_maintain_aspect_ratio(LCComponentHandle component, bool* out_maintain);

/**
 * @brief Set whether to maintain aspect ratio
 * @param component Component handle
 * @param maintain Enable aspect ratio preservation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_maintain_aspect_ratio(LCComponentHandle component, bool maintain);

/**
 * @brief Get child alignment
 * @param component Component handle
 * @param out_alignment Output parameter for alignment
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_child_alignment(LCComponentHandle component, LCPaddingAlignment* out_alignment);

/**
 * @brief Set child alignment
 * @param component Component handle
 * @param alignment Alignment of children within padded area
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_child_alignment(LCComponentHandle component, LCPaddingAlignment alignment);

/* ============================================================================
 * Container Size (inherited from Container)
 * ============================================================================ */

/**
 * @brief Get container width
 * @param component Component handle
 * @param out_width Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_width(LCComponentHandle component, float* out_width);

/**
 * @brief Set container width
 * @param component Component handle
 * @param width Width in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_width(LCComponentHandle component, float width);

/**
 * @brief Get container height
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set container height
 * @param component Component handle
 * @param height Height in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_height(LCComponentHandle component, float height);

/* ============================================================================
 * Padding Properties (inherited from Container)
 * ============================================================================ */

/**
 * @brief Get padding (top, right, bottom, left)
 * @param component Component handle
 * @param out_padding Output parameter for padding as Vec4
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_padding(LCComponentHandle component, LCVec4* out_padding);

/**
 * @brief Set padding (top, right, bottom, left)
 * @param component Component handle
 * @param padding Padding values as Vec4
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_padding(LCComponentHandle component, LCVec4 padding);

/**
 * @brief Set uniform padding on all sides
 * @param component Component handle
 * @param padding Padding value in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_padding_uniform(LCComponentHandle component, float padding);

/* ============================================================================
 * Rendering Properties (inherited from Container)
 * ============================================================================ */

/**
 * @brief Get the render layer
 * @param component Component handle
 * @param out_layer Output parameter for layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_layer(LCComponentHandle component, int* out_layer);

/**
 * @brief Set the render layer
 * @param component Component handle
 * @param layer Render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_layer(LCComponentHandle component, int layer);

/**
 * @brief Get the sorting order
 * @param component Component handle
 * @param out_order Output parameter for sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_sorting_order(LCComponentHandle component, int* out_order);

/**
 * @brief Set the sorting order
 * @param component Component handle
 * @param order Sorting order within layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_sorting_order(LCComponentHandle component, int order);

/**
 * @brief Get whether UI space is used
 * @param component Component handle
 * @param out_use_ui_space Output parameter for use UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space);

/**
 * @brief Set whether to use UI space
 * @param component Component handle
 * @param use_ui_space Use UI space for rendering
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_padding_container_set_use_ui_space(LCComponentHandle component, bool use_ui_space);

#endif /* LUPINE_CAPI_PADDING_CONTAINER_H */
