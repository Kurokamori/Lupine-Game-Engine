/**
 * @file lc_color_rect.h
 * @brief Lupine Engine C API - ColorRect UI Component
 *
 * This header provides the ColorRect component for colored rectangles in UI.
 * ColorRect supports solid colors, rounded corners, and borders.
 */

#ifndef LUPINE_CAPI_COLOR_RECT_H
#define LUPINE_CAPI_COLOR_RECT_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Blend Mode Enum
 * ============================================================================ */

/**
 * @brief Blend mode for ColorRect rendering
 */
typedef enum LCBlendMode {
    LC_BLEND_ALPHA = 0,     /**< Standard alpha blending */
    LC_BLEND_ADDITIVE = 1,  /**< Additive blending */
    LC_BLEND_MULTIPLY = 2,  /**< Multiplicative blending */
    LC_BLEND_OPAQUE = 3,    /**< No blending */
    LC_BLEND_OVERLAY = 4    /**< Overlay blending */
} LCBlendMode;

/* ============================================================================
 * ColorRect Component Functions
 * ============================================================================ */

/**
 * @brief Create a ColorRect component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_color_rect_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Core Properties ----- */

LC_API LCResult lc_color_rect_get_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_color_rect_set_color(LCComponentHandle handle, LCColor color);

/* ----- Layer Properties ----- */

LC_API LCResult lc_color_rect_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_color_rect_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_color_rect_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_color_rect_set_sorting_order(LCComponentHandle handle, int order);

/* ----- Size Properties ----- */

LC_API LCResult lc_color_rect_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_color_rect_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_color_rect_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_color_rect_set_height(LCComponentHandle handle, float height);
LC_API LCResult lc_color_rect_get_size(LCComponentHandle handle, LCVec2* outSize);
LC_API LCResult lc_color_rect_set_size(LCComponentHandle handle, LCVec2 size);

/* ----- Corner Radius ----- */

LC_API LCResult lc_color_rect_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_color_rect_set_corner_radius_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_color_rect_set_corner_radius_all(LCComponentHandle handle, float radius);
LC_API LCResult lc_color_rect_set_corner_radius_individual(LCComponentHandle handle, int index, float radius);
LC_API LCResult lc_color_rect_get_corner_radius(LCComponentHandle handle, int index, float* outRadius);

/* ----- Border Properties ----- */

LC_API LCResult lc_color_rect_get_border_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_color_rect_set_border_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_color_rect_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_color_rect_set_border_color(LCComponentHandle handle, LCColor color);

/* ----- Border Width ----- */

LC_API LCResult lc_color_rect_get_border_width_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_color_rect_set_border_width_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_color_rect_set_border_width_all(LCComponentHandle handle, float width);
LC_API LCResult lc_color_rect_set_border_width_individual(LCComponentHandle handle, int index, float width);
LC_API LCResult lc_color_rect_get_border_width(LCComponentHandle handle, int index, float* outWidth);

/* ----- UI Space ----- */

LC_API LCResult lc_color_rect_get_ui_space(LCComponentHandle handle, bool* outUISpace);
LC_API LCResult lc_color_rect_set_ui_space(LCComponentHandle handle, bool uiSpace);

/* ----- Blend Mode ----- */

LC_API LCResult lc_color_rect_get_blend_mode(LCComponentHandle handle, LCBlendMode* outMode);
LC_API LCResult lc_color_rect_set_blend_mode(LCComponentHandle handle, LCBlendMode mode);

/* ----- Material Override ----- */

LC_API LCResult lc_color_rect_get_material_override(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_color_rect_set_material_override(LCComponentHandle handle, const char* path);

/* ----- Custom Shader (.lsh) ----- */

/**
 * @brief Get/Set the attached Lupine Shader (.lsh) path.
 * An empty path uses the built-in rounded-rect shader.
 */
LC_API LCResult lc_color_rect_get_shader(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_color_rect_set_shader(LCComponentHandle handle, const char* path);

/**
 * @brief Get/Set the serialized exported-shader-parameter values (JSON object string).
 * Maps each uniform name to a typed value, e.g.
 *   {"u_Intensity":{"type":"float","value":1.0}}
 */
LC_API LCResult lc_color_rect_get_shader_parameters(LCComponentHandle handle, char* outJson, uint32_t maxLength);
LC_API LCResult lc_color_rect_set_shader_parameters(LCComponentHandle handle, const char* json);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_COLOR_RECT_H */
