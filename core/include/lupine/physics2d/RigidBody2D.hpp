#pragma once

#include "Physics2DWorld.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Transform.hpp"
#include <box2d/box2d.h>
#include <vector>
#include <memory>

namespace lupine {
namespace physics2d {

// Forward declaration
class Collider2D;
class Physics2DWorld;

/**
 * RigidBody2D
 * Represents a rigid body in 2D physics simulation
 */
class RigidBody2D {
public:
    RigidBody2D(Physics2DWorld* world, const core::UUID& id, BodyType type);
    ~RigidBody2D();

    // Identification
    const core::UUID& GetId() const { return m_Id; }

    // Body type
    BodyType GetBodyType() const { return m_Type; }
    void SetBodyType(BodyType type);

    // Transform
    void SetPosition(const math::Vec2& position);
    math::Vec2 GetPosition() const;

    void SetRotation(float angle);
    float GetRotation() const;

    void SetTransform(const math::Vec2& position, float angle);
    void SetTransform(const math::Transform& transform);
    math::Transform GetTransform() const;

    // Velocity
    void SetLinearVelocity(const math::Vec2& velocity);
    math::Vec2 GetLinearVelocity() const;

    void SetAngularVelocity(float omega);
    float GetAngularVelocity() const;

    // Forces and impulses
    void ApplyForce(const math::Vec2& force, const math::Vec2& point);
    void ApplyForceToCenter(const math::Vec2& force);
    void ApplyTorque(float torque);

    void ApplyLinearImpulse(const math::Vec2& impulse, const math::Vec2& point);
    void ApplyLinearImpulseToCenter(const math::Vec2& impulse);
    void ApplyAngularImpulse(float impulse);

    // Mass properties
    float GetMass() const;
    float GetInertia() const;
    math::Vec2 GetLocalCenter() const;
    math::Vec2 GetWorldCenter() const;

    // Damping
    void SetLinearDamping(float damping);
    float GetLinearDamping() const;

    void SetAngularDamping(float damping);
    float GetAngularDamping() const;

    // Gravity scale
    void SetGravityScale(float scale);
    float GetGravityScale() const;

    // Bullet mode (continuous collision detection)
    void SetBullet(bool flag);
    bool IsBullet() const;

    // Fixed rotation
    void SetFixedRotation(bool flag);
    bool IsFixedRotation() const;

    // Sleeping
    void SetSleepingAllowed(bool flag);
    bool IsSleepingAllowed() const;
    bool IsAwake() const;
    void SetAwake(bool flag);

    // Enabled state
    void SetEnabled(bool flag);
    bool IsEnabled() const;

    // Collider management
    void AddCollider(Collider2D* collider);
    void RemoveCollider(Collider2D* collider);
    const std::vector<Collider2D*>& GetColliders() const { return m_Colliders; }

    // Internal access
    b2BodyId GetBox2DBody() const { return m_BodyId; }
    Physics2DWorld* GetWorld() const { return m_World; }

private:
    Physics2DWorld* m_World;
    core::UUID m_Id;
    BodyType m_Type;
    b2BodyId m_BodyId;
    std::vector<Collider2D*> m_Colliders;

    b2BodyType ConvertBodyType(BodyType type) const;
    BodyType ConvertBodyType(b2BodyType type) const;
};

} // namespace physics2d
} // namespace lupine

