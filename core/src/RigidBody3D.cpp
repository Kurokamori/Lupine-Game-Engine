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
    , m_RigidBody(nullptr)
    , m_CompoundShape(nullptr)
    , m_GravityScale(1.0f)
    , m_Destroying(false)
{

    btDefaultMotionState* motionState = new btDefaultMotionState(btTransform::getIdentity());

    btScalar mass = (type == BodyType::Dynamic) ? 1.0f : 0.0f;
    btVector3 localInertia(0, 0, 0);

    // The body starts with an empty compound. Colliders attach their shapes as children,
    // so a body with no collider has no collision volume at all (rather than an implicit
    // sphere) and a body with several colliders keeps all of them.
    m_CompoundShape = new btCompoundShape();

    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, m_CompoundShape, localInertia);
    m_RigidBody = new btRigidBody(rbInfo);

    if (type == BodyType::Static) {
        m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
    } else if (type == BodyType::Kinematic) {
        m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    }

    world->GetBulletWorld()->addRigidBody(m_RigidBody);
}

RigidBody3D::~RigidBody3D() {
    m_Destroying = true;

    if (m_World && m_World->GetBulletWorld() && m_RigidBody) {
        m_World->GetBulletWorld()->removeRigidBody(m_RigidBody);
    }

    // Colliders are owned by whoever constructed them - the components hold them in
    // unique_ptrs - so the body must not delete them here; that would double-free against
    // the owner. Tell each collider its body is gone instead, so its own destructor knows
    // not to reach back into freed memory.
    std::vector<Collider3D*> colliders;
    colliders.swap(m_Colliders);
    for (Collider3D* collider : colliders) {
        if (collider) {
            collider->OnOwningBodyDestroyed();
        }
    }

    if (m_RigidBody) {
        delete m_RigidBody->getMotionState();
        m_RigidBody->setMotionState(nullptr);
        m_RigidBody->setCollisionShape(nullptr);
        delete m_RigidBody;
        m_RigidBody = nullptr;
    }

    delete m_CompoundShape;
    m_CompoundShape = nullptr;
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
    // Static and kinematic bodies have an inverse mass of zero, which would divide to inf.
    if (m_Type != BodyType::Dynamic) {
        return 0.0f;
    }
    const btScalar invMass = m_RigidBody->getInvMass();
    return invMass > btScalar(0) ? static_cast<float>(1.0f / invMass) : 0.0f;
}

void RigidBody3D::SetInertia(const math::Vec3& inertia) {
    if (m_Type == BodyType::Dynamic) {
        float mass = GetMass();
        m_RigidBody->setMassProps(mass, btVector3(inertia.x, inertia.y, inertia.z));
        m_RigidBody->updateInertiaTensor();
    }
}

math::Vec3 RigidBody3D::GetInertia() const {
    // Bullet doesn't have getLocalInertia(), so we calculate from inverse inertia diagonal
    const btVector3& invInertia = m_RigidBody->getInvInertiaDiagLocal();
    btVector3 inertia(
        invInertia.x() > 0.0f ? 1.0f / invInertia.x() : 0.0f,
        invInertia.y() > 0.0f ? 1.0f / invInertia.y() : 0.0f,
        invInertia.z() > 0.0f ? 1.0f / invInertia.z() : 0.0f
    );
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
    if (!collider) {
        return;
    }
    m_Colliders.push_back(collider);
    RebuildCompoundShape();
}

void RigidBody3D::RemoveCollider(Collider3D* collider) {
    if (m_Destroying) {
        return;
    }

    auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
    if (it != m_Colliders.end()) {
        m_Colliders.erase(it);
        RebuildCompoundShape();
    }
}

void RigidBody3D::DetachColliderShape(btCollisionShape* shape) {
    if (m_Destroying || !m_CompoundShape || !shape) {
        return;
    }

    for (int i = m_CompoundShape->getNumChildShapes() - 1; i >= 0; --i) {
        if (m_CompoundShape->getChildShape(i) == shape) {
            m_CompoundShape->removeChildShapeByIndex(i);
        }
    }
    m_CompoundShape->recalculateLocalAabb();
    RefreshBroadphaseAabb();
}

void RigidBody3D::RebuildCompoundShape() {
    if (m_Destroying || !m_CompoundShape || !m_RigidBody) {
        return;
    }

    for (int i = m_CompoundShape->getNumChildShapes() - 1; i >= 0; --i) {
        m_CompoundShape->removeChildShapeByIndex(i);
    }

    for (Collider3D* collider : m_Colliders) {
        if (!collider || !collider->IsEnabled()) {
            continue;
        }
        btCollisionShape* shape = collider->GetBulletShape();
        if (!shape) {
            continue;
        }

        const math::Vec3 offset = collider->GetOffset();
        const math::Quat rotation = collider->GetRotationOffset();

        btTransform childTransform;
        childTransform.setIdentity();
        childTransform.setOrigin(btVector3(offset.x, offset.y, offset.z));
        childTransform.setRotation(btQuaternion(rotation.x(), rotation.y(), rotation.z(), rotation.w()));

        m_CompoundShape->addChildShape(childTransform, shape);
    }

    m_CompoundShape->recalculateLocalAabb();

    if (m_RigidBody->getCollisionShape() != m_CompoundShape) {
        m_RigidBody->setCollisionShape(m_CompoundShape);
    }

    UpdateMassProperties();
    RefreshBroadphaseAabb();
}

void RigidBody3D::RefreshBroadphaseAabb() {
    if (!m_RigidBody || !m_World || !m_World->GetBulletWorld()) {
        return;
    }
    if (m_RigidBody->getBroadphaseProxy()) {
        m_World->GetBulletWorld()->updateSingleAabb(m_RigidBody);
    }
}

void RigidBody3D::UpdateMassProperties() {
    if (!m_RigidBody || m_Type != BodyType::Dynamic || m_Colliders.empty()) {
        return;
    }

    float totalMass = 0.0f;
    for (auto* collider : m_Colliders) {
        if (collider && collider->IsEnabled()) {
            totalMass += collider->GetDensity();
        }
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

void RigidBody3D::UpdateCollisionFiltering(uint32_t group, uint32_t mask) {
    if (!m_RigidBody || !m_World || !m_World->GetBulletWorld()) {
        return;
    }

    // Get the broadphase proxy and update filter values
    btBroadphaseProxy* proxy = m_RigidBody->getBroadphaseProxy();
    if (proxy) {
        // Bullet's filter fields are full 32-bit ints; truncating to 16 bits would drop
        // collision layers 17-32.
        proxy->m_collisionFilterGroup = static_cast<int>(group);
        proxy->m_collisionFilterMask = static_cast<int>(mask);

        // Only refresh overlapping pairs if the body is actually in the world
        // Check if the proxy has a valid client object (indicates body is registered)
        if (proxy->m_clientObject == nullptr) {
            return;
        }

        // Notify the broadphase that the proxy's filter has changed
        // This refreshes overlapping pairs based on new filter settings
        btBroadphaseInterface* broadphase = m_World->GetBulletWorld()->getBroadphase();
        btCollisionDispatcher* dispatcher = static_cast<btCollisionDispatcher*>(m_World->GetBulletWorld()->getDispatcher());

        // Safely clean proxy from pairs with full null checks
        if (broadphase && dispatcher) {
            btOverlappingPairCache* pairCache = broadphase->getOverlappingPairCache();
            if (pairCache) {
                pairCache->cleanProxyFromPairs(proxy, dispatcher);
            }
        }
    } 
}

}
}

