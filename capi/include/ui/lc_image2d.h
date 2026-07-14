/**
 * @file lc_image2d.h
 * @brief Lupine Engine C API - Image2D UI Component
 *
 * This header provides the Image2D component for displaying textures in UI.
 * Image2D supports texture loading, aspect ratio preservation, anchoring, and more.
 */

#ifndef LUPINE_CAPI_IMAGE2D_H
#define LUPINE_CAPI_IMAGE2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"
#include "ui/lc_color_rect.h"  /* For LCBlendMode */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Aspect mode for Image2D
 */
typedef enum LCAspectMode {
    LC_ASPECT_FIT = 0,       /**< Show entire image, may have letterboxing */
    LC_ASPECT_LETTERBOX = 1, /**< Same as Fit */
    LC_ASPECT_FILL = 2,      /**< Fill rect, may crop image */
    LC_ASPECT_STRETCH = 3    /**< Ignore aspect ratio, stretch to fill */
} LCAspectMode;

/**
 * @brief Mouse interaction behavior for Image2D
 */
typedef enum LCMouseBehaviour {
    LC_MOUSE_IGNORE = 0,       /**< Pass through, don't respond to mouse */
    LC_MOUSE_PROPAGATE_UP = 1, /**< Handle and propagate to parent */
    LC_MOUSE_STOP = 2          /**< Handle and stop propagation */
} LCMouseBehaviour;

/* ============================================================================
 * Image2D Component Functions
 * ============================================================================ */

/**
 * @brief Create an Image2D component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_image2d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Size Properties ----- */

LC_API LCResult lc_image2d_get_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_image2d_set_width(LCComponentHandle handle, float width);
LC_API LCResult lc_image2d_get_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_image2d_set_height(LCComponentHandle handle, float height);

/* ----- Texture Management ----- */

LC_API LCResult lc_image2d_load_texture(LCComponentHandle handle, const char* filepath);
LC_API LCResult lc_image2d_get_texture_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_image2d_set_texture_path(LCComponentHandle handle, const char* path);

/* ----- Material Override ----- */

LC_API LCResult lc_image2d_get_material_override(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_image2d_set_material_override(LCComponentHandle handle, const char* path);

/* ----- Color/Tint ----- */

LC_API LCResult lc_image2d_get_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_image2d_set_color(LCComponentHandle handle, LCColor color);

/* ----- Anchor Properties ----- */

LC_API LCResult lc_image2d_get_anchor_min(LCComponentHandle handle, LCVec2* outAnchor);
LC_API LCResult lc_image2d_set_anchor_min(LCComponentHandle handle, LCVec2 anchor);
LC_API LCResult lc_image2d_get_anchor_max(LCComponentHandle handle, LCVec2* outAnchor);
LC_API LCResult lc_image2d_set_anchor_max(LCComponentHandle handle, LCVec2 anchor);
LC_API LCResult lc_image2d_get_offset_min(LCComponentHandle handle, LCVec2* outOffset);
LC_API LCResult lc_image2d_set_offset_min(LCComponentHandle handle, LCVec2 offset);
LC_API LCResult lc_image2d_get_offset_max(LCComponentHandle handle, LCVec2* outOffset);
LC_API LCResult lc_image2d_set_offset_max(LCComponentHandle handle, LCVec2 offset);

/* ----- Aspect Ratio ----- */

LC_API LCResult lc_image2d_get_preserve_aspect(LCComponentHandle handle, bool* outPreserve);
LC_API LCResult lc_image2d_set_preserve_aspect(LCComponentHandle handle, bool preserve);
LC_API LCResult lc_image2d_get_aspect_mode(LCComponentHandle handle, LCAspectMode* outMode);
LC_API LCResult lc_image2d_set_aspect_mode(LCComponentHandle handle, LCAspectMode mode);

/* ----- Flip Properties ----- */

LC_API LCResult lc_image2d_get_flip_h(LCComponentHandle handle, bool* outFlip);
LC_API LCResult lc_image2d_set_flip_h(LCComponentHandle handle, bool flip);
LC_API LCResult lc_image2d_get_flip_v(LCComponentHandle handle, bool* outFlip);
LC_API LCResult lc_image2d_set_flip_v(LCComponentHandle handle, bool flip);

/* ----- Blend Mode ----- */

LC_API LCResult lc_image2d_get_blend_mode(LCComponentHandle handle, LCBlendMode* outMode);
LC_API LCResult lc_image2d_set_blend_mode(LCComponentHandle handle, LCBlendMode mode);

/* ----- Mouse Behaviour ----- */

LC_API LCResult lc_image2d_get_mouse_behaviour(LCComponentHandle handle, LCMouseBehaviour* outBehaviour);
LC_API LCResult lc_image2d_set_mouse_behaviour(LCComponentHandle handle, LCMouseBehaviour behaviour);

/* ----- Corner Radius ----- */

LC_API LCResult lc_image2d_get_corner_radius_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_image2d_set_corner_radius_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_image2d_set_corner_radius_all(LCComponentHandle handle, float radius);
LC_API LCResult lc_image2d_set_corner_radius_individual(LCComponentHandle handle, int index, float radius);
LC_API LCResult lc_image2d_get_corner_radius(LCComponentHandle handle, int index, float* outRadius);

/* ----- Border Properties ----- */

LC_API LCResult lc_image2d_get_border_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_image2d_set_border_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_image2d_get_border_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_image2d_set_border_color(LCComponentHandle handle, LCColor color);

/* ----- Border Width ----- */

LC_API LCResult lc_image2d_get_border_width_linked(LCComponentHandle handle, bool* outLinked);
LC_API LCResult lc_image2d_set_border_width_linked(LCComponentHandle handle, bool linked);
LC_API LCResult lc_image2d_set_border_width_all(LCComponentHandle handle, float width);
LC_API LCResult lc_image2d_set_border_width_individual(LCComponentHandle handle, int index, float width);
LC_API LCResult lc_image2d_get_border_width(LCComponentHandle handle, int index, float* outWidth);

/* ----- UI Space ----- */

LC_API LCResult lc_image2d_get_ui_space(LCComponentHandle handle, bool* outUISpace);
LC_API LCResult lc_image2d_set_ui_space(LCComponentHandle handle, bool uiSpace);

/* ----- Custom Shader (.lsh) ----- */

/** Get/Set the attached Lupine Shader (.lsh) path. Empty = built-in textured rounded rect. */
LC_API LCResult lc_image2d_get_shader(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_image2d_set_shader(LCComponentHandle handle, const char* path);

/** Get/Set the serialized exported-shader-parameter values (JSON object string). */
LC_API LCResult lc_image2d_get_shader_parameters(LCComponentHandle handle, char* outJson, uint32_t maxLength);
LC_API LCResult lc_image2d_set_shader_parameters(LCComponentHandle handle, const char* json);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_IMAGE2D_H */
