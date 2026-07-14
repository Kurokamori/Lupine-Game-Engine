/**
 * @file lc_character_controller3d.h
 * @brief Lupine Engine C API - CharacterController3D component
 *
 * A robust character controller for 3D games.
 * Uses swept collision detection (shape casting) to prevent tunneling
 * and provides proper collision response for kinematic bodies.
 */

#ifndef LUPINE_CAPI_CHARACTER_CONTROLLER3D_H
#define LUPINE_CAPI_CHARACTER_CONTROLLER3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CharacterController3D Creation
 * ============================================================================ */

/**
 * @brief Create a CharacterController3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Movement API
 * ============================================================================ */

/**
 * @brief Move the character with collision detection and sliding
 * @param component Component handle
 * @param velocity Desired velocity
 * @param delta_time Time step for physics
 * @param out_actual_movement Output parameter for actual movement after collision response
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_move_and_slide(LCComponentHandle component, LCVec3 velocity, float delta_time, LCVec3* out_actual_movement);

/**
 * @brief Move the character and stop on collision
 * @param component Component handle
 * @param velocity Desired velocity
 * @param delta_time Time step for physics
 * @param out_actual_movement Output parameter for actual movement
 * @param out_collided Output parameter - true if collision occurred
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_move_and_collide(LCComponentHandle component, LCVec3 velocity, float delta_time, LCVec3* out_actual_movement, bool* out_collided);

/**
 * @brief Get the current velocity
 * @param component Component handle
 * @param out_velocity Output parameter for velocity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_velocity(LCComponentHandle component, LCVec3* out_velocity);

/**
 * @brief Set the velocity (will be processed in next physics update)
 * @param component Component handle
 * @param velocity Velocity to set
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_velocity(LCComponentHandle component, LCVec3 velocity);

/* ============================================================================
 * Ground Detection
 * ============================================================================ */

/**
 * @brief Check if the character is on the ground
 * @param component Component handle
 * @param out_on_ground Output parameter for ground state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_is_on_ground(LCComponentHandle component, bool* out_on_ground);

/**
 * @brief Check if the character is touching a wall
 * @param component Component handle
 * @param out_on_wall Output parameter for wall state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_is_on_wall(LCComponentHandle component, bool* out_on_wall);

/**
 * @brief Check if the character is touching a ceiling
 * @param component Component handle
 * @param out_on_ceiling Output parameter for ceiling state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_is_on_ceiling(LCComponentHandle component, bool* out_on_ceiling);

/**
 * @brief Get the ground normal vector
 * @param component Component handle
 * @param out_normal Output parameter for ground normal
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_ground_normal(LCComponentHandle component, LCVec3* out_normal);

/**
 * @brief Get the wall normal vector
 * @param component Component handle
 * @param out_normal Output parameter for wall normal
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_wall_normal(LCComponentHandle component, LCVec3* out_normal);

/* ============================================================================
 * Physics Properties
 * ============================================================================ */

/**
 * @brief Get gravity strength
 * @param component Component handle
 * @param out_gravity Output parameter for gravity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_gravity(LCComponentHandle component, float* out_gravity);

/**
 * @brief Set gravity strength
 * @param component Component handle
 * @param gravity Gravity value (positive = downward)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_gravity(LCComponentHandle component, float gravity);

/**
 * @brief Get maximum fall speed
 * @param component Component handle
 * @param out_speed Output parameter for max fall speed
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_max_fall_speed(LCComponentHandle component, float* out_speed);

/**
 * @brief Set maximum fall speed (terminal velocity)
 * @param component Component handle
 * @param speed Maximum fall speed
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_max_fall_speed(LCComponentHandle component, float speed);

/* ============================================================================
 * Detection Properties
 * ============================================================================ */

/**
 * @brief Get ground detection distance
 * @param component Component handle
 * @param out_distance Output parameter for distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_ground_detection_distance(LCComponentHandle component, float* out_distance);

/**
 * @brief Set ground detection distance
 * @param component Component handle
 * @param distance Distance for ground detection raycast
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_ground_detection_distance(LCComponentHandle component, float distance);

/**
 * @brief Get wall detection distance
 * @param component Component handle
 * @param out_distance Output parameter for distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_wall_detection_distance(LCComponentHandle component, float* out_distance);

/**
 * @brief Set wall detection distance
 * @param component Component handle
 * @param distance Distance for wall detection raycast
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_wall_detection_distance(LCComponentHandle component, float distance);

/* ============================================================================
 * Slope and Step Properties
 * ============================================================================ */

/**
 * @brief Get maximum slope angle (in degrees)
 * @param component Component handle
 * @param out_angle Output parameter for angle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_max_slope_angle(LCComponentHandle component, float* out_angle);

/**
 * @brief Set maximum slope angle the character can walk on
 * @param component Component handle
 * @param angle Maximum slope angle in degrees
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_max_slope_angle(LCComponentHandle component, float angle);

/**
 * @brief Get step height (maximum height character can step up)
 * @param component Component handle
 * @param out_height Output parameter for step height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_step_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set step height (for stair climbing)
 * @param component Component handle
 * @param height Maximum step height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_step_height(LCComponentHandle component, float height);

/* ============================================================================
 * Movement Options
 * ============================================================================ */

/**
 * @brief Get snap to ground state
 * @param component Component handle
 * @param out_snap Output parameter for snap state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_snap_to_ground(LCComponentHandle component, bool* out_snap);

/**
 * @brief Set snap to ground (keeps character grounded on slopes)
 * @param component Component handle
 * @param snap Whether to snap to ground
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_snap_to_ground(LCComponentHandle component, bool snap);

/**
 * @brief Get maximum collision bounces
 * @param component Component handle
 * @param out_bounces Output parameter for max bounces
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_get_max_bounces(LCComponentHandle component, int* out_bounces);

/**
 * @brief Set maximum collision bounces (iterations for slide resolution)
 * @param component Component handle
 * @param bounces Maximum number of bounces
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_character_controller3d_set_max_bounces(LCComponentHandle component, int bounces);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_CHARACTER_CONTROLLER3D_H */
