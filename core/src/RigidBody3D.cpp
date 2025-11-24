#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/physics3d/Collider3D.hpp"
#include "lupine/logger/Logger.hpp"
#include <btBulletDynamicsCommon.h>

namespace lupine {
namespace physics3d {

RigidBody3D::RigidBody3D(Physics3DWorld* world, const core::UUID& id, BodyType type)
    : m_World(world)
    , m_Id(id)
    , m_Type(type)
    , m_GravityScale(1.0f)
{

    btDefaultMotionState* motionState = new btDefaultMotionState(btTransform::getIdentity());

    btScalar mass = (type == BodyType::Dynamic) ? 1.0f : 0.0f;
    btVector3 localInertia(0, 0, 0);

    btCollisionShape* shape = new btSphereShape(0.5f);
    if (mass != 0.0f) {
        shape->calculateLocalInertia(mass, localInertia);
    }

    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
    m_RigidBody = new btRigidBody(rbInfo);

    if (type == BodyType::Static) {
        m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
    } else if (type == BodyType::Kinematic) {
        m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    }

    world->GetBulletWorld()->addRigidBody(m_RigidBody);
}

RigidBody3D::~RigidBody3D() {

    if (m_World && m_World->GetBulletWorld()) {
        m_World->GetBulletWorld()->removeRigidBody(m_RigidBody);
    }

    for (auto* collider : m_Colliders) {
        delete collider;
    }
    m_Colliders.clear();

    if (m_RigidBody) {
        delete m_RigidBody->getMotionState();
        delete m_RigidBody->getCollisionShape();
        delete m_RigidBody;
    }
}

void RigidBody3D::SetBodyType(BodyType type) {
    m_Type = type;

    int flags = m_RigidBody->getCollisionFlags();
    flags &= ~(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);

    if (type == BodyType::Static) {
        flags |= btCollisionObject::CF_STATIC_OBJECT;
        m_RigidBody->setMassProps(0.0f, btVector3(0, 0, 0));
    } else if (type == BodyType::Kinematic) {
        flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
        m_RigidBody->setMassProps(0.0f, btVector3(0, 0, 0));
    } else {
        UpdateMassProperties();
    }

    m_RigidBody->setCollisionFlags(flags);
}

void RigidBody3D::SetPosition(const math::Vec3& position) {
    btTransform transform = m_RigidBody->getWorldTransform();
    transform.setOrigin(btVector3(position.x, position.y, position.z));
    m_RigidBody->setWorldTransform(transform);
    m_RigidBody->activate();
}

math::Vec3 RigidBody3D::GetPosition() const {
    btVector3 pos = m_RigidBody->getWorldTransform().getOrigin();
    return math::Vec3(pos.x(), pos.y(), pos.z());
}

void RigidBody3D::SetRotation(const math::Quat& rotation) {
    btTransform transform = m_RigidBody->getWorldTransform();
    transform.setRotation(btQuaternion(rotation.x(), rotation.y(), rotation.z(), rotation.w()));
    m_RigidBody->setWorldTransform(transform);
    m_RigidBody->activate();
}

math::Quat RigidBody3D::GetRotation() const {
    btQuaternion rot = m_RigidBody->getWorldTransform().getRotation();
    return math::Quat(rot.w(), rot.x(), rot.y(), rot.z());
}

void RigidBody3D::SetTransform(const math::Vec3& position, const math::Quat& rotation) {
    btTransform transform;
    transform.setOrigin(btVector3(position.x, position.y, position.z));
    transform.setRotation(btQuaternion(rotation.x(), rotation.y(), rotation.z(), rotation.w()));
    m_RigidBody->setWorldTransform(transform);
    m_RigidBody->activate();
}

void RigidBody3D::SetTransform(const math::Transform& transform) {
    SetTransform(transform.position, transform.rotation);
}

math::Transform RigidBody3D::GetTransform() const {
    math::Transform transform;
    transform.position = GetPosition();
    transform.rotation = GetRotation();
    transform.scale = math::Vec3::One();
    return transform;
}

void RigidBody3D::SetLinearVelocity(const math::Vec3& velocity) {
    m_RigidBody->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
    m_RigidBody->activate();
}

math::Vec3 RigidBody3D::GetLinearVelocity() const {
    btVector3 vel = m_RigidBody->getLinearVelocity();
    return math::Vec3(vel.x(), vel.y(), vel.z());
}

void RigidBody3D::SetAngularVelocity(const math::Vec3& omega) {
    m_RigidBody->setAngularVelocity(btVector3(omega.x, omega.y, omega.z));
    m_RigidBody->activate();
}

math::Vec3 RigidBody3D::GetAngularVelocity() const {
    btVector3 omega = m_RigidBody->getAngularVelocity();
    return math::Vec3(omega.x(), omega.y(), omega.z());
}

void RigidBody3D::ApplyForce(const math::Vec3& force, const math::Vec3& relativePos) {
    m_RigidBody->applyForce(btVector3(force.x, force.y, force.z), btVector3(relativePos.x, relativePos.y, relativePos.z));
    m_RigidBody->activate();
}

void RigidBody3D::ApplyForceToCenter(const math::Vec3& force) {
    m_RigidBody->applyCentralForce(btVector3(force.x, force.y, force.z));
    m_RigidBody->activate();
}

void RigidBody3D::ApplyTorque(const math::Vec3& torque) {
    m_RigidBody->applyTorque(btVector3(torque.x, torque.y, torque.z));
    m_RigidBody->activate();
}

void RigidBody3D::ApplyImpulse(const math::Vec3& impulse, const math::Vec3& relativePos) {
    m_RigidBody->applyImpulse(btVector3(impulse.x, impulse.y, impulse.z), btVector3(relativePos.x, relativePos.y, relativePos.z));
    m_RigidBody->activate();
}

void RigidBody3D::ApplyImpulseToCenter(const math::Vec3& impulse) {
    m_RigidBody->applyCentralImpulse(btVector3(impulse.x, impulse.y, impulse.z));
    m_RigidBody->activate();
}

void RigidBody3D::ApplyTorqueImpulse(const math::Vec3& torque) {
    m_RigidBody->applyTorqueImpulse(btVector3(torque.x, torque.y, torque.z));
    m_RigidBody->activate();
}

void RigidBody3D::ClearForces() {
    m_RigidBody->clearForces();
}

void RigidBody3D::SetMass(float mass) {
    if (m_Type == BodyType::Dynamic) {
        btVector3 inertia;
        m_RigidBody->getCollisionShape()->calculateLocalInertia(mass, inertia);
        m_RigidBody->setMassProps(mass, inertia);
        m_RigidBody->updateInertiaTensor();
    }
}

float RigidBody3D::GetMass() const {
    return 1.0f / m_RigidBody->getInvMass();
}

void RigidBody3D::SetInertia(const math::Vec3& inertia) {
    if (m_Type == BodyType::Dynamic) {
        float mass = GetMass();
        m_RigidBody->setMassProps(mass, btVector3(inertia.x, inertia.y, inertia.z));
        m_RigidBody->updateInertiaTensor();
    }
}

math::Vec3 RigidBody3D::GetInertia() const {
    btVector3 inertia = m_RigidBody->getLocalInertia();
    return math::Vec3(inertia.x(), inertia.y(), inertia.z());
}

math::Vec3 RigidBody3D::GetCenterOfMass() const {
    btVector3 com = m_RigidBody->getCenterOfMassPosition();
    return math::Vec3(com.x(), com.y(), com.z());
}

void RigidBody3D::SetLinearDamping(float damping) {
    m_RigidBody->setDamping(damping, m_RigidBody->getAngularDamping());
}

float RigidBody3D::GetLinearDamping() const {
    return m_RigidBody->getLinearDamping();
}

void RigidBody3D::SetAngularDamping(float damping) {
    m_RigidBody->setDamping(m_RigidBody->getLinearDamping(), damping);
}

float RigidBody3D::GetAngularDamping() const {
    return m_RigidBody->getAngularDamping();
}

void RigidBody3D::SetDamping(float linearDamping, float angularDamping) {
    m_RigidBody->setDamping(linearDamping, angularDamping);
}

void RigidBody3D::SetLinearFactor(const math::Vec3& factor) {
    m_RigidBody->setLinearFactor(btVector3(factor.x, factor.y, factor.z));
}

math::Vec3 RigidBody3D::GetLinearFactor() const {
    btVector3 factor = m_RigidBody->getLinearFactor();
    return math::Vec3(factor.x(), factor.y(), factor.z());
}

void RigidBody3D::SetAngularFactor(const math::Vec3& factor) {
    m_RigidBody->setAngularFactor(btVector3(factor.x, factor.y, factor.z));
}

math::Vec3 RigidBody3D::GetAngularFactor() const {
    btVector3 factor = m_RigidBody->getAngularFactor();
    return math::Vec3(factor.x(), factor.y(), factor.z());
}

void RigidBody3D::SetGravityScale(float scale) {
    m_GravityScale = scale;
    if (m_World) {
        math::Vec3 worldGravity = m_World->GetGravity();
        m_RigidBody->setGravity(btVector3(worldGravity.x * scale, worldGravity.y * scale, worldGravity.z * scale));
    }
}

float RigidBody3D::GetGravityScale() const {
    return m_GravityScale;
}

void RigidBody3D::SetUseGravity(bool useGravity) {
    if (useGravity) {
        SetGravityScale(m_GravityScale);
    } else {
        m_RigidBody->setGravity(btVector3(0, 0, 0));
    }
}

bool RigidBody3D::GetUseGravity() const {
    btVector3 gravity = m_RigidBody->getGravity();
    return gravity.length2() > 0.0f;
}

void RigidBody3D::SetSleepingEnabled(bool enabled) {
    if (enabled) {
        m_RigidBody->forceActivationState(ACTIVE_TAG);
    } else {
        m_RigidBody->setActivationState(DISABLE_DEACTIVATION);
    }
}

bool RigidBody3D::IsSleepingEnabled() const {
    return m_RigidBody->getActivationState() != DISABLE_DEACTIVATION;
}

bool RigidBody3D::IsAwake() const {
    return m_RigidBody->isActive();
}

void RigidBody3D::WakeUp() {
    m_RigidBody->activate(true);
}

void RigidBody3D::PutToSleep() {
    m_RigidBody->setActivationState(WANTS_DEACTIVATION);
}

void RigidBody3D::SetCcdMotionThreshold(float threshold) {
    m_RigidBody->setCcdMotionThreshold(threshold);
}

float RigidBody3D::GetCcdMotionThreshold() const {
    return m_RigidBody->getCcdMotionThreshold();
}

void RigidBody3D::SetCcdSweptSphereRadius(float radius) {
    m_RigidBody->setCcdSweptSphereRadius(radius);
}

float RigidBody3D::GetCcdSweptSphereRadius() const {
    return m_RigidBody->getCcdSweptSphereRadius();
}

void RigidBody3D::AddCollider(Collider3D* collider) {
    m_Colliders.push_back(collider);
    UpdateMassProperties();
}

void RigidBody3D::RemoveCollider(Collider3D* collider) {
    auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
    if (it != m_Colliders.end()) {
        m_Colliders.erase(it);

        if (m_Colliders.empty() && m_RigidBody) {
            m_RigidBody->setCollisionShape(nullptr);
        }

        UpdateMassProperties();
    }
}

void RigidBody3D::UpdateMassProperties() {
    if (m_Type != BodyType::Dynamic || m_Colliders.empty()) {
        return;
    }

    float totalMass = 0.0f;
    for (auto* collider : m_Colliders) {

        totalMass += collider->GetDensity();
    }

    if (totalMass > 0.0f) {
        btVector3 inertia(0, 0, 0);
        if (m_RigidBody->getCollisionShape()) {
            m_RigidBody->getCollisionShape()->calculateLocalInertia(totalMass, inertia);
        }
        m_RigidBody->setMassProps(totalMass, inertia);
        m_RigidBody->updateInertiaTensor();
    }
}

}
}

