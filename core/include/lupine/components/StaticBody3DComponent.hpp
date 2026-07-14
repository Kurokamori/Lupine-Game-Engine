#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * StaticBody3DComponent
 * 
 * A static physics body that doesn't move and has infinite mass.
 * Suitable for walls, floors, platforms, and other immovable objects.
 * 
 * Features:
 * - Zero mass, infinite inertia
 * - Cannot be moved by forces or collisions
 * - Can be manually repositioned (teleported)
 * - Very efficient for static geometry
 * - Automatic transform synchronization with Node3D
 * 
 * Note: Static bodies should not be moved frequently. If you need to move
 * a body regularly, use KinematicBody3DComponent instead.
 */
class StaticBody3DComponent : public core::Component {
public:
    StaticBody3DComponent();
    explicit StaticBody3DComponent(const std::string& name);
    virtual ~StaticBody3DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "StaticBody3DComponent"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Property Accessors =====
    
    // Constant linear velocity (for moving platforms)
    math::Vec3 GetConstantLinearVelocity() const;
    void SetConstantLinearVelocity(const math::Vec3& velocity);
    
    // Constant angular velocity (for rotating platforms)
    math::Vec3 GetConstantAngularVelocity() const;
    void SetConstantAngularVelocity(const math::Vec3& velocity);

    // ===== Physics API =====
    
    // Get the underlying physics body
    physics3d::RigidBody3D* GetPhysicsBody() const { return m_PhysicsBody; }
    
    // Manually set position (teleport)
    void SetPosition(const math::Vec3& position);
    
    // Manually set rotation (teleport)
    void SetRotation(const math::Quat& rotation);

private:
    physics3d::RigidBody3D* m_PhysicsBody;
    core::UUID m_PhysicsBodyId;
    bool m_BodyCreated;
    
    // Cached transform for synchronization
    math::Vec3 m_LastPosition;
    math::Quat m_LastRotation;
    
    // Create physics body in the physics world
    void CreatePhysicsBody();
    
    // Destroy physics body
    void DestroyPhysicsBody();
    
    // Synchronize transform from Node3D to physics body
    void SyncTransformToPhysics();
    
    // Synchronize transform from physics body to Node3D
    void SyncTransformFromPhysics();
};

} // namespace components
} // namespace lupine

