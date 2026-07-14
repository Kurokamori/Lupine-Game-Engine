/**
 * @file lc_dropdown.h
 * @brief Lupine Engine C API - Dropdown (OptionButton) UI component
 */

#ifndef LUPINE_CAPI_DROPDOWN_H
#define LUPINE_CAPI_DROPDOWN_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

LC_API LCResult lc_dropdown_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Items ----- */

LC_API LCResult lc_dropdown_get_item_count(LCComponentHandle handle, int* outCount);
LC_API LCResult lc_dropdown_get_item(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_dropdown_add_item(LCComponentHandle handle, const char* text);
LC_API LCResult lc_dropdown_remove_item(LCComponentHandle handle, int index);
LC_API LCResult lc_dropdown_clear_items(LCComponentHandle handle);

LC_API LCResult lc_dropdown_get_selected_index(LCComponentHandle handle, int* outIndex);
LC_API LCResult lc_dropdown_set_selected_index(LCComponentHandle handle, int index);
LC_API LCResult lc_dropdown_get_selected_text(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);

/* ----- Appearance ----- */

LC_API LCResult lc_dropdown_get_placeholder(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_dropdown_set_placeholder(LCComponentHandle handle, const char* text);
LC_API LCResult lc_dropdown_get_item_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_dropdown_set_item_height(LCComponentHandle handle, float height);
LC_API LCResult lc_dropdown_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_dropdown_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_dropdown_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_dropdown_set_font_size(LCComponentHandle handle, float size);

LC_API LCResult lc_dropdown_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_dropdown_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_dropdown_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_dropdown_set_background_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_DROPDOWN_H */
