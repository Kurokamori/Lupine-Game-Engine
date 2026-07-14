/**
 * @file lc_button3d.h
 * @brief Lupine Engine C API - Button3D Component
 *
 * 3D button with billboard support, StyleBox backgrounds, text labels,
 * per-state modulation, sounds, tweens, and shadows.
 */

#ifndef LUPINE_CAPI_BUTTON3D_H
#define LUPINE_CAPI_BUTTON3D_H

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
 * @brief Button3D mouse states
 */
typedef enum LCButton3DState {
    LC_BUTTON3D_STATE_NORMAL = 0,
    LC_BUTTON3D_STATE_HOVER = 1,
    LC_BUTTON3D_STATE_PRESSED = 2,
    LC_BUTTON3D_STATE_DISABLED = 3
} LCButton3DState;

/**
 * @brief Button3D style mode
 */
typedef enum LCButton3DStyleMode {
    LC_BUTTON3D_STYLE_AUTOMATIC = 0,  /**< Single style with per-state color modulation */
    LC_BUTTON3D_STYLE_MANUAL = 1      /**< Full StyleBox per state */
} LCButton3DStyleMode;

/**
 * @brief Button3D scale mode
 */
typedef enum LCButton3DScaleMode {
    LC_BUTTON3D_SCALE_FIXED = 0,          /**< Fixed size */
    LC_BUTTON3D_SCALE_FIT_TO_TEXT = 1,    /**< Scale to fit text with padding */
    LC_BUTTON3D_SCALE_FIT_TO_TEXT_WIDTH = 2 /**< Scale width only to fit text */
} LCButton3DScaleMode;

/**
 * @brief Button3D billboard mode
 */
typedef enum LCButton3DBillboardMode {
    LC_BUTTON3D_BILLBOARD_DISABLED = 0,  /**< No billboarding */
    LC_BUTTON3D_BILLBOARD_ENABLED = 1,   /**< Full billboard (face camera) */
    LC_BUTTON3D_BILLBOARD_Y_AXIS = 2     /**< Y-axis billboard */
} LCButton3DBillboardMode;

/* ============================================================================
 * Button3D Component Functions
 * ============================================================================ */

/**
 * @brief Create a Button3D component on a node
 * @param node Node handle to attach the component to (should be 3D node)
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_button3d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_button3d_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_button3d_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_button3d_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_button3d_set_height(LCComponentHandle handle, float height);

/* ----- 3D Rendering Properties ----- */

LC_API LCResult lc_button3d_get_billboard_mode(LCComponentHandle handle, LCButton3DBillboardMode* outMode);
LC_API LCResult lc_button3d_set_billboard_mode(LCComponentHandle handle, LCButton3DBillboardMode mode);

LC_API LCResult lc_button3d_get_double_sided(LCComponentHandle handle, bool* outDoubleSided);
LC_API LCResult lc_button3d_set_double_sided(LCComponentHandle handle, bool doubleSided);

LC_API LCResult lc_button3d_get_cast_shadow(LCComponentHandle handle, bool* outCastShadow);
LC_API LCResult lc_button3d_set_cast_shadow(LCComponentHandle handle, bool castShadow);

LC_API LCResult lc_button3d_get_receive_shadow(LCComponentHandle handle, bool* outReceiveShadow);
LC_API LCResult lc_button3d_set_receive_shadow(LCComponentHandle handle, bool receiveShadow);

/* ----- Layer Properties ----- */

LC_API LCResult lc_button3d_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_button3d_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_button3d_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_button3d_set_sorting_order(LCComponentHandle handle, int order);

/* ----- Button State ----- */

LC_API LCResult lc_button3d_get_current_state(LCComponentHandle handle, LCButton3DState* outState);
LC_API LCResult lc_button3d_set_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_button3d_is_enabled(LCComponentHandle handle, bool* outEnabled);

/* ----- Style Mode ----- */

LC_API LCResult lc_button3d_get_style_mode(LCComponentHandle handle, LCButton3DStyleMode* outMode);
LC_API LCResult lc_button3d_set_style_mode(LCComponentHandle handle, LCButton3DStyleMode mode);

/* ----- Scale Mode ----- */

LC_API LCResult lc_button3d_get_scale_mode(LCComponentHandle handle, LCButton3DScaleMode* outMode);
LC_API LCResult lc_button3d_set_scale_mode(LCComponentHandle handle, LCButton3DScaleMode mode);

LC_API LCResult lc_button3d_get_text_padding(LCComponentHandle handle, LCVec2* outPadding);
LC_API LCResult lc_button3d_set_text_padding(LCComponentHandle handle, LCVec2 padding);

/* ----- Background (Base Style) ----- */

LC_API LCResult lc_button3d_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_button3d_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_button3d_get_opacity(LCComponentHandle handle, float* outOpacity);
LC_API LCResult lc_button3d_set_opacity(LCComponentHandle handle, float opacity);

/* ----- Border ----- */

LC_API LCResult lc_button3d_get_border_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_button3d_set_border_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_button3d_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_button3d_set_border_color(LCComponentHandle handle, LCColor color);

LC_API LCResult lc_button3d_get_border_width_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_button3d_set_border_width_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_button3d_get_border_width_left(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_button3d_set_border_width_left(LCComponentHandle handle, float width);
LC_API LCResult lc_button3d_get_border_width_right(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_button3d_set_border_width_right(LCComponentHandle handle, float width);
LC_API LCResult lc_button3d_get_border_width_top(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_button3d_set_border_width_top(LCComponentHandle handle, float width);
LC_API LCResult lc_button3d_get_border_width_bottom(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_button3d_set_border_width_bottom(LCComponentHandle handle, float width);

/* ----- Corner Radius ----- */

LC_API LCResult lc_button3d_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_button3d_set_corner_radius_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_button3d_get_corner_radius_top_left(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_button3d_set_corner_radius_top_left(LCComponentHandle handle, float radius);
LC_API LCResult lc_button3d_get_corner_radius_top_right(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_button3d_set_corner_radius_top_right(LCComponentHandle handle, float radius);
LC_API LCResult lc_button3d_get_corner_radius_bottom_left(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_button3d_set_corner_radius_bottom_left(LCComponentHandle handle, float radius);
LC_API LCResult lc_button3d_get_corner_radius_bottom_right(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_button3d_set_corner_radius_bottom_right(LCComponentHandle handle, float radius);

/* ----- Text Properties ----- */

LC_API LCResult lc_button3d_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength);
LC_API LCResult lc_button3d_set_text(LCComponentHandle handle, const char* text);
LC_API LCResult lc_button3d_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_button3d_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_button3d_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_button3d_set_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_button3d_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_button3d_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_button3d_get_word_wrap(LCComponentHandle handle, bool* outWrap);
LC_API LCResult lc_button3d_set_word_wrap(LCComponentHandle handle, bool wrap);

/* ----- Per-State Modulation ----- */

LC_API LCResult lc_button3d_get_state_modulation(LCComponentHandle handle, LCButton3DState state, LCColor* outColor);
LC_API LCResult lc_button3d_set_state_modulation(LCComponentHandle handle, LCButton3DState state, LCColor color);

/* ----- Per-State Sounds ----- */

LC_API LCResult lc_button3d_set_state_sound_path(LCComponentHandle handle, LCButton3DState state, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_BUTTON3D_H */
