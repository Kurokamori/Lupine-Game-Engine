/**
 * @file lc_rich_text_label.h
 * @brief Lupine Engine C API - RichTextLabel UI component (BBCode text)
 */

#ifndef LUPINE_CAPI_RICH_TEXT_LABEL_H
#define LUPINE_CAPI_RICH_TEXT_LABEL_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

LC_API LCResult lc_rich_text_label_create(LCNodeHandle node, LCComponentHandle* outHandle);

LC_API LCResult lc_rich_text_label_get_text(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_rich_text_label_set_text(LCComponentHandle handle, const char* text);
LC_API LCResult lc_rich_text_label_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_rich_text_label_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_rich_text_label_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_rich_text_label_set_font_size(LCComponentHandle handle, float size);
LC_API LCResult lc_rich_text_label_get_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_rich_text_label_set_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_rich_text_label_get_word_wrap(LCComponentHandle handle, bool* outWrap);
LC_API LCResult lc_rich_text_label_set_word_wrap(LCComponentHandle handle, bool wrap);
LC_API LCResult lc_rich_text_label_get_line_spacing(LCComponentHandle handle, float* outSpacing);
LC_API LCResult lc_rich_text_label_set_line_spacing(LCComponentHandle handle, float spacing);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_RICH_TEXT_LABEL_H */
