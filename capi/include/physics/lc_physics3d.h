/**
 * @file lc_physics3d.h
 * @brief Lupine Engine C API - 3D Physics system
 *
 * This header provides 3D physics functionality including rigid bodies,
 * static bodies, kinematic bodies, and collision detection.
 */

#ifndef LUPINE_CAPI_PHYSICS3D_H
#define LUPINE_CAPI_PHYSICS3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"


/* ============================================================================
 * RigidBody3D Functions
 * ============================================================================ */

/**
 * @brief Create a RigidBody3D component (dynamic physics body)
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Get the mass of a RigidBody3D
 * @param component Component handle
 * @param out_mass Output parameter for mass (kg)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_mass(LCComponentHandle component, float* out_mass);

/**
 * @brief Set the mass of a RigidBody3D
 * @param component Component handle
 * @param mass Mass in kilograms
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_mass(LCComponentHandle component, float mass);

/**
 * @brief Get the gravity scale of a RigidBody3D
 * @param component Component handle
 * @param out_scale Output parameter for gravity scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_gravity_scale(LCComponentHandle component, float* out_scale);

/**
 * @brief Set the gravity scale of a RigidBody3D
 * @param component Component handle
 * @param scale Gravity scale (1.0 = normal, 0.0 = no gravity)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_gravity_scale(LCComponentHandle component, float scale);

/**
 * @brief Get the linear damping of a RigidBody3D
 * @param component Component handle
 * @param out_damping Output parameter for linear damping
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_linear_damping(LCComponentHandle component, float* out_damping);

/**
 * @brief Set the linear damping of a RigidBody3D
 * @param component Component handle
 * @param damping Linear damping (resistance to linear motion)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_linear_damping(LCComponentHandle component, float damping);

/**
 * @brief Get the angular damping of a RigidBody3D
 * @param component Component handle
 * @param out_damping Output parameter for angular damping
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_angular_damping(LCComponentHandle component, float* out_damping);

/**
 * @brief Set the angular damping of a RigidBody3D
 * @param component Component handle
 * @param damping Angular damping (resistance to rotation)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_angular_damping(LCComponentHandle component, float damping);

/**
 * @brief Check if X-axis rotation is locked for a RigidBody3D
 * @param component Component handle
 * @param out_locked Output parameter for lock state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_lock_rotation_x(LCComponentHandle component, bool* out_locked);

/**
 * @brief Set whether X-axis rotation is locked for a RigidBody3D
 * @param component Component handle
 * @param locked Lock X-axis rotation
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_lock_rotation_x(LCComponentHandle component, bool locked);

/**
 * @brief Check if Y-axis rotation is locked for a RigidBody3D
 * @param component Component handle
 * @param out_locked Output parameter for lock state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_lock_rotation_y(LCComponentHandle component, bool* out_locked);

/**
 * @brief Set whether Y-axis rotation is locked for a RigidBody3D
 * @param component Component handle
 * @param locked Lock Y-axis rotation
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_lock_rotation_y(LCComponentHandle component, bool locked);

/**
 * @brief Check if Z-axis rotation is locked for a RigidBody3D
 * @param component Component handle
 * @param out_locked Output parameter for lock state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_lock_rotation_z(LCComponentHandle component, bool* out_locked);

/**
 * @brief Set whether Z-axis rotation is locked for a RigidBody3D
 * @param component Component handle
 * @param locked Lock Z-axis rotation
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_lock_rotation_z(LCComponentHandle component, bool locked);

/**
 * @brief Check if a RigidBody3D can sleep
 * @param component Component handle
 * @param out_can_sleep Output parameter for can sleep state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_can_sleep(LCComponentHandle component, bool* out_can_sleep);

/**
 * @brief Set whether a RigidBody3D can sleep
 * @param component Component handle
 * @param can_sleep Can sleep (allows body to sleep when at rest)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_can_sleep(LCComponentHandle component, bool can_sleep);

/**
 * @brief Check if gravity is enabled for a RigidBody3D
 * @param component Component handle
 * @param out_enabled Output parameter for gravity enabled state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_gravity_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether gravity is enabled for a RigidBody3D
 * @param component Component handle
 * @param enabled Gravity enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_gravity_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Check if continuous collision detection (CCD) is enabled for a RigidBody3D
 * @param component Component handle
 * @param out_use_ccd Output parameter for CCD state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_use_ccd(LCComponentHandle component, bool* out_use_ccd);

/**
 * @brief Set whether continuous collision detection (CCD) is enabled for a RigidBody3D
 * @param component Component handle
 * @param use_ccd Use continuous collision detection (for fast-moving objects)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_use_ccd(LCComponentHandle component, bool use_ccd);

/**
 * @brief Apply force at a point to a RigidBody3D
 * @param component Component handle
 * @param force Force vector
 * @param point Point at which to apply force (world coordinates)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_apply_force(LCComponentHandle component, LCVec3 force, LCVec3 point);

/**
 * @brief Apply force at center to a RigidBody3D
 * @param component Component handle
 * @param force Force vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_apply_force_to_center(LCComponentHandle component, LCVec3 force);

/**
 * @brief Apply torque to a RigidBody3D
 * @param component Component handle
 * @param torque Torque vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_apply_torque(LCComponentHandle component, LCVec3 torque);

/**
 * @brief Apply impulse at a point to a RigidBody3D
 * @param component Component handle
 * @param impulse Impulse vector
 * @param point Point at which to apply impulse (world coordinates)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_apply_impulse(LCComponentHandle component, LCVec3 impulse, LCVec3 point);

/**
 * @brief Apply impulse at center to a RigidBody3D
 * @param component Component handle
 * @param impulse Impulse vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_apply_impulse_to_center(LCComponentHandle component, LCVec3 impulse);

/**
 * @brief Apply torque impulse to a RigidBody3D
 * @param component Component handle
 * @param torque Torque impulse vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_apply_torque_impulse(LCComponentHandle component, LCVec3 torque);

/**
 * @brief Get the linear velocity of a RigidBody3D
 * @param component Component handle
 * @param out_velocity Output parameter for linear velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_linear_velocity(LCComponentHandle component, LCVec3* out_velocity);

/**
 * @brief Set the linear velocity of a RigidBody3D
 * @param component Component handle
 * @param velocity Linear velocity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_linear_velocity(LCComponentHandle component, LCVec3 velocity);

/**
 * @brief Get the angular velocity of a RigidBody3D
 * @param component Component handle
 * @param out_omega Output parameter for angular velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_get_angular_velocity(LCComponentHandle component, LCVec3* out_omega);

/**
 * @brief Set the angular velocity of a RigidBody3D
 * @param component Component handle
 * @param omega Angular velocity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_set_angular_velocity(LCComponentHandle component, LCVec3 omega);

/**
 * @brief Clear all forces on a RigidBody3D
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_rigid_body3d_clear_forces(LCComponentHandle component);

/* ============================================================================
 * StaticBody3D Functions
 * ============================================================================ */

/**
 * @brief Create a StaticBody3D component (static physics body)
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body3d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Get the constant linear velocity of a StaticBody3D
 * @param component Component handle
 * @param out_velocity Output parameter for constant linear velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body3d_get_constant_linear_velocity(LCComponentHandle component, LCVec3* out_velocity);

/**
 * @brief Set the constant linear velocity of a StaticBody3D
 * @param component Component handle
 * @param velocity Constant linear velocity (for moving platforms)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body3d_set_constant_linear_velocity(LCComponentHandle component, LCVec3 velocity);

/**
 * @brief Get the constant angular velocity of a StaticBody3D
 * @param component Component handle
 * @param out_velocity Output parameter for constant angular velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body3d_get_constant_angular_velocity(LCComponentHandle component, LCVec3* out_velocity);

/**
 * @brief Set the constant angular velocity of a StaticBody3D
 * @param component Component handle
 * @param velocity Constant angular velocity (for rotating platforms)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body3d_set_constant_angular_velocity(LCComponentHandle component, LCVec3 velocity);

/**
 * @brief Set the position of a StaticBody3D (teleport)
 * @param component Component handle
 * @param position New position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body3d_set_position(LCComponentHandle component, LCVec3 position);

/**
 * @brief Set the rotation of a StaticBody3D (teleport)
 * @param component Component handle
 * @param rotation New rotation quaternion
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_body3d_set_rotation(LCComponentHandle component, LCQuat rotation);

/* ============================================================================
 * KinematicBody3D Functions
 * ============================================================================ */

/**
 * @brief Create a KinematicBody3D component (kinematic physics body)
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Check if gravity is enabled for a KinematicBody3D
 * @param component Component handle
 * @param out_enabled Output parameter for gravity enabled state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_get_gravity_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether gravity is enabled for a KinematicBody3D
 * @param component Component handle
 * @param enabled Gravity enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_set_gravity_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Get the gravity scale of a KinematicBody3D
 * @param component Component handle
 * @param out_scale Output parameter for gravity scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_get_gravity_scale(LCComponentHandle component, float* out_scale);

/**
 * @brief Set the gravity scale of a KinematicBody3D
 * @param component Component handle
 * @param scale Gravity scale (if gravity is enabled)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_set_gravity_scale(LCComponentHandle component, float scale);

/**
 * @brief Get the linear velocity of a KinematicBody3D
 * @param component Component handle
 * @param out_velocity Output parameter for linear velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_get_linear_velocity(LCComponentHandle component, LCVec3* out_velocity);

/**
 * @brief Set the linear velocity of a KinematicBody3D
 * @param component Component handle
 * @param velocity Linear velocity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_set_linear_velocity(LCComponentHandle component, LCVec3 velocity);

/**
 * @brief Get the angular velocity of a KinematicBody3D
 * @param component Component handle
 * @param out_omega Output parameter for angular velocity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_get_angular_velocity(LCComponentHandle component, LCVec3* out_omega);

/**
 * @brief Set the angular velocity of a KinematicBody3D
 * @param component Component handle
 * @param omega Angular velocity vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_set_angular_velocity(LCComponentHandle component, LCVec3 omega);

/**
 * @brief Move a KinematicBody3D by a delta
 * @param component Component handle
 * @param delta Movement delta vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_move_by(LCComponentHandle component, LCVec3 delta);

/**
 * @brief Rotate a KinematicBody3D by a delta rotation
 * @param component Component handle
 * @param delta_rotation Rotation delta quaternion
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_kinematic_body3d_rotate_by(LCComponentHandle component, LCQuat delta_rotation);


#endif /* LUPINE_CAPI_PHYSICS3D_H */
