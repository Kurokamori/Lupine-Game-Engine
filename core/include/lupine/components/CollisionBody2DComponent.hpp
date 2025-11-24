#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/physics2d/Collider2D.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Color.hpp"
#include <vector>
#include <memory>

namespace lupine {
namespace components {

/**
 * Shape types for CollisionBody2D
 */
enum class CollisionShape2DType {
    Rectangle = 0,
    Circle = 1,
    Triangle = 2,
    Pentagon = 3,
    Hexagon = 4,
    Polygon = 5
};

/**
 * CollisionBody2D Component
 * 
 * Defines the collision shape(s) for a 2D physics body.
 * Can be attached to a node with a physics body component (RigidBody2D, StaticBody2D, etc.)
 * or to a child node of a physics body, allowing multiple shapes per body.
 * 
 * Features:
 * - Multiple predefined shapes (Rectangle, Circle, Triangle, Pentagon, Hexagon)
 * - Custom polygon shape with interactive editing
 * - Debug visualization in editor
 * - Physics material properties (density, friction, restitution)
 * - Sensor/trigger mode
 */
class CollisionBody2DComponent : public core::Component {
public:
    CollisionBody2DComponent();
    explicit CollisionBody2DComponent(const std::string& name);
    virtual ~CollisionBody2DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "CollisionBody2DComponent"; }
    void DefineProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnRender() override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ===== Shape Configuration =====

    /**
     * Get/Set the shape type
     */
    CollisionShape2DType GetShapeType() const;
    void SetShapeType(CollisionShape2DType type);

    /**
     * Get/Set shape size (for Rectangle)
     */
    math::Vec2 GetSize() const;
    void SetSize(const math::Vec2& size);

    /**
     * Get/Set radius (for Circle)
     */
    float GetRadius() const;
    void SetRadius(float radius);

    /**
     * Get/Set polygon vertices (for Polygon and predefined shapes)
     */
    const std::vector<math::Vec2>& GetVertices() const;
    void SetVertices(const std::vector<math::Vec2>& vertices);

    /**
     * Add a vertex to the polygon (for custom polygon editing)
     */
    void AddVertex(const math::Vec2& vertex);

    /**
     * Remove a vertex from the polygon
     */
    void RemoveVertex(int index);

    /**
     * Update a vertex position
     */
    void UpdateVertex(int index, const math::Vec2& position);

    /**
     * Clear all vertices
     */
    void ClearVertices();

    // ===== Physics Material =====

    /**
     * Get/Set density (affects mass)
     */
    float GetDensity() const;
    void SetDensity(float density);

    /**
     * Get/Set friction coefficient
     */
    float GetFriction() const;
    void SetFriction(float friction);

    /**
     * Get/Set restitution (bounciness)
     */
    float GetRestitution() const;
    void SetRestitution(float restitution);

    /**
     * Get/Set sensor mode (trigger without physical collision)
     */
    bool IsSensor() const;
    void SetSensor(bool isSensor);

    /**
     * Get/Set offset from node position
     */
    math::Vec2 GetOffset() const;
    void SetOffset(const math::Vec2& offset);

    // ===== Debug Visualization =====

    /**
     * Get/Set debug color for collision shape rendering
     */
    math::Color GetDebugColor() const;
    void SetDebugColor(const math::Color& color);

private:
    // Shape configuration
    CollisionShape2DType m_ShapeType;
    math::Vec2 m_Size;              // For Rectangle
    float m_Radius;                 // For Circle
    std::vector<math::Vec2> m_Vertices;  // For Polygon and predefined shapes
    math::Vec2 m_Offset;

    // Physics material
    float m_Density;
    float m_Friction;
    float m_Restitution;
    bool m_IsSensor;

    // Debug visualization
    math::Color m_DebugColor;

    // Physics integration
    physics2d::RigidBody2D* m_PhysicsBody;
    std::unique_ptr<physics2d::Collider2D> m_Collider;
    core::UUID m_PhysicsBodyId;

    // Helper methods
    void CreateCollider();
    void DestroyCollider();
    void UpdateCollider();
    void GenerateShapeVertices();
    physics2d::RigidBody2D* FindPhysicsBody();
    void DrawDebugShape();
};

} // namespace components
} // namespace lupine

