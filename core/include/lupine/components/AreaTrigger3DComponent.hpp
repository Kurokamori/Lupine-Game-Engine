#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/math/Math.hpp"
#include <functional>
#include <unordered_set>

namespace lupine {
namespace components {

/**
 * AreaTrigger3DComponent
 * 
 * A sensor/trigger area that detects when other physics bodies enter,
 * stay in, or exit the area. Does not participate in physical collisions.
 * 
 * Features:
 * - Detects overlapping bodies without physical response
 * - Trigger enter/stay/exit callbacks
 * - Can be static or kinematic
 * - Useful for detection zones, pickups, checkpoints, etc.
 * - Automatic transform synchronization with Node3D
 * 
 * Note: Area triggers are sensors - they detect overlaps but don't
 * cause physical collisions. Bodies will pass through them.
 */
class AreaTrigger3DComponent : public core::Component {
public:
    AreaTrigger3DComponent();
    explicit AreaTrigger3DComponent(const std::string& name);
    virtual ~AreaTrigger3DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "AreaTrigger3DComponent"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Property Accessors =====
    
    // Monitoring (whether the area is actively detecting overlaps)
    bool GetMonitoring() const;
    void SetMonitoring(bool monitoring);
    
    // Monitorable (whether other areas can detect this area)
    bool GetMonitorable() const;
    void SetMonitorable(bool monitorable);
    
    // Priority (higher priority areas are checked first)
    int GetPriority() const;
    void SetPriority(int priority);

    // ===== Physics API =====
    
    // Get the underlying physics body
    physics3d::RigidBody3D* GetPhysicsBody() const { return m_PhysicsBody; }
    
    // Get overlapping bodies
    const std::unordered_set<core::UUID>& GetOverlappingBodies() const { return m_OverlappingBodies; }
    
    // Check if a specific body is overlapping
    bool IsOverlapping(const core::UUID& bodyId) const;
    
    // Callback registration (for scripting integration)
    using TriggerCallback = std::function<void(const core::UUID& otherBodyId)>;
    void SetOnBodyEntered(TriggerCallback callback) { m_OnBodyEntered = callback; }
    void SetOnBodyExited(TriggerCallback callback) { m_OnBodyExited = callback; }

private:
    physics3d::RigidBody3D* m_PhysicsBody;
    core::UUID m_PhysicsBodyId;
    bool m_BodyCreated;
    
    // Cached transform for synchronization
    math::Vec3 m_LastPosition;
    math::Quat m_LastRotation;
    
    // Overlapping bodies tracking
    std::unordered_set<core::UUID> m_OverlappingBodies;
    
    // Callbacks
    TriggerCallback m_OnBodyEntered;
    TriggerCallback m_OnBodyExited;
    
    // Create physics body in the physics world
    void CreatePhysicsBody();
    
    // Destroy physics body
    void DestroyPhysicsBody();
    
    // Synchronize transform from Node3D to physics body
    void SyncTransformToPhysics();
    
    // Synchronize transform from physics body to Node3D
    void SyncTransformFromPhysics();
    
    // Collision callbacks (called by physics world)
    void OnTriggerEnter(const core::UUID& otherBodyId);
    void OnTriggerExit(const core::UUID& otherBodyId);
};

} // namespace components
} // namespace lupine

