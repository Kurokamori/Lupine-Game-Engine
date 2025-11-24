#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include "lupine/physics2d/Collider2D.hpp"
#include "lupine/logger/Logger.hpp"
#include <box2d/box2d.h>

namespace lupine {
namespace physics2d {

Physics2DWorld::Physics2DWorld()
    : m_TimeStep(1.0f / 60.0f)
    , m_VelocityIterations(8)
    , m_PositionIterations(3)
{

    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, -9.81f};
    m_WorldId = b2CreateWorld(&worldDef);

}

Physics2DWorld::~Physics2DWorld() {

    m_Bodies.clear();

    if (B2_IS_NON_NULL(m_WorldId)) {
        b2DestroyWorld(m_WorldId);
    }

}

void Physics2DWorld::SetGravity(const math::Vec2& gravity) {
    b2World_SetGravity(m_WorldId, {gravity.x, gravity.y});
}

math::Vec2 Physics2DWorld::GetGravity() const {
    b2Vec2 gravity = b2World_GetGravity(m_WorldId);
    return math::Vec2(gravity.x, gravity.y);
}

void Physics2DWorld::SetTimeStep(float timeStep) {
    m_TimeStep = timeStep;
}

void Physics2DWorld::SetVelocityIterations(int iterations) {
    m_VelocityIterations = iterations;
}

void Physics2DWorld::SetPositionIterations(int iterations) {
    m_PositionIterations = iterations;
}

void Physics2DWorld::Step(float deltaTime) {

    b2World_Step(m_WorldId, deltaTime, m_VelocityIterations);

    ProcessCollisionEvents();
}

void Physics2DWorld::Step() {
    Step(m_TimeStep);
}

RigidBody2D* Physics2DWorld::CreateBody(core::UUID& outId, BodyType type) {
    outId = core::UUID();
    auto body = std::make_unique<RigidBody2D>(this, outId, type);
    RigidBody2D* bodyPtr = body.get();
    m_Bodies[outId] = std::move(body);

    return bodyPtr;
}

RigidBody2D* Physics2DWorld::GetBody(const core::UUID& id) {
    auto it = m_Bodies.find(id);
    if (it != m_Bodies.end()) {
        return it->second.get();
    }
    return nullptr;
}

void Physics2DWorld::DestroyBody(const core::UUID& id) {
    auto it = m_Bodies.find(id);
    if (it != m_Bodies.end()) {
        m_Bodies.erase(it);

    }
}

void Physics2DWorld::RegisterCollisionEnter(const core::UUID& bodyId, CollisionCallback callback) {
    m_CollisionEnterCallbacks[bodyId] = callback;
}

void Physics2DWorld::RegisterCollisionStay(const core::UUID& bodyId, CollisionCallback callback) {
    m_CollisionStayCallbacks[bodyId] = callback;
}

void Physics2DWorld::RegisterCollisionExit(const core::UUID& bodyId, CollisionCallback callback) {
    m_CollisionExitCallbacks[bodyId] = callback;
}

void Physics2DWorld::RegisterTriggerEnter(const core::UUID& bodyId, CollisionCallback callback) {
    m_TriggerEnterCallbacks[bodyId] = callback;
}

void Physics2DWorld::RegisterTriggerStay(const core::UUID& bodyId, CollisionCallback callback) {
    m_TriggerStayCallbacks[bodyId] = callback;
}

void Physics2DWorld::RegisterTriggerExit(const core::UUID& bodyId, CollisionCallback callback) {
    m_TriggerExitCallbacks[bodyId] = callback;
}

void Physics2DWorld::ProcessCollisionEvents() {

    b2ContactEvents events = b2World_GetContactEvents(m_WorldId);

    std::unordered_map<uint64_t, bool> currentContacts;

    for (int i = 0; i < events.beginCount; ++i) {
        b2ContactBeginTouchEvent& event = events.beginEvents[i];

        b2ShapeId shapeA = event.shapeIdA;
        b2ShapeId shapeB = event.shapeIdB;

        b2BodyId bodyIdA = b2Shape_GetBody(shapeA);
        b2BodyId bodyIdB = b2Shape_GetBody(shapeB);

        RigidBody2D* bodyA = nullptr;
        RigidBody2D* bodyB = nullptr;

        for (auto& pair : m_Bodies) {
            if (B2_ID_EQUALS(pair.second->GetBox2DBody(), bodyIdA)) {
                bodyA = pair.second.get();
            }
            if (B2_ID_EQUALS(pair.second->GetBox2DBody(), bodyIdB)) {
                bodyB = pair.second.get();
            }
        }

        if (!bodyA || !bodyB) continue;

        bool isSensor = b2Shape_IsSensor(shapeA) || b2Shape_IsSensor(shapeB);
        uint64_t hash = GetContactHash(bodyA->GetId(), bodyB->GetId());
        currentContacts[hash] = isSensor;

        CollisionInfo info;
        info.bodyA = bodyA->GetId();
        info.bodyB = bodyB->GetId();
        info.point = math::Vec2(0.0f, 0.0f);
        info.normal = math::Vec2(0.0f, 0.0f);
        info.normalImpulse = 0.0f;
        info.tangentImpulse = 0.0f;
        info.isSensor = isSensor;

        if (isSensor) {
            auto itA = m_TriggerEnterCallbacks.find(bodyA->GetId());
            if (itA != m_TriggerEnterCallbacks.end()) {
                itA->second(info);
            }
            auto itB = m_TriggerEnterCallbacks.find(bodyB->GetId());
            if (itB != m_TriggerEnterCallbacks.end()) {
                itB->second(info);
            }
        } else {
            auto itA = m_CollisionEnterCallbacks.find(bodyA->GetId());
            if (itA != m_CollisionEnterCallbacks.end()) {
                itA->second(info);
            }
            auto itB = m_CollisionEnterCallbacks.find(bodyB->GetId());
            if (itB != m_CollisionEnterCallbacks.end()) {
                itB->second(info);
            }
        }
    }

    for (int i = 0; i < events.endCount; ++i) {
        b2ContactEndTouchEvent& event = events.endEvents[i];

        b2ShapeId shapeA = event.shapeIdA;
        b2ShapeId shapeB = event.shapeIdB;

        b2BodyId bodyIdA = b2Shape_GetBody(shapeA);
        b2BodyId bodyIdB = b2Shape_GetBody(shapeB);

        RigidBody2D* bodyA = nullptr;
        RigidBody2D* bodyB = nullptr;

        for (auto& pair : m_Bodies) {
            if (B2_ID_EQUALS(pair.second->GetBox2DBody(), bodyIdA)) {
                bodyA = pair.second.get();
            }
            if (B2_ID_EQUALS(pair.second->GetBox2DBody(), bodyIdB)) {
                bodyB = pair.second.get();
            }
        }

        if (!bodyA || !bodyB) continue;

        uint64_t hash = GetContactHash(bodyA->GetId(), bodyB->GetId());
        auto it = m_ActiveContacts.find(hash);
        if (it == m_ActiveContacts.end()) continue;

        bool isSensor = it->second;

        CollisionInfo info;
        info.bodyA = bodyA->GetId();
        info.bodyB = bodyB->GetId();
        info.point = math::Vec2(0.0f, 0.0f);
        info.normal = math::Vec2(0.0f, 0.0f);
        info.normalImpulse = 0.0f;
        info.tangentImpulse = 0.0f;
        info.isSensor = isSensor;

        if (isSensor) {
            auto itA = m_TriggerExitCallbacks.find(bodyA->GetId());
            if (itA != m_TriggerExitCallbacks.end()) {
                itA->second(info);
            }
            auto itB = m_TriggerExitCallbacks.find(bodyB->GetId());
            if (itB != m_TriggerExitCallbacks.end()) {
                itB->second(info);
            }
        } else {
            auto itA = m_CollisionExitCallbacks.find(bodyA->GetId());
            if (itA != m_CollisionExitCallbacks.end()) {
                itA->second(info);
            }
            auto itB = m_CollisionExitCallbacks.find(bodyB->GetId());
            if (itB != m_CollisionExitCallbacks.end()) {
                itB->second(info);
            }
        }
    }

    for (const auto& pair : currentContacts) {
        if (m_ActiveContacts.find(pair.first) != m_ActiveContacts.end()) {

            for (auto& bodyPairA : m_Bodies) {
                for (auto& bodyPairB : m_Bodies) {
                    if (bodyPairA.first == bodyPairB.first) continue;

                    uint64_t hash = GetContactHash(bodyPairA.first, bodyPairB.first);
                    if (hash == pair.first) {
                        CollisionInfo info;
                        info.bodyA = bodyPairA.first;
                        info.bodyB = bodyPairB.first;
                        info.point = math::Vec2(0.0f, 0.0f);
                        info.normal = math::Vec2(0.0f, 0.0f);
                        info.normalImpulse = 0.0f;
                        info.tangentImpulse = 0.0f;
                        info.isSensor = pair.second;

                        if (pair.second) {
                            auto itA = m_TriggerStayCallbacks.find(bodyPairA.first);
                            if (itA != m_TriggerStayCallbacks.end()) {
                                itA->second(info);
                            }
                            auto itB = m_TriggerStayCallbacks.find(bodyPairB.first);
                            if (itB != m_TriggerStayCallbacks.end()) {
                                itB->second(info);
                            }
                        } else {
                            auto itA = m_CollisionStayCallbacks.find(bodyPairA.first);
                            if (itA != m_CollisionStayCallbacks.end()) {
                                itA->second(info);
                            }
                            auto itB = m_CollisionStayCallbacks.find(bodyPairB.first);
                            if (itB != m_CollisionStayCallbacks.end()) {
                                itB->second(info);
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    m_ActiveContacts = currentContacts;
}

bool Physics2DWorld::Raycast(const math::Vec2& origin, const math::Vec2& direction, float maxDistance, RaycastHit2D& hit, const core::UUID* ignoreBody) {
    math::Vec2 normalizedDir = direction.Normalized();
    math::Vec2 endPoint = origin + normalizedDir * maxDistance;

    b2Vec2 p1 = {origin.x, origin.y};
    b2Vec2 p2 = {endPoint.x, endPoint.y};

    struct RaycastContext {
        const core::UUID* ignoreBodyId;
        std::unordered_map<core::UUID, std::unique_ptr<RigidBody2D>>* bodies;
        RaycastHit2D* closestHit;
        float closestFraction;
    };

    RaycastContext context;
    context.ignoreBodyId = ignoreBody;
    context.bodies = &m_Bodies;
    context.closestHit = &hit;
    context.closestFraction = 1.0f;

    auto raycastCallback = [](b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* ctx) -> float {
        RaycastContext* context = static_cast<RaycastContext*>(ctx);

        b2BodyId hitBodyId = b2Shape_GetBody(shapeId);

        for (auto& pair : *context->bodies) {
            if (B2_ID_EQUALS(pair.second->GetBox2DBody(), hitBodyId)) {

                if (context->ignoreBodyId && pair.first == *context->ignoreBodyId) {
                    return 1.0f;
                }

                if (fraction < context->closestFraction) {
                    context->closestFraction = fraction;
                    context->closestHit->bodyId = pair.first;
                    context->closestHit->point = math::Vec2(point.x, point.y);
                    context->closestHit->normal = math::Vec2(normal.x, normal.y);
                    context->closestHit->fraction = fraction;
                    context->closestHit->hit = true;
                    return fraction;
                }
                break;
            }
        }

        return 1.0f;
    };

    hit.hit = false;
    b2World_CastRay(m_WorldId, p1, p2, b2DefaultQueryFilter(), raycastCallback, &context);

    static int debugCount = 0;
    debugCount++;
    if (debugCount % 60 == 0) {

        if (hit.hit) {

        }
    }

    return hit.hit;
}

std::vector<RaycastHit2D> Physics2DWorld::RaycastAll(const math::Vec2& origin, const math::Vec2& direction, float maxDistance, const core::UUID* ignoreBody) {
    std::vector<RaycastHit2D> hits;

    math::Vec2 normalizedDir = direction.Normalized();
    math::Vec2 endPoint = origin + normalizedDir * maxDistance;

    b2Vec2 p1 = {origin.x, origin.y};
    b2Vec2 p2 = {endPoint.x, endPoint.y};

    RaycastHit2D hit;
    if (Raycast(origin, direction, maxDistance, hit, ignoreBody)) {
        hits.push_back(hit);
    }

    return hits;
}

std::vector<OverlapResult2D> Physics2DWorld::OverlapPoint(const math::Vec2& point) {
    std::vector<OverlapResult2D> results;

    b2Vec2 p = {point.x, point.y};

    for (auto& pair : m_Bodies) {
        RigidBody2D* body = pair.second.get();
        for (Collider2D* collider : body->GetColliders()) {
            b2ShapeId shapeId = collider->GetBox2DShape();

            b2Vec2 localPoint = b2Body_GetLocalPoint(body->GetBox2DBody(), p);
            if (b2Shape_TestPoint(shapeId, localPoint)) {
                OverlapResult2D result;
                result.bodyId = pair.first;
                result.point = point;
                results.push_back(result);
                break;
            }
        }
    }

    return results;
}

std::vector<OverlapResult2D> Physics2DWorld::OverlapCircle(const math::Vec2& center, float radius) {
    std::vector<OverlapResult2D> results;

    b2Circle circle;
    circle.center = {center.x, center.y};
    circle.radius = radius;

    b2Transform transform = b2Transform_identity;

    for (auto& pair : m_Bodies) {
        RigidBody2D* body = pair.second.get();
        math::Vec2 bodyPos = body->GetPosition();

        float distance = (bodyPos - center).Length();
        if (distance < radius + 2.0f) {
            OverlapResult2D result;
            result.bodyId = pair.first;
            result.point = bodyPos;
            results.push_back(result);
        }
    }

    return results;
}

std::vector<OverlapResult2D> Physics2DWorld::OverlapBox(const math::Vec2& center, const math::Vec2& size, float angle) {
    std::vector<OverlapResult2D> results;

    for (auto& pair : m_Bodies) {
        RigidBody2D* body = pair.second.get();
        math::Vec2 bodyPos = body->GetPosition();

        float dx = std::abs(bodyPos.x - center.x);
        float dy = std::abs(bodyPos.y - center.y);

        if (dx < size.x && dy < size.y) {
            OverlapResult2D result;
            result.bodyId = pair.first;
            result.point = bodyPos;
            results.push_back(result);
        }
    }

    return results;
}

bool Physics2DWorld::ShapeCast(const math::Vec2& start, const math::Vec2& end, float radius, ShapeCastHit2D& hit, const core::UUID* ignoreBody) {

    b2Vec2 center = {0.0f, 0.0f};
    b2ShapeProxy proxy = b2MakeProxy(&center, 1, radius);

    b2Vec2 translation = {end.x - start.x, end.y - start.y};

    struct CastContext {
        ShapeCastHit2D* hit;
        float closestFraction;
        math::Vec2 start;
        const core::UUID* ignoreBody;
        std::unordered_map<core::UUID, std::unique_ptr<RigidBody2D>>* bodies;
    };

    CastContext context;
    context.hit = &hit;
    context.closestFraction = 1.0f;
    context.start = start;
    context.ignoreBody = ignoreBody;
    context.bodies = const_cast<std::unordered_map<core::UUID, std::unique_ptr<RigidBody2D>>*>(&m_Bodies);

    auto castCallback = [](b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* ctx) -> float {
        CastContext* context = static_cast<CastContext*>(ctx);

        b2BodyId bodyId = b2Shape_GetBody(shapeId);
        for (auto& pair : *context->bodies) {
            if (pair.second->GetBox2DBody().index1 == bodyId.index1) {

                if (context->ignoreBody && pair.first == *context->ignoreBody) {
                    return 1.0f;
                }

                if (fraction < context->closestFraction) {
                    context->closestFraction = fraction;
                    context->hit->point = math::Vec2(point.x, point.y);
                    context->hit->normal = math::Vec2(normal.x, normal.y);
                    context->hit->fraction = fraction;
                    context->hit->bodyId = pair.first;
                    context->hit->hit = true;
                }
                break;
            }
        }

        return fraction;
    };

    hit.hit = false;

    b2ShapeProxy offsetProxy = b2MakeOffsetProxy(&center, 1, radius, {start.x, start.y}, b2Rot_identity);
    b2World_CastShape(m_WorldId, &offsetProxy, translation, b2DefaultQueryFilter(), castCallback, &context);

    return hit.hit;
}

uint64_t Physics2DWorld::GetContactHash(const core::UUID& a, const core::UUID& b) const {

    uint64_t hashA = std::hash<core::UUID>{}(a);
    uint64_t hashB = std::hash<core::UUID>{}(b);

    if (hashA < hashB) {
        return hashA ^ (hashB << 1);
    } else {
        return hashB ^ (hashA << 1);
    }
}

}
}

