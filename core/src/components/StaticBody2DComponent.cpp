#include "lupine/components/StaticBody2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/physics2d/TransformSync2D.hpp"
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

void StaticBody2DComponent::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
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

void StaticBody2DComponent::OnPhysicsProcess(float) {
    if (!m_PhysicsBody) return;

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        // Compared in world space: an ancestor may have moved even when this node's local
        // transform is unchanged, and the body has to follow.
        const math::Vec2 globalPosition = node2D->GetGlobalPosition();
        const float globalRotation = node2D->GetGlobalRotation();

        if (globalPosition != m_LastPosition || globalRotation != m_LastRotation) {
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

    physicsWorld->SetBodyNode(m_PhysicsBodyId, m_Owner);

    RegisterCollisionCallbacks();

    m_BodyCreated = true;

}

void StaticBody2DComponent::DefineSignals() {
    RegisterSignal({"body_entered",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body starts colliding with this body."});
    RegisterSignal({"body_exited",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body stops colliding with this body."});
}

void StaticBody2DComponent::RegisterCollisionCallbacks() {
    auto* sceneManager = core::SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return;

    physicsWorld->RegisterCollisionEnter(m_PhysicsBodyId,
        [this](const physics2d::CollisionInfo& info) {
            OnCollisionEnterInternal(info);
        });

    physicsWorld->RegisterCollisionExit(m_PhysicsBodyId,
        [this](const physics2d::CollisionInfo& info) {
            OnCollisionExitInternal(info);
        });
}

void StaticBody2DComponent::OnCollisionEnterInternal(const physics2d::CollisionInfo& info) {
    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
    Emit("body_entered", { ResolveOtherBodyNodeArg(otherBodyId) });
}

void StaticBody2DComponent::OnCollisionExitInternal(const physics2d::CollisionInfo& info) {
    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
    Emit("body_exited", { ResolveOtherBodyNodeArg(otherBodyId) });
}

nlohmann::json StaticBody2DComponent::ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const {
    auto* sceneManager = core::SceneManager::GetInstance();
    if (sceneManager) {
        auto* physicsWorld = sceneManager->GetPhysics2DWorld();
        if (physicsWorld) {
            return core::Node::NodeArg(physicsWorld->GetBodyNode(otherBodyId));
        }
    }
    return nlohmann::json(nullptr);
}

void StaticBody2DComponent::DestroyPhysicsBody() {
    if (!m_BodyCreated) return;

    auto* sceneManager = core::SceneManager::GetInstance();

    bool isShuttingDown = (sceneManager && sceneManager->IsShuttingDown()) ||
                          (sceneManager && sceneManager->IsChangingScene()) ||
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

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) return;

    const math::Vec2 globalPosition = node2D->GetGlobalPosition();
    const float globalRotation = node2D->GetGlobalRotation();

    m_PhysicsBody->SetPosition(globalPosition);
    m_PhysicsBody->SetRotation(globalRotation);

    m_LastPosition = globalPosition;
    m_LastRotation = globalRotation;
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

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        node2D->SetPosition(physics2d::GlobalToLocalPosition(*node2D, position));
    }
}

void StaticBody2DComponent::SetRotation(float angle) {
    if (m_PhysicsBody) {
        m_PhysicsBody->SetRotation(angle);
        m_LastRotation = angle;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        node2D->SetRotation(physics2d::GlobalToLocalRotation(*node2D, angle));
    }
}

}
}

