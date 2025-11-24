#pragma once

/**
 * Lupine Engine - Physics2D Module
 *
 * This module provides 2D physics simulation using Box2D 3.x including:
 * - Physics world management
 * - Rigid body dynamics (Static, Dynamic, Kinematic)
 * - Colliders (Box, Circle, Polygon)
 * - Collision detection and callbacks
 * - Trigger events for sensors
 * - Raycasting and overlap queries
 * - Shape casting
 * - Transform synchronization
 */

#include "Physics2DWorld.hpp"
#include "RigidBody2D.hpp"
#include "Collider2D.hpp"

namespace lupine {
namespace physics2d {

/**
 * Initialize the Physics2D module
 * This should be called once at application startup
 */
void InitializePhysics2D();

/**
 * Shutdown the Physics2D module
 * This should be called once at application shutdown
 */
void ShutdownPhysics2D();

} // namespace physics2d
} // namespace lupine

