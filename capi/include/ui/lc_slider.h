/**
 * @file lc_slider.h
 * @brief Lupine Engine C API - Slider UI component
 */

#ifndef LUPINE_CAPI_SLIDER_H
#define LUPINE_CAPI_SLIDER_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Slider orientation
 */
typedef enum LCSliderOrientation {
    LC_SLIDER_HORIZONTAL = 0,
    LC_SLIDER_VERTICAL = 1
} LCSliderOrientation;

/**
 * @brief Create a Slider component on a node
 */
LC_API LCResult lc_slider_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Value ----- */

LC_API LCResult lc_slider_get_min_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_slider_set_min_value(LCComponentHandle handle, float value);
LC_API LCResult lc_slider_get_max_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_slider_set_max_value(LCComponentHandle handle, float value);
LC_API LCResult lc_slider_get_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_slider_set_value(LCComponentHandle handle, float value);
LC_API LCResult lc_slider_get_step(LCComponentHandle handle, float* outStep);
LC_API LCResult lc_slider_set_step(LCComponentHandle handle, float step);
LC_API LCResult lc_slider_get_ratio(LCComponentHandle handle, float* outRatio);

LC_API LCResult lc_slider_get_orientation(LCComponentHandle handle, LCSliderOrientation* outOrientation);
LC_API LCResult lc_slider_set_orientation(LCComponentHandle handle, LCSliderOrientation orientation);
LC_API LCResult lc_slider_get_editable(LCComponentHandle handle, bool* outEditable);
LC_API LCResult lc_slider_set_editable(LCComponentHandle handle, bool editable);

/* ----- Appearance ----- */

LC_API LCResult lc_slider_get_track_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_slider_set_track_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_slider_get_fill_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_slider_set_fill_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_slider_get_handle_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_slider_set_handle_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_slider_get_track_thickness(LCComponentHandle handle, float* outThickness);
LC_API LCResult lc_slider_set_track_thickness(LCComponentHandle handle, float thickness);
LC_API LCResult lc_slider_get_handle_radius(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_slider_set_handle_radius(LCComponentHandle handle, float radius);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_SLIDER_H */
