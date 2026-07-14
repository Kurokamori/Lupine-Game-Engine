#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include <memory>

namespace lupine {
namespace components {

/**
 * KinematicBody2DComponent
 * 
 * A kinematic physics body that is moved by setting velocity directly.
 * Suitable for player-controlled characters, moving platforms, and
 * objects that need precise movement control.
 * 
 * Features:
 * - Zero mass (not affected by forces)
 * - Velocity-based movement
 * - Can push dynamic bodies
 * - Not affected by gravity (unless explicitly enabled)
 * - Precise control over movement
 * - Automatic transform synchronization with Node2D
 * 
 * Note: Kinematic bodies are moved by setting their velocity, not by
 * applying forces. They will push dynamic bodies but won't be pushed back.
 */
class KinematicBody2DComponent : public core::Component {
public:
    KinematicBody2DComponent();
    explicit KinematicBody2DComponent(const std::string& name);
    virtual ~KinematicBody2DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "KinematicBody2DComponent"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Property Accessors =====
    
    // Gravity enabled (kinematic bodies don't respond to gravity by default)
    bool GetGravityEnabled() const;
    void SetGravityEnabled(bool enabled);
    
    // Gravity scale (if gravity is enabled)
    float GetGravityScale() const;
    void SetGravityScale(float scale);

    // ===== Physics API =====
    
    // Get the underlying physics body
    physics2d::RigidBody2D* GetPhysicsBody() const { return m_PhysicsBody; }
    
    // Get/Set velocity
    math::Vec2 GetLinearVelocity() const;
    void SetLinearVelocity(const math::Vec2& velocity);
    
    float GetAngularVelocity() const;
    void SetAngularVelocity(float omega);
    
    // Move the body by a delta (convenience method)
    void MoveBy(const math::Vec2& delta);
    
    // Rotate the body by a delta angle (convenience method)
    void RotateBy(float deltaAngle);

private:
    physics2d::RigidBody2D* m_PhysicsBody;
    core::UUID m_PhysicsBodyId;
    bool m_BodyCreated;
    
    // Cached transform for synchronization
    math::Vec2 m_LastPosition;
    float m_LastRotation;

    // Cached velocities so values set before the physics body exists are
    // preserved and applied once the body is created.
    math::Vec2 m_LinearVelocity = math::Vec2(0.0f, 0.0f);
    float m_AngularVelocity = 0.0f;

    // Create physics body in the physics world
    void CreatePhysicsBody();
    
    // Destroy physics body
    void DestroyPhysicsBody();
    
    // Synchronize transform from Node2D to physics body
    void SyncTransformToPhysics();
    
    // Synchronize transform from physics body to Node2D
    void SyncTransformFromPhysics();
    
    // Update gravity scale based on settings
    void UpdateGravityScale();

    // Wire physics-world collision callbacks to the body_entered/body_exited signals.
    void RegisterCollisionCallbacks();
    void OnCollisionEnterInternal(const physics2d::CollisionInfo& info);
    void OnCollisionExitInternal(const physics2d::CollisionInfo& info);
    nlohmann::json ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const;
};

} // namespace components
} // namespace lupine

