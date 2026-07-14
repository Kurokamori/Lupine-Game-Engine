/**
 * @file lc_item_list.h
 * @brief Lupine Engine C API - ItemList UI component (scrollable selection list)
 */

#ifndef LUPINE_CAPI_ITEM_LIST_H
#define LUPINE_CAPI_ITEM_LIST_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an ItemList component on a node
 */
LC_API LCResult lc_item_list_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Items ----- */

LC_API LCResult lc_item_list_get_item_count(LCComponentHandle handle, int* outCount);
LC_API LCResult lc_item_list_get_item(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_item_list_add_item(LCComponentHandle handle, const char* text);
LC_API LCResult lc_item_list_remove_item(LCComponentHandle handle, int index);
LC_API LCResult lc_item_list_clear_items(LCComponentHandle handle);

LC_API LCResult lc_item_list_get_selected_index(LCComponentHandle handle, int* outIndex);
LC_API LCResult lc_item_list_set_selected_index(LCComponentHandle handle, int index);

/* ----- Appearance ----- */

LC_API LCResult lc_item_list_get_item_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_item_list_set_item_height(LCComponentHandle handle, float height);
LC_API LCResult lc_item_list_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_item_list_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_item_list_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_item_list_set_font_size(LCComponentHandle handle, float size);

LC_API LCResult lc_item_list_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_item_list_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_item_list_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_item_list_set_background_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_item_list_get_selection_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_item_list_set_selection_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_ITEM_LIST_H */
