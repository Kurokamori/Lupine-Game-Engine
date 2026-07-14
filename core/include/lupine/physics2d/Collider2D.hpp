#pragma once

#include "Physics2DWorld.hpp"
#include "RigidBody2D.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/math/Vec2.hpp"
#include <box2d/box2d.h>
#include <vector>

namespace lupine {
namespace physics2d {

// Forward declarations
class RigidBody2D;
struct PhysicsMaterial2D;

/**
 * Base class for all 2D colliders
 */
class Collider2D {
public:
    Collider2D(RigidBody2D* body, const core::UUID& id);
    virtual ~Collider2D();

    // Identification
    const core::UUID& GetId() const { return m_Id; }
    RigidBody2D* GetBody() const { return m_Body; }

    // Material properties
    void SetMaterial(const PhysicsMaterial2D& material);
    PhysicsMaterial2D GetMaterial() const;

    void SetDensity(float density);
    float GetDensity() const;

    void SetFriction(float friction);
    float GetFriction() const;

    void SetRestitution(float restitution);
    float GetRestitution() const;

    // Sensor flag
    void SetSensor(bool isSensor);
    bool IsSensor() const;

    // Collision layer (category bits): which layer(s) this collider occupies.
    void SetCollisionLayer(uint32_t layer);
    uint32_t GetCollisionLayer() const { return m_CollisionLayer; }

    // Collision mask (mask bits): which layer(s) this collider detects/collides with.
    void SetCollisionMask(uint32_t mask);
    uint32_t GetCollisionMask() const { return m_CollisionMask; }

    // Backward-compatible helper: sets both layer (category) and mask to the same
    // value, reproducing the legacy "collide if sharing a layer" behaviour.
    void SetCollisionLayers(uint32_t layers);
    uint32_t GetCollisionLayers() const { return m_CollisionLayer; }

    // Offset from body
    void SetOffset(const math::Vec2& offset);
    math::Vec2 GetOffset() const;

    // World scale of the owning node. A Box2D body carries only a position and a rotation, and
    // a Box2D shape cannot be rescaled once created, so the scale has to be baked into the
    // shape geometry: every extent, vertex and offset below is multiplied by it at create time.
    // Changing it rebuilds the shape.
    void SetScale(const math::Vec2& scale);
    math::Vec2 GetScale() const { return m_Scale; }

    // Enabled state
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // Called by RigidBody2D when the body is destroyed. A collider is owned by whoever
    // constructed it (a component's unique_ptr, typically), not by the body, so the body
    // does not delete it - it only tells the collider to stop referencing it. b2DestroyBody
    // already destroys the body's shapes, so the shape id is dropped here too.
    void OnOwningBodyDestroyed();

    // Internal access
    b2ShapeId GetBox2DShape() const { return m_ShapeId; }

protected:
    RigidBody2D* m_Body;
    core::UUID m_Id;
    b2ShapeId m_ShapeId = b2_nullShapeId;
    math::Vec2 m_Offset;
    PhysicsMaterial2D m_Material;
    bool m_IsSensor;
    uint32_t m_CollisionLayer = 1;              // Category bits: default layer 1
    uint32_t m_CollisionMask = 0xFFFFFFFFu;     // Mask bits: default detect all layers
    math::Vec2 m_Scale = math::Vec2(1.0f, 1.0f);

    // The offset is expressed in the node's local units, so it scales with the node.
    math::Vec2 ScaledOffset() const;

    // Radial policy for shapes Box2D can only describe with a single radius (circle, capsule):
    // a non-uniform scale would turn them into an ellipse/stadium, which Box2D cannot represent,
    // so the radius takes the LARGEST absolute axis. Erring large keeps the collider fully
    // covering the drawn shape - a too-small radius lets fast bodies tunnel into geometry the
    // player can see. (Unity's CircleCollider2D resolves the same conflict the same way.)
    float RadialScale() const;

    // Box2D rejects (asserts on) degenerate geometry, and a zero or near-zero node scale is a
    // legitimate authoring state, so shape creation is skipped below this extent.
    static constexpr float kMinExtent = 1e-4f;

    void UpdateShape();
    virtual void CreateShape() = 0;

    // Destroys the current Box2D shape (if any), purges the contacts that referenced it, and
    // resets m_ShapeId to null.
    void DestroyShape();
};

/**
 * Box collider (rectangle)
 */
class BoxCollider2D : public Collider2D {
public:
    BoxCollider2D(RigidBody2D* body, const core::UUID& id, const math::Vec2& size);
    virtual ~BoxCollider2D() = default;

    void SetSize(const math::Vec2& size);
    math::Vec2 GetSize() const { return m_Size; }

protected:
    void CreateShape() override;

private:
    math::Vec2 m_Size;
};

/**
 * Circle collider
 */
class CircleCollider2D : public Collider2D {
public:
    CircleCollider2D(RigidBody2D* body, const core::UUID& id, float radius);
    virtual ~CircleCollider2D() = default;

    void SetRadius(float radius);
    float GetRadius() const { return m_Radius; }

protected:
    void CreateShape() override;

private:
    float m_Radius;
};

/**
 * Polygon collider (convex polygon)
 */
class PolygonCollider2D : public Collider2D {
public:
    PolygonCollider2D(RigidBody2D* body, const core::UUID& id, const std::vector<math::Vec2>& vertices);
    virtual ~PolygonCollider2D() = default;

    void SetVertices(const std::vector<math::Vec2>& vertices);
    const std::vector<math::Vec2>& GetVertices() const { return m_Vertices; }

protected:
    void CreateShape() override;

private:
    std::vector<math::Vec2> m_Vertices;
};

/**
 * Capsule collider (pill shape)
 */
class CapsuleCollider2D : public Collider2D {
public:
    CapsuleCollider2D(RigidBody2D* body, const core::UUID& id, float length, float radius);
    virtual ~CapsuleCollider2D() = default;

    void SetLength(float length);
    float GetLength() const { return m_Length; }

    void SetRadius(float radius);
    float GetRadius() const { return m_Radius; }

protected:
    void CreateShape() override;

private:
    float m_Length;
    float m_Radius;
};

} // namespace physics2d
} // namespace lupine

