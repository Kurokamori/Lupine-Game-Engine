/**
 * @file lc_panel.h
 * @brief Lupine Engine C API - Panel and Panel3D UI Components
 *
 * This header provides Panel and Panel3D components for styled UI panels.
 * Panels support StyleBox rendering with backgrounds, borders, corners, and shadows.
 */

#ifndef LUPINE_CAPI_PANEL_H
#define LUPINE_CAPI_PANEL_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Panel3D Billboard Mode
 * ============================================================================ */

/**
 * @brief Billboard mode for Panel3D
 */
typedef enum LCPanel3DBillboardMode {
    LC_PANEL3D_BILLBOARD_DISABLED = 0,  /**< No billboarding */
    LC_PANEL3D_BILLBOARD_ENABLED = 1,   /**< Full billboarding, faces camera */
    LC_PANEL3D_BILLBOARD_Y_AXIS = 2     /**< Billboard only around Y axis */
} LCPanel3DBillboardMode;

/* ============================================================================
 * Panel Component Functions
 * ============================================================================ */

/**
 * @brief Create a Panel component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_panel_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_panel_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_panel_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_panel_set_height(LCComponentHandle handle, float height);

/* ----- Layer Properties ----- */

LC_API LCResult lc_panel_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_panel_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_panel_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_panel_set_sorting_order(LCComponentHandle handle, int order);

/* ----- UI Space ----- */

LC_API LCResult lc_panel_get_use_ui_space(LCComponentHandle handle, bool* outUseUISpace);
LC_API LCResult lc_panel_set_use_ui_space(LCComponentHandle handle, bool useUISpace);

/* ----- StyleBox File ----- */

LC_API LCResult lc_panel_load_style(LCComponentHandle handle, const char* filepath);
LC_API LCResult lc_panel_save_style(LCComponentHandle handle, const char* filepath);
LC_API LCResult lc_panel_get_style_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_panel_set_style_path(LCComponentHandle handle, const char* path);

/* ----- Background ----- */

LC_API LCResult lc_panel_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_panel_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_panel_get_opacity(LCComponentHandle handle, float* outOpacity);
LC_API LCResult lc_panel_set_opacity(LCComponentHandle handle, float opacity);

/* ----- Border Width ----- */

LC_API LCResult lc_panel_get_border_width_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_panel_set_border_width_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_panel_get_border_width_left(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel_set_border_width_left(LCComponentHandle handle, float width);
LC_API LCResult lc_panel_get_border_width_right(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel_set_border_width_right(LCComponentHandle handle, float width);
LC_API LCResult lc_panel_get_border_width_top(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel_set_border_width_top(LCComponentHandle handle, float width);
LC_API LCResult lc_panel_get_border_width_bottom(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel_set_border_width_bottom(LCComponentHandle handle, float width);

/* ----- Border Enabled/Color ----- */

LC_API LCResult lc_panel_get_border_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_panel_set_border_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_panel_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_panel_set_border_color(LCComponentHandle handle, LCColor color);

/* ----- Corner Radius ----- */

LC_API LCResult lc_panel_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_panel_set_corner_radius_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_panel_get_corner_radius_top_left(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_panel_set_corner_radius_top_left(LCComponentHandle handle, float radius);
LC_API LCResult lc_panel_get_corner_radius_top_right(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_panel_set_corner_radius_top_right(LCComponentHandle handle, float radius);
LC_API LCResult lc_panel_get_corner_radius_bottom_left(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_panel_set_corner_radius_bottom_left(LCComponentHandle handle, float radius);
LC_API LCResult lc_panel_get_corner_radius_bottom_right(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_panel_set_corner_radius_bottom_right(LCComponentHandle handle, float radius);

/* ----- Corner Detail & Anti-Aliasing ----- */

LC_API LCResult lc_panel_get_corner_detail(LCComponentHandle handle, int* outDetail);
LC_API LCResult lc_panel_set_corner_detail(LCComponentHandle handle, int detail);
LC_API LCResult lc_panel_get_anti_aliasing(LCComponentHandle handle, bool* outAA);
LC_API LCResult lc_panel_set_anti_aliasing(LCComponentHandle handle, bool aa);
LC_API LCResult lc_panel_get_anti_aliasing_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_panel_set_anti_aliasing_size(LCComponentHandle handle, float size);

/* ----- Shadow ----- */

LC_API LCResult lc_panel_get_shadow_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_panel_set_shadow_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_panel_get_shadow_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_panel_set_shadow_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_panel_get_shadow_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_panel_set_shadow_size(LCComponentHandle handle, float size);
LC_API LCResult lc_panel_get_shadow_offset(LCComponentHandle handle, LCVec2* outOffset);
LC_API LCResult lc_panel_set_shadow_offset(LCComponentHandle handle, LCVec2 offset);

/* ----- Custom Shader ----- */

LC_API LCResult lc_panel_get_custom_shader_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_panel_set_custom_shader_path(LCComponentHandle handle, const char* path);

/* ============================================================================
 * Panel3D Component Functions
 * ============================================================================ */

/**
 * @brief Create a Panel3D component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_panel3d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_panel3d_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel3d_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_panel3d_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_panel3d_set_height(LCComponentHandle handle, float height);

/* ----- 3D Rendering Properties ----- */

LC_API LCResult lc_panel3d_get_billboard_mode(LCComponentHandle handle, LCPanel3DBillboardMode* outMode);
LC_API LCResult lc_panel3d_set_billboard_mode(LCComponentHandle handle, LCPanel3DBillboardMode mode);
LC_API LCResult lc_panel3d_get_double_sided(LCComponentHandle handle, bool* outDoubleSided);
LC_API LCResult lc_panel3d_set_double_sided(LCComponentHandle handle, bool doubleSided);
LC_API LCResult lc_panel3d_get_cast_shadow(LCComponentHandle handle, bool* outCastShadow);
LC_API LCResult lc_panel3d_set_cast_shadow(LCComponentHandle handle, bool castShadow);
LC_API LCResult lc_panel3d_get_receive_shadow(LCComponentHandle handle, bool* outReceiveShadow);
LC_API LCResult lc_panel3d_set_receive_shadow(LCComponentHandle handle, bool receiveShadow);

/* ----- Layer Properties ----- */

LC_API LCResult lc_panel3d_get_layer(LCComponentHandle handle, int* outLayer);
LC_API LCResult lc_panel3d_set_layer(LCComponentHandle handle, int layer);
LC_API LCResult lc_panel3d_get_sorting_order(LCComponentHandle handle, int* outOrder);
LC_API LCResult lc_panel3d_set_sorting_order(LCComponentHandle handle, int order);

/* ----- StyleBox File ----- */

LC_API LCResult lc_panel3d_load_style(LCComponentHandle handle, const char* filepath);
LC_API LCResult lc_panel3d_save_style(LCComponentHandle handle, const char* filepath);
LC_API LCResult lc_panel3d_get_style_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_panel3d_set_style_path(LCComponentHandle handle, const char* path);

/* ----- Background ----- */

LC_API LCResult lc_panel3d_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_panel3d_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_panel3d_get_opacity(LCComponentHandle handle, float* outOpacity);
LC_API LCResult lc_panel3d_set_opacity(LCComponentHandle handle, float opacity);

/* ----- Border Width ----- */

LC_API LCResult lc_panel3d_get_border_width_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_panel3d_set_border_width_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_panel3d_get_border_width_left(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel3d_set_border_width_left(LCComponentHandle handle, float width);
LC_API LCResult lc_panel3d_get_border_width_right(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel3d_set_border_width_right(LCComponentHandle handle, float width);
LC_API LCResult lc_panel3d_get_border_width_top(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel3d_set_border_width_top(LCComponentHandle handle, float width);
LC_API LCResult lc_panel3d_get_border_width_bottom(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_panel3d_set_border_width_bottom(LCComponentHandle handle, float width);

/* ----- Border Enabled/Color ----- */

LC_API LCResult lc_panel3d_get_border_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_panel3d_set_border_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_panel3d_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_panel3d_set_border_color(LCComponentHandle handle, LCColor color);

/* ----- Corner Radius ----- */

LC_API LCResult lc_panel3d_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_panel3d_set_corner_radius_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_panel3d_get_corner_radius_top_left(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_panel3d_set_corner_radius_top_left(LCComponentHandle handle, float radius);
LC_API LCResult lc_panel3d_get_corner_radius_top_right(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_panel3d_set_corner_radius_top_right(LCComponentHandle handle, float radius);
LC_API LCResult lc_panel3d_get_corner_radius_bottom_left(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_panel3d_set_corner_radius_bottom_left(LCComponentHandle handle, float radius);
LC_API LCResult lc_panel3d_get_corner_radius_bottom_right(LCComponentHandle handle, float* outRadius);
LC_API LCResult lc_panel3d_set_corner_radius_bottom_right(LCComponentHandle handle, float radius);

/* ----- Corner Detail & Anti-Aliasing ----- */

LC_API LCResult lc_panel3d_get_corner_detail(LCComponentHandle handle, int* outDetail);
LC_API LCResult lc_panel3d_set_corner_detail(LCComponentHandle handle, int detail);
LC_API LCResult lc_panel3d_get_anti_aliasing(LCComponentHandle handle, bool* outAA);
LC_API LCResult lc_panel3d_set_anti_aliasing(LCComponentHandle handle, bool aa);
LC_API LCResult lc_panel3d_get_anti_aliasing_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_panel3d_set_anti_aliasing_size(LCComponentHandle handle, float size);

/* ----- Shadow (StyleBox visual shadow, not 3D shadow) ----- */

LC_API LCResult lc_panel3d_get_shadow_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_panel3d_set_shadow_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_panel3d_get_shadow_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_panel3d_set_shadow_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_panel3d_get_shadow_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_panel3d_set_shadow_size(LCComponentHandle handle, float size);
LC_API LCResult lc_panel3d_get_shadow_offset(LCComponentHandle handle, LCVec2* outOffset);
LC_API LCResult lc_panel3d_set_shadow_offset(LCComponentHandle handle, LCVec2 offset);

/* ----- Custom Shader ----- */

LC_API LCResult lc_panel3d_get_custom_shader_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_panel3d_set_custom_shader_path(LCComponentHandle handle, const char* path);

/* ----- Panel Custom Shader (.lsh) ----- */

/** Get/Set the attached Lupine Shader (.lsh) path on a 2D Panel. Empty = built-in rounded rect. */
LC_API LCResult lc_panel_get_shader(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_panel_set_shader(LCComponentHandle handle, const char* path);

/** Get/Set the serialized exported-shader-parameter values (JSON object string). */
LC_API LCResult lc_panel_get_shader_parameters(LCComponentHandle handle, char* outJson, uint32_t maxLength);
LC_API LCResult lc_panel_set_shader_parameters(LCComponentHandle handle, const char* json);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_PANEL_H */
