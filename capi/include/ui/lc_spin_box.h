/**
 * @file lc_spin_box.h
 * @brief Lupine Engine C API - SpinBox UI component (numeric field)
 */

#ifndef LUPINE_CAPI_SPIN_BOX_H
#define LUPINE_CAPI_SPIN_BOX_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a SpinBox component on a node
 */
LC_API LCResult lc_spin_box_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Value ----- */

LC_API LCResult lc_spin_box_get_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_spin_box_set_value(LCComponentHandle handle, float value);
LC_API LCResult lc_spin_box_get_min_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_spin_box_set_min_value(LCComponentHandle handle, float value);
LC_API LCResult lc_spin_box_get_max_value(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_spin_box_set_max_value(LCComponentHandle handle, float value);
LC_API LCResult lc_spin_box_get_step(LCComponentHandle handle, float* outStep);
LC_API LCResult lc_spin_box_set_step(LCComponentHandle handle, float step);
LC_API LCResult lc_spin_box_get_decimals(LCComponentHandle handle, int* outDecimals);
LC_API LCResult lc_spin_box_set_decimals(LCComponentHandle handle, int decimals);
LC_API LCResult lc_spin_box_get_editable(LCComponentHandle handle, bool* outEditable);
LC_API LCResult lc_spin_box_set_editable(LCComponentHandle handle, bool editable);

LC_API LCResult lc_spin_box_get_prefix(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_spin_box_set_prefix(LCComponentHandle handle, const char* prefix);
LC_API LCResult lc_spin_box_get_suffix(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_spin_box_set_suffix(LCComponentHandle handle, const char* suffix);

/* ----- Font ----- */

LC_API LCResult lc_spin_box_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_spin_box_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_spin_box_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_spin_box_set_font_size(LCComponentHandle handle, float size);

/* ----- Colors ----- */

LC_API LCResult lc_spin_box_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_spin_box_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_spin_box_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_spin_box_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_spin_box_get_button_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_spin_box_set_button_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_SPIN_BOX_H */
