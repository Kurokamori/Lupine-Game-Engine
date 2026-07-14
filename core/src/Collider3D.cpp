#include "lupine/physics3d/Collider3D.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/physics3d/PhysicsShapeCache.hpp"
#include "lupine/logger/Logger.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.h>

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
        m_Body->DetachColliderShape(m_Shape);
        m_Body->RemoveCollider(this);
    }

    CleanupShape();
}

void Collider3D::OnOwningBodyDestroyed() {
    m_Body = nullptr;
}

void Collider3D::SetOffset(const math::Vec3& offset) {
    m_Offset = offset;
    if (m_Body) {
        m_Body->RebuildCompoundShape();
    }
}

void Collider3D::SetRotationOffset(const math::Quat& rotation) {
    m_RotationOffset = rotation;
    if (m_Body) {
        m_Body->RebuildCompoundShape();
    }
}

void Collider3D::SetMaterial(const PhysicsMaterial3D& material) {
    m_Material = material;
    ApplyMaterialToBody();
    UpdateBodyMass();
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

void Collider3D::SetCollisionLayers(uint32_t layers) {
    m_CollisionLayers = layers;
    if (m_Body) {
        // Use same value for both group and mask (objects collide if they share any layer)
        m_Body->UpdateCollisionFiltering(layers, layers);
    }
}

bool Collider3D::IsEnabled() const {
    return m_Enabled;
}

void Collider3D::SetEnabled(bool enabled) {
    if (m_Enabled == enabled) {
        return;
    }
    m_Enabled = enabled;

    if (!m_Body) {
        return;
    }

    // A disabled collider drops out of the body's compound shape entirely, which is what
    // "disabled" means for a collider. Freezing the whole body via DISABLE_SIMULATION (the
    // previous behaviour) could not be undone by activate(), which refuses to leave that state.
    m_Body->RebuildCompoundShape();

    if (btRigidBody* body = m_Body->GetBulletBody()) {
        body->forceActivationState(ACTIVE_TAG);
        body->activate(true);
    }
}

void Collider3D::UpdateBodyMass() {
    if (m_Body) {
        m_Body->RebuildCompoundShape();
    }
}

void Collider3D::ApplyMaterialToBody() {
    // Bullet stores friction and restitution per body, not per shape.
    if (m_Body && m_Body->GetBulletBody()) {
        m_Body->GetBulletBody()->setFriction(m_Material.friction);
        m_Body->GetBulletBody()->setRestitution(m_Material.restitution);
    }
}

void Collider3D::InitializeShape() {
    CreateShape();
    ApplyMaterialToBody();
    if (m_Body) {
        m_Body->RebuildCompoundShape();
    }
}

void Collider3D::RecreateShape() {
    // Detach before destroying: CreateShape() may legitimately decline to produce a shape
    // (degenerate hull, empty mesh), and the body must not keep a child pointing at freed memory.
    if (m_Body) {
        m_Body->DetachColliderShape(m_Shape);
    }

    CleanupShape();
    CreateShape();
    ApplyMaterialToBody();

    if (m_Body) {
        m_Body->RebuildCompoundShape();
    }
}

void Collider3D::CleanupShape() {
    if (m_Shape) {
        delete m_Shape;
        m_Shape = nullptr;
    }
}

BoxCollider3D::BoxCollider3D(RigidBody3D* body, const core::UUID& id, const math::Vec3& size)
    : Collider3D(body, id)
    , m_Size(size)
{
    InitializeShape();
}

void BoxCollider3D::SetSize(const math::Vec3& size) {
    m_Size = size;
    RecreateShape();
}

void BoxCollider3D::CreateShape() {
    m_Shape = new btBoxShape(btVector3(m_Size.x * 0.5f, m_Size.y * 0.5f, m_Size.z * 0.5f));
}

SphereCollider3D::SphereCollider3D(RigidBody3D* body, const core::UUID& id, float radius)
    : Collider3D(body, id)
    , m_Radius(radius)
{
    InitializeShape();
}

void SphereCollider3D::SetRadius(float radius) {
    m_Radius = radius;
    RecreateShape();
}

void SphereCollider3D::CreateShape() {
    m_Shape = new btSphereShape(m_Radius);
}

CapsuleCollider3D::CapsuleCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height)
    : Collider3D(body, id)
    , m_Radius(radius)
    , m_Height(height)
{
    InitializeShape();
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
}

CylinderCollider3D::CylinderCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height)
    : Collider3D(body, id)
    , m_Radius(radius)
    , m_Height(height)
{
    InitializeShape();
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
}

ConeCollider3D::ConeCollider3D(RigidBody3D* body, const core::UUID& id, float radius, float height)
    : Collider3D(body, id)
    , m_Radius(radius)
    , m_Height(height)
{
    InitializeShape();
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
}

PlaneCollider3D::PlaneCollider3D(RigidBody3D* body, const core::UUID& id, const math::Vec3& normal, float distance)
    : Collider3D(body, id)
    , m_Normal(normal)
    , m_Distance(distance)
{
    InitializeShape();
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
}

MeshCollider3D::MeshCollider3D(RigidBody3D* body, const core::UUID& id, const std::vector<math::Vec3>& vertices, bool convex)
    : Collider3D(body, id)
    , m_Vertices(vertices)
    , m_IsConvex(convex)
    , m_TriangleMesh(nullptr)
{
    InitializeShape();
}

MeshCollider3D::MeshCollider3D(RigidBody3D* body, const core::UUID& id, const std::vector<math::Vec3>& vertices, const std::vector<uint32_t>& indices, bool convex)
    : Collider3D(body, id)
    , m_Vertices(vertices)
    , m_Indices(indices)
    , m_IsConvex(convex)
    , m_TriangleMesh(nullptr)
{
    InitializeShape();
}

MeshCollider3D::MeshCollider3D(RigidBody3D* body, const core::UUID& id, btBvhTriangleMeshShape* cachedShape,
                               const std::string& meshPath, const math::Vec3& scale)
    : Collider3D(body, id)
    , m_IsConvex(false)
    , m_TriangleMesh(nullptr)
    , m_UsesCachedShape(true)
    , m_CachedMeshPath(meshPath)
    , m_CachedBvhShape(cachedShape)
{
    if (!cachedShape) {
        
        return;
    }

    // Check if we need scaling
    bool needsScaling = (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f);

    if (needsScaling) {
        // Create a scaled wrapper around the cached BVH shape
        // btScaledBvhTriangleMeshShape is very lightweight - it just applies a scale transform
        btVector3 btScale(scale.x, scale.y, scale.z);
        m_Shape = new btScaledBvhTriangleMeshShape(cachedShape, btScale);
        m_UsesScaledWrapper = true;
    } else {
        // Use the cached shape directly
        // NOTE: We don't own this shape - the cache does
        m_Shape = cachedShape;
        m_UsesScaledWrapper = false;
    }

    // Register with cache that we're using this shape
    PhysicsShapeCache::Instance().AddRef(meshPath);

    ApplyMaterialToBody();
    if (m_Body) {
        m_Body->RebuildCompoundShape();
    }
}

MeshCollider3D::~MeshCollider3D() {
    // Detach here as well as in ~Collider3D: this override runs first and nulls m_Shape,
    // so the base destructor would have nothing left to detach.
    if (m_Body) {
        m_Body->DetachColliderShape(m_Shape);
    }
    CleanupShape();
}

void MeshCollider3D::CleanupShape() {
    if (m_UsesCachedShape) {
        // Release our reference to the cached shape
        PhysicsShapeCache::Instance().Release(m_CachedMeshPath);

        // Only delete the scaled wrapper if we created one
        // The underlying BVH is owned by the cache
        if (m_UsesScaledWrapper && m_Shape) {
            delete m_Shape;
        }
        m_Shape = nullptr;  // Don't let base class delete it
        m_UsesCachedShape = false;
        m_UsesScaledWrapper = false;
        m_CachedMeshPath.clear();
        m_CachedBvhShape = nullptr;
    } else {
        // Standard path - we own the triangle mesh
        if (m_TriangleMesh) {
            delete m_TriangleMesh;
            m_TriangleMesh = nullptr;
        }
        // Let base class handle m_Shape deletion via Collider3D::CleanupShape
        Collider3D::CleanupShape();
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
}

}
}

