/**
 * @file lc_collision2d.h
 * @brief Lupine Engine C API - 2D Collision Bodies
 *
 * This header provides 2D collision shape components for physics bodies.
 */

#ifndef LUPINE_CAPI_COLLISION2D_H
#define LUPINE_CAPI_COLLISION2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/**
 * @brief 2D collision shape types
 */
typedef enum LCCollisionShape2D {
    LC_COLLISION2D_RECTANGLE = 0,
    LC_COLLISION2D_CIRCLE,
    LC_COLLISION2D_TRIANGLE,
    LC_COLLISION2D_PENTAGON,
    LC_COLLISION2D_HEXAGON,
    LC_COLLISION2D_POLYGON
} LCCollisionShape2D;

/* ============================================================================
 * CollisionBody2D Creation/Destruction
 * ============================================================================ */

/**
 * @brief Create a CollisionBody2D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Shape Configuration
 * ============================================================================ */

/**
 * @brief Get the collision shape type
 * @param component Component handle
 * @param out_shape Output parameter for shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_shape_type(LCComponentHandle component, LCCollisionShape2D* out_shape);

/**
 * @brief Set the collision shape type
 * @param component Component handle
 * @param shape Shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_shape_type(LCComponentHandle component, LCCollisionShape2D shape);

/**
 * @brief Get the shape size (for Rectangle)
 * @param component Component handle
 * @param out_size Output parameter for size
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_size(LCComponentHandle component, LCVec2* out_size);

/**
 * @brief Set the shape size (for Rectangle)
 * @param component Component handle
 * @param size Size value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_size(LCComponentHandle component, LCVec2 size);

/**
 * @brief Get the radius (for Circle)
 * @param component Component handle
 * @param out_radius Output parameter for radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_radius(LCComponentHandle component, float* out_radius);

/**
 * @brief Set the radius (for Circle)
 * @param component Component handle
 * @param radius Radius value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_radius(LCComponentHandle component, float radius);

/**
 * @brief Get the offset from node position
 * @param component Component handle
 * @param out_offset Output parameter for offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_offset(LCComponentHandle component, LCVec2* out_offset);

/**
 * @brief Set the offset from node position
 * @param component Component handle
 * @param offset Offset value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_offset(LCComponentHandle component, LCVec2 offset);

/* ============================================================================
 * Polygon Vertices (for custom polygon shapes)
 * ============================================================================ */

/**
 * @brief Get the number of vertices in the polygon
 * @param component Component handle
 * @param out_count Output parameter for vertex count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_vertex_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get a vertex at the specified index
 * @param component Component handle
 * @param index Vertex index
 * @param out_vertex Output parameter for vertex position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_vertex(LCComponentHandle component, int index, LCVec2* out_vertex);

/**
 * @brief Set vertices for a polygon shape
 * @param component Component handle
 * @param vertices Array of vertices
 * @param count Number of vertices
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_vertices(LCComponentHandle component, const LCVec2* vertices, int count);

/**
 * @brief Add a vertex to the polygon
 * @param component Component handle
 * @param vertex Vertex to add
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_add_vertex(LCComponentHandle component, LCVec2 vertex);

/**
 * @brief Update a vertex at the specified index
 * @param component Component handle
 * @param index Vertex index
 * @param vertex New vertex position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_update_vertex(LCComponentHandle component, int index, LCVec2 vertex);

/**
 * @brief Remove a vertex at the specified index
 * @param component Component handle
 * @param index Vertex index
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_remove_vertex(LCComponentHandle component, int index);

/**
 * @brief Clear all vertices from the polygon
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_clear_vertices(LCComponentHandle component);

/* ============================================================================
 * Physics Material Properties
 * ============================================================================ */

/**
 * @brief Get the density
 * @param component Component handle
 * @param out_density Output parameter for density
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_density(LCComponentHandle component, float* out_density);

/**
 * @brief Set the density
 * @param component Component handle
 * @param density Density value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_density(LCComponentHandle component, float density);

/**
 * @brief Get the friction coefficient
 * @param component Component handle
 * @param out_friction Output parameter for friction
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_friction(LCComponentHandle component, float* out_friction);

/**
 * @brief Set the friction coefficient
 * @param component Component handle
 * @param friction Friction value (0.0 to 1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_friction(LCComponentHandle component, float friction);

/**
 * @brief Get the restitution (bounciness)
 * @param component Component handle
 * @param out_restitution Output parameter for restitution
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_restitution(LCComponentHandle component, float* out_restitution);

/**
 * @brief Set the restitution (bounciness)
 * @param component Component handle
 * @param restitution Restitution value (0.0 to 1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_restitution(LCComponentHandle component, float restitution);

/**
 * @brief Check if collision body is a sensor (trigger)
 * @param component Component handle
 * @param out_is_sensor Output parameter for sensor state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_is_sensor(LCComponentHandle component, bool* out_is_sensor);

/**
 * @brief Set whether collision body is a sensor (trigger)
 * @param component Component handle
 * @param is_sensor Sensor state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_sensor(LCComponentHandle component, bool is_sensor);

/**
 * @brief Get the collision layers bitmask
 * @param component Component handle
 * @param out_layers Output parameter for collision layers
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_collision_layers(LCComponentHandle component, uint32_t* out_layers);

/**
 * @brief Set the collision layers bitmask
 * @param component Component handle
 * @param layers Collision layers bitmask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_collision_layers(LCComponentHandle component, uint32_t layers);

/**
 * @brief Get the collision mask bitmask (which layers this body collides with)
 * @param component Component handle
 * @param out_mask Output parameter for collision mask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_collision_mask(LCComponentHandle component, uint32_t* out_mask);

/**
 * @brief Set the collision mask bitmask (which layers this body collides with)
 * @param component Component handle
 * @param mask Collision mask bitmask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_collision_mask(LCComponentHandle component, uint32_t mask);

/* ============================================================================
 * Debug Visualization
 * ============================================================================ */

/**
 * @brief Get the debug visualization color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_get_debug_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the debug visualization color
 * @param component Component handle
 * @param color Debug color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision2d_set_debug_color(LCComponentHandle component, LCColor color);

#endif /* LUPINE_CAPI_COLLISION2D_H */
