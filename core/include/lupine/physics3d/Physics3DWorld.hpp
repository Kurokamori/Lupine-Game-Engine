#pragma once

#include "lupine/core/UUID.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Quat.hpp"
#include "lupine/math/Transform.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <functional>
#include <utility>
#include <vector>

// Forward declare Bullet types
class btDiscreteDynamicsWorld;
class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btCollisionObject;

namespace lupine {
namespace core { class Node; }
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
 * Default layer mask for queries: collide against every layer.
 */
constexpr uint32_t kAllLayers3D = 0xFFFFFFFFu;

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

    // Wholesale teardown, used on scene changes: destroys every body and drops all callbacks,
    // body->node associations and contact tracking, leaving an empty but usable world. The 2D
    // counterpart (Physics2DWorld::Clear) exists for the same reason.
    void Clear();

    // Collision callbacks
    void RegisterCollisionEnter(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterCollisionStay(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterCollisionExit(const core::UUID& bodyId, CollisionCallback callback);

    // Trigger callbacks (for sensors)
    void RegisterTriggerEnter(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterTriggerStay(const core::UUID& bodyId, CollisionCallback callback);
    void RegisterTriggerExit(const core::UUID& bodyId, CollisionCallback callback);

    // Body -> owning Node association (see Physics2DWorld for rationale).
    void SetBodyNode(const core::UUID& bodyId, core::Node* node);
    core::Node* GetBodyNode(const core::UUID& bodyId) const;
    void ClearBodyNode(const core::UUID& bodyId);

    // Reverse lookup: the body owned by a node, or a zero UUID if it owns none.
    // Lets ray/shape casts exclude a node's own body via the ignoreBody filter.
    core::UUID FindBodyForNode(const core::Node* node) const;

    // Raycasting. layerMask follows the same "share any layer" rule the bodies use:
    // a body is considered only if its collision layers intersect the mask.
    bool Raycast(const math::Vec3& origin, const math::Vec3& direction, float maxDistance, RaycastHit3D& hit, const core::UUID* ignoreBody = nullptr, uint32_t layerMask = kAllLayers3D);
    std::vector<RaycastHit3D> RaycastAll(const math::Vec3& origin, const math::Vec3& direction, float maxDistance, const core::UUID* ignoreBody = nullptr, uint32_t layerMask = kAllLayers3D);

    // Overlap queries
    std::vector<OverlapResult3D> OverlapSphere(const math::Vec3& center, float radius, const core::UUID* ignoreBody = nullptr, uint32_t layerMask = kAllLayers3D);
    std::vector<OverlapResult3D> OverlapBox(const math::Vec3& center, const math::Vec3& halfExtents, const math::Quat& rotation = math::Quat::Identity(), const core::UUID* ignoreBody = nullptr, uint32_t layerMask = kAllLayers3D);

    // Shape casting
    bool SphereCast(const math::Vec3& start, const math::Vec3& end, float radius, ShapeCastHit3D& hit, const core::UUID* ignoreBody = nullptr, uint32_t layerMask = kAllLayers3D);

    // Internal access (for RigidBody3D and Collider3D)
    btDiscreteDynamicsWorld* GetBulletWorld() const { return m_DynamicsWorld; }

    // Resolves a Bullet collision object back to the engine body that owns it, or null if
    // the object is not one of ours. Used by manifold processing and the query callbacks.
    RigidBody3D* GetBodyForObject(const btCollisionObject* object) const;

private:
    // An undirected body pair, always stored with the lower id first so the same contact
    // maps to the same key regardless of which body Bullet reports as body0.
    using BodyPairKey = std::pair<uint64_t, uint64_t>;

    enum class ContactPhase {
        Enter,
        Stay,
        Exit
    };

    struct PendingEvent {
        core::UUID target;
        CollisionInfo3D info;
        ContactPhase phase;
        bool isTrigger;
    };

    static BodyPairKey MakePairKey(const core::UUID& a, const core::UUID& b);

    void ProcessCollisionEvents();
    void DestroyAllBodies();
    void DispatchEvents(const std::vector<PendingEvent>& events);
    void PurgeBodyFromContacts(const core::UUID& bodyId);
    const btCollisionObject* GetBulletObject(const core::UUID* bodyId) const;

    btDiscreteDynamicsWorld* m_DynamicsWorld;
    btBroadphaseInterface* m_Broadphase;
    btDefaultCollisionConfiguration* m_CollisionConfiguration;
    btCollisionDispatcher* m_Dispatcher;
    btSequentialImpulseConstraintSolver* m_Solver;

    float m_TimeStep;
    std::unordered_map<core::UUID, std::unique_ptr<RigidBody3D>> m_Bodies;

    // Reverse index so manifold/query results resolve to a body without scanning m_Bodies.
    std::unordered_map<const btCollisionObject*, core::UUID> m_ObjectToBody;

    // Collision callbacks
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionEnterCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionStayCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_CollisionExitCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerEnterCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerStayCallbacks;
    std::unordered_map<core::UUID, CollisionCallback> m_TriggerExitCallbacks;

    std::unordered_map<core::UUID, core::Node*> m_BodyNodes;

    // Track active contacts for enter/stay/exit events. Sensor pairs (either body carrying
    // CF_NO_CONTACT_RESPONSE) are tracked separately and drive the trigger callbacks.
    std::map<BodyPairKey, CollisionInfo3D> m_ActiveCollisions;
    std::map<BodyPairKey, CollisionInfo3D> m_ActiveTriggers;
};

} // namespace physics3d
} // namespace lupine

