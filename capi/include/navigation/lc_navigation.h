/**
 * @file lc_navigation.h
 * @brief Lupine Engine C API - 2D navigation and pathfinding
 *
 * Exposes the NavigationRegion2D and NavigationAgent2D components plus direct
 * access to the global 2D navigation server for path queries.
 */

#ifndef LUPINE_CAPI_NAVIGATION_H
#define LUPINE_CAPI_NAVIGATION_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * NavigationRegion2D
 * ============================================================================ */

/**
 * @brief Create a NavigationRegion2D component.
 * @param name Optional component name (may be NULL).
 * @param out_component Output handle.
 * @return LC_SUCCESS on success, error code otherwise.
 */
LC_API LCResult lc_navigation_region2d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Set the region's outline (local-space boundary vertices).
 * @param component Component handle.
 * @param points Array of vertices.
 * @param count Number of vertices (>= 3).
 */
LC_API LCResult lc_navigation_region2d_set_outline(LCComponentHandle component,
                                                   const LCVec2* points, int count);

/**
 * @brief Append a hole (local-space vertices) carved out of the walkable area.
 */
LC_API LCResult lc_navigation_region2d_add_hole(LCComponentHandle component,
                                                const LCVec2* points, int count);

/** @brief Remove all holes from the region. */
LC_API LCResult lc_navigation_region2d_clear_holes(LCComponentHandle component);

/** @brief Enable or disable the region (disabled regions are excluded from queries). */
LC_API LCResult lc_navigation_region2d_set_enabled(LCComponentHandle component, bool enabled);

/** @brief Query whether the region is enabled. */
LC_API LCResult lc_navigation_region2d_is_enabled(LCComponentHandle component, bool* out_enabled);

/** @brief Re-triangulate the region and publish it to the navigation server. */
LC_API LCResult lc_navigation_region2d_bake(LCComponentHandle component);

/** @brief Number of convex polygons produced by the last bake. */
LC_API LCResult lc_navigation_region2d_get_polygon_count(LCComponentHandle component, int* out_count);

/* ============================================================================
 * NavigationAgent2D
 * ============================================================================ */

/**
 * @brief Create a NavigationAgent2D component.
 * @param name Optional component name (may be NULL).
 * @param out_component Output handle.
 */
LC_API LCResult lc_navigation_agent2d_create(const char* name, LCComponentHandle* out_component);

/** @brief Set the world-space target the agent should navigate toward. */
LC_API LCResult lc_navigation_agent2d_set_target_position(LCComponentHandle component, LCVec2 target);

/** @brief Get the agent's current target position. */
LC_API LCResult lc_navigation_agent2d_get_target_position(LCComponentHandle component, LCVec2* out_target);

/** @brief Get the next world-space point the owner should move toward. */
LC_API LCResult lc_navigation_agent2d_get_next_path_position(LCComponentHandle component, LCVec2* out_position);

/** @brief True once the agent is within the target-desired distance of the goal. */
LC_API LCResult lc_navigation_agent2d_is_navigation_finished(LCComponentHandle component, bool* out_finished);

/** @brief True when a path to the current target exists. */
LC_API LCResult lc_navigation_agent2d_is_target_reachable(LCComponentHandle component, bool* out_reachable);

/** @brief Straight-line distance from the owner to the target. */
LC_API LCResult lc_navigation_agent2d_distance_to_target(LCComponentHandle component, float* out_distance);

/** @brief Number of points in the current path corridor. */
LC_API LCResult lc_navigation_agent2d_get_path_count(LCComponentHandle component, int* out_count);

/** @brief Set the agent's maximum movement speed (used by auto-move). */
LC_API LCResult lc_navigation_agent2d_set_max_speed(LCComponentHandle component, float speed);

/** @brief Get the agent's maximum movement speed. */
LC_API LCResult lc_navigation_agent2d_get_max_speed(LCComponentHandle component, float* out_speed);

/** @brief Enable or disable automatic movement of the owner node. */
LC_API LCResult lc_navigation_agent2d_set_auto_move(LCComponentHandle component, bool auto_move);

/** @brief Discard the current path so it is recomputed on the next update. */
LC_API LCResult lc_navigation_agent2d_reset(LCComponentHandle component);

/* ----- Agent local avoidance (ORCA) ----- */

/** @brief Set the agent's collision radius (used for avoidance and wall clearance). */
LC_API LCResult lc_navigation_agent2d_set_radius(LCComponentHandle component, float radius);

/** @brief Get the agent's collision radius. */
LC_API LCResult lc_navigation_agent2d_get_radius(LCComponentHandle component, float* out_radius);

/** @brief Enable or disable ORCA local avoidance for this agent. */
LC_API LCResult lc_navigation_agent2d_set_avoidance_enabled(LCComponentHandle component, bool enabled);

/** @brief Query whether ORCA local avoidance is enabled. */
LC_API LCResult lc_navigation_agent2d_is_avoidance_enabled(LCComponentHandle component, bool* out_enabled);

/** @brief Get the velocity chosen on the last update (ORCA-corrected when avoidance is on). */
LC_API LCResult lc_navigation_agent2d_get_velocity(LCComponentHandle component, LCVec2* out_velocity);

/* ============================================================================
 * NavigationObstacle2D
 * ============================================================================ */

/**
 * @brief Create a NavigationObstacle2D component.
 * @param name Optional component name (may be NULL).
 * @param out_component Output handle.
 */
LC_API LCResult lc_navigation_obstacle2d_create(const char* name, LCComponentHandle* out_component);

/** @brief Set the obstacle's avoidance disc radius. */
LC_API LCResult lc_navigation_obstacle2d_set_radius(LCComponentHandle component, float radius);

/** @brief Get the obstacle's avoidance disc radius. */
LC_API LCResult lc_navigation_obstacle2d_get_radius(LCComponentHandle component, float* out_radius);

/** @brief Enable or disable dynamic avoidance for this obstacle. */
LC_API LCResult lc_navigation_obstacle2d_set_avoidance_enabled(LCComponentHandle component, bool enabled);

/** @brief Enable or disable static navmesh carving for this obstacle. */
LC_API LCResult lc_navigation_obstacle2d_set_carve(LCComponentHandle component, bool carve);

/** @brief Set the local-space carving polygon vertices. */
LC_API LCResult lc_navigation_obstacle2d_set_vertices(LCComponentHandle component,
                                                      const LCVec2* points, int count);

/** @brief Re-publish the obstacle's carve polygon and avoidance disc. */
LC_API LCResult lc_navigation_obstacle2d_bake(LCComponentHandle component);

/* ============================================================================
 * NavigationServer2D (global queries)
 * ============================================================================ */

/**
 * @brief Compute a path between two world points across all enabled regions.
 * @param from Start point.
 * @param to End point.
 * @param out_points Caller-allocated buffer receiving path points.
 * @param max_points Capacity of out_points.
 * @param out_count Receives the number of points written (may exceed max_points
 *                  conceptually, but no more than max_points are written).
 * @return LC_SUCCESS if a path was found, LC_ERROR_NOT_FOUND if not.
 */
LC_API LCResult lc_navigation_server2d_query_path(LCVec2 from, LCVec2 to,
                                                  LCVec2* out_points, int max_points,
                                                  int* out_count);

/** @brief Closest navigable point to `p`. */
LC_API LCResult lc_navigation_server2d_get_closest_point(LCVec2 p, LCVec2* out_point);

/** @brief True if `p` lies on the navigable surface. */
LC_API LCResult lc_navigation_server2d_is_point_navigable(LCVec2 p, bool* out_navigable);

/** @brief Number of registered navigation regions. */
LC_API LCResult lc_navigation_server2d_get_region_count(int* out_count);

/** @brief Number of registered static carving obstacles. */
LC_API LCResult lc_navigation_server2d_get_obstacle_count(int* out_count);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_NAVIGATION_H */
