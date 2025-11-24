#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include <memory>
#include <functional>

namespace lupine {
namespace components {

/**
 * AreaTrigger2DComponent
 * 
 * A sensor/trigger area that detects when other physics bodies enter,
 * stay in, or exit the area. Does not participate in physical collisions.
 * 
 * Features:
 * - Detects overlapping bodies without physical response
 * - Trigger enter/stay/exit callbacks
 * - Can be static or kinematic
 * - Useful for detection zones, pickups, checkpoints, etc.
 * - Automatic transform synchronization with Node2D
 * 
 * Note: Area triggers are sensors - they detect overlaps but don't
 * cause physical collisions. Bodies will pass through them.
 */
class AreaTrigger2DComponent : public core::Component {
public:
    AreaTrigger2DComponent();
    explicit AreaTrigger2DComponent(const std::string& name);
    virtual ~AreaTrigger2DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "AreaTrigger2DComponent"; }
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
    physics2d::RigidBody2D* GetPhysicsBody() const { return m_PhysicsBody; }
    
    // Callback types
    using TriggerCallback = std::function<void(const physics2d::CollisionInfo&)>;
    
    // Set callbacks for trigger events
    void SetOnTriggerEnter(TriggerCallback callback) { m_OnTriggerEnter = callback; }
    void SetOnTriggerStay(TriggerCallback callback) { m_OnTriggerStay = callback; }
    void SetOnTriggerExit(TriggerCallback callback) { m_OnTriggerExit = callback; }
    
    // Get list of currently overlapping bodies
    const std::vector<core::UUID>& GetOverlappingBodies() const { return m_OverlappingBodies; }
    
    // Check if a specific body is overlapping
    bool IsOverlapping(const core::UUID& bodyId) const;

private:
    physics2d::RigidBody2D* m_PhysicsBody;
    core::UUID m_PhysicsBodyId;
    bool m_BodyCreated;
    
    // Cached transform for synchronization
    math::Vec2 m_LastPosition;
    float m_LastRotation;
    
    // Trigger callbacks
    TriggerCallback m_OnTriggerEnter;
    TriggerCallback m_OnTriggerStay;
    TriggerCallback m_OnTriggerExit;
    
    // List of currently overlapping bodies
    std::vector<core::UUID> m_OverlappingBodies;
    
    // Create physics body in the physics world
    void CreatePhysicsBody();
    
    // Destroy physics body
    void DestroyPhysicsBody();
    
    // Synchronize transform from Node2D to physics body
    void SyncTransformToPhysics();
    
    // Synchronize transform from physics body to Node2D
    void SyncTransformFromPhysics();
    
    // Internal trigger callbacks
    void OnTriggerEnterInternal(const physics2d::CollisionInfo& info);
    void OnTriggerStayInternal(const physics2d::CollisionInfo& info);
    void OnTriggerExitInternal(const physics2d::CollisionInfo& info);
};

} // namespace components
} // namespace lupine

