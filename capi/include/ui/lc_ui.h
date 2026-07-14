/**
 * @file lc_ui.h
 * @brief Lupine Engine C API - UI components (Label, etc.)
 *
 * This header provides UI components for user interfaces.
 */

#ifndef LUPINE_CAPI_UI_H
#define LUPINE_CAPI_UI_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"


/* ============================================================================
 * Label Functions
 * ============================================================================ */

/**
 * @brief Create a Label component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Load font from file path
 * @param component Component handle
 * @param filepath Path to font file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_load_font(LCComponentHandle component, const char* filepath);

/**
 * @brief Get the text of a Label
 * @param component Component handle
 * @param out_text Output buffer for text
 * @param text_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_get_text(LCComponentHandle component, char* out_text, size_t text_size);

/**
 * @brief Set the text of a Label
 * @param component Component handle
 * @param text Text to set
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_set_text(LCComponentHandle component, const char* text);

/**
 * @brief Get the font path of a Label
 * @param component Component handle
 * @param out_path Output buffer for font path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_get_font_path(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set the font path of a Label
 * @param component Component handle
 * @param path Path to font file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_set_font_path(LCComponentHandle component, const char* path);

/**
 * @brief Get the font size of a Label
 * @param component Component handle
 * @param out_size Output parameter for font size
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_get_font_size(LCComponentHandle component, float* out_size);

/**
 * @brief Set the font size of a Label
 * @param component Component handle
 * @param size Font size (pixels)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_set_font_size(LCComponentHandle component, float size);

/**
 * @brief Get the color of a Label
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_get_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the color of a Label
 * @param component Component handle
 * @param color Text color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_set_color(LCComponentHandle component, LCColor color);

/**
 * @brief Check if a Label is centered
 * @param component Component handle
 * @param out_centered Output parameter for centered state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_get_centered(LCComponentHandle component, bool* out_centered);

/**
 * @brief Set whether a Label is centered
 * @param component Component handle
 * @param centered Centered state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_set_centered(LCComponentHandle component, bool centered);

/**
 * @brief Get the offset of a Label
 * @param component Component handle
 * @param out_offset Output parameter for offset
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_get_offset(LCComponentHandle component, LCVec2* out_offset);

/**
 * @brief Set the offset of a Label
 * @param component Component handle
 * @param offset Offset (when not centered)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_label_set_offset(LCComponentHandle component, LCVec2 offset);


#endif /* LUPINE_CAPI_UI_H */
