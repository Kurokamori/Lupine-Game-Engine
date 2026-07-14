/**
 * @file lc_character_controller2d.h
 * @brief Lupine Engine C API - 2D Character Controller
 *
 * This header provides a robust character controller for 2D platformers
 * and top-down games. Uses swept collision detection to prevent tunneling
 * and provides proper collision response.
 *
 * Features:
 * - Swept collision detection (no tunneling)
 * - Ground detection with configurable tolerance
 * - Wall sliding
 * - Slope handling
 * - Snap to ground
 * - Configurable gravity
 */

#ifndef LUPINE_CAPI_CHARACTER_CONTROLLER2D_H
#define LUPINE_CAPI_CHARACTER_CONTROLLER2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CharacterController2D Creation
 * ============================================================================ */

/**
 * @brief Create a CharacterController2D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 * @note Requires KinematicBody2D and CollisionBody2D on the same node
 */
LC_API LCResult lc_character_controller2d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Movement Functions
 * ============================================================================ */

/**
 * @brief Move the character with collision detection and wall sliding
 * @param component Component handle
 * @param velocity Desired velocity vector
 * @param delta_time Time step in seconds
 * @param out_actual_movement Output parameter for actual movement after collision response
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_move_and_slide(
    LCComponentHandle component,
    LCVec2 velocity,
    float delta_time,
    LCVec2* out_actual_movement
);

/**
 * @brief Move the character and stop on collision
 * @param component Component handle
 * @param velocity Desired velocity vector
 * @param delta_time Time step in seconds
 * @param out_actual_movement Output parameter for actual movement
 * @param out_collided Output parameter indicating if collision occurred
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_move_and_collide(
    LCComponentHandle component,
    LCVec2 velocity,
    float delta_time,
    LCVec2* out_actual_movement,
    bool* out_collided
);

/**
 * @brief Set the velocity of the character controller
 * @param component Component handle
 * @param velocity Velocity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_set_velocity(LCComponentHandle component, LCVec2 velocity);

/**
 * @brief Get the current velocity of the character controller
 * @param component Component handle
 * @param out_velocity Output parameter for velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_velocity(LCComponentHandle component, LCVec2* out_velocity);

/* ============================================================================
 * Ground/Wall/Ceiling Detection
 * ============================================================================ */

/**
 * @brief Check if the character is on the ground
 * @param component Component handle
 * @param out_on_ground Output parameter for ground state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_is_on_ground(LCComponentHandle component, bool* out_on_ground);

/**
 * @brief Check if the character is touching a wall
 * @param component Component handle
 * @param out_on_wall Output parameter for wall state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_is_on_wall(LCComponentHandle component, bool* out_on_wall);

/**
 * @brief Check if the character is touching the ceiling
 * @param component Component handle
 * @param out_on_ceiling Output parameter for ceiling state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_is_on_ceiling(LCComponentHandle component, bool* out_on_ceiling);

/**
 * @brief Get the ground normal vector
 * @param component Component handle
 * @param out_normal Output parameter for ground normal
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_ground_normal(LCComponentHandle component, LCVec2* out_normal);

/**
 * @brief Get the wall normal vector
 * @param component Component handle
 * @param out_normal Output parameter for wall normal
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_wall_normal(LCComponentHandle component, LCVec2* out_normal);

/* ============================================================================
 * Configuration Properties
 * ============================================================================ */

/**
 * @brief Get the gravity value
 * @param component Component handle
 * @param out_gravity Output parameter for gravity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_gravity(LCComponentHandle component, float* out_gravity);

/**
 * @brief Set the gravity value
 * @param component Component handle
 * @param gravity Gravity value (positive = downward)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_set_gravity(LCComponentHandle component, float gravity);

/**
 * @brief Get the maximum fall speed
 * @param component Component handle
 * @param out_speed Output parameter for max fall speed
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_max_fall_speed(LCComponentHandle component, float* out_speed);

/**
 * @brief Set the maximum fall speed
 * @param component Component handle
 * @param speed Maximum fall speed (terminal velocity)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_set_max_fall_speed(LCComponentHandle component, float speed);

/**
 * @brief Get the ground detection distance
 * @param component Component handle
 * @param out_distance Output parameter for detection distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_ground_detection_distance(LCComponentHandle component, float* out_distance);

/**
 * @brief Set the ground detection distance
 * @param component Component handle
 * @param distance Distance to check for ground below character
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_set_ground_detection_distance(LCComponentHandle component, float distance);

/**
 * @brief Get the wall detection distance
 * @param component Component handle
 * @param out_distance Output parameter for detection distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_wall_detection_distance(LCComponentHandle component, float* out_distance);

/**
 * @brief Set the wall detection distance
 * @param component Component handle
 * @param distance Distance to check for walls
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_set_wall_detection_distance(LCComponentHandle component, float distance);

/**
 * @brief Get the maximum slope angle the character can walk on
 * @param component Component handle
 * @param out_angle Output parameter for angle in degrees
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_max_slope_angle(LCComponentHandle component, float* out_angle);

/**
 * @brief Set the maximum slope angle the character can walk on
 * @param component Component handle
 * @param angle Maximum walkable slope angle in degrees
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_set_max_slope_angle(LCComponentHandle component, float angle);

/**
 * @brief Get whether snap to ground is enabled
 * @param component Component handle
 * @param out_snap Output parameter for snap state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_snap_to_ground(LCComponentHandle component, bool* out_snap);

/**
 * @brief Set whether to snap to ground when walking down slopes
 * @param component Component handle
 * @param snap Enable/disable snap to ground
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_set_snap_to_ground(LCComponentHandle component, bool snap);

/**
 * @brief Get the maximum number of collision bounces
 * @param component Component handle
 * @param out_bounces Output parameter for max bounces
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_get_max_bounces(LCComponentHandle component, int* out_bounces);

/**
 * @brief Set the maximum number of collision bounces per frame
 * @param component Component handle
 * @param bounces Maximum bounces (iterations for collision resolution)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_character_controller2d_set_max_bounces(LCComponentHandle component, int bounces);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_CHARACTER_CONTROLLER2D_H */
