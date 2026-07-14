/**
 * @file lc_text_edit.h
 * @brief Lupine Engine C API - TextEdit UI component (multi-line text area)
 */

#ifndef LUPINE_CAPI_TEXT_EDIT_H
#define LUPINE_CAPI_TEXT_EDIT_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a TextEdit component on a node
 */
LC_API LCResult lc_text_edit_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Text (UTF-8; get writes into the buffer and reports the required size) ----- */

LC_API LCResult lc_text_edit_get_text(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_text_edit_set_text(LCComponentHandle handle, const char* text);
LC_API LCResult lc_text_edit_get_editable(LCComponentHandle handle, bool* outEditable);
LC_API LCResult lc_text_edit_set_editable(LCComponentHandle handle, bool editable);

/* ----- Caret / selection ----- */

LC_API LCResult lc_text_edit_get_caret_position(LCComponentHandle handle, int* outPosition);
LC_API LCResult lc_text_edit_set_caret_position(LCComponentHandle handle, int position);
LC_API LCResult lc_text_edit_select_all(LCComponentHandle handle);
LC_API LCResult lc_text_edit_deselect(LCComponentHandle handle);

/* ----- Font ----- */

LC_API LCResult lc_text_edit_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_text_edit_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_text_edit_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_text_edit_set_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_text_edit_get_line_spacing(LCComponentHandle handle, float* outSpacing);
LC_API LCResult lc_text_edit_set_line_spacing(LCComponentHandle handle, float spacing);

/* ----- Colors ----- */

LC_API LCResult lc_text_edit_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_text_edit_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_text_edit_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_text_edit_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_text_edit_get_selection_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_text_edit_set_selection_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_TEXT_EDIT_H */
