#include "lupine/components/AreaTrigger3DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics3d;

AreaTrigger3DComponent::AreaTrigger3DComponent()
    : Component("AreaTrigger3DComponent")
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

AreaTrigger3DComponent::AreaTrigger3DComponent(const std::string& name)
    : Component(name)
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

AreaTrigger3DComponent::~AreaTrigger3DComponent() {
    DestroyPhysicsBody();
}

void AreaTrigger3DComponent::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(monitoring, Bool, true, "Trigger"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(monitorable, Bool, true, "Trigger"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(priority, 0, 0, 100, 1, "Trigger"));
}

void AreaTrigger3DComponent::OnAwake() {
    CreatePhysicsBody();
}

void AreaTrigger3DComponent::OnReady() {

    if (m_PhysicsBody) {
        SyncTransformToPhysics();
    }
}

void AreaTrigger3DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void AreaTrigger3DComponent::OnPhysicsProcess(float deltaTime) {
    if (!m_PhysicsBody) return;

    SyncTransformFromPhysics();
}

bool AreaTrigger3DComponent::GetMonitoring() const {
    return GetPropertyValue<bool>("monitoring");
}

void AreaTrigger3DComponent::SetMonitoring(bool monitoring) {
    SetPropertyValue("monitoring", monitoring);
}

bool AreaTrigger3DComponent::GetMonitorable() const {
    return GetPropertyValue<bool>("monitorable");
}

void AreaTrigger3DComponent::SetMonitorable(bool monitorable) {
    SetPropertyValue("monitorable", monitorable);
}

int AreaTrigger3DComponent::GetPriority() const {
    return GetPropertyValue<int>("priority");
}

void AreaTrigger3DComponent::SetPriority(int priority) {
    SetPropertyValue("priority", priority);
}

bool AreaTrigger3DComponent::IsOverlapping(const UUID& bodyId) const {
    return m_OverlappingBodies.find(bodyId) != m_OverlappingBodies.end();
}

void AreaTrigger3DComponent::CreatePhysicsBody() {
    if (m_BodyCreated) return;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) {

        return;
    }

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) {

        return;
    }

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, BodyType::Static);
    if (!m_PhysicsBody) {

        return;
    }

    physicsWorld->RegisterTriggerEnter(m_PhysicsBodyId,
        [this](const physics3d::CollisionInfo3D& info) {

            UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
            OnTriggerEnter(otherBodyId);
        });

    physicsWorld->RegisterTriggerExit(m_PhysicsBodyId,
        [this](const physics3d::CollisionInfo3D& info) {

            UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
            OnTriggerExit(otherBodyId);
        });

    m_BodyCreated = true;

}

void AreaTrigger3DComponent::DestroyPhysicsBody() {
    if (!m_BodyCreated) return;

    auto* sceneManager = SceneManager::GetInstance();

    if (sceneManager && !sceneManager->IsShuttingDown()) {
        auto* physicsWorld = sceneManager->GetPhysics3DWorld();
        if (physicsWorld) {
            physicsWorld->DestroyBody(m_PhysicsBodyId);
        }
    }

    m_PhysicsBody = nullptr;
    m_BodyCreated = false;
    m_OverlappingBodies.clear();
}

void AreaTrigger3DComponent::SyncTransformToPhysics() {
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

void AreaTrigger3DComponent::SyncTransformFromPhysics() {
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

void AreaTrigger3DComponent::OnTriggerEnter(const UUID& otherBodyId) {
    if (!GetMonitoring()) return;

    m_OverlappingBodies.insert(otherBodyId);

    if (m_OnBodyEntered) {
        m_OnBodyEntered(otherBodyId);
    }

}

void AreaTrigger3DComponent::OnTriggerExit(const UUID& otherBodyId) {
    if (!GetMonitoring()) return;

    m_OverlappingBodies.erase(otherBodyId);

    if (m_OnBodyExited) {
        m_OnBodyExited(otherBodyId);
    }

}

}
}
