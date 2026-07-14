/**
 * @file lc_line_edit.h
 * @brief Lupine Engine C API - LineEdit UI component (single-line text field)
 */

#ifndef LUPINE_CAPI_LINE_EDIT_H
#define LUPINE_CAPI_LINE_EDIT_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a LineEdit component on a node
 */
LC_API LCResult lc_line_edit_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Text -----
 * Strings are UTF-8. lc_line_edit_get_text writes into the caller-provided buffer
 * (truncated to bufferSize-1 and null-terminated); outRequired receives the full
 * length in bytes (excluding the terminator). Either out pointer may be NULL.
 */
LC_API LCResult lc_line_edit_get_text(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_line_edit_set_text(LCComponentHandle handle, const char* text);
LC_API LCResult lc_line_edit_get_placeholder(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_line_edit_set_placeholder(LCComponentHandle handle, const char* text);

LC_API LCResult lc_line_edit_get_editable(LCComponentHandle handle, bool* outEditable);
LC_API LCResult lc_line_edit_set_editable(LCComponentHandle handle, bool editable);
LC_API LCResult lc_line_edit_get_secret(LCComponentHandle handle, bool* outSecret);
LC_API LCResult lc_line_edit_set_secret(LCComponentHandle handle, bool secret);
LC_API LCResult lc_line_edit_get_max_length(LCComponentHandle handle, int* outMaxLength);
LC_API LCResult lc_line_edit_set_max_length(LCComponentHandle handle, int maxLength);

/* ----- Caret / selection ----- */

LC_API LCResult lc_line_edit_get_caret_position(LCComponentHandle handle, int* outPosition);
LC_API LCResult lc_line_edit_set_caret_position(LCComponentHandle handle, int position);
LC_API LCResult lc_line_edit_select_all(LCComponentHandle handle);
LC_API LCResult lc_line_edit_deselect(LCComponentHandle handle);

/* ----- Font ----- */

LC_API LCResult lc_line_edit_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_line_edit_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_line_edit_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_line_edit_set_font_size(LCComponentHandle handle, float size);

/* ----- Colors ----- */

LC_API LCResult lc_line_edit_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_line_edit_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_line_edit_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_line_edit_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_line_edit_get_selection_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_line_edit_set_selection_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_LINE_EDIT_H */
