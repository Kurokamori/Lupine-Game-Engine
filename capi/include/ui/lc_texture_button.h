/**
 * @file lc_texture_button.h
 * @brief Lupine Engine C API - TextureButton Component
 *
 * Texture-based button with per-state textures, alpha masking,
 * stretch modes, toggle support, and text overlay.
 */

#ifndef LUPINE_CAPI_TEXTURE_BUTTON_H
#define LUPINE_CAPI_TEXTURE_BUTTON_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"
#include "input/lc_input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief TextureButton mouse states
 */
typedef enum LCTextureButtonState {
    LC_TEXTURE_BUTTON_STATE_NORMAL = 0,
    LC_TEXTURE_BUTTON_STATE_HOVER = 1,
    LC_TEXTURE_BUTTON_STATE_PRESSED = 2,
    LC_TEXTURE_BUTTON_STATE_DISABLED = 3
} LCTextureButtonState;

/**
 * @brief TextureButton style mode
 */
typedef enum LCTextureButtonStyleMode {
    LC_TEXTURE_BUTTON_STYLE_AUTOMATIC = 0,  /**< Single texture with per-state modulation */
    LC_TEXTURE_BUTTON_STYLE_MANUAL = 1      /**< Full texture per state */
} LCTextureButtonStyleMode;

/**
 * @brief TextureButton stretch mode
 */
typedef enum LCTextureButtonStretchMode {
    LC_TEXTURE_BUTTON_STRETCH = 0,        /**< Stretch texture to fill button */
    LC_TEXTURE_BUTTON_KEEP_CENTERED = 1,  /**< Keep texture at original size, centered */
    LC_TEXTURE_BUTTON_NINE_SLICE = 2      /**< Nine-slice scaling */
} LCTextureButtonStretchMode;

/**
 * @brief Click mask mode
 */
typedef enum LCClickMaskMode {
    LC_CLICK_MASK_RECT = 0,           /**< Rectangular click area */
    LC_CLICK_MASK_ALPHA_THRESHOLD = 1 /**< Use texture alpha for click detection */
} LCClickMaskMode;


/* ============================================================================
 * TextureButton Component Functions
 * ============================================================================ */

/**
 * @brief Create a TextureButton component on a node
 */
LC_API LCResult lc_texture_button_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_texture_button_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_texture_button_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_texture_button_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_texture_button_set_height(LCComponentHandle handle, float height);

/* ----- Layer Properties ----- */

LC_API LCResult lc_texture_button_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_texture_button_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_texture_button_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_texture_button_set_sorting_order(LCComponentHandle handle, int order);

/* ----- UI Space ----- */

LC_API LCResult lc_texture_button_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace);
LC_API LCResult lc_texture_button_set_use_ui_space(LCComponentHandle handle, bool useUISpace);

/* ----- Button State ----- */

LC_API LCResult lc_texture_button_get_current_state(LCComponentHandle handle, LCTextureButtonState* outState);
LC_API LCResult lc_texture_button_set_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_texture_button_is_enabled(LCComponentHandle handle, bool* outEnabled);

/* ----- Style Mode ----- */

LC_API LCResult lc_texture_button_get_style_mode(LCComponentHandle handle, LCTextureButtonStyleMode* outMode);
LC_API LCResult lc_texture_button_set_style_mode(LCComponentHandle handle, LCTextureButtonStyleMode mode);

/* ----- Stretch Mode ----- */

LC_API LCResult lc_texture_button_get_stretch_mode(LCComponentHandle handle, LCTextureButtonStretchMode* outMode);
LC_API LCResult lc_texture_button_set_stretch_mode(LCComponentHandle handle, LCTextureButtonStretchMode mode);

/* ----- Toggle Mode ----- */

LC_API LCResult lc_texture_button_get_is_toggle(LCComponentHandle handle, bool* outIsToggle);
LC_API LCResult lc_texture_button_set_is_toggle(LCComponentHandle handle, bool isToggle);
LC_API LCResult lc_texture_button_get_is_checked(LCComponentHandle handle, bool* outIsChecked);
LC_API LCResult lc_texture_button_set_is_checked(LCComponentHandle handle, bool isChecked);

/* ----- Click Mask ----- */

LC_API LCResult lc_texture_button_get_click_mask_mode(LCComponentHandle handle, LCClickMaskMode* outMode);
LC_API LCResult lc_texture_button_set_click_mask_mode(LCComponentHandle handle, LCClickMaskMode mode);
LC_API LCResult lc_texture_button_get_alpha_threshold(LCComponentHandle handle, float* outThreshold);
LC_API LCResult lc_texture_button_set_alpha_threshold(LCComponentHandle handle, float threshold);

/* ----- Mouse Button ----- */

LC_API LCResult lc_texture_button_get_mouse_button(LCComponentHandle handle, LCMouseButton* outButton);
LC_API LCResult lc_texture_button_set_mouse_button(LCComponentHandle handle, LCMouseButton button);

/* ----- Texture Properties (Automatic Mode) ----- */

LC_API LCResult lc_texture_button_get_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_texture_button_set_texture_path(LCComponentHandle handle, const char* path);

/* ----- Per-State Textures (Manual Mode) ----- */

LC_API LCResult lc_texture_button_get_state_texture_path(LCComponentHandle handle, LCTextureButtonState state, char* outPath, uint32_t maxLength);
LC_API LCResult lc_texture_button_set_state_texture_path(LCComponentHandle handle, LCTextureButtonState state, const char* path);

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_texture_button_get_state_modulation(LCComponentHandle handle, LCTextureButtonState state, LCColor* outColor);
LC_API LCResult lc_texture_button_set_state_modulation(LCComponentHandle handle, LCTextureButtonState state, LCColor color);

/* ----- Per-State Sounds ----- */

LC_API LCResult lc_texture_button_set_state_sound_path(LCComponentHandle handle, LCTextureButtonState state, const char* path);

/* ----- Text Properties ----- */

LC_API LCResult lc_texture_button_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength);
LC_API LCResult lc_texture_button_set_text(LCComponentHandle handle, const char* text);
LC_API LCResult lc_texture_button_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_texture_button_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_texture_button_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_texture_button_set_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_texture_button_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_texture_button_set_font_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_TEXTURE_BUTTON_H */
