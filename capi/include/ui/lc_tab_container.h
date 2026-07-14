/**
 * @file lc_tab_container.h
 * @brief Lupine Engine C API - TabContainer UI component
 *
 * A TabContainer is a Container, so inherited container properties (size, padding,
 * background, border, etc.) are available via the lc_container_* functions.
 */

#ifndef LUPINE_CAPI_TAB_CONTAINER_H
#define LUPINE_CAPI_TAB_CONTAINER_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a TabContainer component on a node
 */
LC_API LCResult lc_tab_container_create(LCNodeHandle node, LCComponentHandle* outHandle);

LC_API LCResult lc_tab_container_get_current_tab(LCComponentHandle handle, int* outIndex);
LC_API LCResult lc_tab_container_set_current_tab(LCComponentHandle handle, int index);
LC_API LCResult lc_tab_container_get_tab_count(LCComponentHandle handle, int* outCount);
LC_API LCResult lc_tab_container_get_tab_title(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired);

LC_API LCResult lc_tab_container_get_tab_height(LCComponentHandle handle, float* outHeight);
LC_API LCResult lc_tab_container_set_tab_height(LCComponentHandle handle, float height);
LC_API LCResult lc_tab_container_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired);
LC_API LCResult lc_tab_container_set_font_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_tab_container_get_font_size(LCComponentHandle handle, float* outSize);
LC_API LCResult lc_tab_container_set_font_size(LCComponentHandle handle, float size);

LC_API LCResult lc_tab_container_get_font_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_tab_container_set_font_color(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_tab_container_get_active_tab_color(LCComponentHandle handle, LCColor* outColor);
LC_API LCResult lc_tab_container_set_active_tab_color(LCComponentHandle handle, LCColor color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_TAB_CONTAINER_H */
