#include "lupine/physics3d/Collider3D.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/logger/Logger.hpp"
#include <btBulletDynamicsCommon.h>

namespace lupine {
namespace physics3d {

Collider3D::Collider3D(RigidBody3D* body, const core::UUID& id)
    : m_Body(body)
    , m_Id(id)
    , m_Shape(nullptr)
    , m_Offset(math::Vec3::Zero())
    , m_RotationOffset(math::Quat::Identity())
    , m_Material(PhysicsMaterial3D())
    , m_IsSensor(false)
{
    body->AddCollider(this);
}

Collider3D::~Collider3D() {
    if (m_Body) {
        m_Body->RemoveCollider(this);
    }

    if (m_Shape) {
        delete m_Shape;
    }
}

void Collider3D::SetOffset(const math::Vec3& offset) {
    m_Offset = offset;
    RecreateShape();
}

void Collider3D::SetRotationOffset(const math::Quat& rotation) {
    m_RotationOffset = rotation;
    RecreateShape();
}

void Collider3D::SetMaterial(const PhysicsMaterial3D& material) {
    m_Material = material;
    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setFriction(material.friction);
        m_Body->GetBulletBody()->setRestitution(material.restitution);
        UpdateBodyMass();
    }
}

void Collider3D::SetFriction(float friction) {
    m_Material.friction = friction;
    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setFriction(friction);
    }
}

void Collider3D::SetRestitution(float restitution) {
    m_Material.restitution = restitution;
    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setRestitution(restitution);
    }
}

void Collider3D::SetDensity(float density) {
    m_Material.density = density;
    UpdateBodyMass();
}

void Collider3D::SetIsSensor(bool isSensor) {
    m_IsSensor = isSensor;
    if (m_Body && m_Body->GetBulletBody()) {
        int flags = m_Body->GetBulletBody()->getCollisionFlags();
        if (isSensor) {
            flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
        } else {
            flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
        }
        m_Body->GetBulletBody()->setCollisionFlags(flags);
    }
}

bool Collider3D::IsEnabled() const {
    if (m_Body && m_Body->GetBulletBody()) {
        return m_Body->GetBulletBody()->isActive();
    }
    return false;
}

void Collider3D::SetEnabled(bool enabled) {
    if (m_Body && m_Body->GetBulletBody()) {
        if (enabled) {
            m_Body->GetBulletBody()->activate(true);
        } else {
            m_Body->GetBulletBody()->setActivationState(DISABLE_SIMULATION);
        }
    }
}

void Collider3D::UpdateBodyMass() {
    if (m_Body) {

    }
}

void Collider3D::RecreateShape() {
    if (m_Shape) {
        delete m_Shape;
        m_Shape = nullptr;
    }
    CreateShape();

    if (m_Body && m_Body->GetBulletBody() && m_Shape) {
        m_Body->GetBulletBody()->setCollisionShape(m_Shape);
    }
}

BoxCollider3D::BoxCollider3D(RigidBody3D* body, const core::UUID& id, const math::Vec3& size)
    : Collider3D(body, id)
    , m_Size(size)
{
    CreateShape();
}

void BoxCollider3D::SetSize(const math::Vec3& size) {
    m_Size = size;
    RecreateShape();
}

void BoxCollider3D::CreateShape() {
    m_Shape = new btBoxShape(btVector3(m_Size.x * 0.5f, m_Size.y * 0.5f, m_Size.z * 0.5f));

    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setCollisionShape(m_Shape);
        m_Body->GetBulletBody()->setFriction(m_Material.friction);
        m_Body->GetBulletBody()->setRestitution(m_Material.restitution);
    }
}

SphereCollider3D::SphereCollider3D(RigidBody3D* body, const core::UUID& id, float radius)
    : Collider3D(body, id)
    , m_Radius(radius)
{
    CreateShape();
}

void SphereCollider3D::SetRadius(float radius) {
    m_Radius = radius;
    RecreateShape();
}

void SphereCollider3D::CreateShape() {
    m_Shape = new btSphereShape(m_Radius);

    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setCollisionShape(m_Shape);
        m_Body->GetBulletBody()->setFriction(m_Material.friction);
        m_Body->GetBulletBody()->setRestitution(m_Material.restitution);
    }
}

CapsuleCollider3D::CapsuleCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height)
    : Collider3D(body, id)
    , m_Radius(radius)
    , m_Height(height)
{
    CreateShape();
}

void CapsuleCollider3D::SetRadius(float radius) {
    m_Radius = radius;
    RecreateShape();
}

void CapsuleCollider3D::SetHeight(float height) {
    m_Height = height;
    RecreateShape();
}

void CapsuleCollider3D::CreateShape() {

    m_Shape = new btCapsuleShape(m_Radius, m_Height);

    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setCollisionShape(m_Shape);
        m_Body->GetBulletBody()->setFriction(m_Material.friction);
        m_Body->GetBulletBody()->setRestitution(m_Material.restitution);
    }
}

CylinderCollider3D::CylinderCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height)
    : Collider3D(body, id)
    , m_Radius(radius)
    , m_Height(height)
{
    CreateShape();
}

void CylinderCollider3D::SetRadius(float radius) {
    m_Radius = radius;
    RecreateShape();
}

void CylinderCollider3D::SetHeight(float height) {
    m_Height = height;
    RecreateShape();
}

void CylinderCollider3D::CreateShape() {

    m_Shape = new btCylinderShape(btVector3(m_Radius, m_Height * 0.5f, m_Radius));

    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setCollisionShape(m_Shape);
        m_Body->GetBulletBody()->setFriction(m_Material.friction);
        m_Body->GetBulletBody()->setRestitution(m_Material.restitution);
    }
}

ConeCollider3D::ConeCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height)
    : Collider3D(body, id)
    , m_Radius(radius)
    , m_Height(height)
{
    CreateShape();
}

void ConeCollider3D::SetRadius(float radius) {
    m_Radius = radius;
    RecreateShape();
}

void ConeCollider3D::SetHeight(float height) {
    m_Height = height;
    RecreateShape();
}

void ConeCollider3D::CreateShape() {

    m_Shape = new btConeShape(m_Radius, m_Height);

    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setCollisionShape(m_Shape);
        m_Body->GetBulletBody()->setFriction(m_Material.friction);
        m_Body->GetBulletBody()->setRestitution(m_Material.restitution);
    }
}

PlaneCollider3D::PlaneCollider3D(RigidBody3D* body, const core::UUID& id, const math::Vec3& normal, float distance)
    : Collider3D(body, id)
    , m_Normal(normal)
    , m_Distance(distance)
{
    CreateShape();
}

void PlaneCollider3D::SetNormal(const math::Vec3& normal) {
    m_Normal = normal;
    RecreateShape();
}

void PlaneCollider3D::SetDistance(float distance) {
    m_Distance = distance;
    RecreateShape();
}

void PlaneCollider3D::CreateShape() {

    m_Shape = new btStaticPlaneShape(btVector3(m_Normal.x, m_Normal.y, m_Normal.z), m_Distance);

    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setCollisionShape(m_Shape);
        m_Body->GetBulletBody()->setFriction(m_Material.friction);
        m_Body->GetBulletBody()->setRestitution(m_Material.restitution);
    }
}

MeshCollider3D::MeshCollider3D(RigidBody3D* body, const core::UUID& id, const std::vector<math::Vec3>& vertices, bool convex)
    : Collider3D(body, id)
    , m_Vertices(vertices)
    , m_IsConvex(convex)
    , m_TriangleMesh(nullptr)
{
    CreateShape();
}

MeshCollider3D::MeshCollider3D(RigidBody3D* body, const core::UUID& id, const std::vector<math::Vec3>& vertices, const std::vector<uint32_t>& indices, bool convex)
    : Collider3D(body, id)
    , m_Vertices(vertices)
    , m_Indices(indices)
    , m_IsConvex(convex)
    , m_TriangleMesh(nullptr)
{
    CreateShape();
}

MeshCollider3D::~MeshCollider3D() {

    if (m_TriangleMesh) {
        delete m_TriangleMesh;
        m_TriangleMesh = nullptr;
    }
}

void MeshCollider3D::SetVertices(const std::vector<math::Vec3>& vertices, bool convex) {
    m_Vertices = vertices;
    m_Indices.clear();
    m_IsConvex = convex;
    RecreateShape();
}

void MeshCollider3D::SetMesh(const std::vector<math::Vec3>& vertices, const std::vector<uint32_t>& indices, bool convex) {
    m_Vertices = vertices;
    m_Indices = indices;
    m_IsConvex = convex;
    RecreateShape();
}

void MeshCollider3D::CreateShape() {
    if (m_Vertices.empty()) {
        return;
    }

    if (m_TriangleMesh) {
        delete m_TriangleMesh;
        m_TriangleMesh = nullptr;
    }

    if (m_IsConvex) {

        btConvexHullShape* convexShape = new btConvexHullShape();
        for (const auto& vertex : m_Vertices) {
            convexShape->addPoint(btVector3(vertex.x, vertex.y, vertex.z), false);
        }
        convexShape->recalcLocalAabb();
        m_Shape = convexShape;
    } else {

        m_TriangleMesh = new btTriangleMesh();

        if (!m_Indices.empty()) {

            for (size_t i = 0; i + 2 < m_Indices.size(); i += 3) {
                uint32_t i0 = m_Indices[i];
                uint32_t i1 = m_Indices[i + 1];
                uint32_t i2 = m_Indices[i + 2];

                if (i0 < m_Vertices.size() && i1 < m_Vertices.size() && i2 < m_Vertices.size()) {
                    btVector3 v0(m_Vertices[i0].x, m_Vertices[i0].y, m_Vertices[i0].z);
                    btVector3 v1(m_Vertices[i1].x, m_Vertices[i1].y, m_Vertices[i1].z);
                    btVector3 v2(m_Vertices[i2].x, m_Vertices[i2].y, m_Vertices[i2].z);
                    m_TriangleMesh->addTriangle(v0, v1, v2);
                }
            }
        } else {

            for (size_t i = 0; i + 2 < m_Vertices.size(); i += 3) {
                btVector3 v0(m_Vertices[i].x, m_Vertices[i].y, m_Vertices[i].z);
                btVector3 v1(m_Vertices[i + 1].x, m_Vertices[i + 1].y, m_Vertices[i + 1].z);
                btVector3 v2(m_Vertices[i + 2].x, m_Vertices[i + 2].y, m_Vertices[i + 2].z);
                m_TriangleMesh->addTriangle(v0, v1, v2);
            }
        }

        m_Shape = new btBvhTriangleMeshShape(m_TriangleMesh, true);
    }

    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setCollisionShape(m_Shape);
        m_Body->GetBulletBody()->setFriction(m_Material.friction);
        m_Body->GetBulletBody()->setRestitution(m_Material.restitution);
    }
}

}
}

