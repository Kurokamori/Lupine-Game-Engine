/**
 * @file lc_navigation3d.h
 * @brief Lupine Engine C API - 3D navigation, Recast-style baking and pathfinding
 *
 * Exposes the NavigationRegion3D, NavigationAgent3D and NavigationObstacle3D
 * components plus direct access to the global 3D navigation server for path
 * queries. NavigationRegion3D bakes a walkable navmesh from the collision (and
 * optionally render) geometry of its subtree or the whole scene.
 */

#ifndef LUPINE_CAPI_NAVIGATION3D_H
#define LUPINE_CAPI_NAVIGATION3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * NavigationRegion3D
 * ============================================================================ */

/** @brief Create a NavigationRegion3D component. */
LC_API LCResult lc_navigation_region3d_create(const char* name, LCComponentHandle* out_component);

/** @brief Gather geometry, bake the navmesh, and publish it to the server. */
LC_API LCResult lc_navigation_region3d_bake(LCComponentHandle component);

/** @brief Enable or disable the region (disabled regions are excluded from queries). */
LC_API LCResult lc_navigation_region3d_set_enabled(LCComponentHandle component, bool enabled);

/** @brief Query whether the region is enabled. */
LC_API LCResult lc_navigation_region3d_is_enabled(LCComponentHandle component, bool* out_enabled);

/** @brief Number of convex polygons produced by the last bake. */
LC_API LCResult lc_navigation_region3d_get_polygon_count(LCComponentHandle component, int* out_count);

/** @brief Number of triangles in the last baked navmesh. */
LC_API LCResult lc_navigation_region3d_get_triangle_count(LCComponentHandle component, int* out_count);

/* ----- Bake configuration (call before bake) ----- */

LC_API LCResult lc_navigation_region3d_set_cell_size(LCComponentHandle component, float value);
LC_API LCResult lc_navigation_region3d_set_cell_height(LCComponentHandle component, float value);
LC_API LCResult lc_navigation_region3d_set_agent_height(LCComponentHandle component, float value);
LC_API LCResult lc_navigation_region3d_set_agent_radius(LCComponentHandle component, float value);
LC_API LCResult lc_navigation_region3d_set_agent_max_climb(LCComponentHandle component, float value);
LC_API LCResult lc_navigation_region3d_set_max_slope_degrees(LCComponentHandle component, float value);
LC_API LCResult lc_navigation_region3d_set_min_region_area(LCComponentHandle component, int value);

/** @brief Geometry source: 0 = subtree children, 1 = entire scene. */
LC_API LCResult lc_navigation_region3d_set_geometry_source(LCComponentHandle component, int source);

/** @brief Include collision-shape geometry when baking. */
LC_API LCResult lc_navigation_region3d_set_include_collision(LCComponentHandle component, bool include);

/** @brief Include render-mesh geometry (StaticMesh3D / PrimitiveMesh3D) when baking. */
LC_API LCResult lc_navigation_region3d_set_include_meshes(LCComponentHandle component, bool include);

/* ============================================================================
 * NavigationAgent3D
 * ============================================================================ */

/** @brief Create a NavigationAgent3D component. */
LC_API LCResult lc_navigation_agent3d_create(const char* name, LCComponentHandle* out_component);

/** @brief Set the world-space target the agent should navigate toward. */
LC_API LCResult lc_navigation_agent3d_set_target_position(LCComponentHandle component, LCVec3 target);

/** @brief Get the agent's current target position. */
LC_API LCResult lc_navigation_agent3d_get_target_position(LCComponentHandle component, LCVec3* out_target);

/** @brief Get the next world-space point the owner should move toward. */
LC_API LCResult lc_navigation_agent3d_get_next_path_position(LCComponentHandle component, LCVec3* out_position);

/** @brief True once the agent is within the target-desired distance of the goal. */
LC_API LCResult lc_navigation_agent3d_is_navigation_finished(LCComponentHandle component, bool* out_finished);

/** @brief True when a path to the current target exists. */
LC_API LCResult lc_navigation_agent3d_is_target_reachable(LCComponentHandle component, bool* out_reachable);

/** @brief Straight-line distance from the owner to the target. */
LC_API LCResult lc_navigation_agent3d_distance_to_target(LCComponentHandle component, float* out_distance);

/** @brief Number of points in the current path corridor. */
LC_API LCResult lc_navigation_agent3d_get_path_count(LCComponentHandle component, int* out_count);

/** @brief Set the agent's maximum movement speed (used by auto-move). */
LC_API LCResult lc_navigation_agent3d_set_max_speed(LCComponentHandle component, float speed);

/** @brief Get the agent's maximum movement speed. */
LC_API LCResult lc_navigation_agent3d_get_max_speed(LCComponentHandle component, float* out_speed);

/** @brief Enable or disable automatic movement of the owner node. */
LC_API LCResult lc_navigation_agent3d_set_auto_move(LCComponentHandle component, bool auto_move);

/** @brief Discard the current path so it is recomputed on the next update. */
LC_API LCResult lc_navigation_agent3d_reset(LCComponentHandle component);

/** @brief Set the agent's collision radius (avoidance and wall clearance). */
LC_API LCResult lc_navigation_agent3d_set_radius(LCComponentHandle component, float radius);

/** @brief Get the agent's collision radius. */
LC_API LCResult lc_navigation_agent3d_get_radius(LCComponentHandle component, float* out_radius);

/** @brief Enable or disable ORCA local avoidance for this agent. */
LC_API LCResult lc_navigation_agent3d_set_avoidance_enabled(LCComponentHandle component, bool enabled);

/** @brief Query whether ORCA local avoidance is enabled. */
LC_API LCResult lc_navigation_agent3d_is_avoidance_enabled(LCComponentHandle component, bool* out_enabled);

/** @brief Get the velocity chosen on the last update. */
LC_API LCResult lc_navigation_agent3d_get_velocity(LCComponentHandle component, LCVec3* out_velocity);

/* ============================================================================
 * NavigationObstacle3D
 * ============================================================================ */

/** @brief Create a NavigationObstacle3D component. */
LC_API LCResult lc_navigation_obstacle3d_create(const char* name, LCComponentHandle* out_component);

/** @brief Set the obstacle's avoidance disc radius. */
LC_API LCResult lc_navigation_obstacle3d_set_radius(LCComponentHandle component, float radius);

/** @brief Get the obstacle's avoidance disc radius. */
LC_API LCResult lc_navigation_obstacle3d_get_radius(LCComponentHandle component, float* out_radius);

/** @brief Set the carve-volume vertical extent. */
LC_API LCResult lc_navigation_obstacle3d_set_height(LCComponentHandle component, float height);

/** @brief Enable or disable dynamic avoidance for this obstacle. */
LC_API LCResult lc_navigation_obstacle3d_set_avoidance_enabled(LCComponentHandle component, bool enabled);

/** @brief Enable or disable static navmesh carving for this obstacle. */
LC_API LCResult lc_navigation_obstacle3d_set_carve(LCComponentHandle component, bool carve);

/** @brief Re-publish the obstacle's carve volume and avoidance disc. */
LC_API LCResult lc_navigation_obstacle3d_bake(LCComponentHandle component);

/* ============================================================================
 * NavigationServer3D (global queries)
 * ============================================================================ */

/**
 * @brief Compute a path between two world points across all enabled regions.
 * @return LC_SUCCESS if a path was found, LC_ERROR_NOT_FOUND if not.
 */
LC_API LCResult lc_navigation_server3d_query_path(LCVec3 from, LCVec3 to,
                                                  LCVec3* out_points, int max_points,
                                                  int* out_count);

/** @brief Closest navigable point to `p`. */
LC_API LCResult lc_navigation_server3d_get_closest_point(LCVec3 p, LCVec3* out_point);

/** @brief True if `p` projects onto the navigable surface within `vertical_tolerance`. */
LC_API LCResult lc_navigation_server3d_is_point_navigable(LCVec3 p, float vertical_tolerance,
                                                          bool* out_navigable);

/** @brief Surface elevation (world Y) at horizontal position (x, z), if covered. */
LC_API LCResult lc_navigation_server3d_sample_height(float x, float z, float* out_y, bool* out_found);

/** @brief Number of registered navigation regions. */
LC_API LCResult lc_navigation_server3d_get_region_count(int* out_count);

/** @brief Number of registered static carve volumes. */
LC_API LCResult lc_navigation_server3d_get_carve_volume_count(int* out_count);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_NAVIGATION3D_H */
