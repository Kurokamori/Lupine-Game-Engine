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
 * StaticBody2DComponent
 * 
 * A static physics body that doesn't move and has infinite mass.
 * Suitable for walls, floors, platforms, and other immovable objects.
 * 
 * Features:
 * - Zero mass, infinite inertia
 * - Cannot be moved by forces or collisions
 * - Can be manually repositioned (teleported)
 * - Very efficient for static geometry
 * - Automatic transform synchronization with Node2D
 * 
 * Note: Static bodies should not be moved frequently. If you need to move
 * a body regularly, use KinematicBody2DComponent instead.
 */
class StaticBody2DComponent : public core::Component {
public:
    StaticBody2DComponent();
    explicit StaticBody2DComponent(const std::string& name);
    virtual ~StaticBody2DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "StaticBody2DComponent"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Property Accessors =====
    
    // Constant linear velocity (for moving platforms)
    math::Vec2 GetConstantLinearVelocity() const;
    void SetConstantLinearVelocity(const math::Vec2& velocity);
    
    // Constant angular velocity (for rotating platforms)
    float GetConstantAngularVelocity() const;
    void SetConstantAngularVelocity(float velocity);

    // ===== Physics API =====
    
    // Get the underlying physics body
    physics2d::RigidBody2D* GetPhysicsBody() const { return m_PhysicsBody; }
    
    // Manually set position (teleport). World space, like the physics body itself; the owning
    // node's local transform is derived from it.
    void SetPosition(const math::Vec2& position);

    // Manually set rotation (teleport). World space, as above.
    void SetRotation(float angle);

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

    // Wire physics-world collision callbacks to the body_entered/body_exited signals.
    void RegisterCollisionCallbacks();
    void OnCollisionEnterInternal(const physics2d::CollisionInfo& info);
    void OnCollisionExitInternal(const physics2d::CollisionInfo& info);
    nlohmann::json ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const;
};

} // namespace components
} // namespace lupine

