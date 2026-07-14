/**
 * @file lc_collision3d.h
 * @brief Lupine Engine C API - 3D Collision Bodies
 *
 * This header provides 3D collision shape components for physics bodies.
 */

#ifndef LUPINE_CAPI_COLLISION3D_H
#define LUPINE_CAPI_COLLISION3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/**
 * @brief 3D collision shape types
 */
typedef enum LCCollisionShape3D {
    LC_COLLISION3D_BOX = 0,
    LC_COLLISION3D_SPHERE,
    LC_COLLISION3D_CAPSULE,
    LC_COLLISION3D_CYLINDER,
    LC_COLLISION3D_CONE,
    LC_COLLISION3D_PLANE,
    LC_COLLISION3D_MESH
} LCCollisionShape3D;

/**
 * @brief Mesh solver types for mesh-based collision
 */
typedef enum LCMeshSolverType {
    LC_MESH_SOLVER_BOUNDING_BOX = 0,  /**< Create a bounding box around the mesh */
    LC_MESH_SOLVER_CONVEX,             /**< Create a convex hull from mesh vertices */
    LC_MESH_SOLVER_CONCAVE,            /**< Create a concave triangle mesh (static only) */
    LC_MESH_SOLVER_TRIMESH             /**< Create a triangle mesh (static only) */
} LCMeshSolverType;

/* ============================================================================
 * CollisionMesh3D Creation/Destruction
 * ============================================================================ */

/**
 * @brief Create a CollisionMesh3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Shape Configuration
 * ============================================================================ */

/**
 * @brief Get the collision shape type
 * @param component Component handle
 * @param out_shape Output parameter for shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_shape_type(LCComponentHandle component, LCCollisionShape3D* out_shape);

/**
 * @brief Set the collision shape type
 * @param component Component handle
 * @param shape Shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_shape_type(LCComponentHandle component, LCCollisionShape3D shape);

/**
 * @brief Get the size (for Box)
 * @param component Component handle
 * @param out_size Output parameter for size
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_size(LCComponentHandle component, LCVec3* out_size);

/**
 * @brief Set the size (for Box)
 * @param component Component handle
 * @param size Size value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_size(LCComponentHandle component, LCVec3 size);

/**
 * @brief Get the radius (for Sphere, Capsule, Cylinder, Cone)
 * @param component Component handle
 * @param out_radius Output parameter for radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_radius(LCComponentHandle component, float* out_radius);

/**
 * @brief Set the radius (for Sphere, Capsule, Cylinder, Cone)
 * @param component Component handle
 * @param radius Radius value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_radius(LCComponentHandle component, float radius);

/**
 * @brief Get the height (for Capsule, Cylinder, Cone)
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set the height (for Capsule, Cylinder, Cone)
 * @param component Component handle
 * @param height Height value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_height(LCComponentHandle component, float height);

/**
 * @brief Get the plane normal (for Plane)
 * @param component Component handle
 * @param out_normal Output parameter for plane normal
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_plane_normal(LCComponentHandle component, LCVec3* out_normal);

/**
 * @brief Set the plane normal (for Plane)
 * @param component Component handle
 * @param normal Plane normal (will be normalized)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_plane_normal(LCComponentHandle component, LCVec3 normal);

/**
 * @brief Get the plane distance (for Plane)
 * @param component Component handle
 * @param out_distance Output parameter for plane distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_plane_distance(LCComponentHandle component, float* out_distance);

/**
 * @brief Set the plane distance (for Plane)
 * @param component Component handle
 * @param distance Distance from origin along normal
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_plane_distance(LCComponentHandle component, float distance);

/**
 * @brief Get the plane width (for finite Plane)
 * @param component Component handle
 * @param out_width Output parameter for plane width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_plane_width(LCComponentHandle component, float* out_width);

/**
 * @brief Set the plane width (for finite Plane)
 * @param component Component handle
 * @param width Plane width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_plane_width(LCComponentHandle component, float width);

/**
 * @brief Get the plane length (for finite Plane)
 * @param component Component handle
 * @param out_length Output parameter for plane length
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_plane_length(LCComponentHandle component, float* out_length);

/**
 * @brief Set the plane length (for finite Plane)
 * @param component Component handle
 * @param length Plane length
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_plane_length(LCComponentHandle component, float length);

/**
 * @brief Get the offset from node position
 * @param component Component handle
 * @param out_offset Output parameter for offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_offset(LCComponentHandle component, LCVec3* out_offset);

/**
 * @brief Set the offset from node position
 * @param component Component handle
 * @param offset Offset value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_offset(LCComponentHandle component, LCVec3 offset);

/* ============================================================================
 * Mesh Shape Configuration
 * ============================================================================ */

/**
 * @brief Get the mesh path (for Mesh shape)
 * @param component Component handle
 * @param out_path Buffer to receive path
 * @param buffer_size Size of the buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_mesh_path(LCComponentHandle component, char* out_path, int buffer_size);

/**
 * @brief Set the mesh path (for Mesh shape)
 * @param component Component handle
 * @param path Path to the mesh file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_mesh_path(LCComponentHandle component, const char* path);

/**
 * @brief Get the mesh solver type
 * @param component Component handle
 * @param out_solver Output parameter for solver type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_mesh_solver(LCComponentHandle component, LCMeshSolverType* out_solver);

/**
 * @brief Set the mesh solver type
 * @param component Component handle
 * @param solver Mesh solver type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_mesh_solver(LCComponentHandle component, LCMeshSolverType solver);

/* ============================================================================
 * Physics Material Properties
 * ============================================================================ */

/**
 * @brief Get the density
 * @param component Component handle
 * @param out_density Output parameter for density
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_density(LCComponentHandle component, float* out_density);

/**
 * @brief Set the density
 * @param component Component handle
 * @param density Density value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_density(LCComponentHandle component, float density);

/**
 * @brief Get the friction coefficient
 * @param component Component handle
 * @param out_friction Output parameter for friction
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_friction(LCComponentHandle component, float* out_friction);

/**
 * @brief Set the friction coefficient
 * @param component Component handle
 * @param friction Friction value (0.0 to 1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_friction(LCComponentHandle component, float friction);

/**
 * @brief Get the restitution (bounciness)
 * @param component Component handle
 * @param out_restitution Output parameter for restitution
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_restitution(LCComponentHandle component, float* out_restitution);

/**
 * @brief Set the restitution (bounciness)
 * @param component Component handle
 * @param restitution Restitution value (0.0 to 1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_restitution(LCComponentHandle component, float restitution);

/**
 * @brief Check if collision body is a sensor (trigger)
 * @param component Component handle
 * @param out_is_sensor Output parameter for sensor state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_is_sensor(LCComponentHandle component, bool* out_is_sensor);

/**
 * @brief Set whether collision body is a sensor (trigger)
 * @param component Component handle
 * @param is_sensor Sensor state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_sensor(LCComponentHandle component, bool is_sensor);

/**
 * @brief Get the collision layers bitmask
 * @param component Component handle
 * @param out_layers Output parameter for collision layers
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_collision_layers(LCComponentHandle component, uint32_t* out_layers);

/**
 * @brief Set the collision layers bitmask
 * @param component Component handle
 * @param layers Collision layers bitmask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_collision_layers(LCComponentHandle component, uint32_t layers);

/* ============================================================================
 * Debug Visualization
 * ============================================================================ */

/**
 * @brief Get the debug visualization color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_get_debug_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the debug visualization color
 * @param component Component handle
 * @param color Debug color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_collision3d_set_debug_color(LCComponentHandle component, LCColor color);

#endif /* LUPINE_CAPI_COLLISION3D_H */
