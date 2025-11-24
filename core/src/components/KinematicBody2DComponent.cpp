#include "lupine/components/KinematicBody2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

KinematicBody2DComponent::KinematicBody2DComponent()
    : Component("KinematicBody2DComponent"),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

KinematicBody2DComponent::KinematicBody2DComponent(const std::string& name)
    : Component(name),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

KinematicBody2DComponent::~KinematicBody2DComponent() {
    DestroyPhysicsBody();
}

void KinematicBody2DComponent::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(gravityEnabled, Bool, false, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gravityScale, 1.0f, -10.0f, 10.0f, 0.1f, "Physics"));
}

void KinematicBody2DComponent::OnAwake() {
    CreatePhysicsBody();
}

void KinematicBody2DComponent::OnReady() {

    if (m_PhysicsBody) {
        SyncTransformToPhysics();
    }
}

void KinematicBody2DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void KinematicBody2DComponent::OnPhysicsProcess(float deltaTime) {
    if (!m_PhysicsBody) return;

    SyncTransformFromPhysics();
}

void KinematicBody2DComponent::CreatePhysicsBody() {
    if (m_BodyCreated) return;

    auto* sceneManager = core::SceneManager::GetInstance();
    if (!sceneManager) {

        return;
    }

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) {

        return;
    }

    m_PhysicsBodyId = core::UUID();

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, physics2d::BodyType::Kinematic);
    if (!m_PhysicsBody) {

        return;
    }

    m_BodyCreated = true;

    UpdateGravityScale();
}

void KinematicBody2DComponent::DestroyPhysicsBody() {
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

void KinematicBody2DComponent::SyncTransformToPhysics() {
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

void KinematicBody2DComponent::SyncTransformFromPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) return;

    math::Vec2 position = m_PhysicsBody->GetPosition();
    float rotation = m_PhysicsBody->GetRotation();

    if (position != m_LastPosition || rotation != m_LastRotation) {
        node2D->SetPosition(position);
        node2D->SetRotation(rotation);

        m_LastPosition = position;
        m_LastRotation = rotation;
    }
}

void KinematicBody2DComponent::UpdateGravityScale() {
    if (!m_PhysicsBody) return;

    bool gravityEnabled = GetPropertyValue<bool>("gravityEnabled");
    float gravityScale = GetPropertyValue<float>("gravityScale");

    m_PhysicsBody->SetGravityScale(gravityEnabled ? gravityScale : 0.0f);
}

bool KinematicBody2DComponent::GetGravityEnabled() const {
    return GetPropertyValue<bool>("gravityEnabled");
}

void KinematicBody2DComponent::SetGravityEnabled(bool enabled) {
    SetPropertyValue("gravityEnabled", enabled);
    UpdateGravityScale();
}

float KinematicBody2DComponent::GetGravityScale() const {
    return GetPropertyValue<float>("gravityScale");
}

void KinematicBody2DComponent::SetGravityScale(float scale) {
    SetPropertyValue("gravityScale", scale);
    UpdateGravityScale();
}

math::Vec2 KinematicBody2DComponent::GetLinearVelocity() const {
    return m_PhysicsBody ? m_PhysicsBody->GetLinearVelocity() : math::Vec2(0.0f, 0.0f);
}

void KinematicBody2DComponent::SetLinearVelocity(const math::Vec2& velocity) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetLinearVelocity(velocity);
    }
}

float KinematicBody2DComponent::GetAngularVelocity() const {
    return m_PhysicsBody ? m_PhysicsBody->GetAngularVelocity() : 0.0f;
}

void KinematicBody2DComponent::SetAngularVelocity(float omega) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetAngularVelocity(omega);
    }
}

void KinematicBody2DComponent::MoveBy(const math::Vec2& delta) {
    if (!m_PhysicsBody) return;

    math::Vec2 currentPos = m_PhysicsBody->GetPosition();
    m_PhysicsBody->SetPosition(currentPos + delta);
}

void KinematicBody2DComponent::RotateBy(float deltaAngle) {
    if (!m_PhysicsBody) return;

    float currentRot = m_PhysicsBody->GetRotation();
    m_PhysicsBody->SetRotation(currentRot + deltaAngle);
}

}
}

