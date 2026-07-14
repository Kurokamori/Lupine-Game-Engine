#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/physics3d/Collider3D.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Color.hpp"
#include <vector>
#include <memory>

namespace lupine {
namespace components {

/**
 * Shape types for CollisionMesh3D
 */
enum class CollisionShape3DType {
    Box = 0,
    Sphere = 1,
    Capsule = 2,
    Cylinder = 3,
    Cone = 4,
    Plane = 5,
    Mesh = 6
};

/**
 * Mesh solver types for mesh-based collision shapes
 */
enum class MeshSolverType {
    BoundingBox = 0,  // Create a bounding box around the mesh
    Convex = 1,       // Create a convex hull from mesh vertices
    Concave = 2,      // Create a concave triangle mesh (static only)
    TriMesh = 3       // Create a triangle mesh (static only)
};

/**
 * CollisionMesh3D Component
 * 
 * Defines the collision shape(s) for a 3D physics body.
 * Can be attached to a node with a physics body component (RigidBody3D, StaticBody3D, etc.)
 * or to a child node of a physics body, allowing multiple shapes per body.
 * 
 * Features:
 * - Multiple primitive shapes (Box, Sphere, Capsule, Cylinder, Cone, Plane)
 * - Mesh-based collision with multiple solvers (BoundingBox, Convex, Concave, TriMesh)
 * - Debug visualization in editor
 * - Physics material properties (density, friction, restitution)
 * - Sensor/trigger mode
 */
class CollisionMesh3DComponent : public core::Component {
public:
    CollisionMesh3DComponent();
    explicit CollisionMesh3DComponent(const std::string& name);
    virtual ~CollisionMesh3DComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "CollisionMesh3DComponent"; }
    void DefineProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) override;
    void OnRender() override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ===== Shape Configuration =====

    /**
     * Get/Set the shape type
     */
    CollisionShape3DType GetShapeType() const;
    void SetShapeType(CollisionShape3DType type);

    // Primitive shape parameters
    math::Vec3 GetSize() const;
    void SetSize(const math::Vec3& size);

    float GetRadius() const;
    void SetRadius(float radius);

    float GetHeight() const;
    void SetHeight(float height);

    math::Vec3 GetPlaneNormal() const;
    void SetPlaneNormal(const math::Vec3& normal);

    float GetPlaneDistance() const;
    void SetPlaneDistance(float distance);

    float GetPlaneWidth() const;
    void SetPlaneWidth(float width);

    float GetPlaneLength() const;
    void SetPlaneLength(float length);

    // Mesh shape parameters
    std::string GetMeshPath() const;
    void SetMeshPath(const std::string& path);

    MeshSolverType GetMeshSolver() const;
    void SetMeshSolver(MeshSolverType solver);

    math::Vec3 GetOffset() const;
    void SetOffset(const math::Vec3& offset);

    // ===== Physics Material =====

    float GetDensity() const;
    void SetDensity(float density);

    float GetFriction() const;
    void SetFriction(float friction);

    float GetRestitution() const;
    void SetRestitution(float restitution);

    bool IsSensor() const;
    void SetSensor(bool isSensor);

    /**
     * Get/Set collision layers (bitmask - objects collide if they share any layer)
     */
    uint32_t GetCollisionLayers() const;
    void SetCollisionLayers(uint32_t layers);

    // ===== Debug Visualization =====

    math::Color GetDebugColor() const;
    void SetDebugColor(const math::Color& color);

private:
    // Shape configuration
    CollisionShape3DType m_ShapeType;
    
    // Primitive parameters
    math::Vec3 m_Size;              // For Box
    float m_Radius;                 // For Sphere, Capsule, Cylinder, Cone
    float m_Height;                 // For Capsule, Cylinder, Cone
    math::Vec3 m_PlaneNormal;       // For Plane
    float m_PlaneDistance;          // For Plane (deprecated - use planeWidth/planeLength instead)
    float m_PlaneWidth;             // For Plane (finite plane width)
    float m_PlaneLength;            // For Plane (finite plane length)
    
    // Mesh parameters
    std::string m_MeshPath;
    MeshSolverType m_MeshSolver;
    std::vector<math::Vec3> m_MeshVertices;  // Cached mesh vertices
    std::vector<uint32_t> m_MeshIndices;     // Cached mesh indices (for triangle meshes)

    math::Vec3 m_Offset;

    // Physics material
    float m_Density;
    float m_Friction;
    float m_Restitution;
    bool m_IsSensor;
    uint32_t m_CollisionLayers = 1; // Default: layer 1 only

    // Debug visualization
    math::Color m_DebugColor;

    // Physics integration
    physics3d::RigidBody3D* m_PhysicsBody;
    std::unique_ptr<physics3d::Collider3D> m_Collider;
    core::UUID m_PhysicsBodyId;

    // Helper methods
    void ReadShapeFields(const nlohmann::json& source);
    void CreateCollider();
    void DestroyCollider();
    void UpdateCollider();
    void LoadMeshVertices();
    physics3d::RigidBody3D* FindPhysicsBody();
    void DrawDebugShape();
};

} // namespace components
} // namespace lupine

