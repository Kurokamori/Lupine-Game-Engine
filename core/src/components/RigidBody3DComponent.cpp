#include "lupine/components/RigidBody3DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics3d;

RigidBody3DComponent::RigidBody3DComponent()
    : Component("RigidBody3DComponent")
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

RigidBody3DComponent::RigidBody3DComponent(const std::string& name)
    : Component(name)
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

RigidBody3DComponent::~RigidBody3DComponent() {
    DestroyPhysicsBody();
}

void RigidBody3DComponent::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(mass, 1.0f, 0.01f, 1000.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gravityScale, 1.0f, -10.0f, 10.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(linearDamping, 0.0f, 0.0f, 10.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(angularDamping, 0.0f, 0.0f, 10.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(lockRotationX, Bool, false, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(lockRotationY, Bool, false, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(lockRotationZ, Bool, false, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(canSleep, Bool, true, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(gravityEnabled, Bool, true, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useCCD, Bool, false, "Physics"));
}

void RigidBody3DComponent::OnAwake() {
    CreatePhysicsBody();
}

void RigidBody3DComponent::OnReady() {

    if (m_PhysicsBody) {
        SyncTransformToPhysics();
    }
}

void RigidBody3DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void RigidBody3DComponent::OnPhysicsProcess(float deltaTime) {
    if (!m_PhysicsBody) return;

    SyncTransformFromPhysics();
}

float RigidBody3DComponent::GetMass() const {
    return GetPropertyValue<float>("mass");
}

void RigidBody3DComponent::SetMass(float mass) {
    SetPropertyValue("mass", mass);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetMass(mass);
    }
}

float RigidBody3DComponent::GetGravityScale() const {
    return GetPropertyValue<float>("gravityScale");
}

void RigidBody3DComponent::SetGravityScale(float scale) {
    SetPropertyValue("gravityScale", scale);
    UpdateGravityScale();
}

float RigidBody3DComponent::GetLinearDamping() const {
    return GetPropertyValue<float>("linearDamping");
}

void RigidBody3DComponent::SetLinearDamping(float damping) {
    SetPropertyValue("linearDamping", damping);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetLinearDamping(damping);
    }
}

float RigidBody3DComponent::GetAngularDamping() const {
    return GetPropertyValue<float>("angularDamping");
}

void RigidBody3DComponent::SetAngularDamping(float damping) {
    SetPropertyValue("angularDamping", damping);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetAngularDamping(damping);
    }
}

bool RigidBody3DComponent::GetLockRotationX() const {
    return GetPropertyValue<bool>("lockRotationX");
}

void RigidBody3DComponent::SetLockRotationX(bool lock) {
    SetPropertyValue("lockRotationX", lock);
    UpdateRotationLocks();
}

bool RigidBody3DComponent::GetLockRotationY() const {
    return GetPropertyValue<bool>("lockRotationY");
}

void RigidBody3DComponent::SetLockRotationY(bool lock) {
    SetPropertyValue("lockRotationY", lock);
    UpdateRotationLocks();
}

bool RigidBody3DComponent::GetLockRotationZ() const {
    return GetPropertyValue<bool>("lockRotationZ");
}

void RigidBody3DComponent::SetLockRotationZ(bool lock) {
    SetPropertyValue("lockRotationZ", lock);
    UpdateRotationLocks();
}

bool RigidBody3DComponent::GetCanSleep() const {
    return GetPropertyValue<bool>("canSleep");
}

void RigidBody3DComponent::SetCanSleep(bool canSleep) {
    SetPropertyValue("canSleep", canSleep);

}

bool RigidBody3DComponent::GetGravityEnabled() const {
    return GetPropertyValue<bool>("gravityEnabled");
}

void RigidBody3DComponent::SetGravityEnabled(bool enabled) {
    SetPropertyValue("gravityEnabled", enabled);
    UpdateGravityScale();
}

bool RigidBody3DComponent::GetUseCCD() const {
    return GetPropertyValue<bool>("useCCD");
}

void RigidBody3DComponent::SetUseCCD(bool useCCD) {
    SetPropertyValue("useCCD", useCCD);
    if (m_PhysicsBody && useCCD) {
        m_PhysicsBody->SetCcdMotionThreshold(0.1f);
        m_PhysicsBody->SetCcdSweptSphereRadius(0.2f);
    }
}

void RigidBody3DComponent::ApplyForce(const Vec3& force, const Vec3& point) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyForce(force, point);
    }
}

void RigidBody3DComponent::ApplyForceToCenter(const Vec3& force) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyForceToCenter(force);
    }
}

void RigidBody3DComponent::ApplyImpulse(const Vec3& impulse, const Vec3& point) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyImpulse(impulse, point);
    }
}

void RigidBody3DComponent::ApplyImpulseToCenter(const Vec3& impulse) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyImpulseToCenter(impulse);
    }
}

void RigidBody3DComponent::ApplyTorque(const Vec3& torque) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyTorque(torque);
    }
}

void RigidBody3DComponent::ApplyTorqueImpulse(const Vec3& torque) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyTorqueImpulse(torque);
    }
}

Vec3 RigidBody3DComponent::GetLinearVelocity() const {
    if (m_PhysicsBody) {
        return m_PhysicsBody->GetLinearVelocity();
    }
    return Vec3::Zero();
}

void RigidBody3DComponent::SetLinearVelocity(const Vec3& velocity) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetLinearVelocity(velocity);
    }
}

Vec3 RigidBody3DComponent::GetAngularVelocity() const {
    if (m_PhysicsBody) {
        return m_PhysicsBody->GetAngularVelocity();
    }
    return Vec3::Zero();
}

void RigidBody3DComponent::SetAngularVelocity(const Vec3& omega) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetAngularVelocity(omega);
    }
}

void RigidBody3DComponent::ClearForces() {
    if (m_PhysicsBody) {
        m_PhysicsBody->ClearForces();
    }
}

void RigidBody3DComponent::CreatePhysicsBody() {
    if (m_BodyCreated) return;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) {

        return;
    }

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) {

        return;
    }

    m_PhysicsBodyId = core::UUID();

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, BodyType::Dynamic);
    if (!m_PhysicsBody) {

        return;
    }

    m_PhysicsBody->SetMass(GetMass());
    m_PhysicsBody->SetLinearDamping(GetLinearDamping());
    m_PhysicsBody->SetAngularDamping(GetAngularDamping());

    UpdateGravityScale();
    UpdateRotationLocks();

    if (GetUseCCD()) {
        m_PhysicsBody->SetCcdMotionThreshold(0.1f);
        m_PhysicsBody->SetCcdSweptSphereRadius(0.2f);
    }

    m_BodyCreated = true;

}

void RigidBody3DComponent::DestroyPhysicsBody() {
    if (!m_BodyCreated) return;

    auto* sceneManager = SceneManager::GetInstance();

    bool isShuttingDown = (sceneManager && sceneManager->IsShuttingDown()) ||
                          (m_Owner && m_Owner->GetScene() && m_Owner->GetScene()->IsShuttingDown());

    if (sceneManager && !isShuttingDown) {
        auto* physicsWorld = sceneManager->GetPhysics3DWorld();
        if (physicsWorld) {
            physicsWorld->DestroyBody(m_PhysicsBodyId);
        }
    }

    m_PhysicsBody = nullptr;
    m_BodyCreated = false;
}

void RigidBody3DComponent::SyncTransformToPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 position = node3D->GetGlobalPosition();
    Quat rotation = node3D->GetGlobalRotation();

    m_PhysicsBody->SetPosition(position);
    m_PhysicsBody->SetRotation(rotation);

    m_LastPosition = position;
    m_LastRotation = rotation;
}

void RigidBody3DComponent::SyncTransformFromPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 globalPosition = m_PhysicsBody->GetPosition();
    Quat globalRotation = m_PhysicsBody->GetRotation();

    if (globalPosition != m_LastPosition || globalRotation != m_LastRotation) {

        Vec3 localPosition = globalPosition;
        Quat localRotation = globalRotation;

        if (node3D->GetParent()) {
            Node3D* parent3D = dynamic_cast<Node3D*>(node3D->GetParent());
            if (parent3D) {

                Vec3 parentGlobalPos = parent3D->GetGlobalPosition();
                Quat parentGlobalRot = parent3D->GetGlobalRotation();

                Vec3 relativePos = globalPosition - parentGlobalPos;
                localPosition = parentGlobalRot.Inverse() * relativePos;

                localRotation = parentGlobalRot.Inverse() * globalRotation;
            }
        }

        node3D->SetPosition(localPosition);
        node3D->SetRotation(localRotation);

        m_LastPosition = globalPosition;
        m_LastRotation = globalRotation;
    }
}

void RigidBody3DComponent::UpdateGravityScale() {
    if (!m_PhysicsBody) return;

    if (GetGravityEnabled()) {
        m_PhysicsBody->SetGravityScale(GetGravityScale());
    } else {
        m_PhysicsBody->SetGravityScale(0.0f);
    }
}

void RigidBody3DComponent::UpdateRotationLocks() {
    if (!m_PhysicsBody) return;

    Vec3 angularFactor(
        GetLockRotationX() ? 0.0f : 1.0f,
        GetLockRotationY() ? 0.0f : 1.0f,
        GetLockRotationZ() ? 0.0f : 1.0f
    );

    m_PhysicsBody->SetAngularFactor(angularFactor);
}

}
}

