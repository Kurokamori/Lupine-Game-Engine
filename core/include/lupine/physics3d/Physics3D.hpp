#pragma once

/**
 * Lupine Engine - Physics3D Module
 *
 * This module provides 3D physics simulation using Bullet3 including:
 * - Physics world management
 * - Rigid body dynamics (Static, Dynamic, Kinematic)
 * - Colliders (Box, Sphere, Capsule, Mesh)
 * - Collision detection and callbacks
 * - Trigger events for sensors
 * - Raycasting and overlap queries
 * - Shape casting
 * - Transform synchronization
 */

#include "Physics3DWorld.hpp"
#include "RigidBody3D.hpp"
#include "Collider3D.hpp"

namespace lupine {
namespace physics3d {

/**
 * Initialize the Physics3D module
 * This should be called once at application startup
 */
void InitializePhysics3D();

/**
 * Shutdown the Physics3D module
 * This should be called once at application shutdown
 */
void ShutdownPhysics3D();

} // namespace physics3d
} // namespace lupine

