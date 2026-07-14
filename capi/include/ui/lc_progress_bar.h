/**
 * @file lc_progress_bar.h
 * @brief Lupine Engine C API - ProgressBar and ProgressBar3D UI Components
 *
 * This header provides ProgressBar and ProgressBar3D components for progress display.
 * Progress bars support min/max values, smooth interpolation, fill directions, and styling.
 */

#ifndef LUPINE_CAPI_PROGRESS_BAR_H
#define LUPINE_CAPI_PROGRESS_BAR_H

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
 * @brief Orientation for progress bars
 */
typedef enum LCProgressBarOrientation {
    LC_PROGRESSBAR_HORIZONTAL = 0, /**< Horizontal progress bar */
    LC_PROGRESSBAR_VERTICAL = 1    /**< Vertical progress bar */
} LCProgressBarOrientation;

/**
 * @brief Fill direction for progress bars
 */
typedef enum LCProgressBarFillDirection {
    LC_FILL_LEFT_TO_RIGHT = 0, /**< Fill from left to right */
    LC_FILL_RIGHT_TO_LEFT = 1, /**< Fill from right to left */
    LC_FILL_UP_TO_DOWN = 2,    /**< Fill from top to bottom */
    LC_FILL_DOWN_TO_UP = 3     /**< Fill from bottom to top */
} LCProgressBarFillDirection;

/**
 * @brief Billboard mode for ProgressBar3D
 */
typedef enum LCProgressBar3DBillboardMode {
    LC_PROGRESSBAR3D_BILLBOARD_DISABLED = 0, /**< No billboarding */
    LC_PROGRESSBAR3D_BILLBOARD_ENABLED = 1,  /**< Full billboarding, faces camera */
    LC_PROGRESSBAR3D_BILLBOARD_Y_AXIS = 2    /**< Billboard only around Y axis */
} LCProgressBar3DBillboardMode;

/* ============================================================================
 * ProgressBar Component Functions
 * ============================================================================ */

/**
 * @brief Create a ProgressBar component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_progress_bar_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Value Properties ----- */

LC_API LCResult lc_progress_bar_get_min_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_progress_bar_set_min_value(LCComponentHandle handle, float value);
LC_API LCResult lc_progress_bar_get_max_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_progress_bar_set_max_value(LCComponentHandle handle, float value);
LC_API LCResult lc_progress_bar_get_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_progress_bar_set_value(LCComponentHandle handle, float value);
LC_API LCResult lc_progress_bar_get_step(LCComponentHandle handle, float* outStep);
LC_API LCResult lc_progress_bar_set_step(LCComponentHandle handle, float step);

/* ----- Smooth Properties ----- */

LC_API LCResult lc_progress_bar_get_smooth(LCComponentHandle handle, bool* outSmooth);
LC_API LCResult lc_progress_bar_set_smooth(LCComponentHandle handle, bool smooth);
LC_API LCResult lc_progress_bar_get_smooth_speed(LCComponentHandle handle, float* outSpeed);
LC_API LCResult lc_progress_bar_set_smooth_speed(LCComponentHandle handle, float speed);

/* ----- Size Properties ----- */

LC_API LCResult lc_progress_bar_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_progress_bar_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_progress_bar_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_progress_bar_set_height(LCComponentHandle handle, float height);

/* ----- Orientation & Fill Direction ----- */

LC_API LCResult lc_progress_bar_get_orientation(LCComponentHandle handle, LCProgressBarOrientation* outOrientation);
LC_API LCResult lc_progress_bar_set_orientation(LCComponentHandle handle, LCProgressBarOrientation orientation);
LC_API LCResult lc_progress_bar_get_fill_direction(LCComponentHandle handle, LCProgressBarFillDirection* outDirection);
LC_API LCResult lc_progress_bar_set_fill_direction(LCComponentHandle handle, LCProgressBarFillDirection direction);

/* ----- Background Texture ----- */

LC_API LCResult lc_progress_bar_get_background_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_progress_bar_set_background_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_progress_bar_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_progress_bar_set_background_color(LCComponentHandle handle, LCColor color);

/* ----- Fill Texture ----- */

LC_API LCResult lc_progress_bar_get_fill_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_progress_bar_set_fill_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_progress_bar_get_fill_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_progress_bar_set_fill_color(LCComponentHandle handle, LCColor color);

/* ----- Border Texture ----- */

LC_API LCResult lc_progress_bar_get_border_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_progress_bar_set_border_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_progress_bar_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_progress_bar_set_border_color(LCComponentHandle handle, LCColor color);

/* ----- Value Display ----- */

LC_API LCResult lc_progress_bar_get_show_value(LCComponentHandle handle, bool* outShow);
LC_API LCResult lc_progress_bar_set_show_value(LCComponentHandle handle, bool show);
LC_API LCResult lc_progress_bar_get_value_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_progress_bar_set_value_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_progress_bar_get_value_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_progress_bar_set_value_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_progress_bar_get_value_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_progress_bar_set_value_color(LCComponentHandle handle, LCColor color);

/* ----- Corner Radius ----- */

LC_API LCResult lc_progress_bar_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_progress_bar_set_corner_radius_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_progress_bar_set_corner_radius_all(LCComponentHandle handle, float radius);
LC_API LCResult lc_progress_bar_set_corner_radius_individual(LCComponentHandle handle, int index, float radius);
LC_API LCResult lc_progress_bar_get_corner_radius(LCComponentHandle handle, int index, float* outRadius);

/* ----- Border Width ----- */

LC_API LCResult lc_progress_bar_get_border_width_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_progress_bar_set_border_width_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_progress_bar_set_border_width_all(LCComponentHandle handle, float width);
LC_API LCResult lc_progress_bar_set_border_width_individual(LCComponentHandle handle, int index, float width);
LC_API LCResult lc_progress_bar_get_border_width(LCComponentHandle handle, int index, float* outWidth);

/* ============================================================================
 * ProgressBar3D Component Functions
 * ============================================================================ */

/**
 * @brief Create a ProgressBar3D component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_progress_bar3d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Value Properties ----- */

LC_API LCResult lc_progress_bar3d_get_min_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_progress_bar3d_set_min_value(LCComponentHandle handle, float value);
LC_API LCResult lc_progress_bar3d_get_max_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_progress_bar3d_set_max_value(LCComponentHandle handle, float value);
LC_API LCResult lc_progress_bar3d_get_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_progress_bar3d_set_value(LCComponentHandle handle, float value);
LC_API LCResult lc_progress_bar3d_get_step(LCComponentHandle handle, float* outStep);
LC_API LCResult lc_progress_bar3d_set_step(LCComponentHandle handle, float step);

/* ----- Smooth Properties ----- */

LC_API LCResult lc_progress_bar3d_get_smooth(LCComponentHandle handle, bool* outSmooth);
LC_API LCResult lc_progress_bar3d_set_smooth(LCComponentHandle handle, bool smooth);
LC_API LCResult lc_progress_bar3d_get_smooth_speed(LCComponentHandle handle, float* outSpeed);
LC_API LCResult lc_progress_bar3d_set_smooth_speed(LCComponentHandle handle, float speed);

/* ----- Size Properties ----- */

LC_API LCResult lc_progress_bar3d_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_progress_bar3d_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_progress_bar3d_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_progress_bar3d_set_height(LCComponentHandle handle, float height);

/* ----- Orientation & Fill Direction ----- */

LC_API LCResult lc_progress_bar3d_get_orientation(LCComponentHandle handle, LCProgressBarOrientation* outOrientation);
LC_API LCResult lc_progress_bar3d_set_orientation(LCComponentHandle handle, LCProgressBarOrientation orientation);
LC_API LCResult lc_progress_bar3d_get_fill_direction(LCComponentHandle handle, LCProgressBarFillDirection* outDirection);
LC_API LCResult lc_progress_bar3d_set_fill_direction(LCComponentHandle handle, LCProgressBarFillDirection direction);

/* ----- 3D Rendering Properties ----- */

LC_API LCResult lc_progress_bar3d_get_billboard_mode(LCComponentHandle handle, LCProgressBar3DBillboardMode* outMode);
LC_API LCResult lc_progress_bar3d_set_billboard_mode(LCComponentHandle handle, LCProgressBar3DBillboardMode mode);
LC_API LCResult lc_progress_bar3d_get_double_sided(LCComponentHandle handle, bool* outDoubleSided);
LC_API LCResult lc_progress_bar3d_set_double_sided(LCComponentHandle handle, bool doubleSided);
LC_API LCResult lc_progress_bar3d_get_cast_shadow(LCComponentHandle handle, bool* outCastShadow);
LC_API LCResult lc_progress_bar3d_set_cast_shadow(LCComponentHandle handle, bool castShadow);
LC_API LCResult lc_progress_bar3d_get_receive_shadow(LCComponentHandle handle, bool* outReceiveShadow);
LC_API LCResult lc_progress_bar3d_set_receive_shadow(LCComponentHandle handle, bool receiveShadow);

/* ----- Background Texture ----- */

LC_API LCResult lc_progress_bar3d_get_background_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_progress_bar3d_set_background_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_progress_bar3d_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_progress_bar3d_set_background_color(LCComponentHandle handle, LCColor color);

/* ----- Fill Texture ----- */

LC_API LCResult lc_progress_bar3d_get_fill_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_progress_bar3d_set_fill_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_progress_bar3d_get_fill_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_progress_bar3d_set_fill_color(LCComponentHandle handle, LCColor color);

/* ----- Border Texture ----- */

LC_API LCResult lc_progress_bar3d_get_border_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_progress_bar3d_set_border_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_progress_bar3d_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_progress_bar3d_set_border_color(LCComponentHandle handle, LCColor color);

/* ----- Value Display ----- */

LC_API LCResult lc_progress_bar3d_get_show_value(LCComponentHandle handle, bool* outShow);
LC_API LCResult lc_progress_bar3d_set_show_value(LCComponentHandle handle, bool show);
LC_API LCResult lc_progress_bar3d_get_value_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_progress_bar3d_set_value_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_progress_bar3d_get_value_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_progress_bar3d_set_value_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_progress_bar3d_get_value_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_progress_bar3d_set_value_color(LCComponentHandle handle, LCColor color);

/* ----- Corner Radius ----- */

LC_API LCResult lc_progress_bar3d_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_progress_bar3d_set_corner_radius_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_progress_bar3d_set_corner_radius_all(LCComponentHandle handle, float radius);
LC_API LCResult lc_progress_bar3d_set_corner_radius_individual(LCComponentHandle handle, int index, float radius);
LC_API LCResult lc_progress_bar3d_get_corner_radius(LCComponentHandle handle, int index, float* outRadius);

/* ----- Border Width ----- */

LC_API LCResult lc_progress_bar3d_get_border_width_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_progress_bar3d_set_border_width_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_progress_bar3d_set_border_width_all(LCComponentHandle handle, float width);
LC_API LCResult lc_progress_bar3d_set_border_width_individual(LCComponentHandle handle, int index, float width);
LC_API LCResult lc_progress_bar3d_get_border_width(LCComponentHandle handle, int index, float* outWidth);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_PROGRESS_BAR_H */
