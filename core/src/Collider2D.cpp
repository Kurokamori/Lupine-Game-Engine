#include "lupine/physics2d/Collider2D.hpp"
#include "lupine/logger/Logger.hpp"
#include <box2d/box2d.h>

namespace lupine {
namespace physics2d {

Collider2D::Collider2D(RigidBody2D* body, const core::UUID& id)
    : m_Body(body)
    , m_Id(id)
    , m_Offset(math::Vec2::Zero())
    , m_Material(PhysicsMaterial2D())
    , m_IsSensor(false)
{
    body->AddCollider(this);
}

Collider2D::~Collider2D() {
    if (m_Body) {
        m_Body->RemoveCollider(this);
    }

    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2DestroyShape(m_ShapeId, true);
    }
}

void Collider2D::SetMaterial(const PhysicsMaterial2D& material) {
    m_Material = material;
    UpdateShape();
}

PhysicsMaterial2D Collider2D::GetMaterial() const {
    return m_Material;
}

void Collider2D::SetDensity(float density) {
    m_Material.density = density;
    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2Shape_SetDensity(m_ShapeId, density, true);
    }
}

float Collider2D::GetDensity() const {
    if (B2_IS_NON_NULL(m_ShapeId)) {
        return b2Shape_GetDensity(m_ShapeId);
    }
    return m_Material.density;
}

void Collider2D::SetFriction(float friction) {
    m_Material.friction = friction;
    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2Shape_SetFriction(m_ShapeId, friction);
    }
}

float Collider2D::GetFriction() const {
    if (B2_IS_NON_NULL(m_ShapeId)) {
        return b2Shape_GetFriction(m_ShapeId);
    }
    return m_Material.friction;
}

void Collider2D::SetRestitution(float restitution) {
    m_Material.restitution = restitution;
    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2Shape_SetRestitution(m_ShapeId, restitution);
    }
}

float Collider2D::GetRestitution() const {
    if (B2_IS_NON_NULL(m_ShapeId)) {
        return b2Shape_GetRestitution(m_ShapeId);
    }
    return m_Material.restitution;
}

void Collider2D::SetSensor(bool isSensor) {
    m_IsSensor = isSensor;
    if (B2_IS_NON_NULL(m_ShapeId)) {

        UpdateShape();
    }
}

bool Collider2D::IsSensor() const {
    if (B2_IS_NON_NULL(m_ShapeId)) {
        return b2Shape_IsSensor(m_ShapeId);
    }
    return m_IsSensor;
}

void Collider2D::SetOffset(const math::Vec2& offset) {
    m_Offset = offset;
    UpdateShape();
}

math::Vec2 Collider2D::GetOffset() const {
    return m_Offset;
}

void Collider2D::SetEnabled(bool enabled) {
    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2Shape_EnableSensorEvents(m_ShapeId, enabled && m_IsSensor);
        b2Shape_EnableContactEvents(m_ShapeId, enabled && !m_IsSensor);
    }
}

bool Collider2D::IsEnabled() const {

    return B2_IS_NON_NULL(m_ShapeId);
}

void Collider2D::UpdateShape() {

    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2DestroyShape(m_ShapeId, true);
    }

    CreateShape();
}

BoxCollider2D::BoxCollider2D(RigidBody2D* body, const core::UUID& id, const math::Vec2& size)
    : Collider2D(body, id)
    , m_Size(size)
{
    CreateShape();
}

void BoxCollider2D::SetSize(const math::Vec2& size) {
    m_Size = size;
    UpdateShape();
}

void BoxCollider2D::CreateShape() {
    b2Polygon box;
    if (m_Offset.x != 0.0f || m_Offset.y != 0.0f) {

        b2Vec2 center = {m_Offset.x, m_Offset.y};
        b2Rot rotation = b2Rot_identity;
        box = b2MakeOffsetBox(m_Size.x / 2.0f, m_Size.y / 2.0f, center, rotation);
    } else {

        box = b2MakeBox(m_Size.x / 2.0f, m_Size.y / 2.0f);
    }

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = m_Material.density;
    shapeDef.material.friction = m_Material.friction;
    shapeDef.material.restitution = m_Material.restitution;
    shapeDef.isSensor = m_IsSensor;
    shapeDef.enableSensorEvents = m_IsSensor;
    shapeDef.enableContactEvents = !m_IsSensor;

    m_ShapeId = b2CreatePolygonShape(m_Body->GetBox2DBody(), &shapeDef, &box);
}

CircleCollider2D::CircleCollider2D(RigidBody2D* body, const core::UUID& id, float radius)
    : Collider2D(body, id)
    , m_Radius(radius)
{
    CreateShape();
}

void CircleCollider2D::SetRadius(float radius) {
    m_Radius = radius;
    UpdateShape();
}

void CircleCollider2D::CreateShape() {
    b2Circle circle;
    circle.center = {m_Offset.x, m_Offset.y};
    circle.radius = m_Radius;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = m_Material.density;
    shapeDef.material.friction = m_Material.friction;
    shapeDef.material.restitution = m_Material.restitution;
    shapeDef.isSensor = m_IsSensor;
    shapeDef.enableSensorEvents = m_IsSensor;
    shapeDef.enableContactEvents = !m_IsSensor;

    m_ShapeId = b2CreateCircleShape(m_Body->GetBox2DBody(), &shapeDef, &circle);
}

PolygonCollider2D::PolygonCollider2D(RigidBody2D* body, const core::UUID& id, const std::vector<math::Vec2>& vertices)
    : Collider2D(body, id)
    , m_Vertices(vertices)
{
    CreateShape();
}

void PolygonCollider2D::SetVertices(const std::vector<math::Vec2>& vertices) {
    m_Vertices = vertices;
    UpdateShape();
}

void PolygonCollider2D::CreateShape() {
    if (m_Vertices.empty()) {

        return;
    }

    std::vector<b2Vec2> b2Vertices;
    for (const auto& v : m_Vertices) {
        b2Vertices.push_back({v.x, v.y});
    }

    b2Hull hull = b2ComputeHull(b2Vertices.data(), static_cast<int>(b2Vertices.size()));

    if (hull.count == 0) {

        return;
    }

    b2Polygon polygon;
    if (m_Offset.x != 0.0f || m_Offset.y != 0.0f) {

        b2Vec2 offset = {m_Offset.x, m_Offset.y};
        b2Rot rotation = b2Rot_identity;
        polygon = b2MakeOffsetPolygon(&hull, offset, rotation);
    } else {

        polygon = b2MakePolygon(&hull, 0.0f);
    }

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = m_Material.density;
    shapeDef.material.friction = m_Material.friction;
    shapeDef.material.restitution = m_Material.restitution;
    shapeDef.isSensor = m_IsSensor;
    shapeDef.enableSensorEvents = m_IsSensor;
    shapeDef.enableContactEvents = !m_IsSensor;

    m_ShapeId = b2CreatePolygonShape(m_Body->GetBox2DBody(), &shapeDef, &polygon);
}

CapsuleCollider2D::CapsuleCollider2D(RigidBody2D* body, const core::UUID& id, float length, float radius)
    : Collider2D(body, id)
    , m_Length(length)
    , m_Radius(radius)
{
    CreateShape();
}

void CapsuleCollider2D::SetLength(float length) {
    m_Length = length;
    UpdateShape();
}

void CapsuleCollider2D::SetRadius(float radius) {
    m_Radius = radius;
    UpdateShape();
}

void CapsuleCollider2D::CreateShape() {
    b2Capsule capsule;
    capsule.center1 = {m_Offset.x, m_Offset.y - m_Length / 2.0f};
    capsule.center2 = {m_Offset.x, m_Offset.y + m_Length / 2.0f};
    capsule.radius = m_Radius;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = m_Material.density;
    shapeDef.material.friction = m_Material.friction;
    shapeDef.material.restitution = m_Material.restitution;
    shapeDef.isSensor = m_IsSensor;
    shapeDef.enableSensorEvents = m_IsSensor;
    shapeDef.enableContactEvents = !m_IsSensor;

    m_ShapeId = b2CreateCapsuleShape(m_Body->GetBox2DBody(), &shapeDef, &capsule);
}

}
}

