/**
 * @file lc_physics_query3d.h
 * @brief Lupine Engine C API - 3D Physics Query System
 *
 * This header provides 3D physics query functionality including:
 * - Raycasting (single hit and all hits)
 * - Sphere overlap queries
 * - Box overlap queries
 * - Shape casting (swept sphere)
 *
 * All query functions require an active scene with a Physics3D world.
 */

#ifndef LUPINE_CAPI_PHYSICS_QUERY3D_H
#define LUPINE_CAPI_PHYSICS_QUERY3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * Query Result Types
 * ============================================================================ */

/**
 * @brief 3D Raycast hit result
 */
typedef struct LCRaycastHit3D {
    uint64_t bodyId[2];     /**< Body UUID that was hit (128-bit as two 64-bit values) */
    LCVec3 point;           /**< Hit point in world space */
    LCVec3 normal;          /**< Surface normal at hit point */
    float fraction;         /**< Fraction along ray where hit occurred [0, 1] */
    bool hit;               /**< True if raycast hit something */
} LCRaycastHit3D;

/**
 * @brief 3D Overlap query result
 */
typedef struct LCOverlapResult3D {
    uint64_t bodyId[2];     /**< Body UUID that overlaps (128-bit as two 64-bit values) */
} LCOverlapResult3D;

/**
 * @brief 3D Shape cast hit result
 */
typedef struct LCShapeCastHit3D {
    uint64_t bodyId[2];     /**< Body UUID that was hit (128-bit as two 64-bit values) */
    LCVec3 point;           /**< Hit point in world space */
    LCVec3 normal;          /**< Surface normal at hit point */
    float fraction;         /**< Fraction along cast where hit occurred [0, 1] */
    bool hit;               /**< True if shape cast hit something */
} LCShapeCastHit3D;

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
 * LCRaycastHit3D hit;
 * LCVec3 origin = {0.0f, 0.0f, 0.0f};
 * LCVec3 direction = {1.0f, 0.0f, 0.0f};
 * if (lc_physics3d_raycast(origin, direction, 100.0f, &hit) == LC_SUCCESS && hit.hit) {
 *     printf("Hit at: %f, %f, %f\n", hit.point.x, hit.point.y, hit.point.z);
 * }
 * @endcode
 */
LC_API LCResult lc_physics3d_raycast(LCVec3 origin, LCVec3 direction, float maxDistance, LCRaycastHit3D* hit);

/**
 * @brief Cast a ray and get the first hit, ignoring a specific body
 *
 * Same as lc_physics3d_raycast but allows ignoring a specific physics body
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
LC_API LCResult lc_physics3d_raycast_ignore(LCVec3 origin, LCVec3 direction, float maxDistance,
                                            const uint64_t* ignoreBodyId, LCRaycastHit3D* hit);

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
 * LCRaycastHit3D hits[10];
 * uint32_t hitCount = 0;
 * LCVec3 origin = {0.0f, 0.0f, 0.0f};
 * LCVec3 direction = {1.0f, 0.0f, 0.0f};
 * if (lc_physics3d_raycast_all(origin, direction, 100.0f, hits, 10, &hitCount) == LC_SUCCESS) {
 *     for (uint32_t i = 0; i < hitCount; i++) {
 *         printf("Hit %d at: %f, %f, %f\n", i, hits[i].point.x, hits[i].point.y, hits[i].point.z);
 *     }
 * }
 * @endcode
 */
LC_API LCResult lc_physics3d_raycast_all(LCVec3 origin, LCVec3 direction, float maxDistance,
                                         LCRaycastHit3D* hits, uint32_t maxHits, uint32_t* hitCount);

/**
 * @brief Cast a ray and get all hits, ignoring a specific body
 *
 * Same as lc_physics3d_raycast_all but allows ignoring a specific physics body.
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
LC_API LCResult lc_physics3d_raycast_all_ignore(LCVec3 origin, LCVec3 direction, float maxDistance,
                                                const uint64_t* ignoreBodyId,
                                                LCRaycastHit3D* hits, uint32_t maxHits, uint32_t* hitCount);

/* ============================================================================
 * Overlap Query Functions
 * ============================================================================ */

/**
 * @brief Query all bodies overlapping a sphere
 *
 * Returns all physics bodies that overlap with the specified sphere.
 *
 * @param center Center of the sphere in world space
 * @param radius Radius of the sphere
 * @param results Output buffer for overlap results (must be pre-allocated)
 * @param maxResults Maximum number of results to return
 * @param resultCount Output parameter for actual number of results
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCOverlapResult3D results[10];
 * uint32_t count = 0;
 * LCVec3 center = {0.0f, 0.0f, 0.0f};
 * if (lc_physics3d_overlap_sphere(center, 5.0f, results, 10, &count) == LC_SUCCESS) {
 *     printf("Found %d bodies in sphere\n", count);
 * }
 * @endcode
 */
LC_API LCResult lc_physics3d_overlap_sphere(LCVec3 center, float radius,
                                            LCOverlapResult3D* results, uint32_t maxResults, uint32_t* resultCount);

/**
 * @brief Query all bodies overlapping an axis-aligned box
 *
 * Returns all physics bodies that overlap with the specified box.
 *
 * @param center Center of the box in world space
 * @param halfExtents Half extents of the box (half width, half height, half depth)
 * @param results Output buffer for overlap results (must be pre-allocated)
 * @param maxResults Maximum number of results to return
 * @param resultCount Output parameter for actual number of results
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics3d_overlap_box(LCVec3 center, LCVec3 halfExtents,
                                         LCOverlapResult3D* results, uint32_t maxResults, uint32_t* resultCount);

/**
 * @brief Query all bodies overlapping a rotated box
 *
 * Returns all physics bodies that overlap with the specified oriented box.
 *
 * @param center Center of the box in world space
 * @param halfExtents Half extents of the box (half width, half height, half depth)
 * @param rotation Rotation of the box as a quaternion
 * @param results Output buffer for overlap results (must be pre-allocated)
 * @param maxResults Maximum number of results to return
 * @param resultCount Output parameter for actual number of results
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCOverlapResult3D results[10];
 * uint32_t count = 0;
 * LCVec3 center = {0.0f, 0.0f, 0.0f};
 * LCVec3 halfExtents = {5.0f, 2.5f, 5.0f};
 * LCQuat rotation = lc_quat_identity();
 * if (lc_physics3d_overlap_box_rotated(center, halfExtents, rotation, results, 10, &count) == LC_SUCCESS) {
 *     printf("Found %d bodies in box\n", count);
 * }
 * @endcode
 */
LC_API LCResult lc_physics3d_overlap_box_rotated(LCVec3 center, LCVec3 halfExtents, LCQuat rotation,
                                                 LCOverlapResult3D* results, uint32_t maxResults, uint32_t* resultCount);

/* ============================================================================
 * Shape Cast Functions
 * ============================================================================ */

/**
 * @brief Cast a sphere shape and get the first hit
 *
 * Sweeps a sphere from start to end and returns the first hit (if any).
 * Useful for character movement collision detection.
 *
 * @param start Starting center of the sphere in world space
 * @param end Ending center of the sphere in world space
 * @param radius Radius of the sphere
 * @param hit Output parameter for hit result
 * @return LC_SUCCESS on success (check hit.hit for actual hit), error code otherwise
 * @threadsafety Thread-safe
 *
 * @example
 * @code
 * LCShapeCastHit3D hit;
 * LCVec3 start = {0.0f, 1.0f, 0.0f};
 * LCVec3 end = {10.0f, 1.0f, 0.0f};
 * if (lc_physics3d_sphere_cast(start, end, 0.5f, &hit) == LC_SUCCESS && hit.hit) {
 *     printf("Sphere cast hit at: %f, %f, %f\n", hit.point.x, hit.point.y, hit.point.z);
 * }
 * @endcode
 */
LC_API LCResult lc_physics3d_sphere_cast(LCVec3 start, LCVec3 end, float radius, LCShapeCastHit3D* hit);

/**
 * @brief Cast a sphere shape and get the first hit, ignoring a specific body
 *
 * Same as lc_physics3d_sphere_cast but allows ignoring a specific physics body.
 *
 * @param start Starting center of the sphere in world space
 * @param end Ending center of the sphere in world space
 * @param radius Radius of the sphere
 * @param ignoreBodyId UUID of body to ignore (pass NULL to not ignore any body)
 * @param hit Output parameter for hit result
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics3d_sphere_cast_ignore(LCVec3 start, LCVec3 end, float radius,
                                                const uint64_t* ignoreBodyId, LCShapeCastHit3D* hit);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if the 3D physics world is available
 *
 * Returns true if there is an active scene with a Physics3D world
 * that can process queries.
 *
 * @param available Output parameter for availability status
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics3d_query_available(bool* available);

/**
 * @brief Get the gravity of the 3D physics world
 *
 * @param gravity Output parameter for gravity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics3d_get_gravity(LCVec3* gravity);

/**
 * @brief Set the gravity of the 3D physics world
 *
 * @param gravity New gravity vector (typically negative Y for downward gravity)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics3d_set_gravity(LCVec3 gravity);

/**
 * @brief Get the fixed time step the 3D physics world simulates with
 * @param out_time_step Output parameter for the time step in seconds
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics3d_get_time_step(float* out_time_step);

/**
 * @brief Set the fixed time step the 3D physics world simulates with
 * @param time_step Time step in seconds (e.g. 1/60)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics3d_set_time_step(float time_step);

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
LC_API LCResult lc_physics3d_get_body_node(const uint64_t* bodyId, LCNodeHandle* out_node);

/**
 * @brief Find the physics body owned by a node (the inverse of get_body_node)
 * @param node Node handle to look up
 * @param out_body_id Output parameter for the body UUID (128-bit as two 64-bit values)
 * @return LC_SUCCESS on success, LC_ERROR_NOT_FOUND if the node owns no body
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_physics3d_find_body_for_node(LCNodeHandle node, uint64_t* out_body_id);

#endif /* LUPINE_CAPI_PHYSICS_QUERY3D_H */
