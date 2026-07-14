#include "lupine/components/StaticBody3DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics3d;

StaticBody3DComponent::StaticBody3DComponent()
    : Component("StaticBody3DComponent")
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

StaticBody3DComponent::StaticBody3DComponent(const std::string& name)
    : Component(name)
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

StaticBody3DComponent::~StaticBody3DComponent() {
    DestroyPhysicsBody();
}

void StaticBody3DComponent::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(constantLinearVelocity, Vec3, Vec3::Zero(), "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(constantAngularVelocity, Vec3, Vec3::Zero(), "Physics"));
}

void StaticBody3DComponent::OnAwake() {

    CreatePhysicsBody();
}

void StaticBody3DComponent::OnReady() {

    if (m_PhysicsBody) {
        SyncTransformToPhysics();
    }
}

void StaticBody3DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void StaticBody3DComponent::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
    // See Component::PhysicsWorldRebuildPhase. A static body owns no velocity, so there is
    // nothing to save - it just needs its body back in the fresh world.
    switch (phase) {
        case PhysicsWorldRebuildPhase::SaveState:
            break;

        case PhysicsWorldRebuildPhase::RecreateBodies:
            // The body died with the world: drop the stale handles without touching them.
            m_PhysicsBody = nullptr;
            m_BodyCreated = false;
            CreatePhysicsBody();
            break;

        case PhysicsWorldRebuildPhase::AttachColliders:
            if (m_PhysicsBody) {
                SyncTransformToPhysics();
            }
            break;
    }
}

void StaticBody3DComponent::OnPhysicsProcess(float) {
    if (!m_PhysicsBody) return;

    Vec3 linearVel = GetConstantLinearVelocity();
    Vec3 angularVel = GetConstantAngularVelocity();

    if (linearVel != Vec3::Zero() || angularVel != Vec3::Zero()) {
        m_PhysicsBody->SetLinearVelocity(linearVel);
        m_PhysicsBody->SetAngularVelocity(angularVel);
    }

    SyncTransformFromPhysics();
}

Vec3 StaticBody3DComponent::GetConstantLinearVelocity() const {
    return GetPropertyValue<Vec3>("constantLinearVelocity");
}

void StaticBody3DComponent::SetConstantLinearVelocity(const Vec3& velocity) {
    SetPropertyValue("constantLinearVelocity", velocity);
}

Vec3 StaticBody3DComponent::GetConstantAngularVelocity() const {
    return GetPropertyValue<Vec3>("constantAngularVelocity");
}

void StaticBody3DComponent::SetConstantAngularVelocity(const Vec3& velocity) {
    SetPropertyValue("constantAngularVelocity", velocity);
}

void StaticBody3DComponent::SetPosition(const Vec3& position) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetPosition(position);
        m_LastPosition = position;
    }

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (node3D) {
        node3D->SetPosition(position);
    }
}

void StaticBody3DComponent::SetRotation(const Quat& rotation) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetRotation(rotation);
        m_LastRotation = rotation;
    }

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (node3D) {
        node3D->SetRotation(rotation);
    }
}

void StaticBody3DComponent::CreatePhysicsBody() {
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

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, BodyType::Static);
    if (!m_PhysicsBody) {

        return;
    }

    physicsWorld->SetBodyNode(m_PhysicsBodyId, m_Owner);

    m_BodyCreated = true;

}

void StaticBody3DComponent::DestroyPhysicsBody() {
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

void StaticBody3DComponent::SyncTransformToPhysics() {
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

void StaticBody3DComponent::SyncTransformFromPhysics() {
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

}
}
