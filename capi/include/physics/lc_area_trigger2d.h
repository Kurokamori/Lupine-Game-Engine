/**
 * @file lc_area_trigger2d.h
 * @brief Lupine Engine C API - 2D Area Trigger
 *
 * This header provides 2D area trigger components for detecting
 * overlapping physics bodies without physical collision response.
 */

#ifndef LUPINE_CAPI_AREA_TRIGGER2D_H
#define LUPINE_CAPI_AREA_TRIGGER2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"
#include "physics/lc_collision2d.h"

/* ============================================================================
 * AreaTrigger2D Creation/Destruction
 * ============================================================================ */

/**
 * @brief Create an AreaTrigger2D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Sensor Shape Configuration
 *
 * An AreaTrigger2D owns its own sensor collider, so its shape is configured
 * directly on the component (unlike a solid body that pairs with a separate
 * CollisionBody2D). The shape type values mirror LCCollisionShape2D.
 * ============================================================================ */

/**
 * @brief Get the sensor collision shape type
 * @param component Component handle
 * @param out_shape Output parameter for shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_shape_type(LCComponentHandle component, LCCollisionShape2D* out_shape);

/**
 * @brief Set the sensor collision shape type
 * @param component Component handle
 * @param shape Shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_shape_type(LCComponentHandle component, LCCollisionShape2D shape);

/**
 * @brief Get the sensor shape size (for Rectangle)
 * @param component Component handle
 * @param out_size Output parameter for size
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_size(LCComponentHandle component, LCVec2* out_size);

/**
 * @brief Set the sensor shape size (for Rectangle)
 * @param component Component handle
 * @param size Size value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_size(LCComponentHandle component, LCVec2 size);

/**
 * @brief Get the sensor radius (for Circle)
 * @param component Component handle
 * @param out_radius Output parameter for radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_radius(LCComponentHandle component, float* out_radius);

/**
 * @brief Set the sensor radius (for Circle)
 * @param component Component handle
 * @param radius Radius value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_radius(LCComponentHandle component, float radius);

/**
 * @brief Get the sensor offset from the node position
 * @param component Component handle
 * @param out_offset Output parameter for offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_offset(LCComponentHandle component, LCVec2* out_offset);

/**
 * @brief Set the sensor offset from the node position
 * @param component Component handle
 * @param offset Offset value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_offset(LCComponentHandle component, LCVec2 offset);

/**
 * @brief Get the collision layers bitmask (which layer(s) this area occupies)
 * @param component Component handle
 * @param out_layers Output parameter for collision layers
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_collision_layers(LCComponentHandle component, uint32_t* out_layers);

/**
 * @brief Set the collision layers bitmask (which layer(s) this area occupies)
 * @param component Component handle
 * @param layers Collision layers bitmask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_collision_layers(LCComponentHandle component, uint32_t layers);

/**
 * @brief Get the collision mask bitmask (which layer(s) this area detects)
 * @param component Component handle
 * @param out_mask Output parameter for collision mask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_collision_mask(LCComponentHandle component, uint32_t* out_mask);

/**
 * @brief Set the collision mask bitmask (which layer(s) this area detects)
 * @param component Component handle
 * @param mask Collision mask bitmask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_collision_mask(LCComponentHandle component, uint32_t mask);

/**
 * @brief Get the number of vertices in the sensor polygon
 * @param component Component handle
 * @param out_count Output parameter for vertex count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_vertex_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get a sensor polygon vertex at the specified index
 * @param component Component handle
 * @param index Vertex index
 * @param out_vertex Output parameter for vertex position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_vertex(LCComponentHandle component, int index, LCVec2* out_vertex);

/**
 * @brief Set the sensor polygon vertices
 * @param component Component handle
 * @param vertices Array of vertices
 * @param count Number of vertices
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_vertices(LCComponentHandle component, const LCVec2* vertices, int count);

/* ============================================================================
 * Monitoring Configuration
 * ============================================================================ */

/**
 * @brief Check if monitoring is enabled
 * @param component Component handle
 * @param out_monitoring Output parameter for monitoring state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_monitoring(LCComponentHandle component, bool* out_monitoring);

/**
 * @brief Set whether monitoring is enabled
 * @param component Component handle
 * @param monitoring Monitoring state (true = actively detecting overlaps)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_monitoring(LCComponentHandle component, bool monitoring);

/**
 * @brief Check if monitorable is enabled
 * @param component Component handle
 * @param out_monitorable Output parameter for monitorable state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_monitorable(LCComponentHandle component, bool* out_monitorable);

/**
 * @brief Set whether monitorable is enabled
 * @param component Component handle
 * @param monitorable Monitorable state (true = other areas can detect this area)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_monitorable(LCComponentHandle component, bool monitorable);

/**
 * @brief Get the priority
 * @param component Component handle
 * @param out_priority Output parameter for priority
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_priority(LCComponentHandle component, int* out_priority);

/**
 * @brief Set the priority
 * @param component Component handle
 * @param priority Priority value (higher = checked first)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_set_priority(LCComponentHandle component, int priority);

/* ============================================================================
 * Overlap Detection
 * ============================================================================ */

/**
 * @brief Get the count of currently overlapping bodies
 * @param component Component handle
 * @param out_count Output parameter for overlapping body count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_overlapping_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get the UUID of an overlapping body by index
 * @param component Component handle
 * @param index Index of the overlapping body
 * @param out_uuid Buffer to receive UUID string (must be at least 37 bytes)
 * @param buffer_size Size of the buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_get_overlapping_body(LCComponentHandle component, int index, char* out_uuid, int buffer_size);

/**
 * @brief Check if a specific body is overlapping
 * @param component Component handle
 * @param uuid UUID string of the body to check
 * @param out_is_overlapping Output parameter for overlap state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger2d_is_overlapping(LCComponentHandle component, const char* uuid, bool* out_is_overlapping);

#endif /* LUPINE_CAPI_AREA_TRIGGER2D_H */
