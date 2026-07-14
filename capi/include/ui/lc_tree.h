/**
 * @file lc_tree.h
 * @brief Lupine Engine C API - Tree UI component (hierarchical list)
 */

#ifndef LUPINE_CAPI_TREE_H
#define LUPINE_CAPI_TREE_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

LC_API LCResult lc_tree_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Items (depth = nesting level) ----- */

LC_API LCResult lc_tree_get_item_count(LCComponentHandle handle, int* outCount);
LC_API LCResult lc_tree_get_item_text(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_tree_get_item_depth(LCComponentHandle handle, int index, int* outDepth);
LC_API LCResult lc_tree_add_item(LCComponentHandle handle, const char* text, int depth);
LC_API LCResult lc_tree_clear_items(LCComponentHandle handle);

LC_API LCResult lc_tree_get_selected_index(LCComponentHandle handle, int* outIndex);
LC_API LCResult lc_tree_set_selected_index(LCComponentHandle handle, int index);

/* ----- Expand / collapse ----- */

LC_API LCResult lc_tree_is_expanded(LCComponentHandle handle, int index, bool* outExpanded);
LC_API LCResult lc_tree_set_expanded(LCComponentHandle handle, int index, bool expanded);
LC_API LCResult lc_tree_toggle_expand(LCComponentHandle handle, int index);

/* ----- Appearance ----- */

LC_API LCResult lc_tree_get_item_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_tree_set_item_height(LCComponentHandle handle, float height);
LC_API LCResult lc_tree_get_indent_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_tree_set_indent_width(LCComponentHandle handle, float width);
LC_API LCResult lc_tree_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_tree_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_tree_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_tree_set_font_size(LCComponentHandle handle, float size);

LC_API LCResult lc_tree_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_tree_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_tree_get_selection_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_tree_set_selection_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_TREE_H */
