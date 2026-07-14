#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include "lupine/physics2d/Collider2D.hpp"
#include "lupine/components/CollisionBody2DComponent.hpp"
#include <memory>
#include <functional>
#include <vector>

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
    void DefineSignals() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ===== Sensor Shape Configuration =====

    CollisionShape2DType GetShapeType() const;
    void SetShapeType(CollisionShape2DType type);

    math::Vec2 GetSize() const;
    void SetSize(const math::Vec2& size);

    float GetRadius() const;
    void SetRadius(float radius);

    math::Vec2 GetOffset() const;
    void SetOffset(const math::Vec2& offset);

    uint32_t GetCollisionLayers() const;
    void SetCollisionLayers(uint32_t layers);

    uint32_t GetCollisionMask() const;
    void SetCollisionMask(uint32_t mask);

    const std::vector<math::Vec2>& GetVertices() const { return m_Vertices; }
    void SetVertices(const std::vector<math::Vec2>& vertices);

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

    // Sensor shape configuration (the area creates its own sensor collider so it
    // detects overlaps without requiring a separate CollisionBody2D child).
    CollisionShape2DType m_ShapeType;
    math::Vec2 m_Size;
    float m_Radius;
    math::Vec2 m_Offset;
    std::vector<math::Vec2> m_Vertices;
    uint32_t m_CollisionLayers;
    uint32_t m_CollisionMask;
    std::unique_ptr<physics2d::Collider2D> m_Collider;

    // Cached transform for synchronization
    math::Vec2 m_LastPosition;
    float m_LastRotation;

    // Global scale the sensor shape was built with. A Box2D shape cannot be rescaled after
    // creation, so any change here (including one caused by an ancestor) rebuilds it.
    math::Vec2 m_LastScale = math::Vec2(1.0f, 1.0f);

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

    // Create/destroy/rebuild the sensor collider attached to the body.
    void CreateSensorCollider();
    void DestroySensorCollider();
    void RebuildSensorCollider();

    // Owner's global scale, or (1, 1) when the owner is not a Node2D.
    math::Vec2 GetNodeGlobalScale() const;

    // Register/unregister the trigger callbacks with the physics world.
    void RegisterTriggerCallbacks();
    
    // Synchronize transform from Node2D to physics body
    void SyncTransformToPhysics();
    
    // Synchronize transform from physics body to Node2D
    void SyncTransformFromPhysics();
    
    // Internal trigger callbacks
    void OnTriggerEnterInternal(const physics2d::CollisionInfo& info);
    void OnTriggerStayInternal(const physics2d::CollisionInfo& info);
    void OnTriggerExitInternal(const physics2d::CollisionInfo& info);

    // Resolve the other overlapping body to a node-handle signal argument.
    nlohmann::json ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const;
};

} // namespace components
} // namespace lupine

