#pragma once

#include "lupine/core/UUID.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Quat.hpp"
#include "lupine/math/Transform.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>

// Forward declare Bullet types
class btDiscreteDynamicsWorld;
class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;

namespace lupine {
namespace physics3d {

// Forward declarations
class RigidBody3D;
class Collider3D;

/**
 * Body types for physics simulation
 */
enum class BodyType {
    Static,     // Zero mass, zero velocity, may be manually moved
    Kinematic,  // Zero mass, non-zero velocity set by user, moved by solver
    Dynamic     // Positive mass, non-zero velocity determined by forces, moved by solver
};

/**
 * Collision event data
 */
struct CollisionInfo3D {
    core::UUID bodyA;
    core::UUID bodyB;
    math::Vec3 point;
    math::Vec3 normal;
    float penetrationDepth;
};

/**
 * Raycast hit result
 */
struct RaycastHit3D {
    core::UUID bodyId;
    math::Vec3 point;
    math::Vec3 normal;
    float fraction;  // 0.0 to 1.0, where hit occurred along ray
    bool hit;
};

/**
 * Overlap query result
 */
struct OverlapResult3D {
    core::UUID bodyId;
};

/**
 * Shape cast hit result
 */
struct ShapeCastHit3D {
    core::UUID bodyId;
    math::Vec3 point;
    math::Vec3 normal;
    float fraction;
    bool hit;
};

/**
 * Collision callback function type
 */
using CollisionCallback = std::function<void(const CollisionInfo3D&)>;

/**
 * Physics3D World
 * Manages the Bullet3 physics simulation world
 */
class Physics3DWorld {
public:
    Physics3DWorld();
    ~Physics3DWorld();

    // World configuration
    void SetGravity(const math::Vec3& gravity);
    math::Vec3 GetGravity() const;

    void SetTimeStep(float timeStep);
    float GetTimeStep() const { return m_TimeStep; }

    // Simulation
    void Step(float deltaTime);
    void Step(); // Uses default time step

    // Body management
    RigidBody3D* CreateBody(core::UUID& outId, BodyType type);
    RigidBody3D* GetBody(const core::UUID& id);
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
    bool Raycast(const math::Vec3& origin, const math::Vec3& direction, float maxDistance, RaycastHit3D& hit, const core::UUID* ignoreBody = nullptr);
    std::vector<RaycastHit3D> RaycastAll(const math::Vec3& origin, const math::Vec3& direction, float maxDistance, const core::UUID* ignoreBody = nullptr);

    // Overlap queries
    std::vector<OverlapResult3D> OverlapSphere(const math::Vec3& center, float radius);
    std::vector<OverlapResult3D> OverlapBox(const math::Vec3& center, const math::Vec3& halfExtents, const math::Quat& rotation = math::Quat::Identity());

    // Shape casting
    bool SphereCast(const math::Vec3& start, const math::Vec3& end, float radius, ShapeCastHit3D& hit, const core::UUID* ignoreBody = nullptr);

    // Internal access (for RigidBody3D and Collider3D)
    btDiscreteDynamicsWorld* GetBulletWorld() const { return m_DynamicsWorld; }

private:
    void ProcessCollisionEvents();

    btDiscreteDynamicsWorld* m_DynamicsWorld;
    btBroadphaseInterface* m_Broadphase;
    btDefaultCollisionConfiguration* m_CollisionConfiguration;
    btCollisionDispatcher* m_Dispatcher;
    btSequentialImpulseConstraintSolver* m_Solver;

    float m_TimeStep;
    std::unordered_map<core::UUID, std::unique_ptr<RigidBody3D>> m_Bodies;

    // Collision callbacks
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionEnterCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionStayCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionExitCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerEnterCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerStayCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerExitCallbacks;

    // Track active collisions for enter/exit events
    std::unordered_map<std::string, bool> m_ActiveCollisions;
};

} // namespace physics3d
} // namespace lupine

