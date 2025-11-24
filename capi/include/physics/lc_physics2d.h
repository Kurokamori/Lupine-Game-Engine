/**
 * @file lc_physics2d.h
 * @brief Lupine Engine C API - 2D Physics system
 *
 * This header provides 2D physics functionality including rigid bodies,
 * static bodies, kinematic bodies, and collision detection.
 */

#ifndef LUPINE_CAPI_PHYSICS2D_H
#define LUPINE_CAPI_PHYSICS2D_H

#include "core/lc_core.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * RigidBody2D Functions
 * ============================================================================ */

/**
 * @brief Create a RigidBody2D component (dynamic physics body)
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Get the mass of a RigidBody2D
 * @param component Component handle
 * @param out_mass Output parameter for mass (kg)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_mass(LCComponentHandle component, float* out_mass);

/**
 * @brief Get the gravity scale of a RigidBody2D
 * @param component Component handle
 * @param out_scale Output parameter for gravity scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_gravity_scale(LCComponentHandle component, float* out_scale);

/**
 * @brief Set the gravity scale of a RigidBody2D
 * @param component Component handle
 * @param scale Gravity scale (1.0 = normal, 0.0 = no gravity)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_gravity_scale(LCComponentHandle component, float scale);

/**
 * @brief Get the linear damping of a RigidBody2D
 * @param component Component handle
 * @param out_damping Output parameter for linear damping
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_linear_damping(LCComponentHandle component, float* out_damping);

/**
 * @brief Set the linear damping of a RigidBody2D
 * @param component Component handle
 * @param damping Linear damping (resistance to linear motion)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_linear_damping(LCComponentHandle component, float damping);

/**
 * @brief Get the angular damping of a RigidBody2D
 * @param component Component handle
 * @param out_damping Output parameter for angular damping
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_angular_damping(LCComponentHandle component, float* out_damping);

/**
 * @brief Set the angular damping of a RigidBody2D
 * @param component Component handle
 * @param damping Angular damping (resistance to rotation)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_angular_damping(LCComponentHandle component, float damping);

/**
 * @brief Check if a RigidBody2D has fixed rotation
 * @param component Component handle
 * @param out_fixed Output parameter for fixed rotation state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_fixed_rotation(LCComponentHandle component, bool* out_fixed);

/**
 * @brief Set whether a RigidBody2D has fixed rotation
 * @param component Component handle
 * @param fixed Fixed rotation (prevents rotation)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_fixed_rotation(LCComponentHandle component, bool fixed);

/**
 * @brief Check if a RigidBody2D is in bullet mode
 * @param component Component handle
 * @param out_bullet Output parameter for bullet mode state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_bullet(LCComponentHandle component, bool* out_bullet);

/**
 * @brief Set whether a RigidBody2D is in bullet mode
 * @param component Component handle
 * @param bullet Bullet mode (continuous collision detection for fast objects)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_bullet(LCComponentHandle component, bool bullet);

/**
 * @brief Check if a RigidBody2D can sleep
 * @param component Component handle
 * @param out_can_sleep Output parameter for can sleep state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_can_sleep(LCComponentHandle component, bool* out_can_sleep);

/**
 * @brief Set whether a RigidBody2D can sleep
 * @param component Component handle
 * @param can_sleep Can sleep (allows body to sleep when at rest)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_can_sleep(LCComponentHandle component, bool can_sleep);

/**
 * @brief Check if gravity is enabled for a RigidBody2D
 * @param component Component handle
 * @param out_enabled Output parameter for gravity enabled state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_gravity_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether gravity is enabled for a RigidBody2D
 * @param component Component handle
 * @param enabled Gravity enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_gravity_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Apply force at a point to a RigidBody2D
 * @param component Component handle
 * @param force Force vector
 * @param point Point at which to apply force (world coordinates)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_apply_force(LCComponentHandle component, LCVec2 force, LCVec2 point);

/**
 * @brief Apply force at center to a RigidBody2D
 * @param component Component handle
 * @param force Force vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_apply_force_to_center(LCComponentHandle component, LCVec2 force);

/**
 * @brief Apply torque to a RigidBody2D
 * @param component Component handle
 * @param torque Torque to apply
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_apply_torque(LCComponentHandle component, float torque);

/**
 * @brief Apply linear impulse at a point to a RigidBody2D
 * @param component Component handle
 * @param impulse Impulse vector
 * @param point Point at which to apply impulse (world coordinates)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_apply_linear_impulse(LCComponentHandle component, LCVec2 impulse, LCVec2 point);

/**
 * @brief Apply linear impulse at center to a RigidBody2D
 * @param component Component handle
 * @param impulse Impulse vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_apply_linear_impulse_to_center(LCComponentHandle component, LCVec2 impulse);

/**
 * @brief Apply angular impulse to a RigidBody2D
 * @param component Component handle
 * @param impulse Angular impulse
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_apply_angular_impulse(LCComponentHandle component, float impulse);

/**
 * @brief Get the linear velocity of a RigidBody2D
 * @param component Component handle
 * @param out_velocity Output parameter for linear velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_linear_velocity(LCComponentHandle component, LCVec2* out_velocity);

/**
 * @brief Set the linear velocity of a RigidBody2D
 * @param component Component handle
 * @param velocity Linear velocity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_linear_velocity(LCComponentHandle component, LCVec2 velocity);

/**
 * @brief Get the angular velocity of a RigidBody2D
 * @param component Component handle
 * @param out_omega Output parameter for angular velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_get_angular_velocity(LCComponentHandle component, float* out_omega);

/**
 * @brief Set the angular velocity of a RigidBody2D
 * @param component Component handle
 * @param omega Angular velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body2d_set_angular_velocity(LCComponentHandle component, float omega);

/* ============================================================================
 * StaticBody2D Functions
 * ============================================================================ */

/**
 * @brief Create a StaticBody2D component (static physics body)
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body2d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Get the constant linear velocity of a StaticBody2D
 * @param component Component handle
 * @param out_velocity Output parameter for constant linear velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body2d_get_constant_linear_velocity(LCComponentHandle component, LCVec2* out_velocity);

/**
 * @brief Set the constant linear velocity of a StaticBody2D
 * @param component Component handle
 * @param velocity Constant linear velocity (for moving platforms)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body2d_set_constant_linear_velocity(LCComponentHandle component, LCVec2 velocity);

/**
 * @brief Get the constant angular velocity of a StaticBody2D
 * @param component Component handle
 * @param out_velocity Output parameter for constant angular velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body2d_get_constant_angular_velocity(LCComponentHandle component, float* out_velocity);

/**
 * @brief Set the constant angular velocity of a StaticBody2D
 * @param component Component handle
 * @param velocity Constant angular velocity (for rotating platforms)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body2d_set_constant_angular_velocity(LCComponentHandle component, float velocity);

/**
 * @brief Set the position of a StaticBody2D (teleport)
 * @param component Component handle
 * @param position New position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body2d_set_position(LCComponentHandle component, LCVec2 position);

/**
 * @brief Set the rotation of a StaticBody2D (teleport)
 * @param component Component handle
 * @param angle New rotation angle (radians)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body2d_set_rotation(LCComponentHandle component, float angle);

/* ============================================================================
 * KinematicBody2D Functions
 * ============================================================================ */

/**
 * @brief Create a KinematicBody2D component (kinematic physics body)
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Check if gravity is enabled for a KinematicBody2D
 * @param component Component handle
 * @param out_enabled Output parameter for gravity enabled state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_get_gravity_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether gravity is enabled for a KinematicBody2D
 * @param component Component handle
 * @param enabled Gravity enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_set_gravity_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Get the gravity scale of a KinematicBody2D
 * @param component Component handle
 * @param out_scale Output parameter for gravity scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_get_gravity_scale(LCComponentHandle component, float* out_scale);

/**
 * @brief Set the gravity scale of a KinematicBody2D
 * @param component Component handle
 * @param scale Gravity scale (if gravity is enabled)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_set_gravity_scale(LCComponentHandle component, float scale);

/**
 * @brief Get the linear velocity of a KinematicBody2D
 * @param component Component handle
 * @param out_velocity Output parameter for linear velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_get_linear_velocity(LCComponentHandle component, LCVec2* out_velocity);

/**
 * @brief Set the linear velocity of a KinematicBody2D
 * @param component Component handle
 * @param velocity Linear velocity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_set_linear_velocity(LCComponentHandle component, LCVec2 velocity);

/**
 * @brief Get the angular velocity of a KinematicBody2D
 * @param component Component handle
 * @param out_omega Output parameter for angular velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_get_angular_velocity(LCComponentHandle component, float* out_omega);

/**
 * @brief Set the angular velocity of a KinematicBody2D
 * @param component Component handle
 * @param omega Angular velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_set_angular_velocity(LCComponentHandle component, float omega);

/**
 * @brief Move a KinematicBody2D by a delta
 * @param component Component handle
 * @param delta Movement delta vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_move_by(LCComponentHandle component, LCVec2 delta);

/**
 * @brief Rotate a KinematicBody2D by a delta angle
 * @param component Component handle
 * @param delta_angle Rotation delta (radians)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body2d_rotate_by(LCComponentHandle component, float delta_angle);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_PHYSICS2D_H */
