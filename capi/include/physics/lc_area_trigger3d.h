/**
 * @file lc_area_trigger3d.h
 * @brief Lupine Engine C API - 3D Area Trigger
 *
 * This header provides 3D area trigger components for detecting
 * overlapping physics bodies without physical collision response.
 */

#ifndef LUPINE_CAPI_AREA_TRIGGER3D_H
#define LUPINE_CAPI_AREA_TRIGGER3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * AreaTrigger3D Creation/Destruction
 * ============================================================================ */

/**
 * @brief Create an AreaTrigger3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Monitoring Configuration
 * ============================================================================ */

/**
 * @brief Check if monitoring is enabled
 * @param component Component handle
 * @param out_monitoring Output parameter for monitoring state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_get_monitoring(LCComponentHandle component, bool* out_monitoring);

/**
 * @brief Set whether monitoring is enabled
 * @param component Component handle
 * @param monitoring Monitoring state (true = actively detecting overlaps)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_set_monitoring(LCComponentHandle component, bool monitoring);

/**
 * @brief Check if monitorable is enabled
 * @param component Component handle
 * @param out_monitorable Output parameter for monitorable state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_get_monitorable(LCComponentHandle component, bool* out_monitorable);

/**
 * @brief Set whether monitorable is enabled
 * @param component Component handle
 * @param monitorable Monitorable state (true = other areas can detect this area)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_set_monitorable(LCComponentHandle component, bool monitorable);

/**
 * @brief Get the priority
 * @param component Component handle
 * @param out_priority Output parameter for priority
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_get_priority(LCComponentHandle component, int* out_priority);

/**
 * @brief Set the priority
 * @param component Component handle
 * @param priority Priority value (higher = checked first)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_set_priority(LCComponentHandle component, int priority);

/* ============================================================================
 * Overlap Detection
 * ============================================================================ */

/**
 * @brief Get the count of currently overlapping bodies
 * @param component Component handle
 * @param out_count Output parameter for overlapping body count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_get_overlapping_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get the UUID of an overlapping body by index
 * @param component Component handle
 * @param index Index of the overlapping body
 * @param out_uuid Buffer to receive UUID string (must be at least 37 bytes)
 * @param buffer_size Size of the buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @note Since AreaTrigger3D uses unordered_set, ordering may vary
 */
LC_API LCResult lc_area_trigger3d_get_overlapping_body(LCComponentHandle component, int index, char* out_uuid, int buffer_size);

/**
 * @brief Check if a specific body is overlapping
 * @param component Component handle
 * @param uuid UUID string of the body to check
 * @param out_is_overlapping Output parameter for overlap state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_area_trigger3d_is_overlapping(LCComponentHandle component, const char* uuid, bool* out_is_overlapping);

#endif /* LUPINE_CAPI_AREA_TRIGGER3D_H */
