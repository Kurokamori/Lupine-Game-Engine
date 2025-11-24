#pragma once

#include "lupine/core/UUID.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Transform.hpp"
#include <box2d/box2d.h>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>

namespace lupine {
namespace physics2d {

// Forward declarations
class RigidBody2D;
class Collider2D;

/**
 * Body types for physics simulation
 */
enum class BodyType {
    Static,     // Zero mass, zero velocity, may be manually moved
    Kinematic,  // Zero mass, non-zero velocity set by user, moved by solver
    Dynamic     // Positive mass, non-zero velocity determined by forces, moved by solver
};

/**
 * Physics material properties for colliders
 */
struct PhysicsMaterial2D {
    float density = 1.0f;       // Density in kg/m^2
    float friction = 0.3f;      // Friction coefficient [0, 1]
    float restitution = 0.0f;   // Restitution (bounciness) [0, 1]

    PhysicsMaterial2D() = default;
    PhysicsMaterial2D(float d, float f, float r) : density(d), friction(f), restitution(r) {}
};

/**
 * Collision information passed to callbacks
 */
struct CollisionInfo {
    core::UUID bodyA;           // First body involved in collision
    core::UUID bodyB;           // Second body involved in collision
    math::Vec2 point;           // Contact point in world space
    math::Vec2 normal;          // Normal vector from A to B
    float normalImpulse;        // Normal impulse applied
    float tangentImpulse;       // Tangent impulse applied
    bool isSensor;              // True if either collider is a sensor
};

/**
 * Raycast hit result
 */
struct RaycastHit2D {
    core::UUID bodyId;          // Body that was hit
    math::Vec2 point;           // Hit point in world space
    math::Vec2 normal;          // Surface normal at hit point
    float fraction;             // Fraction along ray where hit occurred [0, 1]
    bool hit;                   // True if raycast hit something

    RaycastHit2D() : hit(false), fraction(1.0f) {}
};

/**
 * Overlap query result
 */
struct OverlapResult2D {
    core::UUID bodyId;          // Body that overlaps
    math::Vec2 point;           // Point of overlap (approximate)
};

/**
 * Shape cast result
 */
struct ShapeCastHit2D {
    core::UUID bodyId;          // Body that was hit
    math::Vec2 point;           // Hit point in world space
    math::Vec2 normal;          // Surface normal at hit point
    float fraction;             // Fraction along cast where hit occurred [0, 1]
    bool hit;                   // True if shape cast hit something

    ShapeCastHit2D() : hit(false), fraction(1.0f) {}
};

// Callback types
using CollisionCallback = std::function<void(const CollisionInfo&)>;

/**
 * Physics2D World
 * Manages the Box2D physics simulation world
 */
class Physics2DWorld {
public:
    Physics2DWorld();
    ~Physics2DWorld();

    // World configuration
    void SetGravity(const math::Vec2& gravity);
    math::Vec2 GetGravity() const;

    void SetTimeStep(float timeStep);
    float GetTimeStep() const { return m_TimeStep; }

    void SetVelocityIterations(int iterations);
    int GetVelocityIterations() const { return m_VelocityIterations; }

    void SetPositionIterations(int iterations);
    int GetPositionIterations() const { return m_PositionIterations; }

    // Simulation
    void Step(float deltaTime);
    void Step(); // Uses default time step

    // Body management
    RigidBody2D* CreateBody(core::UUID& outId, BodyType type);
    RigidBody2D* GetBody(const core::UUID& id);
    void DestroyBody(const core::UUID& id);

    // Collision callbacks
    void RegisterCollisionEnter(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterCollisionStay(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterCollisionExit(const core::UUID& bodyId, CollisionCallback callback);

    // Trigger callbacks (for sensors)
    void RegisterTriggerEnter(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterTriggerStay(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterTriggerExit(const core::UUID& bodyId, CollisionCallback callback);

    // Raycasting
    bool Raycast(const math::Vec2& origin, const math::Vec2& direction, float maxDistance, RaycastHit2D& hit, const core::UUID* ignoreBody = nullptr);
    std::vector<RaycastHit2D> RaycastAll(const math::Vec2& origin, const math::Vec2& direction, float maxDistance, const core::UUID* ignoreBody = nullptr);

    // Overlap queries
    std::vector<OverlapResult2D> OverlapPoint(const math::Vec2& point);
    std::vector<OverlapResult2D> OverlapCircle(const math::Vec2& center, float radius);
    std::vector<OverlapResult2D> OverlapBox(const math::Vec2& center, const math::Vec2& size, float angle = 0.0f);

    // Shape casting
    bool ShapeCast(const math::Vec2& start, const math::Vec2& end, float radius, ShapeCastHit2D& hit, const core::UUID* ignoreBody = nullptr);

    // Internal access (for RigidBody2D and Collider2D)
    b2WorldId GetBox2DWorld() const { return m_WorldId; }

private:
    b2WorldId m_WorldId;
    float m_TimeStep;
    int m_VelocityIterations;
    int m_PositionIterations;

    std::unordered_map<core::UUID, std::unique_ptr<RigidBody2D>> m_Bodies;
    
    // Collision callbacks
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionEnterCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionStayCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionExitCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerEnterCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerStayCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerExitCallbacks;

    // Contact tracking for enter/exit events
    std::unordered_map<uint64_t, bool> m_ActiveContacts; // Hash of body pair -> is sensor

    void ProcessCollisionEvents();
    uint64_t GetContactHash(const core::UUID& a, const core::UUID& b) const;
};

} // namespace physics2d
} // namespace lupine

