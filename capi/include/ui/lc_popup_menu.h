/**
 * @file lc_popup_menu.h
 * @brief Lupine Engine C API - PopupMenu UI component
 */

#ifndef LUPINE_CAPI_POPUP_MENU_H
#define LUPINE_CAPI_POPUP_MENU_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

LC_API LCResult lc_popup_menu_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Items ----- */

LC_API LCResult lc_popup_menu_get_item_count(LCComponentHandle handle, int* outCount);
LC_API LCResult lc_popup_menu_get_item(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_popup_menu_add_item(LCComponentHandle handle, const char* text);
LC_API LCResult lc_popup_menu_remove_item(LCComponentHandle handle, int index);
LC_API LCResult lc_popup_menu_clear_items(LCComponentHandle handle);

/* ----- Open / close ----- */

LC_API LCResult lc_popup_menu_popup(LCComponentHandle handle);
LC_API LCResult lc_popup_menu_popup_at(LCComponentHandle handle, LCVec2 position);
LC_API LCResult lc_popup_menu_close(LCComponentHandle handle);
LC_API LCResult lc_popup_menu_is_open(LCComponentHandle handle, bool* outOpen);

/* ----- Appearance ----- */

LC_API LCResult lc_popup_menu_get_item_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_popup_menu_set_item_height(LCComponentHandle handle, float height);
LC_API LCResult lc_popup_menu_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_popup_menu_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_popup_menu_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_popup_menu_set_font_size(LCComponentHandle handle, float size);

LC_API LCResult lc_popup_menu_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_popup_menu_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_popup_menu_get_background_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_popup_menu_set_background_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_POPUP_MENU_H */
