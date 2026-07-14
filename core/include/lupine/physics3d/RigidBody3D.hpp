#pragma once

#include "Physics3DWorld.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Quat.hpp"
#include "lupine/math/Transform.hpp"
#include <vector>
#include <memory>

// Forward declare Bullet types
class btRigidBody;
class btCollisionShape;
class btCompoundShape;

namespace lupine {
namespace physics3d {

// Forward declaration
class Collider3D;
class Physics3DWorld;

/**
 * RigidBody3D
 * Represents a rigid body in 3D physics simulation
 */
class RigidBody3D {
public:
    RigidBody3D(Physics3DWorld* world, const core::UUID& id, BodyType type);
    ~RigidBody3D();

    // Identification
    const core::UUID& GetId() const { return m_Id; }

    // Body type
    BodyType GetBodyType() const { return m_Type; }
    void SetBodyType(BodyType type);

    // Transform
    void SetPosition(const math::Vec3& position);
    math::Vec3 GetPosition() const;

    void SetRotation(const math::Quat& rotation);
    math::Quat GetRotation() const;

    void SetTransform(const math::Vec3& position, const math::Quat& rotation);
    void SetTransform(const math::Transform& transform);
    math::Transform GetTransform() const;

    // Velocity
    void SetLinearVelocity(const math::Vec3& velocity);
    math::Vec3 GetLinearVelocity() const;

    void SetAngularVelocity(const math::Vec3& omega);
    math::Vec3 GetAngularVelocity() const;

    // Forces and impulses
    void ApplyForce(const math::Vec3& force, const math::Vec3& relativePos);
    void ApplyForceToCenter(const math::Vec3& force);
    void ApplyTorque(const math::Vec3& torque);

    void ApplyImpulse(const math::Vec3& impulse, const math::Vec3& relativePos);
    void ApplyImpulseToCenter(const math::Vec3& impulse);
    void ApplyTorqueImpulse(const math::Vec3& torque);

    void ClearForces();

    // Mass properties
    void SetMass(float mass);
    float GetMass() const;
    
    void SetInertia(const math::Vec3& inertia);
    math::Vec3 GetInertia() const;

    math::Vec3 GetCenterOfMass() const;

    // Damping
    void SetLinearDamping(float damping);
    float GetLinearDamping() const;

    void SetAngularDamping(float damping);
    float GetAngularDamping() const;

    void SetDamping(float linearDamping, float angularDamping);

    // Constraints
    void SetLinearFactor(const math::Vec3& factor);
    math::Vec3 GetLinearFactor() const;

    void SetAngularFactor(const math::Vec3& factor);
    math::Vec3 GetAngularFactor() const;

    // Gravity
    void SetGravityScale(float scale);
    float GetGravityScale() const;

    void SetUseGravity(bool useGravity);
    bool GetUseGravity() const;

    // Sleep state
    void SetSleepingEnabled(bool enabled);
    bool IsSleepingEnabled() const;
    bool IsAwake() const;
    void WakeUp();
    void PutToSleep();

    // Continuous collision detection
    void SetCcdMotionThreshold(float threshold);
    float GetCcdMotionThreshold() const;

    void SetCcdSweptSphereRadius(float radius);
    float GetCcdSweptSphereRadius() const;

    // Collider management
    void AddCollider(Collider3D* collider);
    void RemoveCollider(Collider3D* collider);
    const std::vector<Collider3D*>& GetColliders() const { return m_Colliders; }

    // The body's collision shape is always a compound whose children are the shapes of
    // its enabled colliders, positioned by each collider's offset/rotation offset.
    // Colliders call these when their shape, offset or enabled state changes.
    void RebuildCompoundShape();
    void DetachColliderShape(btCollisionShape* shape);
    btCompoundShape* GetCompoundShape() const { return m_CompoundShape; }

    // True once the destructor has started; colliders use it to skip compound
    // maintenance while the body is tearing them down.
    bool IsDestroying() const { return m_Destroying; }

    // Collision filtering
    void UpdateCollisionFiltering(uint32_t group, uint32_t mask);

    // Internal access
    btRigidBody* GetBulletBody() const { return m_RigidBody; }
    Physics3DWorld* GetWorld() const { return m_World; }

private:
    Physics3DWorld* m_World;
    core::UUID m_Id;
    BodyType m_Type;
    btRigidBody* m_RigidBody;
    btCompoundShape* m_CompoundShape;
    std::vector<Collider3D*> m_Colliders;
    float m_GravityScale;
    bool m_Destroying;

    void UpdateMassProperties();
    void RefreshBroadphaseAabb();
};

} // namespace physics3d
} // namespace lupine

