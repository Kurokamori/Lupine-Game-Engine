/**
 * @file lc_button.h
 * @brief Lupine Engine C API - Button UI component
 *
 * This header provides a comprehensive button component with states, styles,
 * text, sounds, tweens, and callbacks.
 */

#ifndef LUPINE_CAPI_BUTTON_H
#define LUPINE_CAPI_BUTTON_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"


/* ============================================================================
 * Button Enums
 * ============================================================================ */

/**
 * @brief Button mouse states
 */
typedef enum LCButtonState {
    LC_BUTTON_STATE_NORMAL = 0,
    LC_BUTTON_STATE_HOVER = 1,
    LC_BUTTON_STATE_PRESSED = 2,
    LC_BUTTON_STATE_DISABLED = 3,
    LC_BUTTON_STATE_COUNT = 4
} LCButtonState;

/**
 * @brief Button style mode
 */
typedef enum LCButtonStyleMode {
    LC_BUTTON_STYLE_AUTOMATIC = 0,  /* Single style with per-state color modulation */
    LC_BUTTON_STYLE_MANUAL = 1      /* Full StyleBox per state (not yet supported in C API) */
} LCButtonStyleMode;

/**
 * @brief Button scale mode
 */
typedef enum LCButtonScaleMode {
    LC_BUTTON_SCALE_FIXED = 0,           /* Fixed size */
    LC_BUTTON_SCALE_FIT_TO_TEXT = 1,     /* Scale to fit text with padding */
    LC_BUTTON_SCALE_FIT_TO_TEXT_WIDTH = 2 /* Scale width only to fit text */
} LCButtonScaleMode;

/* ============================================================================
 * Button Structures
 * ============================================================================ */

/**
 * @brief Per-state sound data
 */
typedef struct LCButtonStateSound {
    char audioPath[256];      /* Path to audio file */
    char busName[64];         /* Audio bus name (default: "SFX") */
    float volume;             /* Volume (0.0 to 1.0) */
} LCButtonStateSound;

/**
 * @brief Per-state tween data
 */
typedef struct LCButtonStateTween {
    bool enabled;                  /* Tween enabled */
    LCVec2 scaleOffset;           /* Scale change */
    float rotationOffset;          /* Rotation change (degrees) */
    LCVec2 positionOffset;        /* Position offset */
    float duration;                /* Tween duration (seconds) */
} LCButtonStateTween;

/**
 * @brief Button state callback function pointer
 * @param user_data User data passed when setting the callback
 */
typedef void (*LCButtonStateCallback)(void* user_data);

/* ============================================================================
 * Button Functions
 * ============================================================================ */

/**
 * @brief Create a Button component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_button_create(const char* name, LCComponentHandle* out_component);

/* ===== Size Properties ===== */

/**
 * @brief Get the width of a Button
 * @param component Component handle
 * @param out_width Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_width(LCComponentHandle component, float* out_width);

/**
 * @brief Set the width of a Button
 * @param component Component handle
 * @param width Width (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_width(LCComponentHandle component, float width);

/**
 * @brief Get the height of a Button
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set the height of a Button
 * @param component Component handle
 * @param height Height (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_height(LCComponentHandle component, float height);

/* ===== Layer Properties ===== */

/**
 * @brief Get the layer of a Button
 * @param component Component handle
 * @param out_layer Output parameter for layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_layer(LCComponentHandle component, int* out_layer);

/**
 * @brief Set the layer of a Button
 * @param component Component handle
 * @param layer Render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_layer(LCComponentHandle component, int layer);

/**
 * @brief Get the sorting order of a Button
 * @param component Component handle
 * @param out_order Output parameter for sorting order
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_sorting_order(LCComponentHandle component, int* out_order);

/**
 * @brief Set the sorting order of a Button
 * @param component Component handle
 * @param order Sorting order within layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_sorting_order(LCComponentHandle component, int order);

/* ===== UI Space ===== */

/**
 * @brief Check if a Button uses UI space
 * @param component Component handle
 * @param out_use_ui_space Output parameter for UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space);

/**
 * @brief Set whether a Button uses UI space
 * @param component Component handle
 * @param use_ui_space UI space flag
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_use_ui_space(LCComponentHandle component, bool use_ui_space);

/* ===== Button State ===== */

/**
 * @brief Get the current state of a Button
 * @param component Component handle
 * @param out_state Output parameter for button state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_current_state(LCComponentHandle component, LCButtonState* out_state);

/**
 * @brief Set whether a Button is enabled
 * @param component Component handle
 * @param enabled Enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Check if a Button is enabled
 * @param component Component handle
 * @param out_enabled Output parameter for enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_is_enabled(LCComponentHandle component, bool* out_enabled);

/* ===== Style Mode ===== */

/**
 * @brief Get the style mode of a Button
 * @param component Component handle
 * @param out_mode Output parameter for style mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_style_mode(LCComponentHandle component, LCButtonStyleMode* out_mode);

/**
 * @brief Set the style mode of a Button
 * @param component Component handle
 * @param mode Style mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_style_mode(LCComponentHandle component, LCButtonStyleMode mode);

/* ===== Scale Mode ===== */

/**
 * @brief Get the scale mode of a Button
 * @param component Component handle
 * @param out_mode Output parameter for scale mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_scale_mode(LCComponentHandle component, LCButtonScaleMode* out_mode);

/**
 * @brief Set the scale mode of a Button
 * @param component Component handle
 * @param mode Scale mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_scale_mode(LCComponentHandle component, LCButtonScaleMode mode);

/**
 * @brief Get the text padding of a Button
 * @param component Component handle
 * @param out_padding Output parameter for text padding
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_text_padding(LCComponentHandle component, LCVec2* out_padding);

/**
 * @brief Set the text padding of a Button
 * @param component Component handle
 * @param padding Text padding (for FitToText modes)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_text_padding(LCComponentHandle component, LCVec2 padding);

/* ===== Base Style (Automatic Mode) ===== */

/**
 * @brief Get the background color of a Button
 * @param component Component handle
 * @param out_color Output parameter for background color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_background_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the background color of a Button
 * @param component Component handle
 * @param color Background color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_background_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the opacity of a Button
 * @param component Component handle
 * @param out_opacity Output parameter for opacity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_opacity(LCComponentHandle component, float* out_opacity);

/**
 * @brief Set the opacity of a Button
 * @param component Component handle
 * @param opacity Opacity (0.0 to 1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_opacity(LCComponentHandle component, float opacity);

/**
 * @brief Check if border widths are linked
 * @param component Component handle
 * @param out_linked Output parameter for linked state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_border_width_linked(LCComponentHandle component, bool* out_linked);

/**
 * @brief Set whether border widths are linked
 * @param component Component handle
 * @param linked Linked state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_border_width_linked(LCComponentHandle component, bool linked);

/**
 * @brief Get the left border width of a Button
 * @param component Component handle
 * @param out_width Output parameter for border width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_border_width_left(LCComponentHandle component, float* out_width);

/**
 * @brief Set the left border width of a Button
 * @param component Component handle
 * @param width Border width (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_border_width_left(LCComponentHandle component, float width);

/**
 * @brief Get the right border width of a Button
 * @param component Component handle
 * @param out_width Output parameter for border width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_border_width_right(LCComponentHandle component, float* out_width);

/**
 * @brief Set the right border width of a Button
 * @param component Component handle
 * @param width Border width (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_border_width_right(LCComponentHandle component, float width);

/**
 * @brief Get the top border width of a Button
 * @param component Component handle
 * @param out_width Output parameter for border width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_border_width_top(LCComponentHandle component, float* out_width);

/**
 * @brief Set the top border width of a Button
 * @param component Component handle
 * @param width Border width (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_border_width_top(LCComponentHandle component, float width);

/**
 * @brief Get the bottom border width of a Button
 * @param component Component handle
 * @param out_width Output parameter for border width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_border_width_bottom(LCComponentHandle component, float* out_width);

/**
 * @brief Set the bottom border width of a Button
 * @param component Component handle
 * @param width Border width (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_border_width_bottom(LCComponentHandle component, float width);

/**
 * @brief Check if border is enabled
 * @param component Component handle
 * @param out_enabled Output parameter for border enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_border_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether border is enabled
 * @param component Component handle
 * @param enabled Border enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_border_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Get the border color of a Button
 * @param component Component handle
 * @param out_color Output parameter for border color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_border_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the border color of a Button
 * @param component Component handle
 * @param color Border color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_border_color(LCComponentHandle component, LCColor color);

/**
 * @brief Check if corner radii are linked
 * @param component Component handle
 * @param out_linked Output parameter for linked state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_corner_radius_linked(LCComponentHandle component, bool* out_linked);

/**
 * @brief Set whether corner radii are linked
 * @param component Component handle
 * @param linked Linked state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_corner_radius_linked(LCComponentHandle component, bool linked);

/**
 * @brief Get the top-left corner radius of a Button
 * @param component Component handle
 * @param out_radius Output parameter for corner radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_corner_radius_top_left(LCComponentHandle component, float* out_radius);

/**
 * @brief Set the top-left corner radius of a Button
 * @param component Component handle
 * @param radius Corner radius (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_corner_radius_top_left(LCComponentHandle component, float radius);

/**
 * @brief Get the top-right corner radius of a Button
 * @param component Component handle
 * @param out_radius Output parameter for corner radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_corner_radius_top_right(LCComponentHandle component, float* out_radius);

/**
 * @brief Set the top-right corner radius of a Button
 * @param component Component handle
 * @param radius Corner radius (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_corner_radius_top_right(LCComponentHandle component, float radius);

/**
 * @brief Get the bottom-left corner radius of a Button
 * @param component Component handle
 * @param out_radius Output parameter for corner radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_corner_radius_bottom_left(LCComponentHandle component, float* out_radius);

/**
 * @brief Set the bottom-left corner radius of a Button
 * @param component Component handle
 * @param radius Corner radius (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_corner_radius_bottom_left(LCComponentHandle component, float radius);

/**
 * @brief Get the bottom-right corner radius of a Button
 * @param component Component handle
 * @param out_radius Output parameter for corner radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_corner_radius_bottom_right(LCComponentHandle component, float* out_radius);

/**
 * @brief Set the bottom-right corner radius of a Button
 * @param component Component handle
 * @param radius Corner radius (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_corner_radius_bottom_right(LCComponentHandle component, float radius);

/* ===== Text Properties ===== */

/**
 * @brief Get the text of a Button
 * @param component Component handle
 * @param out_text Output buffer for text
 * @param text_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_text(LCComponentHandle component, char* out_text, size_t text_size);

/**
 * @brief Set the text of a Button
 * @param component Component handle
 * @param text Button text
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_text(LCComponentHandle component, const char* text);

/**
 * @brief Get the font path of a Button
 * @param component Component handle
 * @param out_path Output buffer for font path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_font_path(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set the font path of a Button
 * @param component Component handle
 * @param path Path to font file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_font_path(LCComponentHandle component, const char* path);

/**
 * @brief Get the font size of a Button
 * @param component Component handle
 * @param out_size Output parameter for font size
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_font_size(LCComponentHandle component, float* out_size);

/**
 * @brief Set the font size of a Button
 * @param component Component handle
 * @param size Font size (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_font_size(LCComponentHandle component, float size);

/**
 * @brief Get the font color of a Button
 * @param component Component handle
 * @param out_color Output parameter for font color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_font_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the font color of a Button
 * @param component Component handle
 * @param color Font color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_font_color(LCComponentHandle component, LCColor color);

/**
 * @brief Check if word wrap is enabled
 * @param component Component handle
 * @param out_wrap Output parameter for word wrap state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_word_wrap(LCComponentHandle component, bool* out_wrap);

/**
 * @brief Set whether word wrap is enabled
 * @param component Component handle
 * @param wrap Word wrap state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_word_wrap(LCComponentHandle component, bool wrap);

/* ===== Per-State Modulation (Automatic Mode) ===== */

/**
 * @brief Get the color modulation for a specific button state
 * @param component Component handle
 * @param state Button state
 * @param out_color Output parameter for modulation color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_state_modulation(LCComponentHandle component, LCButtonState state, LCColor* out_color);

/**
 * @brief Set the color modulation for a specific button state
 * @param component Component handle
 * @param state Button state
 * @param color Modulation color (multiplied with base colors)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_state_modulation(LCComponentHandle component, LCButtonState state, LCColor color);

/* ===== Per-State Sounds ===== */

/**
 * @brief Get the sound for a specific button state
 * @param component Component handle
 * @param state Button state
 * @param out_sound Output parameter for state sound
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_state_sound(LCComponentHandle component, LCButtonState state, LCButtonStateSound* out_sound);

/**
 * @brief Set the sound for a specific button state
 * @param component Component handle
 * @param state Button state
 * @param sound State sound configuration
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_state_sound(LCComponentHandle component, LCButtonState state, const LCButtonStateSound* sound);

/**
 * @brief Set the sound path for a specific button state
 * @param component Component handle
 * @param state Button state
 * @param path Path to audio file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_state_sound_path(LCComponentHandle component, LCButtonState state, const char* path);

/* ===== Per-State Tweens ===== */

/**
 * @brief Get the tween for a specific button state
 * @param component Component handle
 * @param state Button state
 * @param out_tween Output parameter for state tween
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_get_state_tween(LCComponentHandle component, LCButtonState state, LCButtonStateTween* out_tween);

/**
 * @brief Set the tween for a specific button state
 * @param component Component handle
 * @param state Button state
 * @param tween State tween configuration
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_state_tween(LCComponentHandle component, LCButtonState state, const LCButtonStateTween* tween);

/* ===== Per-State Callbacks ===== */

/**
 * @brief Set a callback for a specific button state
 * @param component Component handle
 * @param state Button state
 * @param callback Callback function (can be NULL to clear)
 * @param user_data User data passed to callback
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_set_state_callback(LCComponentHandle component, LCButtonState state, LCButtonStateCallback callback, void* user_data);

/**
 * @brief Clear the callback for a specific button state
 * @param component Component handle
 * @param state Button state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button_clear_state_callback(LCComponentHandle component, LCButtonState state);


#endif /* LUPINE_CAPI_BUTTON_H */
