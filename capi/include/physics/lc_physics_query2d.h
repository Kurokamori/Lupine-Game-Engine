/**
 * @file lc_physics_query2d.h
 * @brief Lupine Engine C API - 2D Physics Query System
 *
 * This header provides 2D physics query functionality including:
 * - Raycasting (single hit and all hits)
 * - Point overlap queries
 * - Circle overlap queries
 * - Box overlap queries
 * - Shape casting (swept circle)
 *
 * All query functions require an active scene with a Physics2D world.
 */

#ifndef LUPINE_CAPI_PHYSICS_QUERY2D_H
#define LUPINE_CAPI_PHYSICS_QUERY2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * Query Result Types
 * ============================================================================ */

/**
 * @brief 2D Raycast hit result
 */
typedef struct LCRaycastHit2D {
    uint64_t bodyId[2];     /**< Body UUID that was hit (128-bit as two 64-bit values) */
    LCVec2 point;           /**< Hit point in world space */
    LCVec2 normal;          /**< Surface normal at hit point */
    float fraction;         /**< Fraction along ray where hit occurred [0, 1] */
    bool hit;               /**< True if raycast hit something */
} LCRaycastHit2D;

/**
 * @brief 2D Overlap query result
 */
typedef struct LCOverlapResult2D {
    uint64_t bodyId[2];     /**< Body UUID that overlaps (128-bit as two 64-bit values) */
    LCVec2 point;           /**< Point of overlap (approximate) */
} LCOverlapResult2D;

/**
 * @brief 2D Shape cast hit result
 */
typedef struct LCShapeCastHit2D {
    uint64_t bodyId[2];     /**< Body UUID that was hit (128-bit as two 64-bit values) */
    LCVec2 point;           /**< Hit point in world space */
    LCVec2 normal;          /**< Surface normal at hit point */
    float fraction;         /**< Fraction along cast where hit occurred [0, 1] */
    bool hit;               /**< True if shape cast hit something */
} LCShapeCastHit2D;

/* ============================================================================
 * Raycast Functions
 * ============================================================================ */

/**
 * @brief Cast a ray and get the first hit
 *
 * Casts a ray from origin in the specified direction and returns
 * the closest hit (if any).
 *
 * @param origin Starting point of the ray in world space
 * @param direction Direction of the ray (will be normalized internally)
 * @param maxDistance Maximum distance to check
 * @param hit Output parameter for hit result
 * @return LC_SUCCESS on success (check hit.hit for actual hit), error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCRaycastHit2D hit;
 * LCVec2 origin = {0.0f, 0.0f};
 * LCVec2 direction = {1.0f, 0.0f};
 * if (lc_physics2d_raycast(origin, direction, 100.0f, &hit) == LC_SUCCESS && hit.hit) {
 *     printf("Hit at: %f, %f\n", hit.point.x, hit.point.y);
 * }
 * @endcode
 */
LC_API LCResult lc_physics2d_raycast(LCVec2 origin, LCVec2 direction, float maxDistance, LCRaycastHit2D* hit);

/**
 * @brief Cast a ray and get the first hit, ignoring a specific body
 *
 * Same as lc_physics2d_raycast but allows ignoring a specific physics body
 * (useful for player character raycasts that shouldn't hit the player).
 *
 * @param origin Starting point of the ray in world space
 * @param direction Direction of the ray (will be normalized internally)
 * @param maxDistance Maximum distance to check
 * @param ignoreBodyId UUID of body to ignore (pass NULL to not ignore any body)
 * @param hit Output parameter for hit result
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_raycast_ignore(LCVec2 origin, LCVec2 direction, float maxDistance,
                                            const uint64_t* ignoreBodyId, LCRaycastHit2D* hit);

/**
 * @brief Cast a ray and get all hits
 *
 * Casts a ray and returns all bodies that intersect it.
 * Results are sorted by distance (closest first).
 *
 * @param origin Starting point of the ray in world space
 * @param direction Direction of the ray (will be normalized internally)
 * @param maxDistance Maximum distance to check
 * @param hits Output buffer for hit results (must be pre-allocated)
 * @param maxHits Maximum number of hits to return
 * @param hitCount Output parameter for actual number of hits
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCRaycastHit2D hits[10];
 * uint32_t hitCount = 0;
 * LCVec2 origin = {0.0f, 0.0f};
 * LCVec2 direction = {1.0f, 0.0f};
 * if (lc_physics2d_raycast_all(origin, direction, 100.0f, hits, 10, &hitCount) == LC_SUCCESS) {
 *     for (uint32_t i = 0; i < hitCount; i++) {
 *         printf("Hit %d at: %f, %f\n", i, hits[i].point.x, hits[i].point.y);
 *     }
 * }
 * @endcode
 */
LC_API LCResult lc_physics2d_raycast_all(LCVec2 origin, LCVec2 direction, float maxDistance,
                                         LCRaycastHit2D* hits, uint32_t maxHits, uint32_t* hitCount);

/**
 * @brief Cast a ray and get all hits, ignoring a specific body
 *
 * Same as lc_physics2d_raycast_all but allows ignoring a specific physics body.
 *
 * @param origin Starting point of the ray in world space
 * @param direction Direction of the ray (will be normalized internally)
 * @param maxDistance Maximum distance to check
 * @param ignoreBodyId UUID of body to ignore (pass NULL to not ignore any body)
 * @param hits Output buffer for hit results (must be pre-allocated)
 * @param maxHits Maximum number of hits to return
 * @param hitCount Output parameter for actual number of hits
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_raycast_all_ignore(LCVec2 origin, LCVec2 direction, float maxDistance,
                                                const uint64_t* ignoreBodyId,
                                                LCRaycastHit2D* hits, uint32_t maxHits, uint32_t* hitCount);

/* ============================================================================
 * Overlap Query Functions
 * ============================================================================ */

/**
 * @brief Query all bodies overlapping a point
 *
 * Returns all physics bodies that contain the specified point.
 *
 * @param point Point to test in world space
 * @param results Output buffer for overlap results (must be pre-allocated)
 * @param maxResults Maximum number of results to return
 * @param resultCount Output parameter for actual number of results
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCOverlapResult2D results[10];
 * uint32_t count = 0;
 * LCVec2 point = {5.0f, 5.0f};
 * if (lc_physics2d_overlap_point(point, results, 10, &count) == LC_SUCCESS) {
 *     printf("Found %d bodies at point\n", count);
 * }
 * @endcode
 */
LC_API LCResult lc_physics2d_overlap_point(LCVec2 point, LCOverlapResult2D* results,
                                           uint32_t maxResults, uint32_t* resultCount);

/**
 * @brief Query all bodies overlapping a circle
 *
 * Returns all physics bodies that overlap with the specified circle.
 *
 * @param center Center of the circle in world space
 * @param radius Radius of the circle
 * @param results Output buffer for overlap results (must be pre-allocated)
 * @param maxResults Maximum number of results to return
 * @param resultCount Output parameter for actual number of results
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCOverlapResult2D results[10];
 * uint32_t count = 0;
 * LCVec2 center = {0.0f, 0.0f};
 * if (lc_physics2d_overlap_circle(center, 5.0f, results, 10, &count) == LC_SUCCESS) {
 *     printf("Found %d bodies in circle\n", count);
 * }
 * @endcode
 */
LC_API LCResult lc_physics2d_overlap_circle(LCVec2 center, float radius,
                                            LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount);

/**
 * @brief Query all bodies overlapping an axis-aligned box
 *
 * Returns all physics bodies that overlap with the specified box.
 *
 * @param center Center of the box in world space
 * @param size Full size of the box (width, height)
 * @param results Output buffer for overlap results (must be pre-allocated)
 * @param maxResults Maximum number of results to return
 * @param resultCount Output parameter for actual number of results
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_overlap_box(LCVec2 center, LCVec2 size,
                                         LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount);

/**
 * @brief Query all bodies overlapping a rotated box
 *
 * Returns all physics bodies that overlap with the specified oriented box.
 *
 * @param center Center of the box in world space
 * @param size Full size of the box (width, height)
 * @param angle Rotation angle in radians
 * @param results Output buffer for overlap results (must be pre-allocated)
 * @param maxResults Maximum number of results to return
 * @param resultCount Output parameter for actual number of results
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCOverlapResult2D results[10];
 * uint32_t count = 0;
 * LCVec2 center = {0.0f, 0.0f};
 * LCVec2 size = {10.0f, 5.0f};
 * float angle = 0.785f; // 45 degrees
 * if (lc_physics2d_overlap_box_rotated(center, size, angle, results, 10, &count) == LC_SUCCESS) {
 *     printf("Found %d bodies in box\n", count);
 * }
 * @endcode
 */
LC_API LCResult lc_physics2d_overlap_box_rotated(LCVec2 center, LCVec2 size, float angle,
                                                 LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount);

/* ============================================================================
 * Shape Cast Functions
 * ============================================================================ */

/**
 * @brief Cast a circle shape and get the first hit
 *
 * Sweeps a circle from start to end and returns the first hit (if any).
 * Useful for character movement collision detection.
 *
 * @param start Starting center of the circle in world space
 * @param end Ending center of the circle in world space
 * @param radius Radius of the circle
 * @param hit Output parameter for hit result
 * @return LC_SUCCESS on success (check hit.hit for actual hit), error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCShapeCastHit2D hit;
 * LCVec2 start = {0.0f, 0.0f};
 * LCVec2 end = {10.0f, 0.0f};
 * if (lc_physics2d_circle_cast(start, end, 0.5f, &hit) == LC_SUCCESS && hit.hit) {
 *     printf("Circle cast hit at: %f, %f\n", hit.point.x, hit.point.y);
 * }
 * @endcode
 */
LC_API LCResult lc_physics2d_circle_cast(LCVec2 start, LCVec2 end, float radius, LCShapeCastHit2D* hit);

/**
 * @brief Cast a circle shape and get the first hit, ignoring a specific body
 *
 * Same as lc_physics2d_circle_cast but allows ignoring a specific physics body.
 *
 * @param start Starting center of the circle in world space
 * @param end Ending center of the circle in world space
 * @param radius Radius of the circle
 * @param ignoreBodyId UUID of body to ignore (pass NULL to not ignore any body)
 * @param hit Output parameter for hit result
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_circle_cast_ignore(LCVec2 start, LCVec2 end, float radius,
                                                const uint64_t* ignoreBodyId, LCShapeCastHit2D* hit);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if the 2D physics world is available
 *
 * Returns true if there is an active scene with a Physics2D world
 * that can process queries.
 *
 * @param available Output parameter for availability status
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_query_available(bool* available);

/**
 * @brief Get the gravity of the 2D physics world
 *
 * @param gravity Output parameter for gravity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_get_gravity(LCVec2* gravity);

/**
 * @brief Set the gravity of the 2D physics world
 *
 * @param gravity New gravity vector (typically negative Y for downward gravity)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_set_gravity(LCVec2 gravity);

/**
 * @brief Get the fixed time step the 2D physics world simulates with
 * @param out_time_step Output parameter for the time step in seconds
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_get_time_step(float* out_time_step);

/**
 * @brief Set the fixed time step the 2D physics world simulates with
 * @param time_step Time step in seconds (e.g. 1/60)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_set_time_step(float time_step);

/**
 * @brief Get the solver velocity iteration count
 * @param out_iterations Output parameter for iteration count
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_get_velocity_iterations(int* out_iterations);

/**
 * @brief Set the solver velocity iteration count
 * @param iterations Iteration count (higher = more accurate, slower)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_set_velocity_iterations(int iterations);

/**
 * @brief Get the solver position iteration count
 * @param out_iterations Output parameter for iteration count
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_get_position_iterations(int* out_iterations);

/**
 * @brief Set the solver position iteration count
 * @param iterations Iteration count (higher = more accurate, slower)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_set_position_iterations(int iterations);

/* ============================================================================
 * Body <-> Node Resolution
 *
 * Query results (raycast/overlap/shape cast) report a body UUID. These helpers
 * convert between that UUID and the owning scene node so query results can be
 * acted on, and so a node's own body can be passed as an ignore filter.
 * ============================================================================ */

/**
 * @brief Resolve a physics body UUID (as returned by a query) to its owning node
 * @param bodyId Body UUID (128-bit as two 64-bit values, from a query result)
 * @param out_node Output parameter for the owning node handle
 * @return LC_SUCCESS on success, LC_ERROR_NOT_FOUND if the body owns no node
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_get_body_node(const uint64_t* bodyId, LCNodeHandle* out_node);

/**
 * @brief Find the physics body owned by a node (the inverse of get_body_node)
 * @param node Node handle to look up
 * @param out_body_id Output parameter for the body UUID (128-bit as two 64-bit values)
 * @return LC_SUCCESS on success, LC_ERROR_NOT_FOUND if the node owns no body
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics2d_find_body_for_node(LCNodeHandle node, uint64_t* out_body_id);

/* ============================================================================
 * Layer-Masked Query Variants
 *
 * These mirror the queries above but additionally accept a 64-bit collision
 * mask; a shape is considered only when (mask & shape.collisionLayer) != 0.
 * Pass 0xFFFFFFFFFFFFFFFF to consider every layer. Each also accepts an
 * optional body to ignore (pass NULL to ignore none).
 * ============================================================================ */

/**
 * @brief Cast a ray, filtering by collision mask and an optional ignored body
 */
LC_API LCResult lc_physics2d_raycast_masked(LCVec2 origin, LCVec2 direction, float maxDistance,
                                            const uint64_t* ignoreBodyId, uint64_t collisionMask,
                                            LCRaycastHit2D* hit);

/**
 * @brief Cast a ray and get all hits, filtering by collision mask and an optional ignored body
 */
LC_API LCResult lc_physics2d_raycast_all_masked(LCVec2 origin, LCVec2 direction, float maxDistance,
                                                const uint64_t* ignoreBodyId, uint64_t collisionMask,
                                                LCRaycastHit2D* hits, uint32_t maxHits, uint32_t* hitCount);

/**
 * @brief Query all bodies overlapping a point, filtered by collision mask
 */
LC_API LCResult lc_physics2d_overlap_point_masked(LCVec2 point, uint64_t collisionMask,
                                                  LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount);

/**
 * @brief Query all bodies overlapping a circle, filtered by collision mask
 */
LC_API LCResult lc_physics2d_overlap_circle_masked(LCVec2 center, float radius, uint64_t collisionMask,
                                                   LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount);

/**
 * @brief Query all bodies overlapping an (optionally rotated) box, filtered by collision mask
 * @param angle Rotation angle in radians (0 for an axis-aligned box)
 */
LC_API LCResult lc_physics2d_overlap_box_masked(LCVec2 center, LCVec2 size, float angle, uint64_t collisionMask,
                                                LCOverlapResult2D* results, uint32_t maxResults, uint32_t* resultCount);

/**
 * @brief Cast a circle shape, filtering by collision mask and an optional ignored body
 */
LC_API LCResult lc_physics2d_circle_cast_masked(LCVec2 start, LCVec2 end, float radius,
                                                const uint64_t* ignoreBodyId, uint64_t collisionMask,
                                                LCShapeCastHit2D* hit);

#endif /* LUPINE_CAPI_PHYSICS_QUERY2D_H */
