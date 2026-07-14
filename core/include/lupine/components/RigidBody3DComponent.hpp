#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * RigidBody3DComponent
 * 
 * A dynamic physics body that responds to forces and gravity in 3D space.
 * Suitable for objects that need realistic physics simulation like
 * falling objects, projectiles, or player characters.
 * 
 * Features:
 * - Full physics simulation with forces, impulses, and collisions
 * - Mass and inertia properties
 * - Gravity control (can be disabled per-body)
 * - Damping for linear and angular velocity
 * - Continuous collision detection (CCD)
 * - Lock rotation axes
 * - Automatic transform synchronization with Node3D
 */
class RigidBody3DComponent : public core::Component {
public:
    RigidBody3DComponent();
    explicit RigidBody3DComponent(const std::string& name);
    virtual ~RigidBody3DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "RigidBody3DComponent"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Property Accessors =====
    
    // Mass (kg)
    float GetMass() const;
    void SetMass(float mass);
    
    // Gravity scale (multiplier for world gravity)
    float GetGravityScale() const;
    void SetGravityScale(float scale);
    
    // Linear damping (air resistance)
    float GetLinearDamping() const;
    void SetLinearDamping(float damping);
    
    // Angular damping (rotational air resistance)
    float GetAngularDamping() const;
    void SetAngularDamping(float damping);
    
    // Lock rotation axes
    bool GetLockRotationX() const;
    void SetLockRotationX(bool lock);
    
    bool GetLockRotationY() const;
    void SetLockRotationY(bool lock);
    
    bool GetLockRotationZ() const;
    void SetLockRotationZ(bool lock);
    
    // Can sleep (allows body to sleep when at rest for performance)
    bool GetCanSleep() const;
    void SetCanSleep(bool canSleep);
    
    // Gravity enabled (per-body gravity toggle)
    bool GetGravityEnabled() const;
    void SetGravityEnabled(bool enabled);
    
    // Continuous collision detection
    bool GetUseCCD() const;
    void SetUseCCD(bool useCCD);

    // ===== Physics API =====
    
    // Get the underlying physics body
    physics3d::RigidBody3D* GetPhysicsBody() const { return m_PhysicsBody; }
    
    // Apply force at a point
    void ApplyForce(const math::Vec3& force, const math::Vec3& point);
    
    // Apply force at center of mass
    void ApplyForceToCenter(const math::Vec3& force);
    
    // Apply impulse at a point
    void ApplyImpulse(const math::Vec3& impulse, const math::Vec3& point);
    
    // Apply impulse at center of mass
    void ApplyImpulseToCenter(const math::Vec3& impulse);
    
    // Apply torque
    void ApplyTorque(const math::Vec3& torque);
    
    // Apply torque impulse
    void ApplyTorqueImpulse(const math::Vec3& torque);
    
    // Get/Set velocity
    math::Vec3 GetLinearVelocity() const;
    void SetLinearVelocity(const math::Vec3& velocity);
    
    math::Vec3 GetAngularVelocity() const;
    void SetAngularVelocity(const math::Vec3& omega);
    
    // Clear forces
    void ClearForces();

private:
    physics3d::RigidBody3D* m_PhysicsBody;
    core::UUID m_PhysicsBodyId;
    bool m_BodyCreated;

    // Cached transform for synchronization
    math::Vec3 m_LastPosition;
    math::Quat m_LastRotation;

    // Velocity captured just before a physics-world rebuild destroys the body. Unlike the
    // 2D bodies, this component keeps no velocity of its own - the body is the only store -
    // so it has to be parked here across the rebuild and re-applied to the fresh body, or an
    // overlay would lose all its momentum on a change_scene. See OnPhysicsWorldRebuild.
    math::Vec3 m_SavedLinearVelocity = math::Vec3(0.0f, 0.0f, 0.0f);
    math::Vec3 m_SavedAngularVelocity = math::Vec3(0.0f, 0.0f, 0.0f);
    
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
    
    // Update rotation locks
    void UpdateRotationLocks();
};

} // namespace components
} // namespace lupine

