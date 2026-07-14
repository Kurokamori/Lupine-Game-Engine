/**
 * @file lc_shapecast2d.h
 * @brief Lupine Engine C API - ShapeCast2D Component
 *
 * A 2D swept-circle cast that queries the physics world every physics frame and
 * reports the first body the swept shape touches. The thick-ray counterpart to
 * RayCast2D, for "can this fit / move here" checks and forgiving line-of-sight.
 *
 * Features:
 * - Cast defined in node-local space, so it follows the owning node.
 * - Configurable swept-circle radius (0 degenerates to a thin ray).
 * - Collision-layer mask to select which bodies are eligible.
 * - Optional exclusion of the owner's own body hierarchy.
 * - Immediate re-cast via lc_shapecast2d_force_update.
 */

#ifndef LUPINE_CAPI_SHAPECAST2D_H
#define LUPINE_CAPI_SHAPECAST2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ShapeCast2D Component Functions
 * ============================================================================ */

/**
 * @brief Create a ShapeCast2D component on a node
 * @param node The node to add the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shapecast2d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Configuration ----- */

/** @brief Set the cast endpoint, relative to the node in local space. */
LC_API LCResult lc_shapecast2d_set_target_position(LCComponentHandle handle, LCVec2 target);

/** @brief Get the cast endpoint, relative to the node in local space. */
LC_API LCResult lc_shapecast2d_get_target_position(LCComponentHandle handle, LCVec2* outTarget);

/** @brief Set the radius of the swept circle. */
LC_API LCResult lc_shapecast2d_set_shape_radius(LCComponentHandle handle, float radius);

/** @brief Get the radius of the swept circle. */
LC_API LCResult lc_shapecast2d_get_shape_radius(LCComponentHandle handle, float* outRadius);

/** @brief Set the bitmask of collision layers the cast is allowed to hit. */
LC_API LCResult lc_shapecast2d_set_collision_mask(LCComponentHandle handle, uint32_t mask);

/** @brief Get the bitmask of collision layers the cast is allowed to hit. */
LC_API LCResult lc_shapecast2d_get_collision_mask(LCComponentHandle handle, uint32_t* outMask);

/** @brief Set whether hits against the owner's own body hierarchy are ignored. */
LC_API LCResult lc_shapecast2d_set_exclude_parent(LCComponentHandle handle, bool exclude);

/** @brief Get whether hits against the owner's own body hierarchy are ignored. */
LC_API LCResult lc_shapecast2d_get_exclude_parent(LCComponentHandle handle, bool* outExclude);

/** @brief Set whether the debug gizmo is also drawn while the game runs. */
LC_API LCResult lc_shapecast2d_set_visible_in_game(LCComponentHandle handle, bool visible);

/** @brief Get whether the debug gizmo is also drawn while the game runs. */
LC_API LCResult lc_shapecast2d_get_visible_in_game(LCComponentHandle handle, bool* outVisible);

/* ----- Query Results ----- */

/** @brief Re-run the cast immediately instead of waiting for the next physics frame. */
LC_API LCResult lc_shapecast2d_force_update(LCComponentHandle handle);

/** @brief Query whether the most recent cast touched a body. */
LC_API LCResult lc_shapecast2d_is_colliding(LCComponentHandle handle, bool* outColliding);

/**
 * @brief Get the node owning the body that was hit.
 * @param outNode Receives the collider node handle, or an invalid handle if none.
 * @return LC_SUCCESS on success, LC_ERROR_NOT_FOUND if nothing is currently hit.
 */
LC_API LCResult lc_shapecast2d_get_collider(LCComponentHandle handle, LCNodeHandle* outNode);

/** @brief Get the world-space contact point (valid only when colliding). */
LC_API LCResult lc_shapecast2d_get_collision_point(LCComponentHandle handle, LCVec2* outPoint);

/** @brief Get the surface normal at the contact point (valid only when colliding). */
LC_API LCResult lc_shapecast2d_get_collision_normal(LCComponentHandle handle, LCVec2* outNormal);

/** @brief Get the fraction along the cast [0,1] where contact occurred (1 if no hit). */
LC_API LCResult lc_shapecast2d_get_collision_fraction(LCComponentHandle handle, float* outFraction);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_SHAPECAST2D_H */
