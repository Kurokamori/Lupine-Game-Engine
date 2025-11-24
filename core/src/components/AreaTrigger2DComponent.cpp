#include "lupine/components/AreaTrigger2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

AreaTrigger2DComponent::AreaTrigger2DComponent()
    : Component("AreaTrigger2DComponent"),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

AreaTrigger2DComponent::AreaTrigger2DComponent(const std::string& name)
    : Component(name),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

AreaTrigger2DComponent::~AreaTrigger2DComponent() {
    DestroyPhysicsBody();
}

void AreaTrigger2DComponent::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(monitoring, Bool, true, "Trigger"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(monitorable, Bool, true, "Trigger"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(priority, 0, 0, 100, 1, "Trigger"));
}

void AreaTrigger2DComponent::OnAwake() {
    CreatePhysicsBody();
}

void AreaTrigger2DComponent::OnReady() {

    if (m_PhysicsBody) {
        SyncTransformToPhysics();
    }
}

void AreaTrigger2DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void AreaTrigger2DComponent::OnPhysicsProcess(float deltaTime) {
    if (!m_PhysicsBody) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        math::Vec2 nodePos = node2D->GetPosition();
        float nodeRot = node2D->GetRotation();

        if (nodePos != m_LastPosition || nodeRot != m_LastRotation) {
            SyncTransformToPhysics();
        }
    }
}

void AreaTrigger2DComponent::CreatePhysicsBody() {
    if (m_BodyCreated) return;

    auto* sceneManager = core::SceneManager::GetInstance();
    if (!sceneManager) {

        return;
    }

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) {

        return;
    }

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, physics2d::BodyType::Static);
    if (!m_PhysicsBody) {

        return;
    }

    m_BodyCreated = true;
}

void AreaTrigger2DComponent::DestroyPhysicsBody() {
    if (!m_BodyCreated) return;

    auto* sceneManager = core::SceneManager::GetInstance();

    bool isShuttingDown = (sceneManager && sceneManager->IsShuttingDown()) ||
                          (m_Owner && m_Owner->GetScene() && m_Owner->GetScene()->IsShuttingDown());

    if (sceneManager && !isShuttingDown) {
        auto* physicsWorld = sceneManager->GetPhysics2DWorld();
        if (physicsWorld) {
            physicsWorld->DestroyBody(m_PhysicsBodyId);
        }
    }

    m_PhysicsBody = nullptr;
    m_BodyCreated = false;
    m_OverlappingBodies.clear();
}

void AreaTrigger2DComponent::SyncTransformToPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) return;

    math::Vec2 position = node2D->GetPosition();
    float rotation = node2D->GetRotation();

    m_PhysicsBody->SetPosition(position);
    m_PhysicsBody->SetRotation(rotation);

    m_LastPosition = position;
    m_LastRotation = rotation;
}

void AreaTrigger2DComponent::SyncTransformFromPhysics() {

}

bool AreaTrigger2DComponent::GetMonitoring() const {
    return GetPropertyValue<bool>("monitoring");
}

void AreaTrigger2DComponent::SetMonitoring(bool monitoring) {
    SetPropertyValue("monitoring", monitoring);
}

bool AreaTrigger2DComponent::GetMonitorable() const {
    return GetPropertyValue<bool>("monitorable");
}

void AreaTrigger2DComponent::SetMonitorable(bool monitorable) {
    SetPropertyValue("monitorable", monitorable);
}

int AreaTrigger2DComponent::GetPriority() const {
    return GetPropertyValue<int>("priority");
}

void AreaTrigger2DComponent::SetPriority(int priority) {
    SetPropertyValue("priority", priority);
}

bool AreaTrigger2DComponent::IsOverlapping(const core::UUID& bodyId) const {
    return std::find(m_OverlappingBodies.begin(), m_OverlappingBodies.end(), bodyId) != m_OverlappingBodies.end();
}

void AreaTrigger2DComponent::OnTriggerEnterInternal(const physics2d::CollisionInfo& info) {
    if (!GetMonitoring()) return;

    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;

    if (!IsOverlapping(otherBodyId)) {
        m_OverlappingBodies.push_back(otherBodyId);
    }

    if (m_OnTriggerEnter) {
        m_OnTriggerEnter(info);
    }
}

void AreaTrigger2DComponent::OnTriggerStayInternal(const physics2d::CollisionInfo& info) {
    if (!GetMonitoring()) return;

    if (m_OnTriggerStay) {
        m_OnTriggerStay(info);
    }
}

void AreaTrigger2DComponent::OnTriggerExitInternal(const physics2d::CollisionInfo& info) {
    if (!GetMonitoring()) return;

    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;

    auto it = std::find(m_OverlappingBodies.begin(), m_OverlappingBodies.end(), otherBodyId);
    if (it != m_OverlappingBodies.end()) {
        m_OverlappingBodies.erase(it);
    }

    if (m_OnTriggerExit) {
        m_OnTriggerExit(info);
    }
}

}
}

