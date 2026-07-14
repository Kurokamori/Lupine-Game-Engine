#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include "lupine/physics2d/Collider2D.hpp"
#include "lupine/logger/Logger.hpp"
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

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
    // Drop any tracked contacts/overlaps first so the next step does not try to
    // resolve a destroyed Box2D shape/body through stale user data.
    PurgeBodyFromContacts(id);

    auto it = m_Bodies.find(id);
    if (it != m_Bodies.end()) {
        m_Bodies.erase(it);

    }
    m_BodyNodes.erase(id);

    // Each callback captures the component that registered it. Leaving them behind grows the
    // maps without bound across spawn/despawn cycles and keeps a pointer to a destroyed
    // component alive for a body id that could be reused.
    m_CollisionEnterCallbacks.erase(id);
    m_CollisionStayCallbacks.erase(id);
    m_CollisionExitCallbacks.erase(id);
    m_TriggerEnterCallbacks.erase(id);
    m_TriggerStayCallbacks.erase(id);
    m_TriggerExitCallbacks.erase(id);
}

void Physics2DWorld::Clear() {
    m_ActiveContacts.clear();
    m_ActiveSensors.clear();
    m_BodyNodes.clear();

    m_CollisionEnterCallbacks.clear();
    m_CollisionStayCallbacks.clear();
    m_CollisionExitCallbacks.clear();
    m_TriggerEnterCallbacks.clear();
    m_TriggerStayCallbacks.clear();
    m_TriggerExitCallbacks.clear();

    // Destroys every RigidBody2D (each frees its own b2 body). Colliders are owned by the
    // components that created them; by the time this runs the outgoing scene's components
    // are gone, so nothing still references them.
    m_Bodies.clear();
}

void Physics2DWorld::SetBodyNode(const core::UUID& bodyId, core::Node* node) {
    if (node) {
        m_BodyNodes[bodyId] = node;
    } else {
        m_BodyNodes.erase(bodyId);
    }
}

core::Node* Physics2DWorld::GetBodyNode(const core::UUID& bodyId) const {
    auto it = m_BodyNodes.find(bodyId);
    return it != m_BodyNodes.end() ? it->second : nullptr;
}

void Physics2DWorld::ClearBodyNode(const core::UUID& bodyId) {
    m_BodyNodes.erase(bodyId);
}

core::UUID Physics2DWorld::FindBodyForNode(const core::Node* node) const {
    if (!node) return core::UUID(0);
    for (const auto& pair : m_BodyNodes) {
        if (pair.second == node) {
            return pair.first;
        }
    }
    return core::UUID(0);
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

namespace {

// Pack a b2ShapeId into a comparable 64-bit value. Index, world and generation together
// identify a shape uniquely, including across id recycling.
uint64_t ShapeKey(b2ShapeId shape) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(shape.index1)) << 32) |
           (static_cast<uint64_t>(shape.world0) << 16) |
           static_cast<uint64_t>(shape.generation);
}

// Undirected key for a shape pair, stable regardless of which side Box2D calls A.
std::pair<uint64_t, uint64_t> MakeShapePairKey(b2ShapeId a, b2ShapeId b) {
    const uint64_t ka = ShapeKey(a);
    const uint64_t kb = ShapeKey(b);
    return (ka <= kb) ? std::make_pair(ka, kb) : std::make_pair(kb, ka);
}

// Directional key (sensor shape -> visitor shape) for sensor overlap tracking.
std::pair<uint64_t, uint64_t> MakeSensorShapeKey(b2ShapeId sensor, b2ShapeId visitor) {
    return std::make_pair(ShapeKey(sensor), ShapeKey(visitor));
}

} // anonymous namespace

RigidBody2D* Physics2DWorld::ResolveBody(b2BodyId bodyId) const {
    if (!B2_IS_NON_NULL(bodyId) || !b2Body_IsValid(bodyId)) {
        return nullptr;
    }
    return static_cast<RigidBody2D*>(b2Body_GetUserData(bodyId));
}

void Physics2DWorld::DispatchCollision(const std::unordered_map<core::UUID, CollisionCallback>& callbacks,
                                       const core::UUID& bodyId, const CollisionInfo& info) const {
    auto it = callbacks.find(bodyId);
    if (it != callbacks.end() && it->second) {
        it->second(info);
    }
}

void Physics2DWorld::FillSolidContactInfo(const SolidContact& contact, CollisionInfo& info) const {
    info.bodyA = contact.bodyA;
    info.bodyB = contact.bodyB;
    info.point = math::Vec2(0.0f, 0.0f);
    info.normal = math::Vec2(0.0f, 0.0f);
    info.normalImpulse = 0.0f;
    info.tangentImpulse = 0.0f;
    info.isSensor = false;

    // Query the live contact manifold for this shape pair to obtain the current
    // contact point, normal, and solver impulses.
    if (!B2_IS_NON_NULL(contact.shapeA) || !b2Shape_IsValid(contact.shapeA)) {
        return;
    }

    int capacity = b2Shape_GetContactCapacity(contact.shapeA);
    if (capacity <= 0) {
        return;
    }

    std::vector<b2ContactData> data(static_cast<size_t>(capacity));
    int count = b2Shape_GetContactData(contact.shapeA, data.data(), capacity);

    for (int i = 0; i < count; ++i) {
        const b2ContactData& cd = data[i];
        bool matches = (B2_ID_EQUALS(cd.shapeIdA, contact.shapeA) && B2_ID_EQUALS(cd.shapeIdB, contact.shapeB)) ||
                       (B2_ID_EQUALS(cd.shapeIdA, contact.shapeB) && B2_ID_EQUALS(cd.shapeIdB, contact.shapeA));
        if (!matches) {
            continue;
        }

        const b2Manifold& manifold = cd.manifold;
        if (manifold.pointCount <= 0) {
            break;
        }

        // The manifold normal points from shapeA to shapeB. Flip it when the
        // tracked contact orientation is reversed relative to this contact data
        // so the reported normal is consistent with (bodyA -> bodyB).
        math::Vec2 normal(manifold.normal.x, manifold.normal.y);
        if (B2_ID_EQUALS(cd.shapeIdA, contact.shapeB)) {
            normal = math::Vec2(-normal.x, -normal.y);
        }
        info.normal = normal;

        math::Vec2 averagePoint(0.0f, 0.0f);
        float totalNormal = 0.0f;
        float totalTangent = 0.0f;
        for (int p = 0; p < manifold.pointCount; ++p) {
            averagePoint.x += manifold.points[p].point.x;
            averagePoint.y += manifold.points[p].point.y;
            totalNormal += manifold.points[p].normalImpulse;
            totalTangent += manifold.points[p].tangentImpulse;
        }
        float inv = 1.0f / static_cast<float>(manifold.pointCount);
        info.point = math::Vec2(averagePoint.x * inv, averagePoint.y * inv);
        info.normalImpulse = totalNormal;
        info.tangentImpulse = totalTangent;
        break;
    }
}

void Physics2DWorld::PurgeBodyFromContacts(const core::UUID& bodyId) {
    for (auto it = m_ActiveContacts.begin(); it != m_ActiveContacts.end();) {
        if (it->second.bodyA == bodyId || it->second.bodyB == bodyId) {
            it = m_ActiveContacts.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_ActiveSensors.begin(); it != m_ActiveSensors.end();) {
        if (it->second.sensorBody == bodyId || it->second.visitorBody == bodyId) {
            it = m_ActiveSensors.erase(it);
        } else {
            ++it;
        }
    }
}

void Physics2DWorld::PurgeShapeFromContacts(b2ShapeId shape) {
    if (!B2_IS_NON_NULL(shape)) {
        return;
    }
    const uint64_t key = ShapeKey(shape);

    // Collect first, dispatch after: an exit callback may destroy a body, which purges more
    // entries from the very maps we would otherwise be iterating.
    std::vector<CollisionInfo> contactExits;
    for (auto it = m_ActiveContacts.begin(); it != m_ActiveContacts.end();) {
        if (ShapeKey(it->second.shapeA) == key || ShapeKey(it->second.shapeB) == key) {
            CollisionInfo info;
            info.bodyA = it->second.bodyA;
            info.bodyB = it->second.bodyB;
            info.point = math::Vec2(0.0f, 0.0f);
            info.normal = math::Vec2(0.0f, 0.0f);
            info.normalImpulse = 0.0f;
            info.tangentImpulse = 0.0f;
            info.isSensor = false;
            contactExits.push_back(info);
            it = m_ActiveContacts.erase(it);
        } else {
            ++it;
        }
    }

    std::vector<CollisionInfo> sensorExits;
    for (auto it = m_ActiveSensors.begin(); it != m_ActiveSensors.end();) {
        if (ShapeKey(it->second.sensorShape) == key || ShapeKey(it->second.visitorShape) == key) {
            CollisionInfo info;
            info.bodyA = it->second.sensorBody;
            info.bodyB = it->second.visitorBody;
            info.point = math::Vec2(0.0f, 0.0f);
            info.normal = math::Vec2(0.0f, 0.0f);
            info.normalImpulse = 0.0f;
            info.tangentImpulse = 0.0f;
            info.isSensor = true;
            sensorExits.push_back(info);
            it = m_ActiveSensors.erase(it);
        } else {
            ++it;
        }
    }

    for (const CollisionInfo& info : contactExits) {
        DispatchCollision(m_CollisionExitCallbacks, info.bodyA, info);
        DispatchCollision(m_CollisionExitCallbacks, info.bodyB, info);
    }
    for (const CollisionInfo& info : sensorExits) {
        DispatchCollision(m_TriggerExitCallbacks, info.bodyA, info);
        DispatchCollision(m_TriggerExitCallbacks, info.bodyB, info);
    }
}

void Physics2DWorld::ProcessCollisionEvents() {
    ProcessContactEvents();
    ProcessSensorEvents();
}

void Physics2DWorld::ProcessContactEvents() {
    b2ContactEvents events = b2World_GetContactEvents(m_WorldId);

    // Clear the per-step touch markers; begin/stay processing re-marks them.
    for (auto& entry : m_ActiveContacts) {
        entry.second.touchedThisStep = false;
    }

    // ---- Begin touch: new solid contacts ----
    for (int i = 0; i < events.beginCount; ++i) {
        const b2ContactBeginTouchEvent& event = events.beginEvents[i];

        // An earlier begin callback in this batch may have destroyed one of these shapes
        // (or its body), so re-check validity before touching the ids.
        if (!b2Shape_IsValid(event.shapeIdA) || !b2Shape_IsValid(event.shapeIdB)) {
            continue;
        }

        RigidBody2D* bodyA = ResolveBody(b2Shape_GetBody(event.shapeIdA));
        RigidBody2D* bodyB = ResolveBody(b2Shape_GetBody(event.shapeIdB));
        if (!bodyA || !bodyB) continue;

        SolidContact contact;
        contact.bodyA = bodyA->GetId();
        contact.bodyB = bodyB->GetId();
        contact.shapeA = event.shapeIdA;
        contact.shapeB = event.shapeIdB;
        contact.touchedThisStep = true;
        m_ActiveContacts[MakeShapePairKey(contact.shapeA, contact.shapeB)] = contact;

        CollisionInfo info;
        info.bodyA = contact.bodyA;
        info.bodyB = contact.bodyB;
        info.isSensor = false;
        info.normalImpulse = 0.0f;
        info.tangentImpulse = 0.0f;
        if (event.manifold.pointCount > 0) {
            info.point = math::Vec2(event.manifold.points[0].point.x, event.manifold.points[0].point.y);
        } else {
            info.point = math::Vec2(0.0f, 0.0f);
        }
        info.normal = math::Vec2(event.manifold.normal.x, event.manifold.normal.y);

        DispatchCollision(m_CollisionEnterCallbacks, contact.bodyA, info);
        DispatchCollision(m_CollisionEnterCallbacks, contact.bodyB, info);
    }

    // ---- End touch: solid contacts that separated ----
    for (int i = 0; i < events.endCount; ++i) {
        const b2ContactEndTouchEvent& event = events.endEvents[i];

        // End events can be generated by shape/body destruction; the contained
        // shape ids may no longer be valid. Purge-on-destroy already removed the
        // tracked contact in that case, so skip safely.
        if (!b2Shape_IsValid(event.shapeIdA) || !b2Shape_IsValid(event.shapeIdB)) {
            continue;
        }

        auto it = m_ActiveContacts.find(MakeShapePairKey(event.shapeIdA, event.shapeIdB));
        if (it == m_ActiveContacts.end()) continue;

        CollisionInfo info;
        info.bodyA = it->second.bodyA;
        info.bodyB = it->second.bodyB;
        info.point = math::Vec2(0.0f, 0.0f);
        info.normal = math::Vec2(0.0f, 0.0f);
        info.normalImpulse = 0.0f;
        info.tangentImpulse = 0.0f;
        info.isSensor = false;

        m_ActiveContacts.erase(it);

        DispatchCollision(m_CollisionExitCallbacks, info.bodyA, info);
        DispatchCollision(m_CollisionExitCallbacks, info.bodyB, info);
    }

    // ---- Stay: solid contacts that persist from a previous step ----
    // Snapshot before dispatching: a native Stay callback is allowed to destroy a body,
    // and PurgeBodyFromContacts would then erase from the map we are iterating.
    std::vector<SolidContact> staying;
    staying.reserve(m_ActiveContacts.size());
    for (const auto& entry : m_ActiveContacts) {
        if (!entry.second.touchedThisStep) {
            staying.push_back(entry.second);
        }
    }

    for (const SolidContact& contact : staying) {
        // A previous callback in this batch may have invalidated the shapes.
        if (!b2Shape_IsValid(contact.shapeA) || !b2Shape_IsValid(contact.shapeB)) {
            continue;
        }
        if (m_ActiveContacts.find(MakeShapePairKey(contact.shapeA, contact.shapeB)) == m_ActiveContacts.end()) {
            continue;
        }

        CollisionInfo info;
        FillSolidContactInfo(contact, info);

        DispatchCollision(m_CollisionStayCallbacks, contact.bodyA, info);
        DispatchCollision(m_CollisionStayCallbacks, contact.bodyB, info);
    }
}

void Physics2DWorld::ProcessSensorEvents() {
    b2SensorEvents events = b2World_GetSensorEvents(m_WorldId);

    for (auto& entry : m_ActiveSensors) {
        entry.second.touchedThisStep = false;
    }

    // ---- Begin: a visitor shape started overlapping a sensor shape ----
    for (int i = 0; i < events.beginCount; ++i) {
        const b2SensorBeginTouchEvent& event = events.beginEvents[i];

        // An earlier begin callback in this batch may have destroyed one of these shapes.
        if (!b2Shape_IsValid(event.sensorShapeId) || !b2Shape_IsValid(event.visitorShapeId)) {
            continue;
        }

        RigidBody2D* sensorBody = ResolveBody(b2Shape_GetBody(event.sensorShapeId));
        RigidBody2D* visitorBody = ResolveBody(b2Shape_GetBody(event.visitorShapeId));
        if (!sensorBody || !visitorBody) continue;

        SensorOverlap overlap;
        overlap.sensorBody = sensorBody->GetId();
        overlap.visitorBody = visitorBody->GetId();
        overlap.sensorShape = event.sensorShapeId;
        overlap.visitorShape = event.visitorShapeId;
        overlap.touchedThisStep = true;
        m_ActiveSensors[MakeSensorShapeKey(overlap.sensorShape, overlap.visitorShape)] = overlap;

        CollisionInfo info;
        info.bodyA = overlap.sensorBody;
        info.bodyB = overlap.visitorBody;
        info.point = math::Vec2(0.0f, 0.0f);
        info.normal = math::Vec2(0.0f, 0.0f);
        info.normalImpulse = 0.0f;
        info.tangentImpulse = 0.0f;
        info.isSensor = true;

        DispatchCollision(m_TriggerEnterCallbacks, overlap.sensorBody, info);
        DispatchCollision(m_TriggerEnterCallbacks, overlap.visitorBody, info);
    }

    // ---- End: a visitor shape stopped overlapping a sensor shape ----
    for (int i = 0; i < events.endCount; ++i) {
        const b2SensorEndTouchEvent& event = events.endEvents[i];

        // The sensor or visitor shape may already be destroyed; their ids are
        // not safe to dereference in that case. Purge-on-destroy already removed
        // the tracked overlap, so skip safely.
        if (!b2Shape_IsValid(event.sensorShapeId) || !b2Shape_IsValid(event.visitorShapeId)) {
            continue;
        }

        auto it = m_ActiveSensors.find(MakeSensorShapeKey(event.sensorShapeId, event.visitorShapeId));
        if (it == m_ActiveSensors.end()) continue;

        CollisionInfo info;
        info.bodyA = it->second.sensorBody;
        info.bodyB = it->second.visitorBody;
        info.point = math::Vec2(0.0f, 0.0f);
        info.normal = math::Vec2(0.0f, 0.0f);
        info.normalImpulse = 0.0f;
        info.tangentImpulse = 0.0f;
        info.isSensor = true;

        m_ActiveSensors.erase(it);

        DispatchCollision(m_TriggerExitCallbacks, info.bodyA, info);
        DispatchCollision(m_TriggerExitCallbacks, info.bodyB, info);
    }

    // ---- Stay: sensor overlaps that persist from a previous step ----
    // Snapshot before dispatching, for the same reason as the solid-contact Stay loop.
    std::vector<SensorOverlap> staying;
    staying.reserve(m_ActiveSensors.size());
    for (const auto& entry : m_ActiveSensors) {
        if (!entry.second.touchedThisStep) {
            staying.push_back(entry.second);
        }
    }

    for (const SensorOverlap& overlap : staying) {
        if (m_ActiveSensors.find(MakeSensorShapeKey(overlap.sensorShape, overlap.visitorShape)) == m_ActiveSensors.end()) {
            continue;
        }

        CollisionInfo info;
        info.bodyA = overlap.sensorBody;
        info.bodyB = overlap.visitorBody;
        info.point = math::Vec2(0.0f, 0.0f);
        info.normal = math::Vec2(0.0f, 0.0f);
        info.normalImpulse = 0.0f;
        info.tangentImpulse = 0.0f;
        info.isSensor = true;

        DispatchCollision(m_TriggerStayCallbacks, overlap.sensorBody, info);
        DispatchCollision(m_TriggerStayCallbacks, overlap.visitorBody, info);
    }
}

namespace {

// Build a query filter that selects shapes whose collision layer (categoryBits)
// intersects the supplied mask. The query advertises every category so that a
// shape's own mask never rejects the query.
b2QueryFilter MakeQueryFilter(uint64_t collisionMask) {
    b2QueryFilter filter;
    filter.categoryBits = 0xFFFFFFFFFFFFFFFFull;
    filter.maskBits = collisionMask;
    return filter;
}

} // anonymous namespace

bool Physics2DWorld::Raycast(const math::Vec2& origin, const math::Vec2& direction, float maxDistance, RaycastHit2D& hit, const core::UUID* ignoreBody, uint64_t collisionMask) {
    math::Vec2 normalizedDir = direction.Normalized();
    math::Vec2 endPoint = origin + normalizedDir * maxDistance;

    b2Vec2 p1 = {origin.x, origin.y};
    b2Vec2 translation = {endPoint.x - origin.x, endPoint.y - origin.y};

    struct RaycastContext {
        const Physics2DWorld* world;
        const core::UUID* ignoreBodyId;
        RaycastHit2D* closestHit;
        float closestFraction;
    };

    RaycastContext context;
    context.world = this;
    context.ignoreBodyId = ignoreBody;
    context.closestHit = &hit;
    context.closestFraction = 1.0f;

    auto raycastCallback = [](b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* ctx) -> float {
        RaycastContext* context = static_cast<RaycastContext*>(ctx);

        RigidBody2D* body = context->world->ResolveBody(b2Shape_GetBody(shapeId));
        if (!body) {
            return -1.0f;
        }
        if (context->ignoreBodyId && body->GetId() == *context->ignoreBodyId) {
            return -1.0f;
        }

        if (fraction <= context->closestFraction) {
            context->closestFraction = fraction;
            context->closestHit->bodyId = body->GetId();
            context->closestHit->point = math::Vec2(point.x, point.y);
            context->closestHit->normal = math::Vec2(normal.x, normal.y);
            context->closestHit->fraction = fraction;
            context->closestHit->hit = true;
        }
        return fraction;
    };

    hit.hit = false;
    hit.fraction = 1.0f;
    b2World_CastRay(m_WorldId, p1, translation, MakeQueryFilter(collisionMask), raycastCallback, &context);

    return hit.hit;
}

std::vector<RaycastHit2D> Physics2DWorld::RaycastAll(const math::Vec2& origin, const math::Vec2& direction, float maxDistance, const core::UUID* ignoreBody, uint64_t collisionMask) {
    math::Vec2 normalizedDir = direction.Normalized();
    math::Vec2 endPoint = origin + normalizedDir * maxDistance;

    b2Vec2 p1 = {origin.x, origin.y};
    b2Vec2 translation = {endPoint.x - origin.x, endPoint.y - origin.y};

    struct RaycastAllContext {
        const Physics2DWorld* world;
        const core::UUID* ignoreBodyId;
        std::map<uint64_t, RaycastHit2D>* hitsByBody;
    };

    std::map<uint64_t, RaycastHit2D> hitsByBody;
    RaycastAllContext context;
    context.world = this;
    context.ignoreBodyId = ignoreBody;
    context.hitsByBody = &hitsByBody;

    // Return 1.0 to continue the ray unclipped so every intersected shape is
    // reported. Keep the closest hit per body.
    auto raycastCallback = [](b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* ctx) -> float {
        RaycastAllContext* context = static_cast<RaycastAllContext*>(ctx);

        RigidBody2D* body = context->world->ResolveBody(b2Shape_GetBody(shapeId));
        if (!body) {
            return -1.0f;
        }
        if (context->ignoreBodyId && body->GetId() == *context->ignoreBodyId) {
            return -1.0f;
        }

        uint64_t key = static_cast<uint64_t>(body->GetId());
        auto it = context->hitsByBody->find(key);
        if (it == context->hitsByBody->end() || fraction < it->second.fraction) {
            RaycastHit2D hit;
            hit.hit = true;
            hit.bodyId = body->GetId();
            hit.point = math::Vec2(point.x, point.y);
            hit.normal = math::Vec2(normal.x, normal.y);
            hit.fraction = fraction;
            (*context->hitsByBody)[key] = hit;
        }
        return 1.0f;
    };

    b2World_CastRay(m_WorldId, p1, translation, MakeQueryFilter(collisionMask), raycastCallback, &context);

    std::vector<RaycastHit2D> hits;
    hits.reserve(hitsByBody.size());
    for (const auto& entry : hitsByBody) {
        hits.push_back(entry.second);
    }
    std::sort(hits.begin(), hits.end(), [](const RaycastHit2D& a, const RaycastHit2D& b) {
        return a.fraction < b.fraction;
    });

    return hits;
}

std::vector<OverlapResult2D> Physics2DWorld::OverlapPoint(const math::Vec2& point, uint64_t collisionMask) {
    std::vector<OverlapResult2D> results;

    struct OverlapContext {
        const Physics2DWorld* world;
        b2Vec2 point;
        std::vector<OverlapResult2D>* results;
        std::set<uint64_t>* seen;
    };

    std::set<uint64_t> seen;
    OverlapContext context;
    context.world = this;
    context.point = {point.x, point.y};
    context.results = &results;
    context.seen = &seen;

    auto overlapCallback = [](b2ShapeId shapeId, void* ctx) -> bool {
        OverlapContext* context = static_cast<OverlapContext*>(ctx);

        // The broad-phase AABB query is conservative; confirm true containment.
        if (!b2Shape_TestPoint(shapeId, context->point)) {
            return true;
        }

        RigidBody2D* body = context->world->ResolveBody(b2Shape_GetBody(shapeId));
        if (!body) {
            return true;
        }

        uint64_t key = static_cast<uint64_t>(body->GetId());
        if (context->seen->insert(key).second) {
            OverlapResult2D result;
            result.bodyId = body->GetId();
            result.point = math::Vec2(context->point.x, context->point.y);
            context->results->push_back(result);
        }
        return true;
    };

    const float kEpsilon = 0.01f;
    b2AABB aabb;
    aabb.lowerBound = {point.x - kEpsilon, point.y - kEpsilon};
    aabb.upperBound = {point.x + kEpsilon, point.y + kEpsilon};
    b2World_OverlapAABB(m_WorldId, aabb, MakeQueryFilter(collisionMask), overlapCallback, &context);

    return results;
}

std::vector<OverlapResult2D> Physics2DWorld::OverlapCircle(const math::Vec2& center, float radius, uint64_t collisionMask) {
    std::vector<OverlapResult2D> results;

    struct OverlapContext {
        const Physics2DWorld* world;
        math::Vec2 center;
        std::vector<OverlapResult2D>* results;
        std::set<uint64_t>* seen;
    };

    std::set<uint64_t> seen;
    OverlapContext context;
    context.world = this;
    context.center = center;
    context.results = &results;
    context.seen = &seen;

    auto overlapCallback = [](b2ShapeId shapeId, void* ctx) -> bool {
        OverlapContext* context = static_cast<OverlapContext*>(ctx);

        RigidBody2D* body = context->world->ResolveBody(b2Shape_GetBody(shapeId));
        if (!body) {
            return true;
        }

        uint64_t key = static_cast<uint64_t>(body->GetId());
        if (context->seen->insert(key).second) {
            OverlapResult2D result;
            result.bodyId = body->GetId();
            result.point = context->center;
            context->results->push_back(result);
        }
        return true;
    };

    // A single-point proxy with a radius is a circle expressed in world space.
    b2Vec2 worldCenter = {center.x, center.y};
    b2ShapeProxy proxy = b2MakeProxy(&worldCenter, 1, radius);
    b2World_OverlapShape(m_WorldId, &proxy, MakeQueryFilter(collisionMask), overlapCallback, &context);

    return results;
}

std::vector<OverlapResult2D> Physics2DWorld::OverlapBox(const math::Vec2& center, const math::Vec2& size, float angle, uint64_t collisionMask) {
    std::vector<OverlapResult2D> results;

    struct OverlapContext {
        const Physics2DWorld* world;
        math::Vec2 center;
        std::vector<OverlapResult2D>* results;
        std::set<uint64_t>* seen;
    };

    std::set<uint64_t> seen;
    OverlapContext context;
    context.world = this;
    context.center = center;
    context.results = &results;
    context.seen = &seen;

    auto overlapCallback = [](b2ShapeId shapeId, void* ctx) -> bool {
        OverlapContext* context = static_cast<OverlapContext*>(ctx);

        RigidBody2D* body = context->world->ResolveBody(b2Shape_GetBody(shapeId));
        if (!body) {
            return true;
        }

        uint64_t key = static_cast<uint64_t>(body->GetId());
        if (context->seen->insert(key).second) {
            OverlapResult2D result;
            result.bodyId = body->GetId();
            result.point = context->center;
            context->results->push_back(result);
        }
        return true;
    };

    // Build the four oriented corners in world space (size is the full extent).
    float halfW = size.x * 0.5f;
    float halfH = size.y * 0.5f;
    float c = std::cos(angle);
    float s = std::sin(angle);

    auto rotateCorner = [&](float lx, float ly) -> b2Vec2 {
        return b2Vec2{center.x + (lx * c - ly * s), center.y + (lx * s + ly * c)};
    };

    b2Vec2 corners[4] = {
        rotateCorner(-halfW, -halfH),
        rotateCorner(halfW, -halfH),
        rotateCorner(halfW, halfH),
        rotateCorner(-halfW, halfH)
    };

    b2ShapeProxy proxy = b2MakeProxy(corners, 4, 0.0f);
    b2World_OverlapShape(m_WorldId, &proxy, MakeQueryFilter(collisionMask), overlapCallback, &context);

    return results;
}

bool Physics2DWorld::ShapeCast(const math::Vec2& start, const math::Vec2& end, float radius, ShapeCastHit2D& hit, const core::UUID* ignoreBody, uint64_t collisionMask) {

    b2Vec2 center = {0.0f, 0.0f};
    b2Vec2 translation = {end.x - start.x, end.y - start.y};

    struct CastContext {
        const Physics2DWorld* world;
        ShapeCastHit2D* hit;
        float closestFraction;
        const core::UUID* ignoreBody;
    };

    CastContext context;
    context.world = this;
    context.hit = &hit;
    context.closestFraction = 1.0f;
    context.ignoreBody = ignoreBody;

    auto castCallback = [](b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* ctx) -> float {
        CastContext* context = static_cast<CastContext*>(ctx);

        RigidBody2D* body = context->world->ResolveBody(b2Shape_GetBody(shapeId));
        if (!body) {
            return -1.0f;
        }
        if (context->ignoreBody && body->GetId() == *context->ignoreBody) {
            return -1.0f;
        }

        if (fraction <= context->closestFraction) {
            context->closestFraction = fraction;
            context->hit->point = math::Vec2(point.x, point.y);
            context->hit->normal = math::Vec2(normal.x, normal.y);
            context->hit->fraction = fraction;
            context->hit->bodyId = body->GetId();
            context->hit->hit = true;
        }
        return fraction;
    };

    hit.hit = false;
    hit.fraction = 1.0f;

    b2ShapeProxy offsetProxy = b2MakeOffsetProxy(&center, 1, radius, {start.x, start.y}, b2Rot_identity);
    b2World_CastShape(m_WorldId, &offsetProxy, translation, MakeQueryFilter(collisionMask), castCallback, &context);

    return hit.hit;
}

bool Physics2DWorld::ShapeCastConvex(const std::vector<math::Vec2>& points, float radius,
                                     const math::Vec2& translation, ShapeCastHit2D& hit,
                                     const core::UUID* ignoreBody, uint64_t collisionMask) {
    hit.hit = false;
    hit.fraction = 1.0f;

    if (points.empty()) {
        return false;
    }

    std::vector<b2Vec2> proxyPoints;
    proxyPoints.reserve(points.size());
    for (const math::Vec2& p : points) {
        proxyPoints.push_back(b2Vec2{p.x, p.y});
    }

    int count = static_cast<int>(proxyPoints.size());
    if (count > B2_MAX_POLYGON_VERTICES) {
        count = B2_MAX_POLYGON_VERTICES;
    }

    b2ShapeProxy proxy = b2MakeProxy(proxyPoints.data(), count, radius);
    b2Vec2 b2Translation = {translation.x, translation.y};

    struct CastContext {
        const Physics2DWorld* world;
        ShapeCastHit2D* hit;
        float closestFraction;
        const core::UUID* ignoreBody;
    };

    CastContext context;
    context.world = this;
    context.hit = &hit;
    context.closestFraction = 1.0f;
    context.ignoreBody = ignoreBody;

    auto castCallback = [](b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* ctx) -> float {
        CastContext* context = static_cast<CastContext*>(ctx);

        // A kinematic character must not be stopped by triggers/sensors.
        if (b2Shape_IsSensor(shapeId)) {
            return -1.0f;
        }

        RigidBody2D* body = context->world->ResolveBody(b2Shape_GetBody(shapeId));
        if (!body) {
            return -1.0f;
        }
        if (context->ignoreBody && body->GetId() == *context->ignoreBody) {
            return -1.0f;
        }

        if (fraction <= context->closestFraction) {
            context->closestFraction = fraction;
            context->hit->point = math::Vec2(point.x, point.y);
            context->hit->normal = math::Vec2(normal.x, normal.y);
            context->hit->fraction = fraction;
            context->hit->bodyId = body->GetId();
            context->hit->hit = true;
        }
        return fraction;
    };

    b2World_CastShape(m_WorldId, &proxy, b2Translation, MakeQueryFilter(collisionMask), castCallback, &context);

    return hit.hit;
}

bool Physics2DWorld::ComputeConvexDepenetration(const std::vector<math::Vec2>& points, float radius,
                                                const core::UUID* ignoreBody, uint64_t collisionMask,
                                                math::Vec2& outCorrection) {
    outCorrection = math::Vec2::Zero();

    if (points.size() < 3) {
        return false;
    }

    // Build the character convex hull once. The points are supplied in world
    // space; each recovery iteration translates the polygon by the accumulated
    // correction rather than rebuilding it.
    std::vector<b2Vec2> hullPoints;
    hullPoints.reserve(points.size());
    for (const math::Vec2& p : points) {
        hullPoints.push_back(b2Vec2{p.x, p.y});
    }
    int hullCount = static_cast<int>(hullPoints.size());
    if (hullCount > B2_MAX_POLYGON_VERTICES) {
        hullCount = B2_MAX_POLYGON_VERTICES;
    }

    b2Hull hull = b2ComputeHull(hullPoints.data(), hullCount);
    if (hull.count < 3) {
        return false;
    }
    b2Polygon characterPoly = b2MakePolygon(&hull, radius);

    struct OverlapContext {
        const Physics2DWorld* world;
        const core::UUID* ignoreBody;
        std::vector<b2ShapeId>* shapes;
    };

    math::Vec2 accumulated = math::Vec2::Zero();
    const int kMaxIterations = 4;
    const float kEpsilon = 0.01f;
    bool corrected = false;

    for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
        // Broad-phase: collect the solid shapes overlapping the character at its
        // current (already partially-pushed) position.
        std::vector<b2Vec2> queryPoints;
        queryPoints.reserve(points.size());
        for (const math::Vec2& p : points) {
            queryPoints.push_back(b2Vec2{p.x + accumulated.x, p.y + accumulated.y});
        }
        int queryCount = static_cast<int>(queryPoints.size());
        if (queryCount > B2_MAX_POLYGON_VERTICES) {
            queryCount = B2_MAX_POLYGON_VERTICES;
        }
        b2ShapeProxy queryProxy = b2MakeProxy(queryPoints.data(), queryCount, radius);

        std::vector<b2ShapeId> overlapShapes;
        OverlapContext octx;
        octx.world = this;
        octx.ignoreBody = ignoreBody;
        octx.shapes = &overlapShapes;

        auto overlapCallback = [](b2ShapeId shapeId, void* ctx) -> bool {
            OverlapContext* context = static_cast<OverlapContext*>(ctx);
            if (b2Shape_IsSensor(shapeId)) {
                return true;
            }
            RigidBody2D* body = context->world->ResolveBody(b2Shape_GetBody(shapeId));
            if (!body) {
                return true;
            }
            if (context->ignoreBody && body->GetId() == *context->ignoreBody) {
                return true;
            }
            context->shapes->push_back(shapeId);
            return true;
        };

        b2World_OverlapShape(m_WorldId, &queryProxy, MakeQueryFilter(collisionMask), overlapCallback, &octx);

        if (overlapShapes.empty()) {
            break;
        }

        // The polygon is expressed in world space, so its only transform is the
        // accumulated push applied so far this call.
        b2Transform characterXf = { {accumulated.x, accumulated.y}, b2Rot_identity };

        float deepestPenetration = 0.0f;
        math::Vec2 bestPush = math::Vec2::Zero();
        bool foundPenetration = false;

        for (b2ShapeId shapeId : overlapShapes) {
            b2Transform obstacleXf = b2Body_GetTransform(b2Shape_GetBody(shapeId));
            b2ShapeType shapeType = b2Shape_GetType(shapeId);

            b2Manifold manifold = {};
            // True when the manifold normal points character -> obstacle (the
            // character is shape A); false when it points obstacle -> character.
            bool normalCharToObstacle = true;

            switch (shapeType) {
                case b2_polygonShape: {
                    b2Polygon obstacle = b2Shape_GetPolygon(shapeId);
                    manifold = b2CollidePolygons(&characterPoly, characterXf, &obstacle, obstacleXf);
                    break;
                }
                case b2_circleShape: {
                    b2Circle obstacle = b2Shape_GetCircle(shapeId);
                    manifold = b2CollidePolygonAndCircle(&characterPoly, characterXf, &obstacle, obstacleXf);
                    break;
                }
                case b2_capsuleShape: {
                    b2Capsule obstacle = b2Shape_GetCapsule(shapeId);
                    manifold = b2CollidePolygonAndCapsule(&characterPoly, characterXf, &obstacle, obstacleXf);
                    break;
                }
                case b2_segmentShape: {
                    b2Segment obstacle = b2Shape_GetSegment(shapeId);
                    manifold = b2CollideSegmentAndPolygon(&obstacle, obstacleXf, &characterPoly, characterXf);
                    normalCharToObstacle = false;
                    break;
                }
                case b2_chainSegmentShape: {
                    b2ChainSegment obstacle = b2Shape_GetChainSegment(shapeId);
                    b2SimplexCache cache = {};
                    manifold = b2CollideChainSegmentAndPolygon(&obstacle, obstacleXf, &characterPoly, characterXf, &cache);
                    normalCharToObstacle = false;
                    break;
                }
                default:
                    break;
            }

            if (manifold.pointCount == 0) {
                continue;
            }

            float minSeparation = 0.0f;
            for (int i = 0; i < manifold.pointCount; ++i) {
                if (manifold.points[i].separation < minSeparation) {
                    minSeparation = manifold.points[i].separation;
                }
            }

            // Speculative contacts report a non-negative separation; only true
            // overlap needs correcting.
            if (minSeparation >= 0.0f) {
                continue;
            }

            float penetration = -minSeparation;
            if (penetration > deepestPenetration) {
                deepestPenetration = penetration;
                math::Vec2 normal(manifold.normal.x, manifold.normal.y);
                if (normalCharToObstacle) {
                    // Move the character opposite the character -> obstacle normal.
                    bestPush = normal * (-penetration);
                } else {
                    // Normal already points obstacle -> character.
                    bestPush = normal * penetration;
                }
                foundPenetration = true;
            }
        }

        if (!foundPenetration) {
            break;
        }

        float pushLength = bestPush.Length();
        if (pushLength <= 0.0001f) {
            break;
        }

        math::Vec2 pushDir = bestPush / pushLength;
        accumulated = accumulated + pushDir * (pushLength + kEpsilon);
        corrected = true;
    }

    outCorrection = accumulated;
    return corrected;
}

}
}

