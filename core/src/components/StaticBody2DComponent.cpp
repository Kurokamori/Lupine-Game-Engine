#include "lupine/components/StaticBody2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

StaticBody2DComponent::StaticBody2DComponent()
    : Component("StaticBody2DComponent"),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

StaticBody2DComponent::StaticBody2DComponent(const std::string& name)
    : Component(name),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

StaticBody2DComponent::~StaticBody2DComponent() {
    DestroyPhysicsBody();
}

void StaticBody2DComponent::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(constantLinearVelocity, Vec2, math::Vec2(0.0f, 0.0f), "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(constantAngularVelocity, 0.0f, -10.0f, 10.0f, 0.1f, "Physics"));
}

void StaticBody2DComponent::OnAwake() {

    CreatePhysicsBody();
}

void StaticBody2DComponent::OnReady() {

    if (m_PhysicsBody) {
        SyncTransformToPhysics();
    }
}

void StaticBody2DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void StaticBody2DComponent::OnPhysicsProcess(float deltaTime) {
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

void StaticBody2DComponent::CreatePhysicsBody() {
    if (m_BodyCreated) {

        return;
    }

    auto* sceneManager = core::SceneManager::GetInstance();
    if (!sceneManager) {

        return;
    }

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) {

        return;
    }

    m_PhysicsBodyId = core::UUID();

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, physics2d::BodyType::Static);
    if (!m_PhysicsBody) {

        return;
    }

    m_BodyCreated = true;

}

void StaticBody2DComponent::DestroyPhysicsBody() {
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
}

void StaticBody2DComponent::SyncTransformToPhysics() {
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

void StaticBody2DComponent::SyncTransformFromPhysics() {

}

math::Vec2 StaticBody2DComponent::GetConstantLinearVelocity() const {
    return GetPropertyValue<math::Vec2>("constantLinearVelocity");
}

void StaticBody2DComponent::SetConstantLinearVelocity(const math::Vec2& velocity) {
    SetPropertyValue("constantLinearVelocity", velocity);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetLinearVelocity(velocity);
    }
}

float StaticBody2DComponent::GetConstantAngularVelocity() const {
    return GetPropertyValue<float>("constantAngularVelocity");
}

void StaticBody2DComponent::SetConstantAngularVelocity(float velocity) {
    SetPropertyValue("constantAngularVelocity", velocity);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetAngularVelocity(velocity);
    }
}

void StaticBody2DComponent::SetPosition(const math::Vec2& position) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetPosition(position);
        m_LastPosition = position;
    }

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        node2D->SetPosition(position);
    }
}

void StaticBody2DComponent::SetRotation(float angle) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetRotation(angle);
        m_LastRotation = angle;
    }

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        node2D->SetRotation(angle);
    }
}

}
}

