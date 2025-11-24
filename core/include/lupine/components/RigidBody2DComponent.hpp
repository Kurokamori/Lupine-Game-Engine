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
 * RigidBody2DComponent
 * 
 * A dynamic physics body that responds to forces and gravity.
 * Suitable for objects that need realistic physics simulation like
 * falling objects, projectiles, or player characters.
 * 
 * Features:
 * - Full physics simulation with forces, impulses, and collisions
 * - Mass and inertia properties
 * - Gravity control (can be disabled per-body)
 * - Damping for linear and angular velocity
 * - Continuous collision detection (bullet mode)
 * - Fixed rotation option
 * - Automatic transform synchronization with Node2D
 */
class RigidBody2DComponent : public core::Component {
public:
    RigidBody2DComponent();
    explicit RigidBody2DComponent(const std::string& name);
    virtual ~RigidBody2DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "RigidBody2DComponent"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Property Accessors =====
    
    // Mass (kg) - Read-only, calculated from colliders
    float GetMass() const;
    
    // Gravity scale (1.0 = normal gravity, 0.0 = no gravity)
    float GetGravityScale() const;
    void SetGravityScale(float scale);
    
    // Linear damping (resistance to linear motion)
    float GetLinearDamping() const;
    void SetLinearDamping(float damping);
    
    // Angular damping (resistance to rotation)
    float GetAngularDamping() const;
    void SetAngularDamping(float damping);
    
    // Fixed rotation (prevents rotation)
    bool GetFixedRotation() const;
    void SetFixedRotation(bool fixed);
    
    // Bullet mode (continuous collision detection for fast-moving objects)
    bool GetBullet() const;
    void SetBullet(bool bullet);
    
    // Can sleep (allows body to sleep when at rest for performance)
    bool GetCanSleep() const;
    void SetCanSleep(bool canSleep);
    
    // Gravity enabled (per-body gravity toggle)
    bool GetGravityEnabled() const;
    void SetGravityEnabled(bool enabled);

    // ===== Physics API =====
    
    // Get the underlying physics body
    physics2d::RigidBody2D* GetPhysicsBody() const { return m_PhysicsBody; }
    
    // Apply force at a point
    void ApplyForce(const math::Vec2& force, const math::Vec2& point);
    
    // Apply force at center
    void ApplyForceToCenter(const math::Vec2& force);
    
    // Apply torque
    void ApplyTorque(float torque);
    
    // Apply impulse at a point
    void ApplyLinearImpulse(const math::Vec2& impulse, const math::Vec2& point);
    
    // Apply impulse at center
    void ApplyLinearImpulseToCenter(const math::Vec2& impulse);
    
    // Apply angular impulse
    void ApplyAngularImpulse(float impulse);
    
    // Get/Set velocity
    math::Vec2 GetLinearVelocity() const;
    void SetLinearVelocity(const math::Vec2& velocity);
    
    float GetAngularVelocity() const;
    void SetAngularVelocity(float omega);

private:
    physics2d::RigidBody2D* m_PhysicsBody;
    core::UUID m_PhysicsBodyId;
    bool m_BodyCreated;
    
    // Cached transform for synchronization
    math::Vec2 m_LastPosition;
    float m_LastRotation;
    
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
};

} // namespace components
} // namespace lupine

