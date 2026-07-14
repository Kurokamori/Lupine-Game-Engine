#include "lupine/components/RigidBody2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/physics2d/TransformSync2D.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

RigidBody2DComponent::RigidBody2DComponent()
    : Component("RigidBody2DComponent"),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

RigidBody2DComponent::RigidBody2DComponent(const std::string& name)
    : Component(name),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

RigidBody2DComponent::~RigidBody2DComponent() {
    DestroyPhysicsBody();
}

void RigidBody2DComponent::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(mass, 1.0f, 0.01f, 1000.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gravityScale, 1.0f, -10.0f, 10.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(linearDamping, 0.0f, 0.0f, 10.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(angularDamping, 0.0f, 0.0f, 10.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fixedRotation, Bool, false, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(bullet, Bool, false, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(canSleep, Bool, true, "Physics"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(gravityEnabled, Bool, true, "Physics"));
}

void RigidBody2DComponent::OnAwake() {
    CreatePhysicsBody();
}

void RigidBody2DComponent::OnReady() {

    if (m_PhysicsBody) {
        SyncTransformToPhysics();
    }
}

void RigidBody2DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void RigidBody2DComponent::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
    switch (phase) {
        case PhysicsWorldRebuildPhase::SaveState:
            // The body owns the live velocity (the members below are only the seed
            // CreatePhysicsBody re-applies), so capture it before the body is destroyed or
            // an overlay would lose all its momentum across a change_scene.
            if (m_PhysicsBody) {
                m_LinearVelocity = m_PhysicsBody->GetLinearVelocity();
                m_AngularVelocity = m_PhysicsBody->GetAngularVelocity();
            }
            break;

        case PhysicsWorldRebuildPhase::RecreateBodies:
            // The world was cleared, so the body is already gone - drop the stale handles
            // directly rather than going through DestroyPhysicsBody, which would call
            // DestroyBody on a body that no longer exists. CreatePhysicsBody re-seeds the
            // velocity captured above.
            m_PhysicsBody = nullptr;
            m_BodyCreated = false;
            CreatePhysicsBody();
            break;

        case PhysicsWorldRebuildPhase::AttachColliders:
            // Mirrors OnReady: put the fresh body at the node's current world transform.
            if (m_PhysicsBody) {
                SyncTransformToPhysics();
            }
            break;
    }
}

void RigidBody2DComponent::OnPhysicsProcess(float) {
    if (!m_PhysicsBody) return;

    SyncTransformFromPhysics();
}

void RigidBody2DComponent::CreatePhysicsBody() {
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

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, physics2d::BodyType::Dynamic);
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

void RigidBody2DComponent::DefineSignals() {
    RegisterSignal({"body_entered",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body starts colliding with this body."});
    RegisterSignal({"body_exited",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body stops colliding with this body."});
}

void RigidBody2DComponent::RegisterCollisionCallbacks() {
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

void RigidBody2DComponent::OnCollisionEnterInternal(const physics2d::CollisionInfo& info) {
    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
    Emit("body_entered", { ResolveOtherBodyNodeArg(otherBodyId) });
}

void RigidBody2DComponent::OnCollisionExitInternal(const physics2d::CollisionInfo& info) {
    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
    Emit("body_exited", { ResolveOtherBodyNodeArg(otherBodyId) });
}

nlohmann::json RigidBody2DComponent::ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const {
    auto* sceneManager = core::SceneManager::GetInstance();
    if (sceneManager) {
        auto* physicsWorld = sceneManager->GetPhysics2DWorld();
        if (physicsWorld) {
            return core::Node::NodeArg(physicsWorld->GetBodyNode(otherBodyId));
        }
    }
    return nlohmann::json(nullptr);
}

void RigidBody2DComponent::DestroyPhysicsBody() {
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

void RigidBody2DComponent::SyncTransformToPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) return;

    // The body lives in world space; m_LastPosition/m_LastRotation cache the global transform.
    const math::Vec2 globalPosition = node2D->GetGlobalPosition();
    const float globalRotation = node2D->GetGlobalRotation();

    m_PhysicsBody->SetPosition(globalPosition);
    m_PhysicsBody->SetRotation(globalRotation);

    m_LastPosition = globalPosition;
    m_LastRotation = globalRotation;
}

void RigidBody2DComponent::SyncTransformFromPhysics() {
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

void RigidBody2DComponent::UpdateGravityScale() {
    if (!m_PhysicsBody) return;

    bool gravityEnabled = GetPropertyValue<bool>("gravityEnabled");
    float gravityScale = GetPropertyValue<float>("gravityScale");

    m_PhysicsBody->SetGravityScale(gravityEnabled ? gravityScale : 0.0f);
}

float RigidBody2DComponent::GetMass() const {

    return m_PhysicsBody ? m_PhysicsBody->GetMass() : 0.0f;
}

float RigidBody2DComponent::GetGravityScale() const {
    return GetPropertyValue<float>("gravityScale");
}

void RigidBody2DComponent::SetGravityScale(float scale) {
    SetPropertyValue("gravityScale", scale);
    UpdateGravityScale();
}

float RigidBody2DComponent::GetLinearDamping() const {
    return m_PhysicsBody ? m_PhysicsBody->GetLinearDamping() : GetPropertyValue<float>("linearDamping");
}

void RigidBody2DComponent::SetLinearDamping(float damping) {
    SetPropertyValue("linearDamping", damping);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetLinearDamping(damping);
    }
}

float RigidBody2DComponent::GetAngularDamping() const {
    return m_PhysicsBody ? m_PhysicsBody->GetAngularDamping() : GetPropertyValue<float>("angularDamping");
}

void RigidBody2DComponent::SetAngularDamping(float damping) {
    SetPropertyValue("angularDamping", damping);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetAngularDamping(damping);
    }
}

bool RigidBody2DComponent::GetFixedRotation() const {
    return m_PhysicsBody ? m_PhysicsBody->IsFixedRotation() : GetPropertyValue<bool>("fixedRotation");
}

void RigidBody2DComponent::SetFixedRotation(bool fixed) {
    SetPropertyValue("fixedRotation", fixed);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetFixedRotation(fixed);
    }
}

bool RigidBody2DComponent::GetBullet() const {
    return m_PhysicsBody ? m_PhysicsBody->IsBullet() : GetPropertyValue<bool>("bullet");
}

void RigidBody2DComponent::SetBullet(bool bullet) {
    SetPropertyValue("bullet", bullet);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetBullet(bullet);
    }
}

bool RigidBody2DComponent::GetCanSleep() const {
    return m_PhysicsBody ? m_PhysicsBody->IsSleepingAllowed() : GetPropertyValue<bool>("canSleep");
}

void RigidBody2DComponent::SetCanSleep(bool canSleep) {
    SetPropertyValue("canSleep", canSleep);
    if (m_PhysicsBody) {
        m_PhysicsBody->SetSleepingAllowed(canSleep);
    }
}

bool RigidBody2DComponent::GetGravityEnabled() const {
    return GetPropertyValue<bool>("gravityEnabled");
}

void RigidBody2DComponent::SetGravityEnabled(bool enabled) {
    SetPropertyValue("gravityEnabled", enabled);
    UpdateGravityScale();
}

void RigidBody2DComponent::ApplyForce(const math::Vec2& force, const math::Vec2& point) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyForce(force, point);
    }
}

void RigidBody2DComponent::ApplyForceToCenter(const math::Vec2& force) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyForceToCenter(force);
    }
}

void RigidBody2DComponent::ApplyTorque(float torque) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyTorque(torque);
    }
}

void RigidBody2DComponent::ApplyLinearImpulse(const math::Vec2& impulse, const math::Vec2& point) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyLinearImpulse(impulse, point);
    }
}

void RigidBody2DComponent::ApplyLinearImpulseToCenter(const math::Vec2& impulse) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyLinearImpulseToCenter(impulse);
    }
}

void RigidBody2DComponent::ApplyAngularImpulse(float impulse) {
    if (m_PhysicsBody) {
        m_PhysicsBody->ApplyAngularImpulse(impulse);
    }
}

math::Vec2 RigidBody2DComponent::GetLinearVelocity() const {
    return m_PhysicsBody ? m_PhysicsBody->GetLinearVelocity() : m_LinearVelocity;
}

void RigidBody2DComponent::SetLinearVelocity(const math::Vec2& velocity) {
    m_LinearVelocity = velocity;
    if (m_PhysicsBody) {
        m_PhysicsBody->SetLinearVelocity(velocity);
    }
}

float RigidBody2DComponent::GetAngularVelocity() const {
    return m_PhysicsBody ? m_PhysicsBody->GetAngularVelocity() : m_AngularVelocity;
}

void RigidBody2DComponent::SetAngularVelocity(float omega) {
    m_AngularVelocity = omega;
    if (m_PhysicsBody) {
        m_PhysicsBody->SetAngularVelocity(omega);
    }
}

}
}

