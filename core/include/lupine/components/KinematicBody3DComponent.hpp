#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * KinematicBody3DComponent
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
 * - Automatic transform synchronization with Node3D
 * 
 * Note: Kinematic bodies are moved by setting their velocity, not by
 * applying forces. They will push dynamic bodies but won't be pushed back.
 */
class KinematicBody3DComponent : public core::Component {
public:
    KinematicBody3DComponent();
    explicit KinematicBody3DComponent(const std::string& name);
    virtual ~KinematicBody3DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "KinematicBody3DComponent"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Property Accessors =====
    
    // Gravity enabled (kinematic bodies don't use gravity by default)
    bool GetGravityEnabled() const;
    void SetGravityEnabled(bool enabled);
    
    // Gravity scale (multiplier for world gravity, only if gravity enabled)
    float GetGravityScale() const;
    void SetGravityScale(float scale);

    // ===== Physics API =====
    
    // Get the underlying physics body
    physics3d::RigidBody3D* GetPhysicsBody() const { return m_PhysicsBody; }
    
    // Get/Set velocity
    math::Vec3 GetLinearVelocity() const;
    void SetLinearVelocity(const math::Vec3& velocity);
    
    math::Vec3 GetAngularVelocity() const;
    void SetAngularVelocity(const math::Vec3& omega);
    
    // Move the body by a delta (convenience method)
    void MoveBy(const math::Vec3& delta);
    
    // Rotate the body by a delta rotation (convenience method)
    void RotateBy(const math::Quat& deltaRotation);

private:
    physics3d::RigidBody3D* m_PhysicsBody;
    core::UUID m_PhysicsBodyId;
    bool m_BodyCreated;
    
    // Cached transform for synchronization
    math::Vec3 m_LastPosition;
    math::Quat m_LastRotation;

    // Cached velocities so values set before the physics body exists are
    // preserved and applied once the body is created.
    math::Vec3 m_LinearVelocity = math::Vec3::Zero();
    math::Vec3 m_AngularVelocity = math::Vec3::Zero();

    // Create physics body in the physics world
    void CreatePhysicsBody();
    
    // Destroy physics body
    void DestroyPhysicsBody();
    
    // Synchronize transform from Node3D to physics body
    void SyncTransformToPhysics();
    
    // Synchronize transform from physics body to Node3D
    void SyncTransformFromPhysics();
    
    // Update gravity scale based on settings
    void UpdateGravityScale();
};

} // namespace components
} // namespace lupine

