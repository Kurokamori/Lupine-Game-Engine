#include "lupine/components/KinematicBody2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/physics2d/TransformSync2D.hpp"
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

void KinematicBody2DComponent::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
    // See Component::PhysicsWorldRebuildPhase: only reached by a component that outlives a
    // scene swap, i.e. one inside an add_scene overlay.
    switch (phase) {
        case PhysicsWorldRebuildPhase::SaveState:
            // The body holds the authoritative velocity; m_LinearVelocity is only the seed
            // CreatePhysicsBody re-applies. Capture it before the body dies.
            if (m_PhysicsBody) {
                m_LinearVelocity = m_PhysicsBody->GetLinearVelocity();
                m_AngularVelocity = m_PhysicsBody->GetAngularVelocity();
            }
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

void KinematicBody2DComponent::OnPhysicsProcess(float) {
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

    physicsWorld->SetBodyNode(m_PhysicsBodyId, m_Owner);

    RegisterCollisionCallbacks();

    m_BodyCreated = true;

    m_PhysicsBody->SetLinearVelocity(m_LinearVelocity);
    m_PhysicsBody->SetAngularVelocity(m_AngularVelocity);

    UpdateGravityScale();
}

void KinematicBody2DComponent::DefineSignals() {
    RegisterSignal({"body_entered",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body starts colliding with this body."});
    RegisterSignal({"body_exited",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body stops colliding with this body."});
}

void KinematicBody2DComponent::RegisterCollisionCallbacks() {
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

void KinematicBody2DComponent::OnCollisionEnterInternal(const physics2d::CollisionInfo& info) {
    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
    Emit("body_entered", { ResolveOtherBodyNodeArg(otherBodyId) });
}

void KinematicBody2DComponent::OnCollisionExitInternal(const physics2d::CollisionInfo& info) {
    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
    Emit("body_exited", { ResolveOtherBodyNodeArg(otherBodyId) });
}

nlohmann::json KinematicBody2DComponent::ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const {
    auto* sceneManager = core::SceneManager::GetInstance();
    if (sceneManager) {
        auto* physicsWorld = sceneManager->GetPhysics2DWorld();
        if (physicsWorld) {
            return core::Node::NodeArg(physicsWorld->GetBodyNode(otherBodyId));
        }
    }
    return nlohmann::json(nullptr);
}

void KinematicBody2DComponent::DestroyPhysicsBody() {
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

void KinematicBody2DComponent::SyncTransformToPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) return;

    // The body lives in world space; the character controller and every 2D query read it there.
    const math::Vec2 globalPosition = node2D->GetGlobalPosition();
    const float globalRotation = node2D->GetGlobalRotation();

    m_PhysicsBody->SetPosition(globalPosition);
    m_PhysicsBody->SetRotation(globalRotation);

    m_LastPosition = globalPosition;
    m_LastRotation = globalRotation;
}

void KinematicBody2DComponent::SyncTransformFromPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) return;

    const math::Vec2 globalPosition = m_PhysicsBody->GetPosition();
    const float globalRotation = m_PhysicsBody->GetRotation();

    if (globalPosition != m_LastPosition || globalRotation != m_LastRotation) {
        node2D->SetPosition(physics2d::GlobalToLocalPosition(*node2D, globalPosition));
        node2D->SetRotation(physics2d::GlobalToLocalRotation(*node2D, globalRotation));

        m_LastPosition = globalPosition;
        m_LastRotation = globalRotation;
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
    return m_PhysicsBody ? m_PhysicsBody->GetLinearVelocity() : m_LinearVelocity;
}

void KinematicBody2DComponent::SetLinearVelocity(const math::Vec2& velocity) {
    m_LinearVelocity = velocity;
    if (m_PhysicsBody) {
        m_PhysicsBody->SetLinearVelocity(velocity);
    }
}

float KinematicBody2DComponent::GetAngularVelocity() const {
    return m_PhysicsBody ? m_PhysicsBody->GetAngularVelocity() : m_AngularVelocity;
}

void KinematicBody2DComponent::SetAngularVelocity(float omega) {
    m_AngularVelocity = omega;
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

