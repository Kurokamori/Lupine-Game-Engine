/**
 * @file lc_parallax_background.h
 * @brief Lupine Engine C API - ParallaxBackground Component
 *
 * Drives a set of ParallaxLayer children to create a multi-depth scrolling
 * background. Attach to a Node2D whose direct children each carry a
 * ParallaxLayer; every frame this reads the active Camera2D (or a manual scroll)
 * and repositions each layer according to its motionScale.
 */

#ifndef LUPINE_CAPI_PARALLAX_BACKGROUND_H
#define LUPINE_CAPI_PARALLAX_BACKGROUND_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a ParallaxBackground component on a node
 * @param node The node to add the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_parallax_background_create(LCNodeHandle node, LCComponentHandle* outHandle);

/** @brief Set the per-axis multiplier applied to the camera-derived scroll. */
LC_API LCResult lc_parallax_background_set_scroll_scale(LCComponentHandle handle, LCVec2 scale);

/** @brief Get the per-axis scroll multiplier. */
LC_API LCResult lc_parallax_background_get_scroll_scale(LCComponentHandle handle, LCVec2* outScale);

/** @brief Set the constant offset added to the scroll before it reaches the layers. */
LC_API LCResult lc_parallax_background_set_scroll_base_offset(LCComponentHandle handle, LCVec2 offset);

/** @brief Get the constant scroll base offset. */
LC_API LCResult lc_parallax_background_get_scroll_base_offset(LCComponentHandle handle, LCVec2* outOffset);

/** @brief Set whether the camera is ignored and the scroll comes only from set_scroll_offset. */
LC_API LCResult lc_parallax_background_set_ignore_camera_scroll(LCComponentHandle handle, bool ignore);

/** @brief Get whether the camera scroll is ignored. */
LC_API LCResult lc_parallax_background_get_ignore_camera_scroll(LCComponentHandle handle, bool* outIgnore);

/** @brief Set the manual scroll used when ignore_camera_scroll is set. */
LC_API LCResult lc_parallax_background_set_scroll_offset(LCComponentHandle handle, LCVec2 scroll);

/** @brief Get the manual scroll value. */
LC_API LCResult lc_parallax_background_get_scroll_offset(LCComponentHandle handle, LCVec2* outScroll);

/** @brief Get the scroll value computed and applied to the layers on the last update. */
LC_API LCResult lc_parallax_background_get_applied_scroll(LCComponentHandle handle, LCVec2* outScroll);

/** @brief Recompute the scroll and reposition every layer immediately. */
LC_API LCResult lc_parallax_background_update_layers(LCComponentHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_PARALLAX_BACKGROUND_H */
