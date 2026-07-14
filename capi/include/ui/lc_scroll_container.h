/**
 * @file lc_scroll_container.h
 * @brief Lupine Engine C API - ScrollContainer UI component
 *
 * ScrollContainer is a Container that clips its content to its viewport and
 * scrolls it with the mouse wheel or draggable scrollbars. Inherited Container
 * properties (size, padding, background, border, etc.) are available through the
 * lc_container_* functions, since a ScrollContainer is a Container.
 */

#ifndef LUPINE_CAPI_SCROLL_CONTAINER_H
#define LUPINE_CAPI_SCROLL_CONTAINER_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Scrollbar visibility policy
 */
typedef enum LCScrollBarVisibility {
    LC_SCROLLBAR_AUTO = 0,   /**< Show only when content overflows */
    LC_SCROLLBAR_ALWAYS = 1,
    LC_SCROLLBAR_NEVER = 2
} LCScrollBarVisibility;

/**
 * @brief Create a ScrollContainer component on a node
 * @param node Node handle to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_scroll_container_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Scroll position ----- */

LC_API LCResult lc_scroll_container_get_h_scroll(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_scroll_container_set_h_scroll(LCComponentHandle handle, float value);
LC_API LCResult lc_scroll_container_get_v_scroll(LCComponentHandle handle, float* outValue);
LC_API LCResult lc_scroll_container_set_v_scroll(LCComponentHandle handle, float value);
LC_API LCResult lc_scroll_container_get_content_size(LCComponentHandle handle, LCVec2* outSize);

/* ----- Enable flags ----- */

LC_API LCResult lc_scroll_container_get_h_scroll_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_scroll_container_set_h_scroll_enabled(LCComponentHandle handle, bool enabled);
LC_API LCResult lc_scroll_container_get_v_scroll_enabled(LCComponentHandle handle, bool* outEnabled);
LC_API LCResult lc_scroll_container_set_v_scroll_enabled(LCComponentHandle handle, bool enabled);

/* ----- Appearance / behavior ----- */

LC_API LCResult lc_scroll_container_get_scroll_speed(LCComponentHandle handle, float* outSpeed);
LC_API LCResult lc_scroll_container_set_scroll_speed(LCComponentHandle handle, float speed);
LC_API LCResult lc_scroll_container_get_scroll_bar_width(LCComponentHandle handle, float* outWidth);
LC_API LCResult lc_scroll_container_set_scroll_bar_width(LCComponentHandle handle, float width);

LC_API LCResult lc_scroll_container_get_h_scroll_bar_visibility(LCComponentHandle handle, LCScrollBarVisibility* outVis);
LC_API LCResult lc_scroll_container_set_h_scroll_bar_visibility(LCComponentHandle handle, LCScrollBarVisibility vis);
LC_API LCResult lc_scroll_container_get_v_scroll_bar_visibility(LCComponentHandle handle, LCScrollBarVisibility* outVis);
LC_API LCResult lc_scroll_container_set_v_scroll_bar_visibility(LCComponentHandle handle, LCScrollBarVisibility vis);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_SCROLL_CONTAINER_H */
