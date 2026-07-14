/**
 * @file lc_shape2d.h
 * @brief Lupine Engine C API - Shape2D component
 *
 * This header provides 2D shape rendering.
 * Features:
 * - Multiple shape types (circle, square, triangle, pentagon, hexagon)
 * - Fill and border options
 * - Configurable colors and sizes
 */

#ifndef LUPINE_CAPI_SHAPE2D_H
#define LUPINE_CAPI_SHAPE2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * Enums
 * ============================================================================ */

/**
 * @brief Shape types for Shape2D
 */
typedef enum LCShape2DType {
    LC_SHAPE2D_CIRCLE = 0,
    LC_SHAPE2D_SQUARE = 1,
    LC_SHAPE2D_TRIANGLE = 2,
    LC_SHAPE2D_PENTAGON = 3,
    LC_SHAPE2D_HEXAGON = 4
} LCShape2DType;

/* ============================================================================
 * Shape2D Creation
 * ============================================================================ */

/**
 * @brief Create a Shape2D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Shape Type
 * ============================================================================ */

/**
 * @brief Get shape type
 * @param component Component handle
 * @param out_type Output parameter for shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_shape_type(LCComponentHandle component, LCShape2DType* out_type);

/**
 * @brief Set shape type
 * @param component Component handle
 * @param type Shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_shape_type(LCComponentHandle component, LCShape2DType type);

/* ============================================================================
 * Fill Properties
 * ============================================================================ */

/**
 * @brief Get fill color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set fill color
 * @param component Component handle
 * @param color Fill color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get whether shape is filled
 * @param component Component handle
 * @param out_filled Output parameter for filled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_filled(LCComponentHandle component, bool* out_filled);

/**
 * @brief Set whether shape is filled
 * @param component Component handle
 * @param filled Enable fill
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_filled(LCComponentHandle component, bool filled);

/* ============================================================================
 * Size Properties
 * ============================================================================ */

/**
 * @brief Get overall size
 * @param component Component handle
 * @param out_size Output parameter for size
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_size(LCComponentHandle component, float* out_size);

/**
 * @brief Set overall size
 * @param component Component handle
 * @param size Size in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_size(LCComponentHandle component, float size);

/**
 * @brief Get width (for rectangles)
 * @param component Component handle
 * @param out_width Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_width(LCComponentHandle component, float* out_width);

/**
 * @brief Set width (for rectangles)
 * @param component Component handle
 * @param width Width in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_width(LCComponentHandle component, float width);

/**
 * @brief Get height (for rectangles)
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set height (for rectangles)
 * @param component Component handle
 * @param height Height in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_height(LCComponentHandle component, float height);

/**
 * @brief Get radius (for circles and polygons)
 * @param component Component handle
 * @param out_radius Output parameter for radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_radius(LCComponentHandle component, float* out_radius);

/**
 * @brief Set radius (for circles and polygons)
 * @param component Component handle
 * @param radius Radius in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_radius(LCComponentHandle component, float radius);

/* ============================================================================
 * Border Properties
 * ============================================================================ */

/**
 * @brief Get whether border is enabled
 * @param component Component handle
 * @param out_enabled Output parameter for border state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_border_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether border is enabled
 * @param component Component handle
 * @param enabled Enable border
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_border_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Get border color
 * @param component Component handle
 * @param out_color Output parameter for border color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_border_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set border color
 * @param component Component handle
 * @param color Border color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_border_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get border width
 * @param component Component handle
 * @param out_width Output parameter for border width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_border_width(LCComponentHandle component, float* out_width);

/**
 * @brief Set border width
 * @param component Component handle
 * @param width Border width in pixels
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_border_width(LCComponentHandle component, float width);

/* ============================================================================
 * Circle Properties
 * ============================================================================ */

/**
 * @brief Get circle segment count (smoothness)
 * @param component Component handle
 * @param out_segments Output parameter for segments
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_circle_segments(LCComponentHandle component, int* out_segments);

/**
 * @brief Set circle segment count (smoothness)
 * @param component Component handle
 * @param segments Number of segments
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_circle_segments(LCComponentHandle component, int segments);

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

/**
 * @brief Get the render layer
 * @param component Component handle
 * @param out_layer Output parameter for layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_layer(LCComponentHandle component, int* out_layer);

/**
 * @brief Set the render layer
 * @param component Component handle
 * @param layer Render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_layer(LCComponentHandle component, int layer);

/**
 * @brief Get the sorting order
 * @param component Component handle
 * @param out_order Output parameter for sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_sorting_order(LCComponentHandle component, int* out_order);

/**
 * @brief Set the sorting order
 * @param component Component handle
 * @param order Sorting order within layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_sorting_order(LCComponentHandle component, int order);

/**
 * @brief Get whether UI space is used
 * @param component Component handle
 * @param out_ui_space Output parameter for UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_get_ui_space(LCComponentHandle component, bool* out_ui_space);

/**
 * @brief Set whether to use UI space
 * @param component Component handle
 * @param ui_space Use UI space for rendering
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shape2d_set_ui_space(LCComponentHandle component, bool ui_space);

/* ----- Custom Shader (.lsh) ----- */

/**
 * @brief Get/Set the attached Lupine Shader (.lsh) path. Empty = built-in primitives.
 * When set, the shape (fill + border) is rendered by the custom shader.
 */
LC_API LCResult lc_shape2d_get_shader(LCComponentHandle component, char* out_path, uint32_t max_length);
LC_API LCResult lc_shape2d_set_shader(LCComponentHandle component, const char* path);

/**
 * @brief Get/Set the serialized exported-shader-parameter values (JSON object string).
 */
LC_API LCResult lc_shape2d_get_shader_parameters(LCComponentHandle component, char* out_json, uint32_t max_length);
LC_API LCResult lc_shape2d_set_shader_parameters(LCComponentHandle component, const char* json);

#endif /* LUPINE_CAPI_SHAPE2D_H */
