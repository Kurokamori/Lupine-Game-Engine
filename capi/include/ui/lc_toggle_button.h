/**
 * @file lc_toggle_button.h
 * @brief Lupine Engine C API - ToggleButton Component
 *
 * Toggle on/off button with StyleBox backgrounds, text labels,
 * per-state modulation, sounds, and tweens.
 */

#ifndef LUPINE_CAPI_TOGGLE_BUTTON_H
#define LUPINE_CAPI_TOGGLE_BUTTON_H

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
 * @brief ToggleButton mouse states
 */
typedef enum LCToggleButtonState {
    LC_TOGGLE_BUTTON_STATE_NORMAL = 0,
    LC_TOGGLE_BUTTON_STATE_HOVER = 1,
    LC_TOGGLE_BUTTON_STATE_PRESSED = 2,
    LC_TOGGLE_BUTTON_STATE_TOGGLED = 3,
    LC_TOGGLE_BUTTON_STATE_TOGGLED_HOVER = 4,
    LC_TOGGLE_BUTTON_STATE_TOGGLED_PRESSED = 5,
    LC_TOGGLE_BUTTON_STATE_DISABLED = 6
} LCToggleButtonState;

/**
 * @brief ToggleButton style mode
 */
typedef enum LCToggleButtonStyleMode {
    LC_TOGGLE_BUTTON_STYLE_AUTOMATIC = 0,  /**< Single style with per-state modulation */
    LC_TOGGLE_BUTTON_STYLE_MANUAL = 1      /**< Full StyleBox per state */
} LCToggleButtonStyleMode;

/* ============================================================================
 * ToggleButton Component Functions
 * ============================================================================ */

/**
 * @brief Create a ToggleButton component on a node
 */
LC_API LCResult lc_toggle_button_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_toggle_button_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_toggle_button_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_toggle_button_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_toggle_button_set_height(LCComponentHandle handle, float height);

/* ----- Layer Properties ----- */

LC_API LCResult lc_toggle_button_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_toggle_button_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_toggle_button_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_toggle_button_set_sorting_order(LCComponentHandle handle, int order);

/* ----- UI Space ----- */

LC_API LCResult lc_toggle_button_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace);
LC_API LCResult lc_toggle_button_set_use_ui_space(LCComponentHandle handle, bool useUISpace);

/* ----- Button State ----- */

LC_API LCResult lc_toggle_button_set_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_toggle_button_is_enabled(LCComponentHandle handle, bool* outEnabled);

/* ----- Style Mode ----- */

LC_API LCResult lc_toggle_button_get_style_mode(LCComponentHandle handle, LCToggleButtonStyleMode* outMode);
LC_API LCResult lc_toggle_button_set_style_mode(LCComponentHandle handle, LCToggleButtonStyleMode mode);

/* ----- Toggle State ----- */

LC_API LCResult lc_toggle_button_is_toggled(LCComponentHandle handle, bool* outToggled);
LC_API LCResult lc_toggle_button_set_toggled(LCComponentHandle handle, bool toggled);

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_toggle_button_get_state_modulation(LCComponentHandle handle, LCToggleButtonState state, LCColor* outColor);
LC_API LCResult lc_toggle_button_set_state_modulation(LCComponentHandle handle, LCToggleButtonState state, LCColor color);

/* ----- Per-State Sounds ----- */

LC_API LCResult lc_toggle_button_set_state_sound_path(LCComponentHandle handle, LCToggleButtonState state, const char* path);

/* ----- Text Properties ----- */

LC_API LCResult lc_toggle_button_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength);
LC_API LCResult lc_toggle_button_set_text(LCComponentHandle handle, const char* text);
LC_API LCResult lc_toggle_button_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_toggle_button_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_toggle_button_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_toggle_button_set_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_toggle_button_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_toggle_button_set_font_color(LCComponentHandle handle, LCColor color);

/* ----- StyleBox Properties ----- */

LC_API LCResult lc_toggle_button_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_toggle_button_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_toggle_button_get_opacity(LCComponentHandle handle, float* outOpacity);
LC_API LCResult lc_toggle_button_set_opacity(LCComponentHandle handle, float opacity);
LC_API LCResult lc_toggle_button_get_border_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_toggle_button_set_border_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_toggle_button_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_toggle_button_set_border_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_TOGGLE_BUTTON_H */
