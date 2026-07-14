/**
 * @file lc_label3d.h
 * @brief Lupine Engine C API - Label3D UI Component
 *
 * This header provides the Label3D component for rendering text in 3D space.
 * Label3D supports fonts, text styling, and billboard modes.
 */

#ifndef LUPINE_CAPI_LABEL3D_H
#define LUPINE_CAPI_LABEL3D_H

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
 * @brief Billboard mode for Label3D
 */
typedef enum LCLabel3DBillboardMode {
    LC_LABEL3D_BILLBOARD_DISABLED = 0, /**< No billboarding */
    LC_LABEL3D_BILLBOARD_ENABLED = 1,  /**< Full billboarding, faces camera */
    LC_LABEL3D_BILLBOARD_Y_AXIS = 2    /**< Billboard only around Y axis */
} LCLabel3DBillboardMode;

/* ============================================================================
 * Label3D Component Functions
 * ============================================================================ */

/**
 * @brief Create a Label3D component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_label3d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Font Management ----- */

/**
 * @brief Load a font from file
 * @param handle Component handle
 * @param filepath Path to the font file (.ttf, .otf)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_label3d_load_font(LCComponentHandle handle, const char* filepath);

LC_API LCResult lc_label3d_get_font_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);
LC_API LCResult lc_label3d_set_font_path(LCComponentHandle handle, const char* path);

/* ----- Text Properties ----- */

LC_API LCResult lc_label3d_get_text(LCComponentHandle handle, char* outText, uint32_t maxLength);
LC_API LCResult lc_label3d_set_text(LCComponentHandle handle, const char* text);

/* ----- Font Size ----- */

LC_API LCResult lc_label3d_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_label3d_set_font_size(LCComponentHandle handle, float size);

/* ----- Color ----- */

LC_API LCResult lc_label3d_get_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_label3d_set_color(LCComponentHandle handle, LCColor color);

/* ----- Text Alignment ----- */

LC_API LCResult lc_label3d_get_centered(LCComponentHandle handle, bool* outCentered);
LC_API LCResult lc_label3d_set_centered(LCComponentHandle handle, bool centered);

/* ----- Offset ----- */

LC_API LCResult lc_label3d_get_offset(LCComponentHandle handle, LCVec2* outOffset);
LC_API LCResult lc_label3d_set_offset(LCComponentHandle handle, LCVec2 offset);

/* ----- Billboard Mode ----- */

LC_API LCResult lc_label3d_get_billboard_mode(LCComponentHandle handle, LCLabel3DBillboardMode* outMode);
LC_API LCResult lc_label3d_set_billboard_mode(LCComponentHandle handle, LCLabel3DBillboardMode mode);

/* ----- Pixel Size ----- */

LC_API LCResult lc_label3d_get_pixel_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_label3d_set_pixel_size(LCComponentHandle handle, float size);

/* ----- Text Measurement ----- */

/**
 * @brief Calculate the size of the rendered text
 * @param handle Component handle
 * @param outSize Output parameter for the text size (width, height)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_label3d_calculate_text_size(LCComponentHandle handle, LCVec2* outSize);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_LABEL3D_H */
