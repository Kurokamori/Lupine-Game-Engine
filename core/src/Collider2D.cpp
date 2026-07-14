#include "lupine/physics2d/Collider2D.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include "lupine/logger/Logger.hpp"
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>

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
    DestroyShape();

    if (m_Body) {
        m_Body->RemoveCollider(this);
    }
}

void Collider2D::OnOwningBodyDestroyed() {
    // b2DestroyBody destroys the body's shapes with it, so the id must be dropped without
    // calling b2DestroyShape on it.
    if (m_Body && B2_IS_NON_NULL(m_ShapeId)) {
        if (Physics2DWorld* world = m_Body->GetWorld()) {
            world->PurgeShapeFromContacts(m_ShapeId);
        }
    }
    m_ShapeId = b2_nullShapeId;
    m_Body = nullptr;
}

void Collider2D::DestroyShape() {
    if (!B2_IS_NON_NULL(m_ShapeId)) {
        return;
    }

    // Box2D emits end-touch events for the destroyed shape with an id that is already
    // invalid, and the event loop has to skip those. Purge the tracked contacts here (which
    // fires the exit callbacks) so a contact does not survive the shape it belonged to.
    if (m_Body) {
        if (Physics2DWorld* world = m_Body->GetWorld()) {
            world->PurgeShapeFromContacts(m_ShapeId);
        }
    }

    b2DestroyShape(m_ShapeId, true);
    m_ShapeId = b2_nullShapeId;
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

void Collider2D::SetCollisionLayer(uint32_t layer) {
    m_CollisionLayer = layer;
    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2Filter filter = b2Shape_GetFilter(m_ShapeId);
        filter.categoryBits = static_cast<uint64_t>(layer);
        b2Shape_SetFilter(m_ShapeId, filter);
    }
}

void Collider2D::SetCollisionMask(uint32_t mask) {
    m_CollisionMask = mask;
    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2Filter filter = b2Shape_GetFilter(m_ShapeId);
        filter.maskBits = static_cast<uint64_t>(mask);
        b2Shape_SetFilter(m_ShapeId, filter);
    }
}

void Collider2D::SetCollisionLayers(uint32_t layers) {
    m_CollisionLayer = layers;
    m_CollisionMask = layers;
    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2Filter filter = b2Shape_GetFilter(m_ShapeId);
        filter.categoryBits = static_cast<uint64_t>(layers);
        filter.maskBits = static_cast<uint64_t>(layers);
        b2Shape_SetFilter(m_ShapeId, filter);
    }
}

void Collider2D::SetOffset(const math::Vec2& offset) {
    m_Offset = offset;
    UpdateShape();
}

math::Vec2 Collider2D::GetOffset() const {
    return m_Offset;
}

void Collider2D::SetScale(const math::Vec2& scale) {
    if (scale.x == m_Scale.x && scale.y == m_Scale.y) {
        return;
    }

    m_Scale = scale;
    UpdateShape();
}

math::Vec2 Collider2D::ScaledOffset() const {
    return math::Vec2(m_Offset.x * m_Scale.x, m_Offset.y * m_Scale.y);
}

float Collider2D::RadialScale() const {
    return std::max(std::abs(m_Scale.x), std::abs(m_Scale.y));
}

void Collider2D::SetEnabled(bool enabled) {
    if (B2_IS_NON_NULL(m_ShapeId)) {
        b2Shape_EnableSensorEvents(m_ShapeId, enabled);
        b2Shape_EnableContactEvents(m_ShapeId, enabled && !m_IsSensor);
    }
}

bool Collider2D::IsEnabled() const {

    return B2_IS_NON_NULL(m_ShapeId);
}

void Collider2D::UpdateShape() {
    // DestroyShape() nulls m_ShapeId. That matters when CreateShape() declines to build a
    // replacement (empty or degenerate polygon): without it, every later SetFriction /
    // SetDensity / SetSensor call - and the destructor - would operate on a destroyed shape id.
    DestroyShape();

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
    // A mirrored (negative) scale leaves an axis-aligned box unchanged apart from the sign of
    // its half-extents, which Box2D requires to be positive.
    const float halfWidth = std::abs(m_Size.x * m_Scale.x) / 2.0f;
    const float halfHeight = std::abs(m_Size.y * m_Scale.y) / 2.0f;
    if (halfWidth < kMinExtent || halfHeight < kMinExtent) {
        return;
    }

    const math::Vec2 offset = ScaledOffset();

    b2Polygon box;
    if (offset.x != 0.0f || offset.y != 0.0f) {

        b2Vec2 center = {offset.x, offset.y};
        b2Rot rotation = b2Rot_identity;
        box = b2MakeOffsetBox(halfWidth, halfHeight, center, rotation);
    } else {

        box = b2MakeBox(halfWidth, halfHeight);
    }

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = m_Material.density;
    shapeDef.material.friction = m_Material.friction;
    shapeDef.material.restitution = m_Material.restitution;
    shapeDef.isSensor = m_IsSensor;
    shapeDef.enableSensorEvents = true;
    shapeDef.enableContactEvents = !m_IsSensor;
    shapeDef.filter.categoryBits = static_cast<uint64_t>(m_CollisionLayer);
    shapeDef.filter.maskBits = static_cast<uint64_t>(m_CollisionMask);

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
    const float radius = m_Radius * RadialScale();
    if (radius < kMinExtent) {
        return;
    }

    const math::Vec2 offset = ScaledOffset();

    b2Circle circle;
    circle.center = {offset.x, offset.y};
    circle.radius = radius;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = m_Material.density;
    shapeDef.material.friction = m_Material.friction;
    shapeDef.material.restitution = m_Material.restitution;
    shapeDef.isSensor = m_IsSensor;
    shapeDef.enableSensorEvents = true;
    shapeDef.enableContactEvents = !m_IsSensor;
    shapeDef.filter.categoryBits = static_cast<uint64_t>(m_CollisionLayer);
    shapeDef.filter.maskBits = static_cast<uint64_t>(m_CollisionMask);

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

    // A mirrored scale reverses the winding; b2ComputeHull rebuilds it from the point set, so
    // the scaled vertices can be handed over as they are.
    std::vector<b2Vec2> b2Vertices;
    b2Vertices.reserve(m_Vertices.size());
    for (const math::Vec2& v : m_Vertices) {
        b2Vertices.push_back({v.x * m_Scale.x, v.y * m_Scale.y});
    }

    b2Hull hull = b2ComputeHull(b2Vertices.data(), static_cast<int>(b2Vertices.size()));

    if (hull.count == 0) {

        return;
    }

    const math::Vec2 scaledOffset = ScaledOffset();

    b2Polygon polygon;
    if (scaledOffset.x != 0.0f || scaledOffset.y != 0.0f) {

        b2Vec2 offset = {scaledOffset.x, scaledOffset.y};
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
    shapeDef.enableSensorEvents = true;
    shapeDef.enableContactEvents = !m_IsSensor;
    shapeDef.filter.categoryBits = static_cast<uint64_t>(m_CollisionLayer);
    shapeDef.filter.maskBits = static_cast<uint64_t>(m_CollisionMask);

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
    // The capsule's axis is vertical, so its length follows the Y scale while its radius obeys
    // the single-radius policy documented on Collider2D::RadialScale.
    const float halfLength = std::abs(m_Length * m_Scale.y) / 2.0f;
    const float radius = m_Radius * RadialScale();
    if (radius < kMinExtent) {
        return;
    }

    const math::Vec2 offset = ScaledOffset();

    b2Capsule capsule;
    capsule.center1 = {offset.x, offset.y - halfLength};
    capsule.center2 = {offset.x, offset.y + halfLength};
    capsule.radius = radius;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = m_Material.density;
    shapeDef.material.friction = m_Material.friction;
    shapeDef.material.restitution = m_Material.restitution;
    shapeDef.isSensor = m_IsSensor;
    shapeDef.enableSensorEvents = true;
    shapeDef.enableContactEvents = !m_IsSensor;
    shapeDef.filter.categoryBits = static_cast<uint64_t>(m_CollisionLayer);
    shapeDef.filter.maskBits = static_cast<uint64_t>(m_CollisionMask);

    m_ShapeId = b2CreateCapsuleShape(m_Body->GetBox2DBody(), &shapeDef, &capsule);
}

}
}

