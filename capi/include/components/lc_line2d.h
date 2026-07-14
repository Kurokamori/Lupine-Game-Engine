/**
 * @file lc_line2d.h
 * @brief Lupine Engine C API - Line2D Component
 *
 * A 2D polyline component that renders lines with multiple points.
 * Can be used for drawing paths, borders, or decorative lines.
 *
 * Features:
 * - Multiple points forming a polyline
 * - Customizable stroke color and width
 * - Line cap styles (butt, round, square)
 * - Line join styles (miter, round, bevel)
 * - Closed loop option
 * - Anti-aliasing support
 * - Layer ordering
 */

#ifndef LUPINE_CAPI_LINE2D_H
#define LUPINE_CAPI_LINE2D_H

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
 * @brief Line cap styles (end of line)
 */
typedef enum LCLineCapStyle {
    LC_LINE_CAP_BUTT = 0,    /**< Flat cap at line end */
    LC_LINE_CAP_ROUND = 1,   /**< Rounded cap */
    LC_LINE_CAP_SQUARE = 2   /**< Square cap extending beyond end point */
} LCLineCapStyle;

/**
 * @brief Line join styles (corners)
 */
typedef enum LCLineJoinStyle {
    LC_LINE_JOIN_MITER = 0,  /**< Sharp corner */
    LC_LINE_JOIN_ROUND = 1,  /**< Rounded corner */
    LC_LINE_JOIN_BEVEL = 2   /**< Beveled corner */
} LCLineJoinStyle;

/* ============================================================================
 * Line2D Component Functions
 * ============================================================================ */

/**
 * @brief Create a Line2D component on a node
 * @param node The node to add the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Point Management ----- */

/**
 * @brief Get number of points in the line
 * @param handle Component handle
 * @param outCount Output parameter for point count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_point_count(LCComponentHandle handle, uint32_t* outCount);

/**
 * @brief Get point at index
 * @param handle Component handle
 * @param index Point index
 * @param outPoint Output parameter for point position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_point(LCComponentHandle handle, uint32_t index, LCVec2* outPoint);

/**
 * @brief Set point at index
 * @param handle Component handle
 * @param index Point index
 * @param point New point position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_point(LCComponentHandle handle, uint32_t index, LCVec2 point);

/**
 * @brief Add a point to the end of the line
 * @param handle Component handle
 * @param point Point position to add
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_add_point(LCComponentHandle handle, LCVec2 point);

/**
 * @brief Insert a point at index
 * @param handle Component handle
 * @param index Index to insert at
 * @param point Point position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_insert_point(LCComponentHandle handle, uint32_t index, LCVec2 point);

/**
 * @brief Remove point at index
 * @param handle Component handle
 * @param index Point index to remove
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_remove_point(LCComponentHandle handle, uint32_t index);

/**
 * @brief Clear all points
 * @param handle Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_clear_points(LCComponentHandle handle);

/**
 * @brief Set all points at once
 * @param handle Component handle
 * @param points Array of points
 * @param count Number of points
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_points(LCComponentHandle handle, const LCVec2* points, uint32_t count);

/**
 * @brief Get all points
 * @param handle Component handle
 * @param outPoints Output array for points (must be pre-allocated)
 * @param maxCount Maximum number of points to retrieve
 * @param outCount Actual number of points retrieved
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_points(LCComponentHandle handle, LCVec2* outPoints, uint32_t maxCount, uint32_t* outCount);

/* ----- Beginning and End Points ----- */

/**
 * @brief Get beginning position (first point)
 * @param handle Component handle
 * @param outPoint Output parameter for point position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_beginning(LCComponentHandle handle, LCVec2* outPoint);

/**
 * @brief Set beginning position (first point)
 * @param handle Component handle
 * @param point New point position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_beginning(LCComponentHandle handle, LCVec2 point);

/**
 * @brief Get end position (last point)
 * @param handle Component handle
 * @param outPoint Output parameter for point position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_end(LCComponentHandle handle, LCVec2* outPoint);

/**
 * @brief Set end position (last point)
 * @param handle Component handle
 * @param point New point position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_end(LCComponentHandle handle, LCVec2 point);

/* ----- Stroke Properties ----- */

/**
 * @brief Get stroke color
 * @param handle Component handle
 * @param outColor Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_stroke_color(LCComponentHandle handle, LCColor* outColor);

/**
 * @brief Set stroke color
 * @param handle Component handle
 * @param color New stroke color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_stroke_color(LCComponentHandle handle, LCColor color);

/**
 * @brief Get stroke width
 * @param handle Component handle
 * @param outWidth Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_stroke_width(LCComponentHandle handle, float* outWidth);

/**
 * @brief Set stroke width
 * @param handle Component handle
 * @param width New stroke width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_stroke_width(LCComponentHandle handle, float width);

/* ----- Closed Loop ----- */

/**
 * @brief Get whether the line forms a closed loop
 * @param handle Component handle
 * @param outClosed Output parameter for closed state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_closed_loop(LCComponentHandle handle, bool* outClosed);

/**
 * @brief Set whether the line forms a closed loop
 * @param handle Component handle
 * @param closed New closed state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_closed_loop(LCComponentHandle handle, bool closed);

/* ----- Cap and Join Styles ----- */

/**
 * @brief Get line cap style
 * @param handle Component handle
 * @param outStyle Output parameter for cap style
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_cap_style(LCComponentHandle handle, LCLineCapStyle* outStyle);

/**
 * @brief Set line cap style
 * @param handle Component handle
 * @param style New cap style
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_cap_style(LCComponentHandle handle, LCLineCapStyle style);

/**
 * @brief Get line join style
 * @param handle Component handle
 * @param outStyle Output parameter for join style
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_join_style(LCComponentHandle handle, LCLineJoinStyle* outStyle);

/**
 * @brief Set line join style
 * @param handle Component handle
 * @param style New join style
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_join_style(LCComponentHandle handle, LCLineJoinStyle style);

/* ----- Quality Properties ----- */

/**
 * @brief Get anti-aliasing enabled state
 * @param handle Component handle
 * @param outEnabled Output parameter for AA state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_anti_aliasing(LCComponentHandle handle, bool* outEnabled);

/**
 * @brief Set anti-aliasing enabled state
 * @param handle Component handle
 * @param enabled New AA state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_anti_aliasing(LCComponentHandle handle, bool enabled);

/**
 * @brief Get smoothness (for round caps/joins)
 * @param handle Component handle
 * @param outSmoothness Output parameter for smoothness
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_smoothness(LCComponentHandle handle, int* outSmoothness);

/**
 * @brief Set smoothness (for round caps/joins)
 * @param handle Component handle
 * @param smoothness New smoothness value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_smoothness(LCComponentHandle handle, int smoothness);

/* ----- Rendering Properties ----- */

/**
 * @brief Get render layer
 * @param handle Component handle
 * @param outLayer Output parameter for layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_layer(LCComponentHandle handle, int* outLayer);

/**
 * @brief Set render layer
 * @param handle Component handle
 * @param layer New layer value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_layer(LCComponentHandle handle, int layer);

/**
 * @brief Get sorting order within layer
 * @param handle Component handle
 * @param outOrder Output parameter for sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_sorting_order(LCComponentHandle handle, int* outOrder);

/**
 * @brief Set sorting order within layer
 * @param handle Component handle
 * @param order New sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_sorting_order(LCComponentHandle handle, int order);

/**
 * @brief Get UI space flag
 * @param handle Component handle
 * @param outUISpace Output parameter for UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_get_ui_space(LCComponentHandle handle, bool* outUISpace);

/**
 * @brief Set UI space flag
 * @param handle Component handle
 * @param uiSpace New UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_line2d_set_ui_space(LCComponentHandle handle, bool uiSpace);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_LINE2D_H */
