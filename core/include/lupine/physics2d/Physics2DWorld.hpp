#pragma once

#include "lupine/core/UUID.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Transform.hpp"
#include <box2d/box2d.h>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>
#include <map>
#include <cstdint>

namespace lupine {
namespace core { class Node; }
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
    bool hit;                   // True if raycast hit something
    float fraction;             // Fraction along ray where hit occurred [0, 1]

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
    bool hit;                   // True if shape cast hit something
    float fraction;             // Fraction along cast where hit occurred [0, 1]

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

    // Destroy every remaining body and reset all tracked contact/overlap/owner
    // state at once. Used by the scene manager on a scene change: individual
    // physics-body components skip their per-body teardown during the swap (to
    // avoid double-freeing colliders that the body also owns), and this clears
    // whatever is left after the outgoing scene's nodes are gone.
    void Clear();

    // Collision callbacks
    void RegisterCollisionEnter(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterCollisionStay(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterCollisionExit(const core::UUID& bodyId, CollisionCallback callback);

    // Trigger callbacks (for sensors)
    void RegisterTriggerEnter(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterTriggerStay(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterTriggerExit(const core::UUID& bodyId, CollisionCallback callback);

    // Body -> owning Node association. Physics-body components register their
    // owner so collision/trigger signals can resolve the other body to a node.
    void SetBodyNode(const core::UUID& bodyId, core::Node* node);
    core::Node* GetBodyNode(const core::UUID& bodyId) const;
    void ClearBodyNode(const core::UUID& bodyId);

    // Reverse lookup: the body owned by a node, or a zero UUID if it owns none.
    // Lets ray/shape casts exclude a node's own body via the ignoreBody filter.
    core::UUID FindBodyForNode(const core::Node* node) const;

    // Raycasting. The collision mask selects which collision layers are hit
    // (a shape is considered when (mask & shape.categoryBits) != 0).
    bool Raycast(const math::Vec2& origin, const math::Vec2& direction, float maxDistance, RaycastHit2D& hit, const core::UUID* ignoreBody = nullptr, uint64_t collisionMask = 0xFFFFFFFFFFFFFFFFull);
    std::vector<RaycastHit2D> RaycastAll(const math::Vec2& origin, const math::Vec2& direction, float maxDistance, const core::UUID* ignoreBody = nullptr, uint64_t collisionMask = 0xFFFFFFFFFFFFFFFFull);

    // Overlap queries. These perform true geometric overlap tests against the
    // shapes in the world and honour the supplied collision mask.
    std::vector<OverlapResult2D> OverlapPoint(const math::Vec2& point, uint64_t collisionMask = 0xFFFFFFFFFFFFFFFFull);
    std::vector<OverlapResult2D> OverlapCircle(const math::Vec2& center, float radius, uint64_t collisionMask = 0xFFFFFFFFFFFFFFFFull);
    std::vector<OverlapResult2D> OverlapBox(const math::Vec2& center, const math::Vec2& size, float angle = 0.0f, uint64_t collisionMask = 0xFFFFFFFFFFFFFFFFull);

    // Shape casting
    bool ShapeCast(const math::Vec2& start, const math::Vec2& end, float radius, ShapeCastHit2D& hit, const core::UUID* ignoreBody = nullptr, uint64_t collisionMask = 0xFFFFFFFFFFFFFFFFull);

    // Sweep an arbitrary convex shape through the world along `translation` and
    // report the first solid (non-sensor) contact. `points` are the convex shape
    // vertices in WORLD space; a single point combined with a non-zero `radius`
    // denotes a circle, while three or more points denote a polygon (`radius`
    // rounds the polygon, normally 0). Used by the kinematic character controller
    // so the swept shape matches the body's real collider instead of an
    // approximating circle.
    bool ShapeCastConvex(const std::vector<math::Vec2>& points, float radius,
                         const math::Vec2& translation, ShapeCastHit2D& hit,
                         const core::UUID* ignoreBody = nullptr,
                         uint64_t collisionMask = 0xFFFFFFFFFFFFFFFFull);

    // Resolve any overlap of an arbitrary convex polygon against solid world
    // geometry, returning the minimum translation that pushes it clear. `points`
    // are the polygon vertices in WORLD space (>= 3); `radius` rounds the polygon.
    // Returns true and fills `outCorrection` when a non-zero correction is needed.
    bool ComputeConvexDepenetration(const std::vector<math::Vec2>& points, float radius,
                                    const core::UUID* ignoreBody, uint64_t collisionMask,
                                    math::Vec2& outCorrection);

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

    std::unordered_map<core::UUID, core::Node*> m_BodyNodes;

    // Persistent contact tracking for solid (non-sensor) contacts. Keyed by the ordered pair
    // of *shape* ids, not body ids: a body may carry several colliders (a capsule plus a foot
    // box, say), and keying by body would let the second shape pair overwrite the first, so
    // the first pair's end-touch would fire an exit while the body is still in contact.
    struct SolidContact {
        core::UUID bodyA;
        core::UUID bodyB;
        b2ShapeId shapeA;
        b2ShapeId shapeB;
        bool touchedThisStep;
    };
    std::map<std::pair<uint64_t, uint64_t>, SolidContact> m_ActiveContacts;

    // Persistent sensor overlap tracking, keyed by the (sensor shape, visitor shape) pair for
    // the same reason. The sensor side owns the trigger events.
    struct SensorOverlap {
        core::UUID sensorBody;
        core::UUID visitorBody;
        b2ShapeId sensorShape;
        b2ShapeId visitorShape;
        bool touchedThisStep;
    };
    std::map<std::pair<uint64_t, uint64_t>, SensorOverlap> m_ActiveSensors;

    void ProcessCollisionEvents();
    void ProcessContactEvents();
    void ProcessSensorEvents();

    // Resolve a Box2D body handle back to the owning RigidBody2D via user data.
    RigidBody2D* ResolveBody(b2BodyId bodyId) const;

    // Build a populated CollisionInfo for a live solid contact between two shapes.
    void FillSolidContactInfo(const SolidContact& contact, CollisionInfo& info) const;

    // Remove any tracked contacts/overlaps that reference the given body.
    void PurgeBodyFromContacts(const core::UUID& bodyId);

    void DispatchCollision(const std::unordered_map<core::UUID, CollisionCallback>& callbacks,
                           const core::UUID& bodyId, const CollisionInfo& info) const;

public:
    // Called by Collider2D when its shape is about to be destroyed (destruction, or a
    // rebuild via UpdateShape). Box2D reports the resulting end-touch events with shape ids
    // that are already invalid, so the tracked contact would otherwise survive forever:
    // Stay would keep firing and Exit would never arrive. Purging here fires the pending
    // exit/trigger-exit callbacks and drops the entries.
    void PurgeShapeFromContacts(b2ShapeId shape);
};

} // namespace physics2d
} // namespace lupine

