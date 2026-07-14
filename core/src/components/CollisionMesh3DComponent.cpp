#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/components/RigidBody3DComponent.hpp"
#include "lupine/components/StaticBody3DComponent.hpp"
#include "lupine/components/KinematicBody3DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/asset/ModelAsset.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/physics3d/PhysicsShapeCache.hpp"
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics3d;

namespace {

// Serialize() writes vectors as arrays while the property registry stores them as {x,y,z}
// objects, and both layouts reach Deserialize(), so every read has to accept either one.
Vec3 ReadJsonVec3(const nlohmann::json& value, const Vec3& fallback) {
    if (value.is_array() && value.size() >= 3) {
        return Vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
    }
    if (value.is_object()) {
        return Vec3(
            value.value("x", fallback.x),
            value.value("y", fallback.y),
            value.value("z", fallback.z));
    }
    return fallback;
}

Color ReadJsonColor(const nlohmann::json& value, const Color& fallback) {
    if (value.is_array() && value.size() >= 4) {
        return Color(
            value[0].get<float>(),
            value[1].get<float>(),
            value[2].get<float>(),
            value[3].get<float>());
    }
    if (value.is_object()) {
        return Color(
            value.value("r", fallback.r),
            value.value("g", fallback.g),
            value.value("b", fallback.b),
            value.value("a", fallback.a));
    }
    return fallback;
}

}

CollisionMesh3DComponent::CollisionMesh3DComponent()
    : Component("CollisionMesh3DComponent")
    , m_ShapeType(CollisionShape3DType::Box)
    , m_Size(Vec3(1.0f, 1.0f, 1.0f))
    , m_Radius(0.5f)
    , m_Height(1.0f)
    , m_PlaneNormal(Vec3(0.0f, 1.0f, 0.0f))
    , m_PlaneDistance(0.0f)
    , m_PlaneWidth(10.0f)
    , m_PlaneLength(10.0f)
    , m_MeshSolver(MeshSolverType::Convex)
    , m_Offset(Vec3::Zero())
    , m_Density(1.0f)
    , m_Friction(0.5f)
    , m_Restitution(0.0f)
    , m_IsSensor(false)
    , m_DebugColor(Color(0.0f, 1.0f, 0.0f, 0.5f))
    , m_PhysicsBody(nullptr)
{
}

CollisionMesh3DComponent::CollisionMesh3DComponent(const std::string& name)
    : Component(name)
    , m_ShapeType(CollisionShape3DType::Box)
    , m_Size(Vec3(1.0f, 1.0f, 1.0f))
    , m_Radius(0.5f)
    , m_Height(1.0f)
    , m_PlaneNormal(Vec3(0.0f, 1.0f, 0.0f))
    , m_PlaneDistance(0.0f)
    , m_PlaneWidth(10.0f)
    , m_PlaneLength(10.0f)
    , m_MeshSolver(MeshSolverType::Convex)
    , m_Offset(Vec3::Zero())
    , m_Density(1.0f)
    , m_Friction(0.5f)
    , m_Restitution(0.0f)
    , m_IsSensor(false)
    , m_DebugColor(Color(0.0f, 1.0f, 0.0f, 0.5f))
    , m_PhysicsBody(nullptr)
{
}

CollisionMesh3DComponent::~CollisionMesh3DComponent() {
    DestroyCollider();
}

void CollisionMesh3DComponent::DefineProperties() {

    DefineProperty(PROPERTY_ENUM_GROUP(shapeType, 0, "Shape", Box, Sphere, Capsule, Cylinder, Cone, Plane, Mesh));

    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec3, Vec3(1.0f, 1.0f, 1.0f), "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 0.5f, 0.01f, 100.0f, 0.1f, "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 1.0f, 0.01f, 100.0f, 0.1f, "Shape"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(planeNormal, Vec3, Vec3(0.0f, 1.0f, 0.0f), "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(planeDistance, 0.0f, -100.0f, 100.0f, 0.1f, "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(planeWidth, 10.0f, 0.1f, 1000.0f, 0.1f, "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(planeLength, 10.0f, 0.1f, 1000.0f, 0.1f, "Shape"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(meshPath, String, std::string(""), "Mesh"));
    DefineProperty(PROPERTY_ENUM_GROUP(meshSolver, 1, "Mesh", BoundingBox, Convex, Concave, TriMesh));

    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec3, Vec3::Zero(), "Shape"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(density, 1.0f, 0.01f, 100.0f, 0.1f, "Material"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(friction, 0.5f, 0.0f, 1.0f, 0.01f, "Material"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(restitution, 0.0f, 0.0f, 1.0f, 0.01f, "Material"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(isSensor, Bool, false, "Material"));
    DefineProperty(PROPERTY_GROUP(collisionLayers, Int, 1, Layers3D, "", "Material"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, Color(0.0f, 1.0f, 0.0f, 0.5f), "Debug"));
}

nlohmann::json CollisionMesh3DComponent::Serialize() const {
    nlohmann::json json = Component::Serialize();

    json["shapeType"] = static_cast<int>(m_ShapeType);
    json["size"] = {m_Size.x, m_Size.y, m_Size.z};
    json["radius"] = m_Radius;
    json["height"] = m_Height;
    json["planeNormal"] = {m_PlaneNormal.x, m_PlaneNormal.y, m_PlaneNormal.z};
    json["planeDistance"] = m_PlaneDistance;
    json["planeWidth"] = m_PlaneWidth;
    json["planeLength"] = m_PlaneLength;
    json["meshPath"] = m_MeshPath;
    json["meshSolver"] = static_cast<int>(m_MeshSolver);
    json["offset"] = {m_Offset.x, m_Offset.y, m_Offset.z};
    json["density"] = m_Density;
    json["friction"] = m_Friction;
    json["restitution"] = m_Restitution;
    json["isSensor"] = m_IsSensor;
    json["debugColor"] = {m_DebugColor.r, m_DebugColor.g, m_DebugColor.b, m_DebugColor.a};

    return json;
}

void CollisionMesh3DComponent::ReadShapeFields(const nlohmann::json& source) {
    if (!source.is_object()) {
        return;
    }

    if (source.contains("shapeType")) m_ShapeType = static_cast<CollisionShape3DType>(source["shapeType"].get<int>());
    if (source.contains("size")) m_Size = ReadJsonVec3(source["size"], m_Size);
    if (source.contains("radius")) m_Radius = source["radius"].get<float>();
    if (source.contains("height")) m_Height = source["height"].get<float>();
    if (source.contains("planeNormal")) m_PlaneNormal = ReadJsonVec3(source["planeNormal"], m_PlaneNormal);
    if (source.contains("planeDistance")) m_PlaneDistance = source["planeDistance"].get<float>();
    if (source.contains("planeWidth")) m_PlaneWidth = source["planeWidth"].get<float>();
    if (source.contains("planeLength")) m_PlaneLength = source["planeLength"].get<float>();
    if (source.contains("meshPath")) m_MeshPath = source["meshPath"].get<std::string>();
    if (source.contains("meshSolver")) m_MeshSolver = static_cast<MeshSolverType>(source["meshSolver"].get<int>());
    if (source.contains("offset")) m_Offset = ReadJsonVec3(source["offset"], m_Offset);
    if (source.contains("density")) m_Density = source["density"].get<float>();
    if (source.contains("friction")) m_Friction = source["friction"].get<float>();
    if (source.contains("restitution")) m_Restitution = source["restitution"].get<float>();
    if (source.contains("isSensor")) m_IsSensor = source["isSensor"].get<bool>();
    if (source.contains("debugColor")) m_DebugColor = ReadJsonColor(source["debugColor"], m_DebugColor);
}

void CollisionMesh3DComponent::Deserialize(const nlohmann::json& json) {
    Component::Deserialize(json);

    // Shape state lives in two places in a saved component: the top-level keys written by
    // Serialize() below, and the "properties" block that Component::Serialize() writes from
    // the property registry. Hand-authored scenes and prefabs carry only the latter, so
    // reading top-level alone silently left every field at its constructor default - a
    // 1x1x1 box at zero offset, whatever the file actually asked for.
    ReadShapeFields(json);

    // The registry is what the inspector edits and what SetShapeType() and friends keep in
    // sync, so it is authoritative and is applied last: a stale top-level key left behind by
    // an older save must not win over the value the file actually shows.
    if (json.contains("properties")) {
        ReadShapeFields(json["properties"]);
    }

    m_CollisionLayers = static_cast<uint32_t>(GetPropertyValue<int>("collisionLayers"));
}

void CollisionMesh3DComponent::OnAwake() {
    CreateCollider();
}

void CollisionMesh3DComponent::OnReady() {
    // If collider wasn't created in OnAwake (e.g., physics body wasn't ready yet), try again
    if (!m_Collider) {
        CreateCollider();
    }
}

void CollisionMesh3DComponent::OnDestroy() {
    DestroyCollider();
}

void CollisionMesh3DComponent::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
    // See Component::PhysicsWorldRebuildPhase. This component owns no body - it attaches a
    // collider to a body found on its own node or its parent - so it does all of its work in
    // the AttachColliders phase, once those bodies have been recreated.
    switch (phase) {
        case PhysicsWorldRebuildPhase::SaveState:
            break;

        case PhysicsWorldRebuildPhase::RecreateBodies:
            // The body this collider was attached to has been destroyed, which already told
            // the collider its body is gone (RigidBody3D's destructor), so the reset is safe.
            // DestroyCollider also nulls m_PhysicsBody, which is now a dangling pointer into
            // the old world.
            DestroyCollider();
            break;

        case PhysicsWorldRebuildPhase::AttachColliders:
            // CreateCollider re-finds the body itself.
            CreateCollider();
            break;
    }
}

void CollisionMesh3DComponent::OnRender() {
    DrawDebugShape();
}

void CollisionMesh3DComponent::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "shapeType") {
        m_ShapeType = static_cast<CollisionShape3DType>(newValue.get<int>());
        UpdateCollider();
    } else if (propertyName == "size") {

        if (newValue.is_array()) {
            m_Size = Vec3(newValue[0], newValue[1], newValue[2]);
        } else {
            m_Size = Vec3(newValue["x"], newValue["y"], newValue["z"]);
        }
        UpdateCollider();
    } else if (propertyName == "radius") {
        m_Radius = newValue.get<float>();
        UpdateCollider();
    } else if (propertyName == "height") {
        m_Height = newValue.get<float>();
        UpdateCollider();
    } else if (propertyName == "planeNormal") {

        if (newValue.is_array()) {
            m_PlaneNormal = Vec3(newValue[0], newValue[1], newValue[2]);
        } else {
            m_PlaneNormal = Vec3(newValue["x"], newValue["y"], newValue["z"]);
        }
        UpdateCollider();
    } else if (propertyName == "planeDistance") {
        m_PlaneDistance = newValue.get<float>();
        UpdateCollider();
    } else if (propertyName == "planeWidth") {
        m_PlaneWidth = newValue.get<float>();
        UpdateCollider();
    } else if (propertyName == "planeLength") {
        m_PlaneLength = newValue.get<float>();
        UpdateCollider();
    } else if (propertyName == "meshPath") {
        m_MeshPath = newValue.get<std::string>();
        LoadMeshVertices();
        UpdateCollider();
    } else if (propertyName == "meshSolver") {
        m_MeshSolver = static_cast<MeshSolverType>(newValue.get<int>());
        UpdateCollider();
    } else if (propertyName == "offset") {

        if (newValue.is_array()) {
            m_Offset = Vec3(newValue[0], newValue[1], newValue[2]);
        } else {
            m_Offset = Vec3(newValue["x"], newValue["y"], newValue["z"]);
        }
        if (m_Collider) {
            m_Collider->SetOffset(m_Offset);
        }
    } else if (propertyName == "density") {
        m_Density = newValue.get<float>();
        if (m_Collider) {
            m_Collider->SetDensity(m_Density);
        }
    } else if (propertyName == "friction") {
        m_Friction = newValue.get<float>();
        if (m_Collider) {
            m_Collider->SetFriction(m_Friction);
        }
    } else if (propertyName == "restitution") {
        m_Restitution = newValue.get<float>();
        if (m_Collider) {
            m_Collider->SetRestitution(m_Restitution);
        }
    } else if (propertyName == "isSensor") {
        m_IsSensor = newValue.get<bool>();
        if (m_Collider) {
            m_Collider->SetIsSensor(m_IsSensor);
        }
    } else if (propertyName == "collisionLayers") {
        m_CollisionLayers = static_cast<uint32_t>(newValue.get<int>());
        if (m_Collider) {
            m_Collider->SetCollisionLayers(m_CollisionLayers);
        }
    } else if (propertyName == "debugColor") {

        if (newValue.is_array()) {
            m_DebugColor = Color(newValue[0], newValue[1], newValue[2], newValue[3]);
        } else {
            m_DebugColor = Color(newValue["r"], newValue["g"], newValue["b"], newValue["a"]);
        }
    }
}

CollisionShape3DType CollisionMesh3DComponent::GetShapeType() const {
    return m_ShapeType;
}

void CollisionMesh3DComponent::SetShapeType(CollisionShape3DType type) {
    m_ShapeType = type;
    SetPropertyValue("shapeType", static_cast<int>(type));
    UpdateCollider();
}

Vec3 CollisionMesh3DComponent::GetSize() const {
    return m_Size;
}

void CollisionMesh3DComponent::SetSize(const Vec3& size) {
    m_Size = size;
    SetPropertyValue("size", nlohmann::json::array({size.x, size.y, size.z}));
    UpdateCollider();
}

float CollisionMesh3DComponent::GetRadius() const {
    return m_Radius;
}

void CollisionMesh3DComponent::SetRadius(float radius) {
    m_Radius = radius;
    SetPropertyValue("radius", radius);
    UpdateCollider();
}

float CollisionMesh3DComponent::GetHeight() const {
    return m_Height;
}

void CollisionMesh3DComponent::SetHeight(float height) {
    m_Height = height;
    SetPropertyValue("height", height);
    UpdateCollider();
}

Vec3 CollisionMesh3DComponent::GetPlaneNormal() const {
    return m_PlaneNormal;
}

void CollisionMesh3DComponent::SetPlaneNormal(const Vec3& normal) {
    m_PlaneNormal = normal;
    SetPropertyValue("planeNormal", nlohmann::json::array({normal.x, normal.y, normal.z}));
    UpdateCollider();
}

float CollisionMesh3DComponent::GetPlaneDistance() const {
    return m_PlaneDistance;
}

void CollisionMesh3DComponent::SetPlaneDistance(float distance) {
    m_PlaneDistance = distance;
    SetPropertyValue("planeDistance", distance);
    UpdateCollider();
}

float CollisionMesh3DComponent::GetPlaneWidth() const {
    return m_PlaneWidth;
}

void CollisionMesh3DComponent::SetPlaneWidth(float width) {
    m_PlaneWidth = width;
    SetPropertyValue("planeWidth", width);
    UpdateCollider();
}

float CollisionMesh3DComponent::GetPlaneLength() const {
    return m_PlaneLength;
}

void CollisionMesh3DComponent::SetPlaneLength(float length) {
    m_PlaneLength = length;
    SetPropertyValue("planeLength", length);
    UpdateCollider();
}

std::string CollisionMesh3DComponent::GetMeshPath() const {
    return m_MeshPath;
}

void CollisionMesh3DComponent::SetMeshPath(const std::string& path) {
    m_MeshPath = path;
    SetPropertyValue("meshPath", path);
    LoadMeshVertices();
    UpdateCollider();
}

MeshSolverType CollisionMesh3DComponent::GetMeshSolver() const {
    return m_MeshSolver;
}

void CollisionMesh3DComponent::SetMeshSolver(MeshSolverType solver) {
    m_MeshSolver = solver;
    SetPropertyValue("meshSolver", static_cast<int>(solver));
    UpdateCollider();
}

Vec3 CollisionMesh3DComponent::GetOffset() const {
    return m_Offset;
}

void CollisionMesh3DComponent::SetOffset(const Vec3& offset) {
    m_Offset = offset;
    SetPropertyValue("offset", nlohmann::json::array({offset.x, offset.y, offset.z}));
    if (m_Collider) {
        m_Collider->SetOffset(m_Offset);
    }
}

float CollisionMesh3DComponent::GetDensity() const {
    return m_Density;
}

void CollisionMesh3DComponent::SetDensity(float density) {
    m_Density = density;
    SetPropertyValue("density", density);
    if (m_Collider) {
        m_Collider->SetDensity(m_Density);
    }
}

float CollisionMesh3DComponent::GetFriction() const {
    return m_Friction;
}

void CollisionMesh3DComponent::SetFriction(float friction) {
    m_Friction = friction;
    SetPropertyValue("friction", friction);
    if (m_Collider) {
        m_Collider->SetFriction(m_Friction);
    }
}

float CollisionMesh3DComponent::GetRestitution() const {
    return m_Restitution;
}

void CollisionMesh3DComponent::SetRestitution(float restitution) {
    m_Restitution = restitution;
    SetPropertyValue("restitution", restitution);
    if (m_Collider) {
        m_Collider->SetRestitution(m_Restitution);
    }
}

bool CollisionMesh3DComponent::IsSensor() const {
    return m_IsSensor;
}

void CollisionMesh3DComponent::SetSensor(bool isSensor) {
    m_IsSensor = isSensor;
    SetPropertyValue("isSensor", isSensor);
    if (m_Collider) {
        m_Collider->SetIsSensor(m_IsSensor);
    }
}

uint32_t CollisionMesh3DComponent::GetCollisionLayers() const {
    return m_CollisionLayers;
}

void CollisionMesh3DComponent::SetCollisionLayers(uint32_t layers) {
    m_CollisionLayers = layers;
    SetPropertyValue("collisionLayers", static_cast<int>(layers));
    if (m_Collider) {
        m_Collider->SetCollisionLayers(layers);
    }
}

Color CollisionMesh3DComponent::GetDebugColor() const {
    return m_DebugColor;
}

void CollisionMesh3DComponent::SetDebugColor(const Color& color) {
    m_DebugColor = color;
    SetPropertyValue("debugColor", nlohmann::json::array({color.r, color.g, color.b, color.a}));
}

physics3d::RigidBody3D* CollisionMesh3DComponent::FindPhysicsBody() {
    if (!m_Owner) return nullptr;

    auto rigidBody = m_Owner->GetComponent<RigidBody3DComponent>();
    if (rigidBody && rigidBody->GetPhysicsBody()) {
        return rigidBody->GetPhysicsBody();
    }

    auto staticBody = m_Owner->GetComponent<StaticBody3DComponent>();
    if (staticBody && staticBody->GetPhysicsBody()) {
        return staticBody->GetPhysicsBody();
    }

    auto kinematicBody = m_Owner->GetComponent<KinematicBody3DComponent>();
    if (kinematicBody && kinematicBody->GetPhysicsBody()) {
        return kinematicBody->GetPhysicsBody();
    }

    auto parent = m_Owner->GetParent();
    if (parent) {
        rigidBody = parent->GetComponent<RigidBody3DComponent>();
        if (rigidBody && rigidBody->GetPhysicsBody()) {
            return rigidBody->GetPhysicsBody();
        }

        staticBody = parent->GetComponent<StaticBody3DComponent>();
        if (staticBody && staticBody->GetPhysicsBody()) {
            return staticBody->GetPhysicsBody();
        }

        kinematicBody = parent->GetComponent<KinematicBody3DComponent>();
        if (kinematicBody && kinematicBody->GetPhysicsBody()) {
            return kinematicBody->GetPhysicsBody();
        }
    }

    return nullptr;
}

void CollisionMesh3DComponent::LoadMeshVertices() {
    m_MeshVertices.clear();
    m_MeshIndices.clear();

    if (m_MeshPath.empty()) {
        return;
    }

    // Try to get cached mesh data first (avoids re-loading the mesh file)
    if (PhysicsShapeCache::Instance().GetMeshData(m_MeshPath, m_MeshVertices, m_MeshIndices)) {
        return;
    }

    // Fallback to direct loading (this path typically won't be hit due to cache)
    asset::AssetRef<asset::ModelAsset> modelAsset(new asset::ModelAsset());
    bool loaded = modelAsset->LoadFromFile(m_MeshPath, true);

    if (!loaded || !modelAsset.IsValid()) {
        return;
    }

    const auto& meshes = modelAsset->GetMeshes();
    for (const auto& mesh : meshes) {
        uint32_t vertexOffset = static_cast<uint32_t>(m_MeshVertices.size());

        for (const auto& vertex : mesh.vertices) {
            m_MeshVertices.push_back(vertex.position);
        }

        for (const auto& index : mesh.indices) {
            m_MeshIndices.push_back(vertexOffset + index);
        }
    }
}

void CollisionMesh3DComponent::CreateCollider() {

    m_PhysicsBody = FindPhysicsBody();
    if (!m_PhysicsBody) {
        return;
    }

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    Vec3 scale = node3D ? node3D->GetGlobalScale() : Vec3(1.0f, 1.0f, 1.0f);

    PhysicsMaterial3D material(m_Friction, m_Restitution, m_Density);

    switch (m_ShapeType) {
        case CollisionShape3DType::Box: {
            UUID colliderID;
            Vec3 scaledSize = m_Size * scale;
            auto boxCollider = std::make_unique<BoxCollider3D>(m_PhysicsBody, colliderID, scaledSize);
            boxCollider->SetMaterial(material);
            boxCollider->SetIsSensor(m_IsSensor);
            boxCollider->SetOffset(m_Offset);
            boxCollider->SetCollisionLayers(m_CollisionLayers);
            m_Collider = std::move(boxCollider);

            break;
        }

        case CollisionShape3DType::Sphere: {
            UUID colliderID;

            float scaledRadius = m_Radius * std::max({scale.x, scale.y, scale.z});
            auto sphereCollider = std::make_unique<SphereCollider3D>(m_PhysicsBody, colliderID, scaledRadius);
            sphereCollider->SetMaterial(material);
            sphereCollider->SetIsSensor(m_IsSensor);
            sphereCollider->SetOffset(m_Offset);
            sphereCollider->SetCollisionLayers(m_CollisionLayers);
            m_Collider = std::move(sphereCollider);

            break;
        }

        case CollisionShape3DType::Capsule: {
            UUID colliderID;
            float scaledRadius = m_Radius * std::max(scale.x, scale.z);
            float scaledHeight = m_Height * scale.y;
            auto capsuleCollider = std::make_unique<CapsuleCollider3D>(m_PhysicsBody, colliderID, scaledRadius, scaledHeight);
            capsuleCollider->SetMaterial(material);
            capsuleCollider->SetIsSensor(m_IsSensor);
            capsuleCollider->SetOffset(m_Offset);
            capsuleCollider->SetCollisionLayers(m_CollisionLayers);
            m_Collider = std::move(capsuleCollider);

            break;
        }

        case CollisionShape3DType::Cylinder: {
            UUID colliderID;
            float scaledRadius = m_Radius * std::max(scale.x, scale.z);
            float scaledHeight = m_Height * scale.y;
            auto cylinderCollider = std::make_unique<CylinderCollider3D>(m_PhysicsBody, colliderID, scaledRadius, scaledHeight);
            cylinderCollider->SetMaterial(material);
            cylinderCollider->SetIsSensor(m_IsSensor);
            cylinderCollider->SetOffset(m_Offset);
            cylinderCollider->SetCollisionLayers(m_CollisionLayers);
            m_Collider = std::move(cylinderCollider);

            break;
        }

        case CollisionShape3DType::Cone: {
            UUID colliderID;
            float scaledRadius = m_Radius * std::max(scale.x, scale.z);
            float scaledHeight = m_Height * scale.y;
            auto coneCollider = std::make_unique<ConeCollider3D>(m_PhysicsBody, colliderID, scaledRadius, scaledHeight);
            coneCollider->SetMaterial(material);
            coneCollider->SetIsSensor(m_IsSensor);
            coneCollider->SetOffset(m_Offset);
            coneCollider->SetCollisionLayers(m_CollisionLayers);
            m_Collider = std::move(coneCollider);

            break;
        }

        case CollisionShape3DType::Plane: {

            UUID colliderID;

            float thickness = 0.01f;
            Vec3 planeSize;

            Vec3 absNormal(std::abs(m_PlaneNormal.x), std::abs(m_PlaneNormal.y), std::abs(m_PlaneNormal.z));

            if (absNormal.y > absNormal.x && absNormal.y > absNormal.z) {

                planeSize = Vec3(m_PlaneWidth, thickness, m_PlaneLength) * scale;
            } else if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {

                planeSize = Vec3(thickness, m_PlaneLength, m_PlaneWidth) * scale;
            } else {

                planeSize = Vec3(m_PlaneWidth, m_PlaneLength, thickness) * scale;
            }

            auto boxCollider = std::make_unique<BoxCollider3D>(m_PhysicsBody, colliderID, planeSize);
            boxCollider->SetMaterial(material);
            boxCollider->SetIsSensor(m_IsSensor);
            boxCollider->SetOffset(m_Offset + m_PlaneNormal * m_PlaneDistance);
            boxCollider->SetCollisionLayers(m_CollisionLayers);
            m_Collider = std::move(boxCollider);

            break;
        }

        case CollisionShape3DType::Mesh: {
            if (m_MeshVertices.empty()) {
                LoadMeshVertices();
            }

            if (m_MeshVertices.empty()) {

                return;
            }

            UUID colliderID;
            std::vector<Vec3> vertices;

            switch (m_MeshSolver) {
                case MeshSolverType::BoundingBox: {

                    Vec3 min = m_MeshVertices[0];
                    Vec3 max = m_MeshVertices[0];
                    for (const auto& v : m_MeshVertices) {
                        min.x = std::min(min.x, v.x);
                        min.y = std::min(min.y, v.y);
                        min.z = std::min(min.z, v.z);
                        max.x = std::max(max.x, v.x);
                        max.y = std::max(max.y, v.y);
                        max.z = std::max(max.z, v.z);
                    }
                    Vec3 size = max - min;
                    auto boxCollider = std::make_unique<BoxCollider3D>(m_PhysicsBody, colliderID, size);
                    boxCollider->SetMaterial(material);
                    boxCollider->SetIsSensor(m_IsSensor);
                    boxCollider->SetOffset(m_Offset + (min + max) * 0.5f);
                    boxCollider->SetCollisionLayers(m_CollisionLayers);
                    m_Collider = std::move(boxCollider);

                    break;
                }

                case MeshSolverType::Convex: {

                    std::unique_ptr<MeshCollider3D> meshCollider;
                    if (!m_MeshIndices.empty()) {
                        meshCollider = std::make_unique<MeshCollider3D>(m_PhysicsBody, colliderID, m_MeshVertices, m_MeshIndices, true);
                    } else {
                        meshCollider = std::make_unique<MeshCollider3D>(m_PhysicsBody, colliderID, m_MeshVertices, true);
                    }
                    meshCollider->SetMaterial(material);
                    meshCollider->SetIsSensor(m_IsSensor);
                    meshCollider->SetOffset(m_Offset);
                    meshCollider->SetCollisionLayers(m_CollisionLayers);
                    m_Collider = std::move(meshCollider);

                    break;
                }

                case MeshSolverType::Concave:
                case MeshSolverType::TriMesh: {
                    std::unique_ptr<MeshCollider3D> meshCollider;

                    // Try to use cached shape for better performance
                    if (!m_MeshPath.empty()) {
                        btBvhTriangleMeshShape* cachedShape = PhysicsShapeCache::Instance().GetOrCreateShape(m_MeshPath);
                        if (cachedShape) {
                            // Use the cached shape with scaling support
                            meshCollider = std::make_unique<MeshCollider3D>(
                                m_PhysicsBody, colliderID, cachedShape, m_MeshPath, scale);
                        }
                    }

                    // Fallback to building shape from vertices if cache failed
                    if (!meshCollider) {
                        if (!m_MeshIndices.empty()) {
                            meshCollider = std::make_unique<MeshCollider3D>(m_PhysicsBody, colliderID, m_MeshVertices, m_MeshIndices, false);
                        } else {
                            meshCollider = std::make_unique<MeshCollider3D>(m_PhysicsBody, colliderID, m_MeshVertices, false);
                        }
                    }

                    meshCollider->SetMaterial(material);
                    meshCollider->SetIsSensor(m_IsSensor);
                    meshCollider->SetOffset(m_Offset);
                    meshCollider->SetCollisionLayers(m_CollisionLayers);
                    m_Collider = std::move(meshCollider);
                    break;
                }
            }
            break;
        }
    }
}

void CollisionMesh3DComponent::DestroyCollider() {
    m_Collider.reset();
    m_PhysicsBody = nullptr;
}

void CollisionMesh3DComponent::UpdateCollider() {

    if (!m_Owner) {
        return;
    }

    DestroyCollider();
    CreateCollider();
}

void CollisionMesh3DComponent::DrawDebugShape() {

    if (!DebugDraw::IsAvailable()) {
        return;
    }

    if (!DebugDraw::IsCollisionShapesEnabled()) {
        return;
    }

    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera3D) {
        return;
    }

    if (!m_Owner) {
        return;
    }

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return;
    }

    Vec3 position = node3D->GetGlobalPosition();
    Vec3 scale = node3D->GetGlobalScale();
    Quat rotation = node3D->GetGlobalRotation();
    Mat4 rotationMatrix = rotation.ToMat4();

    Vec3 rotatedOffset = m_Offset;
    if (m_Offset.x != 0.0f || m_Offset.y != 0.0f || m_Offset.z != 0.0f) {
        Vec4 offset4(m_Offset.x, m_Offset.y, m_Offset.z, 0.0f);
        Vec4 rotated4 = rotationMatrix * offset4;
        rotatedOffset = Vec3(rotated4.x, rotated4.y, rotated4.z);
    }
    position = position + rotatedOffset;

    switch (m_ShapeType) {
        case CollisionShape3DType::Box: {
            Vec3 scaledSize = m_Size * scale;

            OBB obb(position, scaledSize * 0.5f, rotation);
            DebugDraw::OrientedBox3D(obb, m_DebugColor, 0.0f);
            break;
        }

        case CollisionShape3DType::Sphere: {

            float scaledRadius = m_Radius * std::max({scale.x, scale.y, scale.z});
            DebugDraw::Sphere(position, scaledRadius, m_DebugColor, true);
            break;
        }

        case CollisionShape3DType::Capsule: {

            float scaledRadius = m_Radius * std::max(scale.x, scale.z);
            float scaledHeight = m_Height * scale.y;

            Vec3 upAxis = Vec3(0, 1, 0);
            Vec4 rotatedUp4 = rotationMatrix * Vec4(upAxis.x, upAxis.y, upAxis.z, 0.0f);
            Vec3 rotatedUp(rotatedUp4.x, rotatedUp4.y, rotatedUp4.z);
            rotatedUp = rotatedUp.Normalized();

            Vec3 topPos = position + rotatedUp * (scaledHeight * 0.5f);
            Vec3 bottomPos = position - rotatedUp * (scaledHeight * 0.5f);

            DebugDraw::Sphere(topPos, scaledRadius, m_DebugColor, true);
            DebugDraw::Sphere(bottomPos, scaledRadius, m_DebugColor, true);

            Vec3 rightAxis = Vec3(1, 0, 0);
            Vec4 rotatedRight4 = rotationMatrix * Vec4(rightAxis.x, rightAxis.y, rightAxis.z, 0.0f);
            Vec3 rotatedRight(rotatedRight4.x, rotatedRight4.y, rotatedRight4.z);
            rotatedRight = rotatedRight.Normalized();

            DebugDraw::Line(bottomPos + rotatedRight * scaledRadius, topPos + rotatedRight * scaledRadius, m_DebugColor);
            DebugDraw::Line(bottomPos - rotatedRight * scaledRadius, topPos - rotatedRight * scaledRadius, m_DebugColor);
            break;
        }

        case CollisionShape3DType::Cylinder: {

            float scaledRadius = m_Radius * std::max(scale.x, scale.z);
            float scaledHeight = m_Height * scale.y;

            Vec3 upAxis = Vec3(0, 1, 0);
            Vec4 rotatedUp4 = rotationMatrix * Vec4(upAxis.x, upAxis.y, upAxis.z, 0.0f);
            Vec3 rotatedUp(rotatedUp4.x, rotatedUp4.y, rotatedUp4.z);
            rotatedUp = rotatedUp.Normalized();

            Vec3 topPos = position + rotatedUp * (scaledHeight * 0.5f);
            Vec3 bottomPos = position - rotatedUp * (scaledHeight * 0.5f);

            DebugDraw::Circle(topPos, rotatedUp, scaledRadius, m_DebugColor);
            DebugDraw::Circle(bottomPos, rotatedUp, scaledRadius, m_DebugColor);

            Vec3 rightAxis = Vec3(1, 0, 0);
            Vec4 rotatedRight4 = rotationMatrix * Vec4(rightAxis.x, rightAxis.y, rightAxis.z, 0.0f);
            Vec3 rotatedRight(rotatedRight4.x, rotatedRight4.y, rotatedRight4.z);
            rotatedRight = rotatedRight.Normalized();

            DebugDraw::Line(bottomPos + rotatedRight * scaledRadius, topPos + rotatedRight * scaledRadius, m_DebugColor);
            DebugDraw::Line(bottomPos - rotatedRight * scaledRadius, topPos - rotatedRight * scaledRadius, m_DebugColor);
            break;
        }

        case CollisionShape3DType::Cone: {

            float scaledRadius = m_Radius * std::max(scale.x, scale.z);
            float scaledHeight = m_Height * scale.y;

            Vec3 upAxis = Vec3(0, 1, 0);
            Vec4 rotatedUp4 = rotationMatrix * Vec4(upAxis.x, upAxis.y, upAxis.z, 0.0f);
            Vec3 rotatedUp(rotatedUp4.x, rotatedUp4.y, rotatedUp4.z);
            rotatedUp = rotatedUp.Normalized();

            Vec3 basePos = position - rotatedUp * (scaledHeight * 0.5f);
            Vec3 apex = position + rotatedUp * (scaledHeight * 0.5f);

            DebugDraw::Circle(basePos, rotatedUp, scaledRadius, m_DebugColor);

            Vec3 rightAxis = Vec3(1, 0, 0);
            Vec4 rotatedRight4 = rotationMatrix * Vec4(rightAxis.x, rightAxis.y, rightAxis.z, 0.0f);
            Vec3 rotatedRight(rotatedRight4.x, rotatedRight4.y, rotatedRight4.z);
            rotatedRight = rotatedRight.Normalized();

            Vec3 forwardAxis = Vec3(0, 0, 1);
            Vec4 rotatedForward4 = rotationMatrix * Vec4(forwardAxis.x, forwardAxis.y, forwardAxis.z, 0.0f);
            Vec3 rotatedForward(rotatedForward4.x, rotatedForward4.y, rotatedForward4.z);
            rotatedForward = rotatedForward.Normalized();

            DebugDraw::Line(basePos + rotatedRight * scaledRadius, apex, m_DebugColor);
            DebugDraw::Line(basePos - rotatedRight * scaledRadius, apex, m_DebugColor);
            DebugDraw::Line(basePos + rotatedForward * scaledRadius, apex, m_DebugColor);
            DebugDraw::Line(basePos - rotatedForward * scaledRadius, apex, m_DebugColor);
            break;
        }

        case CollisionShape3DType::Plane:

            {

                Vec4 rotatedNormal4 = rotationMatrix * Vec4(m_PlaneNormal.x, m_PlaneNormal.y, m_PlaneNormal.z, 0.0f);
                Vec3 rotatedNormal(rotatedNormal4.x, rotatedNormal4.y, rotatedNormal4.z);
                rotatedNormal = rotatedNormal.Normalized();

                Vec3 planePos = position + rotatedNormal * m_PlaneDistance;
                float thickness = 0.01f;
                Vec3 planeSize;

                Vec3 absNormal(std::abs(m_PlaneNormal.x), std::abs(m_PlaneNormal.y), std::abs(m_PlaneNormal.z));

                if (absNormal.y > absNormal.x && absNormal.y > absNormal.z) {

                    planeSize = Vec3(m_PlaneWidth, thickness, m_PlaneLength) * scale;
                } else if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {

                    planeSize = Vec3(thickness, m_PlaneLength, m_PlaneWidth) * scale;
                } else {

                    planeSize = Vec3(m_PlaneWidth, m_PlaneLength, thickness) * scale;
                }

                OBB planeOBB(planePos, planeSize * 0.5f, rotation);
                DebugDraw::OrientedBox3D(planeOBB, m_DebugColor, 0.0f);
            }
            break;

        case CollisionShape3DType::Mesh:

            if (!m_MeshVertices.empty()) {
                switch (m_MeshSolver) {
                    case MeshSolverType::BoundingBox: {

                        Vec3 min = m_MeshVertices[0];
                        Vec3 max = m_MeshVertices[0];
                        for (const auto& v : m_MeshVertices) {
                            min.x = std::min(min.x, v.x);
                            min.y = std::min(min.y, v.y);
                            min.z = std::min(min.z, v.z);
                            max.x = std::max(max.x, v.x);
                            max.y = std::max(max.y, v.y);
                            max.z = std::max(max.z, v.z);
                        }
                        Vec3 size = (max - min) * scale;
                        Vec3 center = (min + max) * 0.5f;

                        Vec4 rotatedCenter4 = rotationMatrix * Vec4(center.x, center.y, center.z, 0.0f);
                        Vec3 rotatedCenter(rotatedCenter4.x, rotatedCenter4.y, rotatedCenter4.z);

                        OBB meshOBB(position + rotatedCenter, size * 0.5f, rotation);
                        DebugDraw::OrientedBox3D(meshOBB, m_DebugColor, 0.0f);
                        break;
                    }

                    case MeshSolverType::Convex: {

                        const size_t MAX_DEBUG_TRIANGLES = 1000;

                        if (!m_MeshIndices.empty()) {

                            size_t triangleCount = m_MeshIndices.size() / 3;
                            size_t maxTriangles = std::min(triangleCount, MAX_DEBUG_TRIANGLES);

                            for (size_t tri = 0; tri < maxTriangles; ++tri) {
                                size_t i = tri * 3;
                                uint32_t i0 = m_MeshIndices[i];
                                uint32_t i1 = m_MeshIndices[i + 1];
                                uint32_t i2 = m_MeshIndices[i + 2];

                                if (i0 < m_MeshVertices.size() && i1 < m_MeshVertices.size() && i2 < m_MeshVertices.size()) {

                                    Vec4 v0_4 = rotationMatrix * Vec4(m_MeshVertices[i0].x * scale.x, m_MeshVertices[i0].y * scale.y, m_MeshVertices[i0].z * scale.z, 0.0f);
                                    Vec4 v1_4 = rotationMatrix * Vec4(m_MeshVertices[i1].x * scale.x, m_MeshVertices[i1].y * scale.y, m_MeshVertices[i1].z * scale.z, 0.0f);
                                    Vec4 v2_4 = rotationMatrix * Vec4(m_MeshVertices[i2].x * scale.x, m_MeshVertices[i2].y * scale.y, m_MeshVertices[i2].z * scale.z, 0.0f);

                                    Vec3 v0 = position + Vec3(v0_4.x, v0_4.y, v0_4.z);
                                    Vec3 v1 = position + Vec3(v1_4.x, v1_4.y, v1_4.z);
                                    Vec3 v2 = position + Vec3(v2_4.x, v2_4.y, v2_4.z);

                                    DebugDraw::Line(v0, v1, m_DebugColor);
                                    DebugDraw::Line(v1, v2, m_DebugColor);
                                    DebugDraw::Line(v2, v0, m_DebugColor);
                                }
                            }
                        } else {

                            size_t triangleCount = m_MeshVertices.size() / 3;
                            size_t maxTriangles = std::min(triangleCount, MAX_DEBUG_TRIANGLES);

                            for (size_t tri = 0; tri < maxTriangles; ++tri) {
                                size_t i = tri * 3;

                                Vec4 v0_4 = rotationMatrix * Vec4(m_MeshVertices[i].x * scale.x, m_MeshVertices[i].y * scale.y, m_MeshVertices[i].z * scale.z, 0.0f);
                                Vec4 v1_4 = rotationMatrix * Vec4(m_MeshVertices[i+1].x * scale.x, m_MeshVertices[i+1].y * scale.y, m_MeshVertices[i+1].z * scale.z, 0.0f);
                                Vec4 v2_4 = rotationMatrix * Vec4(m_MeshVertices[i+2].x * scale.x, m_MeshVertices[i+2].y * scale.y, m_MeshVertices[i+2].z * scale.z, 0.0f);

                                Vec3 v0 = position + Vec3(v0_4.x, v0_4.y, v0_4.z);
                                Vec3 v1 = position + Vec3(v1_4.x, v1_4.y, v1_4.z);
                                Vec3 v2 = position + Vec3(v2_4.x, v2_4.y, v2_4.z);

                                DebugDraw::Line(v0, v1, m_DebugColor);
                                DebugDraw::Line(v1, v2, m_DebugColor);
                                DebugDraw::Line(v2, v0, m_DebugColor);
                            }
                        }
                        break;
                    }

                    case MeshSolverType::Concave:
                    case MeshSolverType::TriMesh: {

                        const size_t MAX_DEBUG_TRIANGLES = 1000;

                        if (!m_MeshIndices.empty()) {

                            size_t triangleCount = m_MeshIndices.size() / 3;
                            size_t maxTriangles = std::min(triangleCount, MAX_DEBUG_TRIANGLES);

                            for (size_t tri = 0; tri < maxTriangles; ++tri) {
                                size_t i = tri * 3;
                                uint32_t i0 = m_MeshIndices[i];
                                uint32_t i1 = m_MeshIndices[i + 1];
                                uint32_t i2 = m_MeshIndices[i + 2];

                                if (i0 < m_MeshVertices.size() && i1 < m_MeshVertices.size() && i2 < m_MeshVertices.size()) {

                                    Vec4 v0_4 = rotationMatrix * Vec4(m_MeshVertices[i0].x * scale.x, m_MeshVertices[i0].y * scale.y, m_MeshVertices[i0].z * scale.z, 0.0f);
                                    Vec4 v1_4 = rotationMatrix * Vec4(m_MeshVertices[i1].x * scale.x, m_MeshVertices[i1].y * scale.y, m_MeshVertices[i1].z * scale.z, 0.0f);
                                    Vec4 v2_4 = rotationMatrix * Vec4(m_MeshVertices[i2].x * scale.x, m_MeshVertices[i2].y * scale.y, m_MeshVertices[i2].z * scale.z, 0.0f);

                                    Vec3 v0 = position + Vec3(v0_4.x, v0_4.y, v0_4.z);
                                    Vec3 v1 = position + Vec3(v1_4.x, v1_4.y, v1_4.z);
                                    Vec3 v2 = position + Vec3(v2_4.x, v2_4.y, v2_4.z);

                                    DebugDraw::Line(v0, v1, m_DebugColor);
                                    DebugDraw::Line(v1, v2, m_DebugColor);
                                    DebugDraw::Line(v2, v0, m_DebugColor);
                                }
                            }
                        } else {

                            size_t triangleCount = m_MeshVertices.size() / 3;
                            size_t maxTriangles = std::min(triangleCount, MAX_DEBUG_TRIANGLES);

                            for (size_t tri = 0; tri < maxTriangles; ++tri) {
                                size_t i = tri * 3;

                                Vec4 v0_4 = rotationMatrix * Vec4(m_MeshVertices[i].x * scale.x, m_MeshVertices[i].y * scale.y, m_MeshVertices[i].z * scale.z, 0.0f);
                                Vec4 v1_4 = rotationMatrix * Vec4(m_MeshVertices[i+1].x * scale.x, m_MeshVertices[i+1].y * scale.y, m_MeshVertices[i+1].z * scale.z, 0.0f);
                                Vec4 v2_4 = rotationMatrix * Vec4(m_MeshVertices[i+2].x * scale.x, m_MeshVertices[i+2].y * scale.y, m_MeshVertices[i+2].z * scale.z, 0.0f);

                                Vec3 v0 = position + Vec3(v0_4.x, v0_4.y, v0_4.z);
                                Vec3 v1 = position + Vec3(v1_4.x, v1_4.y, v1_4.z);
                                Vec3 v2 = position + Vec3(v2_4.x, v2_4.y, v2_4.z);

                                DebugDraw::Line(v0, v1, m_DebugColor);
                                DebugDraw::Line(v1, v2, m_DebugColor);
                                DebugDraw::Line(v2, v0, m_DebugColor);
                            }
                        }
                        break;
                    }
                }
            }
            break;
    }
}

}
}

