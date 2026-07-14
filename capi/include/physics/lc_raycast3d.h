/**
 * @file lc_raycast3d.h
 * @brief Lupine Engine C API - RayCast3D Component
 *
 * A 3D ray that queries the physics world every physics frame and reports the
 * first body it intersects. Point it with the target position and read back the
 * hit each frame with the query functions below.
 *
 * Features:
 * - Ray defined in node-local space, so it follows the owning node.
 * - Optional exclusion of the owner's own body hierarchy.
 * - Immediate re-cast via lc_raycast3d_force_update.
 *
 * Note: the 3D physics world does not currently apply collision layer masks to
 * raycasts, so unlike RayCast2D this component has no collision mask.
 */

#ifndef LUPINE_CAPI_RAYCAST3D_H
#define LUPINE_CAPI_RAYCAST3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * RayCast3D Component Functions
 * ============================================================================ */

/**
 * @brief Create a RayCast3D component on a node
 * @param node The node to add the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_raycast3d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Configuration ----- */

/**
 * @brief Set the ray endpoint, relative to the node in local space.
 */
LC_API LCResult lc_raycast3d_set_target_position(LCComponentHandle handle, LCVec3 target);

/**
 * @brief Get the ray endpoint, relative to the node in local space.
 */
LC_API LCResult lc_raycast3d_get_target_position(LCComponentHandle handle, LCVec3* outTarget);

/**
 * @brief Set whether hits against the owner's own body hierarchy are ignored.
 */
LC_API LCResult lc_raycast3d_set_exclude_parent(LCComponentHandle handle, bool exclude);

/**
 * @brief Get whether hits against the owner's own body hierarchy are ignored.
 */
LC_API LCResult lc_raycast3d_get_exclude_parent(LCComponentHandle handle, bool* outExclude);

/**
 * @brief Set whether the debug ray gizmo is also drawn while the game runs.
 */
LC_API LCResult lc_raycast3d_set_visible_in_game(LCComponentHandle handle, bool visible);

/**
 * @brief Get whether the debug ray gizmo is also drawn while the game runs.
 */
LC_API LCResult lc_raycast3d_get_visible_in_game(LCComponentHandle handle, bool* outVisible);

/* ----- Query Results ----- */

/**
 * @brief Re-run the cast immediately instead of waiting for the next physics frame.
 */
LC_API LCResult lc_raycast3d_force_update(LCComponentHandle handle);

/**
 * @brief Query whether the most recent cast intersected a body.
 */
LC_API LCResult lc_raycast3d_is_colliding(LCComponentHandle handle, bool* outColliding);

/**
 * @brief Get the node owning the body that was hit.
 * @param outNode Receives the collider node handle, or an invalid handle if none.
 * @return LC_SUCCESS on success, LC_ERROR_NOT_FOUND if nothing is currently hit.
 */
LC_API LCResult lc_raycast3d_get_collider(LCComponentHandle handle, LCNodeHandle* outNode);

/**
 * @brief Get the world-space point where the ray hit (valid only when colliding).
 */
LC_API LCResult lc_raycast3d_get_collision_point(LCComponentHandle handle, LCVec3* outPoint);

/**
 * @brief Get the surface normal at the hit point (valid only when colliding).
 */
LC_API LCResult lc_raycast3d_get_collision_normal(LCComponentHandle handle, LCVec3* outNormal);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_RAYCAST3D_H */
