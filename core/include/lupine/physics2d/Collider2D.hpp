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

    // Offset from body
    void SetOffset(const math::Vec2& offset);
    math::Vec2 GetOffset() const;

    // Enabled state
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // Internal access
    b2ShapeId GetBox2DShape() const { return m_ShapeId; }

protected:
    RigidBody2D* m_Body;
    core::UUID m_Id;
    b2ShapeId m_ShapeId;
    math::Vec2 m_Offset;
    PhysicsMaterial2D m_Material;
    bool m_IsSensor;

    void UpdateShape();
    virtual void CreateShape() = 0;
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

