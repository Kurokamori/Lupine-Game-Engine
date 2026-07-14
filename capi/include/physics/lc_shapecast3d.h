/**
 * @file lc_shapecast3d.h
 * @brief Lupine Engine C API - ShapeCast3D Component
 *
 * A 3D swept-sphere cast that queries the physics world every physics frame and
 * reports the first body the swept sphere touches. The thick-ray counterpart to
 * RayCast3D, for "can this fit / move here" checks and forgiving line-of-sight.
 *
 * Features:
 * - Cast defined in node-local space, so it follows the owning node.
 * - Configurable swept-sphere radius (0 degenerates to a thin ray).
 * - Optional exclusion of the owner's own body hierarchy.
 * - Immediate re-cast via lc_shapecast3d_force_update.
 *
 * Note: the 3D physics world does not currently apply collision layer masks to
 * casts, so unlike ShapeCast2D this component has no collision mask.
 */

#ifndef LUPINE_CAPI_SHAPECAST3D_H
#define LUPINE_CAPI_SHAPECAST3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ShapeCast3D Component Functions
 * ============================================================================ */

/**
 * @brief Create a ShapeCast3D component on a node
 * @param node The node to add the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_shapecast3d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Configuration ----- */

/** @brief Set the cast endpoint, relative to the node in local space. */
LC_API LCResult lc_shapecast3d_set_target_position(LCComponentHandle handle, LCVec3 target);

/** @brief Get the cast endpoint, relative to the node in local space. */
LC_API LCResult lc_shapecast3d_get_target_position(LCComponentHandle handle, LCVec3* outTarget);

/** @brief Set the radius of the swept sphere. */
LC_API LCResult lc_shapecast3d_set_shape_radius(LCComponentHandle handle, float radius);

/** @brief Get the radius of the swept sphere. */
LC_API LCResult lc_shapecast3d_get_shape_radius(LCComponentHandle handle, float* outRadius);

/** @brief Set whether hits against the owner's own body hierarchy are ignored. */
LC_API LCResult lc_shapecast3d_set_exclude_parent(LCComponentHandle handle, bool exclude);

/** @brief Get whether hits against the owner's own body hierarchy are ignored. */
LC_API LCResult lc_shapecast3d_get_exclude_parent(LCComponentHandle handle, bool* outExclude);

/** @brief Set whether the debug gizmo is also drawn while the game runs. */
LC_API LCResult lc_shapecast3d_set_visible_in_game(LCComponentHandle handle, bool visible);

/** @brief Get whether the debug gizmo is also drawn while the game runs. */
LC_API LCResult lc_shapecast3d_get_visible_in_game(LCComponentHandle handle, bool* outVisible);

/* ----- Query Results ----- */

/** @brief Re-run the cast immediately instead of waiting for the next physics frame. */
LC_API LCResult lc_shapecast3d_force_update(LCComponentHandle handle);

/** @brief Query whether the most recent cast touched a body. */
LC_API LCResult lc_shapecast3d_is_colliding(LCComponentHandle handle, bool* outColliding);

/**
 * @brief Get the node owning the body that was hit.
 * @param outNode Receives the collider node handle, or an invalid handle if none.
 * @return LC_SUCCESS on success, LC_ERROR_NOT_FOUND if nothing is currently hit.
 */
LC_API LCResult lc_shapecast3d_get_collider(LCComponentHandle handle, LCNodeHandle* outNode);

/** @brief Get the world-space contact point (valid only when colliding). */
LC_API LCResult lc_shapecast3d_get_collision_point(LCComponentHandle handle, LCVec3* outPoint);

/** @brief Get the surface normal at the contact point (valid only when colliding). */
LC_API LCResult lc_shapecast3d_get_collision_normal(LCComponentHandle handle, LCVec3* outNormal);

/** @brief Get the fraction along the cast [0,1] where contact occurred (1 if no hit). */
LC_API LCResult lc_shapecast3d_get_collision_fraction(LCComponentHandle handle, float* outFraction);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_SHAPECAST3D_H */
