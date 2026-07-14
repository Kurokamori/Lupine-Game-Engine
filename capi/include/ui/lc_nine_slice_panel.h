/**
 * @file lc_nine_slice_panel.h
 * @brief Lupine Engine C API - NineSlicePanel component
 *
 * This header provides a panel that uses nine-slice texture scaling.
 * Features:
 * - Nine-slice margins for corners and edges
 * - Texture path for the panel image
 * - Color modulation
 * - Scalable panel with preserved corners
 */

#ifndef LUPINE_CAPI_NINE_SLICE_PANEL_H
#define LUPINE_CAPI_NINE_SLICE_PANEL_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * NineSlicePanel Creation
 * ============================================================================ */

/**
 * @brief Create a NineSlicePanel component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Size Properties
 * ============================================================================ */

/**
 * @brief Get panel width
 * @param component Component handle
 * @param out_width Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_width(LCComponentHandle component, float* out_width);

/**
 * @brief Set panel width
 * @param component Component handle
 * @param width Width in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_width(LCComponentHandle component, float width);

/**
 * @brief Get panel height
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set panel height
 * @param component Component handle
 * @param height Height in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_height(LCComponentHandle component, float height);

/* ============================================================================
 * Texture Properties
 * ============================================================================ */

/**
 * @brief Get texture path
 * @param component Component handle
 * @param out_buffer Output buffer for path
 * @param buffer_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_texture_path(LCComponentHandle component, char* out_buffer, size_t buffer_size);

/**
 * @brief Set texture path
 * @param component Component handle
 * @param path Path to the texture file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_texture_path(LCComponentHandle component, const char* path);

/**
 * @brief Get color modulation
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_modulate(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set color modulation
 * @param component Component handle
 * @param color Color to modulate the texture
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_modulate(LCComponentHandle component, LCColor color);

/* ============================================================================
 * Nine-Slice Margins
 * ============================================================================ */

/**
 * @brief Get left margin
 * @param component Component handle
 * @param out_margin Output parameter for margin
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_margin_left(LCComponentHandle component, float* out_margin);

/**
 * @brief Set left margin
 * @param component Component handle
 * @param margin Left margin in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_margin_left(LCComponentHandle component, float margin);

/**
 * @brief Get right margin
 * @param component Component handle
 * @param out_margin Output parameter for margin
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_margin_right(LCComponentHandle component, float* out_margin);

/**
 * @brief Set right margin
 * @param component Component handle
 * @param margin Right margin in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_margin_right(LCComponentHandle component, float margin);

/**
 * @brief Get top margin
 * @param component Component handle
 * @param out_margin Output parameter for margin
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_margin_top(LCComponentHandle component, float* out_margin);

/**
 * @brief Set top margin
 * @param component Component handle
 * @param margin Top margin in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_margin_top(LCComponentHandle component, float margin);

/**
 * @brief Get bottom margin
 * @param component Component handle
 * @param out_margin Output parameter for margin
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_margin_bottom(LCComponentHandle component, float* out_margin);

/**
 * @brief Set bottom margin
 * @param component Component handle
 * @param margin Bottom margin in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_margin_bottom(LCComponentHandle component, float margin);

/**
 * @brief Set all margins at once
 * @param component Component handle
 * @param left Left margin
 * @param right Right margin
 * @param top Top margin
 * @param bottom Bottom margin
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_margins(LCComponentHandle component, float left, float right, float top, float bottom);

/**
 * @brief Set uniform margins on all sides
 * @param component Component handle
 * @param margin Margin value in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_margins_uniform(LCComponentHandle component, float margin);

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

/**
 * @brief Get the render layer
 * @param component Component handle
 * @param out_layer Output parameter for layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_layer(LCComponentHandle component, int* out_layer);

/**
 * @brief Set the render layer
 * @param component Component handle
 * @param layer Render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_layer(LCComponentHandle component, int layer);

/**
 * @brief Get the sorting order
 * @param component Component handle
 * @param out_order Output parameter for sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_sorting_order(LCComponentHandle component, int* out_order);

/**
 * @brief Set the sorting order
 * @param component Component handle
 * @param order Sorting order within layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_sorting_order(LCComponentHandle component, int order);

/**
 * @brief Get whether UI space is used
 * @param component Component handle
 * @param out_use_ui_space Output parameter for use UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space);

/**
 * @brief Set whether to use UI space
 * @param component Component handle
 * @param use_ui_space Use UI space for rendering
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_nine_slice_panel_set_use_ui_space(LCComponentHandle component, bool use_ui_space);

#endif /* LUPINE_CAPI_NINE_SLICE_PANEL_H */
