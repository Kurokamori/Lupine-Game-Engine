#include "lupine/physics2d/RigidBody2D.hpp"
#include "lupine/physics2d/Collider2D.hpp"
#include "lupine/logger/Logger.hpp"
#include <box2d/box2d.h>

namespace lupine {
namespace physics2d {

RigidBody2D::RigidBody2D(Physics2DWorld* world, const core::UUID& id, BodyType type)
    : m_World(world)
    , m_Id(id)
    , m_Type(type)
{

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = ConvertBodyType(type);
    bodyDef.position = {0.0f, 0.0f};
    bodyDef.rotation = b2Rot_identity;

    m_BodyId = b2CreateBody(world->GetBox2DWorld(), &bodyDef);
}

RigidBody2D::~RigidBody2D() {

    for (Collider2D* collider : m_Colliders) {
        delete collider;
    }
    m_Colliders.clear();

    if (B2_IS_NON_NULL(m_BodyId)) {
        b2DestroyBody(m_BodyId);
    }
}

void RigidBody2D::SetBodyType(BodyType type) {
    m_Type = type;
    b2Body_SetType(m_BodyId, ConvertBodyType(type));
}

void RigidBody2D::SetPosition(const math::Vec2& position) {
    b2Vec2 pos = {position.x, position.y};
    b2Rot rot = b2Body_GetRotation(m_BodyId);
    b2Body_SetTransform(m_BodyId, pos, rot);
}

math::Vec2 RigidBody2D::GetPosition() const {
    b2Vec2 pos = b2Body_GetPosition(m_BodyId);
    return math::Vec2(pos.x, pos.y);
}

void RigidBody2D::SetRotation(float angle) {
    b2Vec2 pos = b2Body_GetPosition(m_BodyId);
    b2Rot rot = b2MakeRot(angle);
    b2Body_SetTransform(m_BodyId, pos, rot);
}

float RigidBody2D::GetRotation() const {
    b2Rot rot = b2Body_GetRotation(m_BodyId);
    return b2Rot_GetAngle(rot);
}

void RigidBody2D::SetTransform(const math::Vec2& position, float angle) {
    b2Vec2 pos = {position.x, position.y};
    b2Rot rot = b2MakeRot(angle);
    b2Body_SetTransform(m_BodyId, pos, rot);
}

void RigidBody2D::SetTransform(const math::Transform& transform) {

    math::Vec2 position(transform.position.x, transform.position.y);

    math::Vec3 euler = transform.GetEulerAngles();
    float angle = euler.z;

    SetTransform(position, angle);
}

math::Transform RigidBody2D::GetTransform() const {
    math::Vec2 pos2d = GetPosition();
    float angle = GetRotation();

    math::Transform transform;
    transform.position = math::Vec3(pos2d.x, pos2d.y, 0.0f);
    transform.rotation = math::Quat::FromAxisAngle(math::Vec3::UnitZ(), angle);
    transform.scale = math::Vec3::One();

    return transform;
}

void RigidBody2D::SetLinearVelocity(const math::Vec2& velocity) {
    b2Vec2 vel = {velocity.x, velocity.y};
    b2Body_SetLinearVelocity(m_BodyId, vel);
}

math::Vec2 RigidBody2D::GetLinearVelocity() const {
    b2Vec2 vel = b2Body_GetLinearVelocity(m_BodyId);
    return math::Vec2(vel.x, vel.y);
}

void RigidBody2D::SetAngularVelocity(float omega) {
    b2Body_SetAngularVelocity(m_BodyId, omega);
}

float RigidBody2D::GetAngularVelocity() const {
    return b2Body_GetAngularVelocity(m_BodyId);
}

void RigidBody2D::ApplyForce(const math::Vec2& force, const math::Vec2& point) {
    b2Vec2 f = {force.x, force.y};
    b2Vec2 p = {point.x, point.y};
    b2Body_ApplyForce(m_BodyId, f, p, true);
}

void RigidBody2D::ApplyForceToCenter(const math::Vec2& force) {
    b2Vec2 f = {force.x, force.y};
    b2Vec2 center = b2Body_GetWorldCenterOfMass(m_BodyId);
    b2Body_ApplyForce(m_BodyId, f, center, true);
}

void RigidBody2D::ApplyTorque(float torque) {
    b2Body_ApplyTorque(m_BodyId, torque, true);
}

void RigidBody2D::ApplyLinearImpulse(const math::Vec2& impulse, const math::Vec2& point) {
    b2Vec2 i = {impulse.x, impulse.y};
    b2Vec2 p = {point.x, point.y};
    b2Body_ApplyLinearImpulse(m_BodyId, i, p, true);
}

void RigidBody2D::ApplyLinearImpulseToCenter(const math::Vec2& impulse) {
    b2Vec2 i = {impulse.x, impulse.y};
    b2Vec2 center = b2Body_GetWorldCenterOfMass(m_BodyId);
    b2Body_ApplyLinearImpulse(m_BodyId, i, center, true);
}

void RigidBody2D::ApplyAngularImpulse(float impulse) {
    b2Body_ApplyAngularImpulse(m_BodyId, impulse, true);
}

float RigidBody2D::GetMass() const {
    return b2Body_GetMass(m_BodyId);
}

float RigidBody2D::GetInertia() const {
    return b2Body_GetRotationalInertia(m_BodyId);
}

math::Vec2 RigidBody2D::GetLocalCenter() const {
    b2Vec2 center = b2Body_GetLocalCenterOfMass(m_BodyId);
    return math::Vec2(center.x, center.y);
}

math::Vec2 RigidBody2D::GetWorldCenter() const {
    b2Vec2 center = b2Body_GetWorldCenterOfMass(m_BodyId);
    return math::Vec2(center.x, center.y);
}

void RigidBody2D::SetLinearDamping(float damping) {
    b2Body_SetLinearDamping(m_BodyId, damping);
}

float RigidBody2D::GetLinearDamping() const {
    return b2Body_GetLinearDamping(m_BodyId);
}

void RigidBody2D::SetAngularDamping(float damping) {
    b2Body_SetAngularDamping(m_BodyId, damping);
}

float RigidBody2D::GetAngularDamping() const {
    return b2Body_GetAngularDamping(m_BodyId);
}

void RigidBody2D::SetGravityScale(float scale) {
    b2Body_SetGravityScale(m_BodyId, scale);
}

float RigidBody2D::GetGravityScale() const {
    return b2Body_GetGravityScale(m_BodyId);
}

void RigidBody2D::SetBullet(bool flag) {
    b2Body_SetBullet(m_BodyId, flag);
}

bool RigidBody2D::IsBullet() const {
    return b2Body_IsBullet(m_BodyId);
}

void RigidBody2D::SetFixedRotation(bool flag) {
    b2Body_SetFixedRotation(m_BodyId, flag);
}

bool RigidBody2D::IsFixedRotation() const {
    return b2Body_IsFixedRotation(m_BodyId);
}

void RigidBody2D::SetSleepingAllowed(bool flag) {
    b2Body_EnableSleep(m_BodyId, flag);
}

bool RigidBody2D::IsSleepingAllowed() const {
    return b2Body_IsSleepEnabled(m_BodyId);
}

bool RigidBody2D::IsAwake() const {
    return b2Body_IsAwake(m_BodyId);
}

void RigidBody2D::SetAwake(bool flag) {
    b2Body_SetAwake(m_BodyId, flag);
}

void RigidBody2D::SetEnabled(bool flag) {
    if (flag) {
        b2Body_Enable(m_BodyId);
    } else {
        b2Body_Disable(m_BodyId);
    }
}

bool RigidBody2D::IsEnabled() const {
    return b2Body_IsEnabled(m_BodyId);
}

void RigidBody2D::AddCollider(Collider2D* collider) {
    m_Colliders.push_back(collider);
}

void RigidBody2D::RemoveCollider(Collider2D* collider) {
    auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
    if (it != m_Colliders.end()) {
        m_Colliders.erase(it);
    }
}

b2BodyType RigidBody2D::ConvertBodyType(BodyType type) const {
    switch (type) {
        case BodyType::Static:    return b2_staticBody;
        case BodyType::Kinematic: return b2_kinematicBody;
        case BodyType::Dynamic:   return b2_dynamicBody;
        default:                  return b2_staticBody;
    }
}

BodyType RigidBody2D::ConvertBodyType(b2BodyType type) const {
    switch (type) {
        case b2_staticBody:    return BodyType::Static;
        case b2_kinematicBody: return BodyType::Kinematic;
        case b2_dynamicBody:   return BodyType::Dynamic;
        default:               return BodyType::Static;
    }
}

}
}

