#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/physics3d/Collider3D.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/math/Math.hpp"
#include <functional>
#include <memory>
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
    void DefineSignals() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ===== Property Accessors =====

    // Sensor shape. The area owns its own collider; without one it has no volume to
    // detect overlaps in.
    CollisionShape3DType GetShapeType() const;
    void SetShapeType(CollisionShape3DType type);

    math::Vec3 GetSize() const;
    void SetSize(const math::Vec3& size);

    float GetRadius() const;
    void SetRadius(float radius);

    float GetHeight() const;
    void SetHeight(float height);

    math::Vec3 GetShapeOffset() const;
    void SetShapeOffset(const math::Vec3& offset);

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

    // Sensor shape state, mirrored from the properties.
    CollisionShape3DType m_ShapeType;
    math::Vec3 m_Size;
    float m_Radius;
    float m_Height;
    math::Vec3 m_ShapeOffset;

    // The area's own sensor collider. Owned here because no CollisionMesh3D component is
    // required for an area to work.
    std::unique_ptr<physics3d::Collider3D> m_Collider;

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

    // Build (or rebuild) the sensor collider from the current shape properties and flag the
    // body as a sensor so it reports overlaps without pushing anything away.
    void CreateSensorCollider();
    void DestroySensorCollider();
    void RebuildSensorCollider();

    // Synchronize transform from Node3D to physics body
    void SyncTransformToPhysics();
    
    // Synchronize transform from physics body to Node3D
    void SyncTransformFromPhysics();
    
    // Collision callbacks (called by physics world)
    void OnTriggerEnter(const core::UUID& otherBodyId);
    void OnTriggerExit(const core::UUID& otherBodyId);

    // Resolve the other overlapping body to a node-handle signal argument.
    nlohmann::json ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const;
};

} // namespace components
} // namespace lupine

