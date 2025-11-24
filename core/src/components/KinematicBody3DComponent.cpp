#include "lupine/components/KinematicBody3DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics3d;

KinematicBody3DComponent::KinematicBody3DComponent()
    : Component("KinematicBody3DComponent")
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

KinematicBody3DComponent::KinematicBody3DComponent(const std::string& name)
    : Component(name)
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

KinematicBody3DComponent::~KinematicBody3DComponent() {
    DestroyPhysicsBody();
}

void KinematicBody3DComponent::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(gravityEnabled, Bool, false, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gravityScale, 1.0f, -10.0f, 10.0f, 0.1f, "Physics"));
}

void KinematicBody3DComponent::OnAwake() {
    CreatePhysicsBody();
}

void KinematicBody3DComponent::OnReady() {

    if (m_PhysicsBody) {
        SyncTransformToPhysics();
    }
}

void KinematicBody3DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void KinematicBody3DComponent::OnPhysicsProcess(float deltaTime) {
    if (!m_PhysicsBody) return;

    SyncTransformFromPhysics();
}

bool KinematicBody3DComponent::GetGravityEnabled() const {
    return GetPropertyValue<bool>("gravityEnabled");
}

void KinematicBody3DComponent::SetGravityEnabled(bool enabled) {
    SetPropertyValue("gravityEnabled", enabled);
    UpdateGravityScale();
}

float KinematicBody3DComponent::GetGravityScale() const {
    return GetPropertyValue<float>("gravityScale");
}

void KinematicBody3DComponent::SetGravityScale(float scale) {
    SetPropertyValue("gravityScale", scale);
    UpdateGravityScale();
}

Vec3 KinematicBody3DComponent::GetLinearVelocity() const {
    if (m_PhysicsBody) {
        return m_PhysicsBody->GetLinearVelocity();
    }
    return Vec3::Zero();
}

void KinematicBody3DComponent::SetLinearVelocity(const Vec3& velocity) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetLinearVelocity(velocity);
    }
}

Vec3 KinematicBody3DComponent::GetAngularVelocity() const {
    if (m_PhysicsBody) {
        return m_PhysicsBody->GetAngularVelocity();
    }
    return Vec3::Zero();
}

void KinematicBody3DComponent::SetAngularVelocity(const Vec3& omega) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetAngularVelocity(omega);
    }
}

void KinematicBody3DComponent::MoveBy(const Vec3& delta) {
    if (m_PhysicsBody) {
        Vec3 currentPos = m_PhysicsBody->GetPosition();
        m_PhysicsBody->SetPosition(currentPos + delta);
    }
}

void KinematicBody3DComponent::RotateBy(const Quat& deltaRotation) {
    if (m_PhysicsBody) {
        Quat currentRot = m_PhysicsBody->GetRotation();
        m_PhysicsBody->SetRotation(currentRot * deltaRotation);
    }
}

void KinematicBody3DComponent::CreatePhysicsBody() {
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

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, BodyType::Kinematic);
    if (!m_PhysicsBody) {

        return;
    }

    UpdateGravityScale();

    m_BodyCreated = true;

}

void KinematicBody3DComponent::DestroyPhysicsBody() {
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

void KinematicBody3DComponent::SyncTransformToPhysics() {
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

void KinematicBody3DComponent::SyncTransformFromPhysics() {
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

void KinematicBody3DComponent::UpdateGravityScale() {
    if (!m_PhysicsBody) return;

    if (GetGravityEnabled()) {
        m_PhysicsBody->SetGravityScale(GetGravityScale());
    } else {
        m_PhysicsBody->SetGravityScale(0.0f);
    }
}

}
}
