#include "lupine/physics3d/Physics3DWorld.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/physics3d/Collider3D.hpp"
#include "lupine/logger/Logger.hpp"
#include <btBulletDynamicsCommon.h>

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

    if (m_DynamicsWorld) {
        int numObjects = m_DynamicsWorld->getNumCollisionObjects();
        for (int i = numObjects - 1; i >= 0; i--) {
            btCollisionObject* obj = m_DynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMotionState()) {
                delete body->getMotionState();
            }
            m_DynamicsWorld->removeCollisionObject(obj);
        }
    }

    m_Bodies.clear();

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
        m_Bodies.erase(it);

    }
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

    int numManifolds = m_Dispatcher->getNumManifolds();

    std::unordered_map<std::string, bool> currentCollisions;

    for (int i = 0; i < numManifolds; i++) {
        btPersistentManifold* contactManifold = m_Dispatcher->getManifoldByIndexInternal(i);
        const btCollisionObject* obA = contactManifold->getBody0();
        const btCollisionObject* obB = contactManifold->getBody1();

        int numContacts = contactManifold->getNumContacts();
        if (numContacts > 0) {

            RigidBody3D* bodyA = nullptr;
            RigidBody3D* bodyB = nullptr;

            for (auto& pair : m_Bodies) {
                if (pair.second->GetBulletBody() == obA) {
                    bodyA = pair.second.get();
                }
                if (pair.second->GetBulletBody() == obB) {
                    bodyB = pair.second.get();
                }
            }

            if (bodyA && bodyB) {

                std::string collisionKey = bodyA->GetId().ToString() + "_" + bodyB->GetId().ToString();
                currentCollisions[collisionKey] = true;

                btManifoldPoint& pt = contactManifold->getContactPoint(0);
                btVector3 ptA = pt.getPositionWorldOnA();
                btVector3 ptB = pt.getPositionWorldOnB();
                btVector3 normalOnB = pt.m_normalWorldOnB;

                CollisionInfo3D info;
                info.bodyA = bodyA->GetId();
                info.bodyB = bodyB->GetId();
                info.point = math::Vec3((ptA.x() + ptB.x()) * 0.5f, (ptA.y() + ptB.y()) * 0.5f, (ptA.z() + ptB.z()) * 0.5f);
                info.normal = math::Vec3(normalOnB.x(), normalOnB.y(), normalOnB.z());
                info.penetrationDepth = pt.getDistance();

                bool isNewCollision = m_ActiveCollisions.find(collisionKey) == m_ActiveCollisions.end();

                if (isNewCollision) {

                    auto itA = m_CollisionEnterCallbacks.find(bodyA->GetId());
                    if (itA != m_CollisionEnterCallbacks.end()) {
                        itA->second(info);
                    }
                    auto itB = m_CollisionEnterCallbacks.find(bodyB->GetId());
                    if (itB != m_CollisionEnterCallbacks.end()) {
                        itB->second(info);
                    }
                } else {

                    auto itA = m_CollisionStayCallbacks.find(bodyA->GetId());
                    if (itA != m_CollisionStayCallbacks.end()) {
                        itA->second(info);
                    }
                    auto itB = m_CollisionStayCallbacks.find(bodyB->GetId());
                    if (itB != m_CollisionStayCallbacks.end()) {
                        itB->second(info);
                    }
                }
            }
        }
    }

    for (auto& pair : m_ActiveCollisions) {
        if (currentCollisions.find(pair.first) == currentCollisions.end()) {

        }
    }

    m_ActiveCollisions = currentCollisions;
}

bool Physics3DWorld::Raycast(const math::Vec3& origin, const math::Vec3& direction, float maxDistance, RaycastHit3D& hit, const core::UUID* ignoreBody) {
    math::Vec3 normalizedDir = direction.Normalized();
    math::Vec3 endPoint = origin + normalizedDir * maxDistance;

    btVector3 from(origin.x, origin.y, origin.z);
    btVector3 to(endPoint.x, endPoint.y, endPoint.z);

    btCollisionWorld::ClosestRayResultCallback rayCallback(from, to);
    m_DynamicsWorld->rayTest(from, to, rayCallback);

    if (rayCallback.hasHit()) {

        const btCollisionObject* obj = rayCallback.m_collisionObject;
        for (auto& pair : m_Bodies) {
            if (pair.second->GetBulletBody() == obj) {

                if (ignoreBody && pair.first == *ignoreBody) {
                    hit.hit = false;
                    return false;
                }

                hit.bodyId = pair.first;
                hit.point = math::Vec3(rayCallback.m_hitPointWorld.x(), rayCallback.m_hitPointWorld.y(), rayCallback.m_hitPointWorld.z());
                hit.normal = math::Vec3(rayCallback.m_hitNormalWorld.x(), rayCallback.m_hitNormalWorld.y(), rayCallback.m_hitNormalWorld.z());
                hit.fraction = rayCallback.m_closestHitFraction;
                hit.hit = true;
                return true;
            }
        }
    }

    hit.hit = false;
    return false;
}

std::vector<RaycastHit3D> Physics3DWorld::RaycastAll(const math::Vec3& origin, const math::Vec3& direction, float maxDistance, const core::UUID* ignoreBody) {
    std::vector<RaycastHit3D> hits;

    math::Vec3 normalizedDir = direction.Normalized();
    math::Vec3 endPoint = origin + normalizedDir * maxDistance;

    btVector3 from(origin.x, origin.y, origin.z);
    btVector3 to(endPoint.x, endPoint.y, endPoint.z);

    btCollisionWorld::AllHitsRayResultCallback rayCallback(from, to);
    m_DynamicsWorld->rayTest(from, to, rayCallback);

    for (int i = 0; i < rayCallback.m_collisionObjects.size(); i++) {
        const btCollisionObject* obj = rayCallback.m_collisionObjects[i];
        for (auto& pair : m_Bodies) {
            if (pair.second->GetBulletBody() == obj) {

                if (ignoreBody && pair.first == *ignoreBody) {
                    continue;
                }

                RaycastHit3D hit;
                hit.bodyId = pair.first;
                hit.point = math::Vec3(rayCallback.m_hitPointWorld[i].x(), rayCallback.m_hitPointWorld[i].y(), rayCallback.m_hitPointWorld[i].z());
                hit.normal = math::Vec3(rayCallback.m_hitNormalWorld[i].x(), rayCallback.m_hitNormalWorld[i].y(), rayCallback.m_hitNormalWorld[i].z());
                hit.fraction = rayCallback.m_hitFractions[i];
                hit.hit = true;
                hits.push_back(hit);
                break;
            }
        }
    }

    return hits;
}

std::vector<OverlapResult3D> Physics3DWorld::OverlapSphere(const math::Vec3& center, float radius) {
    std::vector<OverlapResult3D> results;

    btSphereShape sphereShape(radius);
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(center.x, center.y, center.z));

    btCollisionObject testObject;
    testObject.setCollisionShape(&sphereShape);
    testObject.setWorldTransform(transform);

    struct OverlapCallback : public btCollisionWorld::ContactResultCallback {
        std::vector<OverlapResult3D>* results;
        std::unordered_map<core::UUID, std::unique_ptr<RigidBody3D>>* bodies;

        btScalar addSingleResult(btManifoldPoint& cp,
            const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
            const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1) override {

            const btCollisionObject* obj = colObj1Wrap->getCollisionObject();
            for (auto& pair : *bodies) {
                if (pair.second->GetBulletBody() == obj) {
                    OverlapResult3D result;
                    result.bodyId = pair.first;
                    results->push_back(result);
                    break;
                }
            }
            return 0;
        }
    };

    OverlapCallback callback;
    callback.results = &results;
    callback.bodies = &m_Bodies;

    m_DynamicsWorld->contactTest(&testObject, callback);

    return results;
}

std::vector<OverlapResult3D> Physics3DWorld::OverlapBox(const math::Vec3& center, const math::Vec3& halfExtents, const math::Quat& rotation) {
    std::vector<OverlapResult3D> results;

    btBoxShape boxShape(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(center.x, center.y, center.z));
    transform.setRotation(btQuaternion(rotation.x(), rotation.y(), rotation.z(), rotation.w()));

    btCollisionObject testObject;
    testObject.setCollisionShape(&boxShape);
    testObject.setWorldTransform(transform);

    struct OverlapCallback : public btCollisionWorld::ContactResultCallback {
        std::vector<OverlapResult3D>* results;
        std::unordered_map<core::UUID, std::unique_ptr<RigidBody3D>>* bodies;

        btScalar addSingleResult(btManifoldPoint& cp,
            const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
            const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1) override {

            const btCollisionObject* obj = colObj1Wrap->getCollisionObject();
            for (auto& pair : *bodies) {
                if (pair.second->GetBulletBody() == obj) {
                    OverlapResult3D result;
                    result.bodyId = pair.first;
                    results->push_back(result);
                    break;
                }
            }
            return 0;
        }
    };

    OverlapCallback callback;
    callback.results = &results;
    callback.bodies = &m_Bodies;

    m_DynamicsWorld->contactTest(&testObject, callback);

    return results;
}

bool Physics3DWorld::SphereCast(const math::Vec3& start, const math::Vec3& end, float radius, ShapeCastHit3D& hit, const core::UUID* ignoreBody) {
    btSphereShape sphereShape(radius);

    btTransform fromTransform;
    fromTransform.setIdentity();
    fromTransform.setOrigin(btVector3(start.x, start.y, start.z));

    btTransform toTransform;
    toTransform.setIdentity();
    toTransform.setOrigin(btVector3(end.x, end.y, end.z));

    struct FilteredConvexResultCallback : public btCollisionWorld::ClosestConvexResultCallback {
        const core::UUID* ignoreBody;
        std::unordered_map<core::UUID, std::unique_ptr<RigidBody3D>>* bodies;

        FilteredConvexResultCallback(const btVector3& from, const btVector3& to,
                                     const core::UUID* ignore,
                                     std::unordered_map<core::UUID, std::unique_ptr<RigidBody3D>>* bodiesMap)
            : btCollisionWorld::ClosestConvexResultCallback(from, to)
            , ignoreBody(ignore)
            , bodies(bodiesMap) {}

        btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool normalInWorldSpace) override {

            if (ignoreBody) {
                for (auto& pair : *bodies) {
                    if (pair.second->GetBulletBody() == convexResult.m_hitCollisionObject) {
                        if (pair.first == *ignoreBody) {
                            return 1.0f;
                        }
                        break;
                    }
                }
            }

            return btCollisionWorld::ClosestConvexResultCallback::addSingleResult(convexResult, normalInWorldSpace);
        }
    };

    FilteredConvexResultCallback callback(fromTransform.getOrigin(), toTransform.getOrigin(), ignoreBody, &m_Bodies);
    m_DynamicsWorld->convexSweepTest(&sphereShape, fromTransform, toTransform, callback);

    if (callback.hasHit()) {
        const btCollisionObject* obj = callback.m_hitCollisionObject;
        for (auto& pair : m_Bodies) {
            if (pair.second->GetBulletBody() == obj) {
                hit.bodyId = pair.first;
                hit.point = math::Vec3(callback.m_hitPointWorld.x(), callback.m_hitPointWorld.y(), callback.m_hitPointWorld.z());
                hit.normal = math::Vec3(callback.m_hitNormalWorld.x(), callback.m_hitNormalWorld.y(), callback.m_hitNormalWorld.z());
                hit.fraction = callback.m_closestHitFraction;
                hit.hit = true;
                return true;
            }
        }
    }

    hit.hit = false;
    return false;
}

}
}

