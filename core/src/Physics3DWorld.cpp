#include "lupine/physics3d/Physics3DWorld.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/physics3d/Collider3D.hpp"
#include "lupine/logger/Logger.hpp"
#include <btBulletDynamicsCommon.h>
#include <algorithm>
#include <iterator>
#include <unordered_set>

namespace lupine {
namespace physics3d {

Physics3DWorld::Physics3DWorld()
    : m_TimeStep(1.0f / 60.0f)
{

    m_CollisionConfiguration = new btDefaultCollisionConfiguration();

    m_Dispatcher = new btCollisionDispatcher(m_CollisionConfiguration);

    m_Broadphase = new btDbvtBroadphase();

    m_Solver = new btSequentialImpulseConstraintSolver();

    m_DynamicsWorld = new btDiscreteDynamicsWorld(m_Dispatcher, m_Broadphase, m_Solver, m_CollisionConfiguration);

    m_DynamicsWorld->setGravity(btVector3(0.0f, -9.81f, 0.0f));

}

Physics3DWorld::~Physics3DWorld() {

    Clear();

    if (m_DynamicsWorld) {
        delete m_DynamicsWorld;
        m_DynamicsWorld = nullptr;
    }
    if (m_Solver) {
        delete m_Solver;
        m_Solver = nullptr;
    }
    if (m_Broadphase) {
        delete m_Broadphase;
        m_Broadphase = nullptr;
    }
    if (m_Dispatcher) {
        delete m_Dispatcher;
        m_Dispatcher = nullptr;
    }
    if (m_CollisionConfiguration) {
        delete m_CollisionConfiguration;
        m_CollisionConfiguration = nullptr;
    }

}

void Physics3DWorld::SetGravity(const math::Vec3& gravity) {
    m_DynamicsWorld->setGravity(btVector3(gravity.x, gravity.y, gravity.z));
}

math::Vec3 Physics3DWorld::GetGravity() const {
    btVector3 gravity = m_DynamicsWorld->getGravity();
    return math::Vec3(gravity.x(), gravity.y(), gravity.z());
}

void Physics3DWorld::SetTimeStep(float timeStep) {
    m_TimeStep = timeStep;
}

void Physics3DWorld::Step(float deltaTime) {

    m_DynamicsWorld->stepSimulation(deltaTime, 10);

    ProcessCollisionEvents();
}

void Physics3DWorld::Step() {
    Step(m_TimeStep);
}

RigidBody3D* Physics3DWorld::CreateBody(core::UUID& outId, BodyType type) {
    outId = core::UUID();
    auto body = std::make_unique<RigidBody3D>(this, outId, type);
    RigidBody3D* bodyPtr = body.get();
    m_Bodies[outId] = std::move(body);

    if (bodyPtr->GetBulletBody()) {
        m_ObjectToBody[bodyPtr->GetBulletBody()] = outId;
    }

    return bodyPtr;
}

RigidBody3D* Physics3DWorld::GetBody(const core::UUID& id) {
    auto it = m_Bodies.find(id);
    if (it != m_Bodies.end()) {
        return it->second.get();
    }
    return nullptr;
}

void Physics3DWorld::DestroyBody(const core::UUID& id) {
    auto it = m_Bodies.find(id);
    if (it != m_Bodies.end()) {
        if (it->second->GetBulletBody()) {
            m_ObjectToBody.erase(it->second->GetBulletBody());
        }
        m_Bodies.erase(it);
    }

    m_BodyNodes.erase(id);

    // Drop every reference to the destroyed body. The callbacks capture the registering
    // component, so leaving them behind is both an unbounded leak across spawn/despawn
    // cycles and a stale pointer waiting for a recycled body id.
    m_CollisionEnterCallbacks.erase(id);
    m_CollisionStayCallbacks.erase(id);
    m_CollisionExitCallbacks.erase(id);
    m_TriggerEnterCallbacks.erase(id);
    m_TriggerStayCallbacks.erase(id);
    m_TriggerExitCallbacks.erase(id);

    PurgeBodyFromContacts(id);
}

void Physics3DWorld::Clear() {
    m_ActiveCollisions.clear();
    m_ActiveTriggers.clear();
    m_BodyNodes.clear();

    m_CollisionEnterCallbacks.clear();
    m_CollisionStayCallbacks.clear();
    m_CollisionExitCallbacks.clear();
    m_TriggerEnterCallbacks.clear();
    m_TriggerStayCallbacks.clear();
    m_TriggerExitCallbacks.clear();

    DestroyAllBodies();
}

void Physics3DWorld::DestroyAllBodies() {
    if (m_DynamicsWorld) {
        // Constraints reference the bodies they join, so they must leave the world before the
        // bodies do. Nothing in the engine creates them today, but GetBulletWorld() is public,
        // so an extension can. They are not ours to delete - only to detach.
        for (int i = m_DynamicsWorld->getNumConstraints() - 1; i >= 0; i--) {
            m_DynamicsWorld->removeConstraint(m_DynamicsWorld->getConstraint(i));
        }
    }

    // ~RigidBody3D removes its own btRigidBody from the world and owns that body's motion
    // state, compound shape and collider back-references. Destroying the bodies through their
    // unique_ptrs is therefore the whole teardown - freeing any of it here as well would be a
    // double delete.
    m_Bodies.clear();
    m_ObjectToBody.clear();

    if (m_DynamicsWorld) {
        // Anything still registered was not created through CreateBody, so it is not ours
        // to free - just detach it.
        for (int i = m_DynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            m_DynamicsWorld->removeCollisionObject(m_DynamicsWorld->getCollisionObjectArray()[i]);
        }
    }
}

void Physics3DWorld::PurgeBodyFromContacts(const core::UUID& bodyId) {
    const uint64_t raw = static_cast<uint64_t>(bodyId);

    for (auto it = m_ActiveCollisions.begin(); it != m_ActiveCollisions.end();) {
        it = (it->first.first == raw || it->first.second == raw) ? m_ActiveCollisions.erase(it) : std::next(it);
    }
    for (auto it = m_ActiveTriggers.begin(); it != m_ActiveTriggers.end();) {
        it = (it->first.first == raw || it->first.second == raw) ? m_ActiveTriggers.erase(it) : std::next(it);
    }
}

Physics3DWorld::BodyPairKey Physics3DWorld::MakePairKey(const core::UUID& a, const core::UUID& b) {
    const uint64_t rawA = static_cast<uint64_t>(a);
    const uint64_t rawB = static_cast<uint64_t>(b);
    return rawA <= rawB ? BodyPairKey(rawA, rawB) : BodyPairKey(rawB, rawA);
}

RigidBody3D* Physics3DWorld::GetBodyForObject(const btCollisionObject* object) const {
    if (!object) {
        return nullptr;
    }
    auto indexIt = m_ObjectToBody.find(object);
    if (indexIt == m_ObjectToBody.end()) {
        return nullptr;
    }
    auto bodyIt = m_Bodies.find(indexIt->second);
    return bodyIt != m_Bodies.end() ? bodyIt->second.get() : nullptr;
}

const btCollisionObject* Physics3DWorld::GetBulletObject(const core::UUID* bodyId) const {
    if (!bodyId) {
        return nullptr;
    }
    auto it = m_Bodies.find(*bodyId);
    return it != m_Bodies.end() ? it->second->GetBulletBody() : nullptr;
}

void Physics3DWorld::SetBodyNode(const core::UUID& bodyId, core::Node* node) {
    if (node) {
        m_BodyNodes[bodyId] = node;
    } else {
        m_BodyNodes.erase(bodyId);
    }
}

core::Node* Physics3DWorld::GetBodyNode(const core::UUID& bodyId) const {
    auto it = m_BodyNodes.find(bodyId);
    return it != m_BodyNodes.end() ? it->second : nullptr;
}

void Physics3DWorld::ClearBodyNode(const core::UUID& bodyId) {
    m_BodyNodes.erase(bodyId);
}

core::UUID Physics3DWorld::FindBodyForNode(const core::Node* node) const {
    if (!node) return core::UUID(0);
    for (const auto& pair : m_BodyNodes) {
        if (pair.second == node) {
            return pair.first;
        }
    }
    return core::UUID(0);
}

void Physics3DWorld::RegisterCollisionEnter(const core::UUID& bodyId, CollisionCallback callback) {
    m_CollisionEnterCallbacks[bodyId] = callback;
}

void Physics3DWorld::RegisterCollisionStay(const core::UUID& bodyId, CollisionCallback callback) {
    m_CollisionStayCallbacks[bodyId] = callback;
}

void Physics3DWorld::RegisterCollisionExit(const core::UUID& bodyId, CollisionCallback callback) {
    m_CollisionExitCallbacks[bodyId] = callback;
}

void Physics3DWorld::RegisterTriggerEnter(const core::UUID& bodyId, CollisionCallback callback) {
    m_TriggerEnterCallbacks[bodyId] = callback;
}

void Physics3DWorld::RegisterTriggerStay(const core::UUID& bodyId, CollisionCallback callback) {
    m_TriggerStayCallbacks[bodyId] = callback;
}

void Physics3DWorld::RegisterTriggerExit(const core::UUID& bodyId, CollisionCallback callback) {
    m_TriggerExitCallbacks[bodyId] = callback;
}

void Physics3DWorld::ProcessCollisionEvents() {

    std::map<BodyPairKey, CollisionInfo3D> currentCollisions;
    std::map<BodyPairKey, CollisionInfo3D> currentTriggers;
    std::vector<PendingEvent> events;

    const int numManifolds = m_Dispatcher->getNumManifolds();

    for (int i = 0; i < numManifolds; i++) {
        btPersistentManifold* contactManifold = m_Dispatcher->getManifoldByIndexInternal(i);
        if (contactManifold->getNumContacts() <= 0) {
            continue;
        }

        const btCollisionObject* obA = contactManifold->getBody0();
        const btCollisionObject* obB = contactManifold->getBody1();

        RigidBody3D* bodyA = GetBodyForObject(obA);
        RigidBody3D* bodyB = GetBodyForObject(obB);
        if (!bodyA || !bodyB) {
            continue;
        }

        // Sensors keep generating manifolds even though the solver applies no response,
        // so a pair where either side is a sensor is a trigger overlap rather than a collision.
        const bool isTrigger =
            ((obA->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE) != 0) ||
            ((obB->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE) != 0);

        const btManifoldPoint& pt = contactManifold->getContactPoint(0);
        const btVector3 ptA = pt.getPositionWorldOnA();
        const btVector3 ptB = pt.getPositionWorldOnB();
        const btVector3 normalOnB = pt.m_normalWorldOnB;

        CollisionInfo3D info;
        info.bodyA = bodyA->GetId();
        info.bodyB = bodyB->GetId();
        info.point = math::Vec3((ptA.x() + ptB.x()) * 0.5f, (ptA.y() + ptB.y()) * 0.5f, (ptA.z() + ptB.z()) * 0.5f);
        info.normal = math::Vec3(normalOnB.x(), normalOnB.y(), normalOnB.z());
        info.penetrationDepth = pt.getDistance();

        const BodyPairKey key = MakePairKey(info.bodyA, info.bodyB);
        auto& current = isTrigger ? currentTriggers : currentCollisions;
        const auto& active = isTrigger ? m_ActiveTriggers : m_ActiveCollisions;

        // A body pair can produce several manifolds (one per shape pair on a multi-collider
        // body). The first one seen this step represents the pair.
        if (!current.emplace(key, info).second) {
            continue;
        }

        const ContactPhase phase = (active.find(key) == active.end()) ? ContactPhase::Enter : ContactPhase::Stay;
        events.push_back(PendingEvent{ info.bodyA, info, phase, isTrigger });
        events.push_back(PendingEvent{ info.bodyB, info, phase, isTrigger });
    }

    // Pairs that were touching last step but produced no manifold this step have separated.
    for (const auto& entry : m_ActiveCollisions) {
        if (currentCollisions.find(entry.first) == currentCollisions.end()) {
            events.push_back(PendingEvent{ entry.second.bodyA, entry.second, ContactPhase::Exit, false });
            events.push_back(PendingEvent{ entry.second.bodyB, entry.second, ContactPhase::Exit, false });
        }
    }
    for (const auto& entry : m_ActiveTriggers) {
        if (currentTriggers.find(entry.first) == currentTriggers.end()) {
            events.push_back(PendingEvent{ entry.second.bodyA, entry.second, ContactPhase::Exit, true });
            events.push_back(PendingEvent{ entry.second.bodyB, entry.second, ContactPhase::Exit, true });
        }
    }

    // Commit the new contact state before dispatching, so a callback that destroys a body
    // (and purges its contacts) cannot mutate a container we are still iterating.
    m_ActiveCollisions = std::move(currentCollisions);
    m_ActiveTriggers = std::move(currentTriggers);

    DispatchEvents(events);
}

void Physics3DWorld::DispatchEvents(const std::vector<PendingEvent>& events) {
    for (const PendingEvent& event : events) {
        // An earlier callback in this batch may have destroyed the body this event targets.
        if (m_Bodies.find(event.target) == m_Bodies.end()) {
            continue;
        }

        const std::unordered_map<core::UUID, CollisionCallback>* callbacks = nullptr;
        if (event.isTrigger) {
            switch (event.phase) {
                case ContactPhase::Enter: callbacks = &m_TriggerEnterCallbacks; break;
                case ContactPhase::Stay:  callbacks = &m_TriggerStayCallbacks;  break;
                case ContactPhase::Exit:  callbacks = &m_TriggerExitCallbacks;  break;
            }
        } else {
            switch (event.phase) {
                case ContactPhase::Enter: callbacks = &m_CollisionEnterCallbacks; break;
                case ContactPhase::Stay:  callbacks = &m_CollisionStayCallbacks;  break;
                case ContactPhase::Exit:  callbacks = &m_CollisionExitCallbacks;  break;
            }
        }

        auto it = callbacks->find(event.target);
        if (it == callbacks->end() || !it->second) {
            continue;
        }

        // Copy the callback: invoking it may unregister (and thereby erase) itself.
        CollisionCallback callback = it->second;
        callback(event.info);
    }
}

namespace {

// Bullet's default needsCollision() gates a query proxy against the callback's own
// group/mask. The engine's bodies advertise the same bitmask as both their group and their
// mask ("bodies interact if they share any layer"), so a query mirrors that: it sets both
// fields to the requested layer mask.
void ApplyQueryFilter(btCollisionWorld::RayResultCallback& callback, uint32_t layerMask) {
    callback.m_collisionFilterGroup = static_cast<int>(layerMask);
    callback.m_collisionFilterMask = static_cast<int>(layerMask);
}

void ApplyQueryFilter(btCollisionWorld::ContactResultCallback& callback, uint32_t layerMask) {
    callback.m_collisionFilterGroup = static_cast<int>(layerMask);
    callback.m_collisionFilterMask = static_cast<int>(layerMask);
}

void ApplyQueryFilter(btCollisionWorld::ConvexResultCallback& callback, uint32_t layerMask) {
    callback.m_collisionFilterGroup = static_cast<int>(layerMask);
    callback.m_collisionFilterMask = static_cast<int>(layerMask);
}

bool ProxyIsIgnored(const btBroadphaseProxy* proxy, const btCollisionObject* ignoreObject) {
    return ignoreObject != nullptr && proxy != nullptr && proxy->m_clientObject == ignoreObject;
}

} // namespace

bool Physics3DWorld::Raycast(const math::Vec3& origin, const math::Vec3& direction, float maxDistance, RaycastHit3D& hit, const core::UUID* ignoreBody, uint32_t layerMask) {
    hit.hit = false;

    const math::Vec3 normalizedDir = direction.Normalized();
    const math::Vec3 endPoint = origin + normalizedDir * maxDistance;

    const btVector3 from(origin.x, origin.y, origin.z);
    const btVector3 to(endPoint.x, endPoint.y, endPoint.z);

    struct FilteredRayCallback : public btCollisionWorld::ClosestRayResultCallback {
        const btCollisionObject* ignoreObject;

        FilteredRayCallback(const btVector3& rayFrom, const btVector3& rayTo, const btCollisionObject* ignore)
            : btCollisionWorld::ClosestRayResultCallback(rayFrom, rayTo)
            , ignoreObject(ignore) {}

        // Rejecting the ignored body here (rather than after the fact) lets the traversal
        // keep looking for whatever lies behind it, instead of reporting a miss.
        bool needsCollision(btBroadphaseProxy* proxy0) const override {
            if (ProxyIsIgnored(proxy0, ignoreObject)) {
                return false;
            }
            return btCollisionWorld::ClosestRayResultCallback::needsCollision(proxy0);
        }
    };

    FilteredRayCallback rayCallback(from, to, GetBulletObject(ignoreBody));
    ApplyQueryFilter(rayCallback, layerMask);
    m_DynamicsWorld->rayTest(from, to, rayCallback);

    if (!rayCallback.hasHit()) {
        return false;
    }

    RigidBody3D* body = GetBodyForObject(rayCallback.m_collisionObject);
    if (!body) {
        return false;
    }

    hit.bodyId = body->GetId();
    hit.point = math::Vec3(rayCallback.m_hitPointWorld.x(), rayCallback.m_hitPointWorld.y(), rayCallback.m_hitPointWorld.z());
    hit.normal = math::Vec3(rayCallback.m_hitNormalWorld.x(), rayCallback.m_hitNormalWorld.y(), rayCallback.m_hitNormalWorld.z());
    hit.fraction = rayCallback.m_closestHitFraction;
    hit.hit = true;
    return true;
}

std::vector<RaycastHit3D> Physics3DWorld::RaycastAll(const math::Vec3& origin, const math::Vec3& direction, float maxDistance, const core::UUID* ignoreBody, uint32_t layerMask) {
    std::vector<RaycastHit3D> hits;

    const math::Vec3 normalizedDir = direction.Normalized();
    const math::Vec3 endPoint = origin + normalizedDir * maxDistance;

    const btVector3 from(origin.x, origin.y, origin.z);
    const btVector3 to(endPoint.x, endPoint.y, endPoint.z);

    struct FilteredAllHitsCallback : public btCollisionWorld::AllHitsRayResultCallback {
        const btCollisionObject* ignoreObject;

        FilteredAllHitsCallback(const btVector3& rayFrom, const btVector3& rayTo, const btCollisionObject* ignore)
            : btCollisionWorld::AllHitsRayResultCallback(rayFrom, rayTo)
            , ignoreObject(ignore) {}

        bool needsCollision(btBroadphaseProxy* proxy0) const override {
            if (ProxyIsIgnored(proxy0, ignoreObject)) {
                return false;
            }
            return btCollisionWorld::AllHitsRayResultCallback::needsCollision(proxy0);
        }
    };

    FilteredAllHitsCallback rayCallback(from, to, GetBulletObject(ignoreBody));
    ApplyQueryFilter(rayCallback, layerMask);
    m_DynamicsWorld->rayTest(from, to, rayCallback);

    hits.reserve(static_cast<size_t>(rayCallback.m_collisionObjects.size()));

    for (int i = 0; i < rayCallback.m_collisionObjects.size(); i++) {
        RigidBody3D* body = GetBodyForObject(rayCallback.m_collisionObjects[i]);
        if (!body) {
            continue;
        }

        RaycastHit3D hit;
        hit.bodyId = body->GetId();
        hit.point = math::Vec3(rayCallback.m_hitPointWorld[i].x(), rayCallback.m_hitPointWorld[i].y(), rayCallback.m_hitPointWorld[i].z());
        hit.normal = math::Vec3(rayCallback.m_hitNormalWorld[i].x(), rayCallback.m_hitNormalWorld[i].y(), rayCallback.m_hitNormalWorld[i].z());
        hit.fraction = rayCallback.m_hitFractions[i];
        hit.hit = true;
        hits.push_back(hit);
    }

    std::sort(hits.begin(), hits.end(), [](const RaycastHit3D& a, const RaycastHit3D& b) {
        return a.fraction < b.fraction;
    });

    return hits;
}

namespace {

// Shared by the overlap queries: records each overlapping body once, no matter how many
// contact points the pair generates, and skips the ignored body.
struct OverlapCollector : public btCollisionWorld::ContactResultCallback {
    Physics3DWorld* world;
    std::vector<OverlapResult3D>* results;
    std::unordered_set<uint64_t>* seen;
    const btCollisionObject* ignoreObject;

    bool needsCollision(btBroadphaseProxy* proxy0) const override {
        if (ProxyIsIgnored(proxy0, ignoreObject)) {
            return false;
        }
        return btCollisionWorld::ContactResultCallback::needsCollision(proxy0);
    }

    btScalar addSingleResult(btManifoldPoint&,
        const btCollisionObjectWrapper*, int, int,
        const btCollisionObjectWrapper* colObj1Wrap, int, int) override {

        RigidBody3D* body = world->GetBodyForObject(colObj1Wrap->getCollisionObject());
        if (body && seen->insert(static_cast<uint64_t>(body->GetId())).second) {
            OverlapResult3D result;
            result.bodyId = body->GetId();
            results->push_back(result);
        }
        return 0;
    }
};

} // namespace

std::vector<OverlapResult3D> Physics3DWorld::OverlapSphere(const math::Vec3& center, float radius, const core::UUID* ignoreBody, uint32_t layerMask) {
    std::vector<OverlapResult3D> results;
    std::unordered_set<uint64_t> seen;

    btSphereShape sphereShape(radius);
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(center.x, center.y, center.z));

    btCollisionObject testObject;
    testObject.setCollisionShape(&sphereShape);
    testObject.setWorldTransform(transform);

    OverlapCollector callback;
    callback.world = this;
    callback.results = &results;
    callback.seen = &seen;
    callback.ignoreObject = GetBulletObject(ignoreBody);
    ApplyQueryFilter(callback, layerMask);

    m_DynamicsWorld->contactTest(&testObject, callback);

    return results;
}

std::vector<OverlapResult3D> Physics3DWorld::OverlapBox(const math::Vec3& center, const math::Vec3& halfExtents, const math::Quat& rotation, const core::UUID* ignoreBody, uint32_t layerMask) {
    std::vector<OverlapResult3D> results;
    std::unordered_set<uint64_t> seen;

    btBoxShape boxShape(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(center.x, center.y, center.z));
    transform.setRotation(btQuaternion(rotation.x(), rotation.y(), rotation.z(), rotation.w()));

    btCollisionObject testObject;
    testObject.setCollisionShape(&boxShape);
    testObject.setWorldTransform(transform);

    OverlapCollector callback;
    callback.world = this;
    callback.results = &results;
    callback.seen = &seen;
    callback.ignoreObject = GetBulletObject(ignoreBody);
    ApplyQueryFilter(callback, layerMask);

    m_DynamicsWorld->contactTest(&testObject, callback);

    return results;
}

bool Physics3DWorld::SphereCast(const math::Vec3& start, const math::Vec3& end, float radius, ShapeCastHit3D& hit, const core::UUID* ignoreBody, uint32_t layerMask) {
    hit.hit = false;

    btSphereShape sphereShape(radius);

    btTransform fromTransform;
    fromTransform.setIdentity();
    fromTransform.setOrigin(btVector3(start.x, start.y, start.z));

    btTransform toTransform;
    toTransform.setIdentity();
    toTransform.setOrigin(btVector3(end.x, end.y, end.z));

    struct FilteredConvexResultCallback : public btCollisionWorld::ClosestConvexResultCallback {
        const btCollisionObject* ignoreObject;

        FilteredConvexResultCallback(const btVector3& from, const btVector3& to, const btCollisionObject* ignore)
            : btCollisionWorld::ClosestConvexResultCallback(from, to)
            , ignoreObject(ignore) {}

        bool needsCollision(btBroadphaseProxy* proxy0) const override {
            if (ProxyIsIgnored(proxy0, ignoreObject)) {
                return false;
            }
            return btCollisionWorld::ClosestConvexResultCallback::needsCollision(proxy0);
        }
    };

    FilteredConvexResultCallback callback(fromTransform.getOrigin(), toTransform.getOrigin(), GetBulletObject(ignoreBody));
    ApplyQueryFilter(callback, layerMask);
    m_DynamicsWorld->convexSweepTest(&sphereShape, fromTransform, toTransform, callback);

    if (!callback.hasHit()) {
        return false;
    }

    RigidBody3D* body = GetBodyForObject(callback.m_hitCollisionObject);
    if (!body) {
        return false;
    }

    hit.bodyId = body->GetId();
    hit.point = math::Vec3(callback.m_hitPointWorld.x(), callback.m_hitPointWorld.y(), callback.m_hitPointWorld.z());
    hit.normal = math::Vec3(callback.m_hitNormalWorld.x(), callback.m_hitNormalWorld.y(), callback.m_hitNormalWorld.z());
    hit.fraction = callback.m_closestHitFraction;
    hit.hit = true;
    return true;
}

}
}

