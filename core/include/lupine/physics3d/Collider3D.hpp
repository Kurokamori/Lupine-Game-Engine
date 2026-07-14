#pragma once

#include "lupine/core/UUID.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Quat.hpp"
#include <vector>
#include <string>

// Forward declare Bullet types
class btCollisionShape;
class btTriangleMesh;
class btBvhTriangleMeshShape;

namespace lupine {
namespace physics3d {

// Forward declaration
class RigidBody3D;

/**
 * Physics material properties
 */
struct PhysicsMaterial3D {
    float friction = 0.5f;
    float restitution = 0.0f;  // Bounciness
    float density = 1.0f;

    PhysicsMaterial3D() = default;
    PhysicsMaterial3D(float f, float r, float d) : friction(f), restitution(r), density(d) {}
};

/**
 * Base class for all 3D colliders
 */
class Collider3D {
public:
    Collider3D(RigidBody3D* body, const core::UUID& id);
    virtual ~Collider3D();

    // Identification
    const core::UUID& GetId() const { return m_Id; }
    RigidBody3D* GetBody() const { return m_Body; }

    // Offset from body center
    void SetOffset(const math::Vec3& offset);
    math::Vec3 GetOffset() const { return m_Offset; }

    void SetRotationOffset(const math::Quat& rotation);
    math::Quat GetRotationOffset() const { return m_RotationOffset; }

    // Material properties
    void SetMaterial(const PhysicsMaterial3D& material);
    PhysicsMaterial3D GetMaterial() const { return m_Material; }

    void SetFriction(float friction);
    float GetFriction() const { return m_Material.friction; }

    void SetRestitution(float restitution);
    float GetRestitution() const { return m_Material.restitution; }

    void SetDensity(float density);
    float GetDensity() const { return m_Material.density; }

    // Sensor (trigger) mode
    void SetIsSensor(bool isSensor);
    bool IsSensor() const { return m_IsSensor; }

    // Collision layers (bitmask - objects collide if they share any layer)
    void SetCollisionLayers(uint32_t layers);
    uint32_t GetCollisionLayers() const { return m_CollisionLayers; }

    // Enabled state
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // Called by RigidBody3D when the body is destroyed. A collider is owned by whoever
    // constructed it (a component's unique_ptr, typically), not by the body, so the body
    // does not delete it - it only tells the collider to stop referencing it.
    void OnOwningBodyDestroyed();

    // Internal access
    btCollisionShape* GetBulletShape() const { return m_Shape; }

protected:
    RigidBody3D* m_Body;
    core::UUID m_Id;
    btCollisionShape* m_Shape;
    math::Vec3 m_Offset;
    math::Quat m_RotationOffset;
    PhysicsMaterial3D m_Material;
    bool m_IsSensor;
    bool m_Enabled = true;
    uint32_t m_CollisionLayers = 1; // Default: layer 1 only

    void UpdateBodyMass();
    virtual void CreateShape() = 0;
    virtual void CleanupShape();

    // Builds the shape and attaches it to the body's compound. Derived constructors call
    // this instead of CreateShape() so the new child reaches the compound.
    void InitializeShape();

    // Detaches the current child from the compound before destroying it, so the body never
    // references a freed shape even when CreateShape() declines to build one (degenerate
    // vertices, empty mesh).
    void RecreateShape();

    void ApplyMaterialToBody();
};

/**
 * Box collider (rectangular prism)
 */
class BoxCollider3D : public Collider3D {
public:
    BoxCollider3D(RigidBody3D* body, const core::UUID& id, const math::Vec3& size);
    virtual ~BoxCollider3D() = default;

    void SetSize(const math::Vec3& size);
    math::Vec3 GetSize() const { return m_Size; }

protected:
    void CreateShape() override;

private:
    math::Vec3 m_Size;
};

/**
 * Sphere collider
 */
class SphereCollider3D : public Collider3D {
public:
    SphereCollider3D(RigidBody3D* body, const core::UUID& id, float radius);
    virtual ~SphereCollider3D() = default;

    void SetRadius(float radius);
    float GetRadius() const { return m_Radius; }

protected:
    void CreateShape() override;

private:
    float m_Radius;
};

/**
 * Capsule collider (cylinder with hemispherical ends)
 */
class CapsuleCollider3D : public Collider3D {
public:
    CapsuleCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height);
    virtual ~CapsuleCollider3D() = default;

    void SetRadius(float radius);
    float GetRadius() const { return m_Radius; }

    void SetHeight(float height);
    float GetHeight() const { return m_Height; }

protected:
    void CreateShape() override;

private:
    float m_Radius;
    float m_Height;
};

/**
 * Cylinder collider
 */
class CylinderCollider3D : public Collider3D {
public:
    CylinderCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height);
    virtual ~CylinderCollider3D() = default;

    void SetRadius(float radius);
    float GetRadius() const { return m_Radius; }

    void SetHeight(float height);
    float GetHeight() const { return m_Height; }

protected:
    void CreateShape() override;

private:
    float m_Radius;
    float m_Height;
};

/**
 * Cone collider
 */
class ConeCollider3D : public Collider3D {
public:
    ConeCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height);
    virtual ~ConeCollider3D() = default;

    void SetRadius(float radius);
    float GetRadius() const { return m_Radius; }

    void SetHeight(float height);
    float GetHeight() const { return m_Height; }

protected:
    void CreateShape() override;

private:
    float m_Radius;
    float m_Height;
};

/**
 * Plane collider (infinite static plane)
 */
class PlaneCollider3D : public Collider3D {
public:
    PlaneCollider3D(RigidBody3D* body, const core::UUID& id, const math::Vec3& normal, float distance);
    virtual ~PlaneCollider3D() = default;

    void SetNormal(const math::Vec3& normal);
    math::Vec3 GetNormal() const { return m_Normal; }

    void SetDistance(float distance);
    float GetDistance() const { return m_Distance; }

protected:
    void CreateShape() override;

private:
    math::Vec3 m_Normal;
    float m_Distance;
};

/**
 * Mesh collider (convex hull or triangle mesh)
 */
class MeshCollider3D : public Collider3D {
public:
    // Standard constructors - build shape from vertices
    MeshCollider3D(RigidBody3D* body, const core::UUID& id, const std::vector<math::Vec3>& vertices, bool convex = true);
    MeshCollider3D(RigidBody3D* body, const core::UUID& id, const std::vector<math::Vec3>& vertices, const std::vector<uint32_t>& indices, bool convex = true);

    // Cached shape constructor - uses a shared BVH shape from PhysicsShapeCache
    // The cachedShape is NOT owned by this collider - it's owned by the cache
    // If scale != (1,1,1), a btScaledBvhTriangleMeshShape wrapper is created
    MeshCollider3D(RigidBody3D* body, const core::UUID& id, btBvhTriangleMeshShape* cachedShape,
                   const std::string& meshPath, const math::Vec3& scale = math::Vec3(1.0f, 1.0f, 1.0f));

    virtual ~MeshCollider3D();

    void SetVertices(const std::vector<math::Vec3>& vertices, bool convex = true);
    void SetMesh(const std::vector<math::Vec3>& vertices, const std::vector<uint32_t>& indices, bool convex = true);
    const std::vector<math::Vec3>& GetVertices() const { return m_Vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

    bool IsConvex() const { return m_IsConvex; }
    bool UsesCachedShape() const { return m_UsesCachedShape; }
    const std::string& GetCachedMeshPath() const { return m_CachedMeshPath; }

protected:
    void CreateShape() override;
    void CleanupShape() override;

private:
    std::vector<math::Vec3> m_Vertices;
    std::vector<uint32_t> m_Indices;
    bool m_IsConvex;
    btTriangleMesh* m_TriangleMesh;  // Need to keep this alive for btBvhTriangleMeshShape

    // For cached shapes
    bool m_UsesCachedShape = false;
    bool m_UsesScaledWrapper = false;
    std::string m_CachedMeshPath;
    btBvhTriangleMeshShape* m_CachedBvhShape = nullptr;  // NOT owned - from cache
};

} // namespace physics3d
} // namespace lupine

