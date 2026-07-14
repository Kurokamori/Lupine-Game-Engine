/**
 * @file lc_parallax_layer.h
 * @brief Lupine Engine C API - ParallaxLayer Component
 *
 * One depth layer of a parallax background. Attach to a Node2D that is a direct
 * child of the node carrying a ParallaxBackground component; the background
 * repositions this layer each frame so it scrolls at a fraction of the camera's
 * speed determined by motionScale.
 */

#ifndef LUPINE_CAPI_PARALLAX_LAYER_H
#define LUPINE_CAPI_PARALLAX_LAYER_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a ParallaxLayer component on a node
 * @param node The node to add the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_parallax_layer_create(LCNodeHandle node, LCComponentHandle* outHandle);

/** @brief Set the per-axis scroll rate. (1,1) world-fixed, (0,0) screen-locked. */
LC_API LCResult lc_parallax_layer_set_motion_scale(LCComponentHandle handle, LCVec2 scale);

/** @brief Get the per-axis scroll rate. */
LC_API LCResult lc_parallax_layer_get_motion_scale(LCComponentHandle handle, LCVec2* outScale);

/** @brief Set a constant offset added to the layer's parallax displacement. */
LC_API LCResult lc_parallax_layer_set_motion_offset(LCComponentHandle handle, LCVec2 offset);

/** @brief Get the constant motion offset. */
LC_API LCResult lc_parallax_layer_get_motion_offset(LCComponentHandle handle, LCVec2* outOffset);

/** @brief Set the per-axis mirroring distance for seamless tiling (0 = none). */
LC_API LCResult lc_parallax_layer_set_motion_mirroring(LCComponentHandle handle, LCVec2 mirroring);

/** @brief Get the per-axis mirroring distance. */
LC_API LCResult lc_parallax_layer_get_motion_mirroring(LCComponentHandle handle, LCVec2* outMirroring);

/** @brief Get the captured authored local position the motion is applied relative to. */
LC_API LCResult lc_parallax_layer_get_home_position(LCComponentHandle handle, LCVec2* outHome);

/** @brief Re-capture the current node local position as the home position. */
LC_API LCResult lc_parallax_layer_capture_home_position(LCComponentHandle handle);

/**
 * @brief Apply a parallax scroll displacement to the layer immediately.
 *
 * Repositions the layer relative to its home position by `scroll` (after the
 * layer's motionScale/offset/mirroring are factored by the caller). Normally
 * driven each frame by the owning ParallaxBackground; exposed here so scrolling
 * can be driven manually from host code.
 */
LC_API LCResult lc_parallax_layer_apply_scroll(LCComponentHandle handle, LCVec2 scroll);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_PARALLAX_LAYER_H */
