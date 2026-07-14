#include "lupine/components/AreaTrigger3DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics3d;

AreaTrigger3DComponent::AreaTrigger3DComponent()
    : Component("AreaTrigger3DComponent")
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_ShapeType(CollisionShape3DType::Box)
    , m_Size(Vec3(1.0f, 1.0f, 1.0f))
    , m_Radius(0.5f)
    , m_Height(1.0f)
    , m_ShapeOffset(Vec3::Zero())
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

AreaTrigger3DComponent::AreaTrigger3DComponent(const std::string& name)
    : Component(name)
    , m_PhysicsBody(nullptr)
    , m_BodyCreated(false)
    , m_ShapeType(CollisionShape3DType::Box)
    , m_Size(Vec3(1.0f, 1.0f, 1.0f))
    , m_Radius(0.5f)
    , m_Height(1.0f)
    , m_ShapeOffset(Vec3::Zero())
    , m_LastPosition(Vec3::Zero())
    , m_LastRotation(Quat::Identity())
{
}

AreaTrigger3DComponent::~AreaTrigger3DComponent() {
    DestroyPhysicsBody();
}

void AreaTrigger3DComponent::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(monitoring, Bool, true, "Trigger"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(monitorable, Bool, true, "Trigger"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(priority, 0, 0, 100, 1, "Trigger"));

    DefineProperty(PROPERTY_ENUM_GROUP(shapeType, 0, "Shape", Box, Sphere, Capsule, Cylinder, Cone));
    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec3, math::Vec3(1.0f, 1.0f, 1.0f), "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 0.5f, 0.01f, 1000.0f, 0.05f, "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 1.0f, 0.01f, 1000.0f, 0.05f, "Shape"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shapeOffset, Vec3, math::Vec3::Zero(), "Shape"));
}

void AreaTrigger3DComponent::OnAwake() {
    m_ShapeType = static_cast<CollisionShape3DType>(GetPropertyValue<int>("shapeType"));
    m_Size = GetPropertyValue<math::Vec3>("size");
    m_Radius = GetPropertyValue<float>("radius");
    m_Height = GetPropertyValue<float>("height");
    m_ShapeOffset = GetPropertyValue<math::Vec3>("shapeOffset");

    CreatePhysicsBody();
}

void AreaTrigger3DComponent::OnReady() {

    if (m_PhysicsBody) {
        CreateSensorCollider();
        SyncTransformToPhysics();
    }
}

void AreaTrigger3DComponent::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "shapeType") {
        m_ShapeType = static_cast<CollisionShape3DType>(newValue.get<int>());
        RebuildSensorCollider();
    } else if (propertyName == "size") {
        m_Size = math::Vec3(newValue["x"].get<float>(), newValue["y"].get<float>(), newValue["z"].get<float>());
        RebuildSensorCollider();
    } else if (propertyName == "radius") {
        m_Radius = newValue.get<float>();
        RebuildSensorCollider();
    } else if (propertyName == "height") {
        m_Height = newValue.get<float>();
        RebuildSensorCollider();
    } else if (propertyName == "shapeOffset") {
        m_ShapeOffset = math::Vec3(newValue["x"].get<float>(), newValue["y"].get<float>(), newValue["z"].get<float>());
        if (m_Collider) {
            m_Collider->SetOffset(m_ShapeOffset);
        }
    }
}

CollisionShape3DType AreaTrigger3DComponent::GetShapeType() const {
    return static_cast<CollisionShape3DType>(GetPropertyValue<int>("shapeType"));
}

void AreaTrigger3DComponent::SetShapeType(CollisionShape3DType type) {
    m_ShapeType = type;
    SetPropertyValue("shapeType", static_cast<int>(type));
    RebuildSensorCollider();
}

Vec3 AreaTrigger3DComponent::GetSize() const {
    return GetPropertyValue<math::Vec3>("size");
}

void AreaTrigger3DComponent::SetSize(const Vec3& size) {
    m_Size = size;
    SetPropertyValue("size", size);
    RebuildSensorCollider();
}

float AreaTrigger3DComponent::GetRadius() const {
    return GetPropertyValue<float>("radius");
}

void AreaTrigger3DComponent::SetRadius(float radius) {
    m_Radius = radius;
    SetPropertyValue("radius", radius);
    RebuildSensorCollider();
}

float AreaTrigger3DComponent::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void AreaTrigger3DComponent::SetHeight(float height) {
    m_Height = height;
    SetPropertyValue("height", height);
    RebuildSensorCollider();
}

Vec3 AreaTrigger3DComponent::GetShapeOffset() const {
    return GetPropertyValue<math::Vec3>("shapeOffset");
}

void AreaTrigger3DComponent::SetShapeOffset(const Vec3& offset) {
    m_ShapeOffset = offset;
    SetPropertyValue("shapeOffset", offset);
    if (m_Collider) {
        m_Collider->SetOffset(offset);
    }
}

void AreaTrigger3DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void AreaTrigger3DComponent::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
    // See Component::PhysicsWorldRebuildPhase. A trigger owns both a body and its sensor
    // collider, so both are rebuilt here (mirroring OnAwake then OnReady).
    switch (phase) {
        case PhysicsWorldRebuildPhase::SaveState:
            break;

        case PhysicsWorldRebuildPhase::RecreateBodies:
            // Destroying the body already told the collider its body is gone (see
            // RigidBody3D's destructor), so resetting the unique_ptr is safe and does not
            // reach into the destroyed world. Reset it directly rather than calling
            // DestroyPhysicsBody, which would DestroyBody a body that no longer exists.
            m_Collider.reset();
            m_PhysicsBody = nullptr;
            m_BodyCreated = false;
            CreatePhysicsBody();
            break;

        case PhysicsWorldRebuildPhase::AttachColliders:
            if (m_PhysicsBody) {
                CreateSensorCollider();
                SyncTransformToPhysics();
            }
            break;
    }
}

void AreaTrigger3DComponent::OnPhysicsProcess(float) {
    if (!m_PhysicsBody) return;

    SyncTransformFromPhysics();
}

bool AreaTrigger3DComponent::GetMonitoring() const {
    return GetPropertyValue<bool>("monitoring");
}

void AreaTrigger3DComponent::SetMonitoring(bool monitoring) {
    SetPropertyValue("monitoring", monitoring);
}

bool AreaTrigger3DComponent::GetMonitorable() const {
    return GetPropertyValue<bool>("monitorable");
}

void AreaTrigger3DComponent::SetMonitorable(bool monitorable) {
    SetPropertyValue("monitorable", monitorable);
}

int AreaTrigger3DComponent::GetPriority() const {
    return GetPropertyValue<int>("priority");
}

void AreaTrigger3DComponent::SetPriority(int priority) {
    SetPropertyValue("priority", priority);
}

bool AreaTrigger3DComponent::IsOverlapping(const UUID& bodyId) const {
    return m_OverlappingBodies.find(bodyId) != m_OverlappingBodies.end();
}

void AreaTrigger3DComponent::CreatePhysicsBody() {
    if (m_BodyCreated) return;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) {

        return;
    }

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) {

        return;
    }

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, BodyType::Static);
    if (!m_PhysicsBody) {

        return;
    }

    physicsWorld->SetBodyNode(m_PhysicsBodyId, m_Owner);

    physicsWorld->RegisterTriggerEnter(m_PhysicsBodyId,
        [this](const physics3d::CollisionInfo3D& info) {

            UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
            OnTriggerEnter(otherBodyId);
        });

    physicsWorld->RegisterTriggerExit(m_PhysicsBodyId,
        [this](const physics3d::CollisionInfo3D& info) {

            UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;
            OnTriggerExit(otherBodyId);
        });

    m_BodyCreated = true;

}

void AreaTrigger3DComponent::CreateSensorCollider() {
    if (!m_PhysicsBody) return;

    DestroySensorCollider();

    // Zero friction/restitution/density: a sensor contributes no mass and no response.
    const PhysicsMaterial3D material(0.0f, 0.0f, 0.0f);
    UUID colliderId;

    switch (m_ShapeType) {
        case CollisionShape3DType::Sphere:
            m_Collider = std::make_unique<SphereCollider3D>(m_PhysicsBody, colliderId, m_Radius);
            break;
        case CollisionShape3DType::Capsule:
            m_Collider = std::make_unique<CapsuleCollider3D>(m_PhysicsBody, colliderId, m_Radius, m_Height);
            break;
        case CollisionShape3DType::Cylinder:
            m_Collider = std::make_unique<CylinderCollider3D>(m_PhysicsBody, colliderId, m_Radius, m_Height);
            break;
        case CollisionShape3DType::Cone:
            m_Collider = std::make_unique<ConeCollider3D>(m_PhysicsBody, colliderId, m_Radius, m_Height);
            break;
        case CollisionShape3DType::Box:
        default:
            m_Collider = std::make_unique<BoxCollider3D>(m_PhysicsBody, colliderId, m_Size);
            break;
    }

    if (!m_Collider) return;

    m_Collider->SetMaterial(material);
    m_Collider->SetOffset(m_ShapeOffset);

    // Marks the body CF_NO_CONTACT_RESPONSE. Without it the area would physically block
    // whatever walked into it, and the physics world would report its manifolds as
    // collisions rather than trigger overlaps.
    m_Collider->SetIsSensor(true);
}

void AreaTrigger3DComponent::DestroySensorCollider() {
    m_Collider.reset();
}

void AreaTrigger3DComponent::RebuildSensorCollider() {
    if (m_PhysicsBody) {
        CreateSensorCollider();
    }
}

void AreaTrigger3DComponent::DestroyPhysicsBody() {
    if (!m_BodyCreated) return;

    auto* sceneManager = SceneManager::GetInstance();

    // The collider is owned by this component but registered with the body. Release it
    // before the body goes away so the body is not asked to detach a shape it no longer has.
    DestroySensorCollider();

    if (sceneManager && !sceneManager->IsShuttingDown()) {
        auto* physicsWorld = sceneManager->GetPhysics3DWorld();
        if (physicsWorld) {
            physicsWorld->DestroyBody(m_PhysicsBodyId);
        }
    }

    m_PhysicsBody = nullptr;
    m_BodyCreated = false;
    m_OverlappingBodies.clear();
}

void AreaTrigger3DComponent::SyncTransformToPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 position = node3D->GetGlobalPosition();
    Quat rotation = node3D->GetGlobalRotation();

    m_PhysicsBody->SetPosition(position);
    m_PhysicsBody->SetRotation(rotation);

    m_LastPosition = position;
    m_LastRotation = rotation;
}

void AreaTrigger3DComponent::SyncTransformFromPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 globalPosition = m_PhysicsBody->GetPosition();
    Quat globalRotation = m_PhysicsBody->GetRotation();

    if (globalPosition != m_LastPosition || globalRotation != m_LastRotation) {

        Vec3 localPosition = globalPosition;
        Quat localRotation = globalRotation;

        if (node3D->GetParent()) {
            Node3D* parent3D = dynamic_cast<Node3D*>(node3D->GetParent());
            if (parent3D) {

                Vec3 parentGlobalPos = parent3D->GetGlobalPosition();
                Quat parentGlobalRot = parent3D->GetGlobalRotation();

                Vec3 relativePos = globalPosition - parentGlobalPos;
                localPosition = parentGlobalRot.Inverse() * relativePos;

                localRotation = parentGlobalRot.Inverse() * globalRotation;
            }
        }

        node3D->SetPosition(localPosition);
        node3D->SetRotation(localRotation);

        m_LastPosition = globalPosition;
        m_LastRotation = globalRotation;
    }
}

void AreaTrigger3DComponent::OnTriggerEnter(const UUID& otherBodyId) {
    if (!GetMonitoring()) return;

    m_OverlappingBodies.insert(otherBodyId);

    if (m_OnBodyEntered) {
        m_OnBodyEntered(otherBodyId);
    }
    Emit("body_entered", { ResolveOtherBodyNodeArg(otherBodyId) });
}

void AreaTrigger3DComponent::OnTriggerExit(const UUID& otherBodyId) {
    if (!GetMonitoring()) return;

    m_OverlappingBodies.erase(otherBodyId);

    if (m_OnBodyExited) {
        m_OnBodyExited(otherBodyId);
    }
    Emit("body_exited", { ResolveOtherBodyNodeArg(otherBodyId) });
}

nlohmann::json AreaTrigger3DComponent::ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const {
    auto* sceneManager = core::SceneManager::GetInstance();
    if (sceneManager) {
        auto* physicsWorld = sceneManager->GetPhysics3DWorld();
        if (physicsWorld) {
            return core::Node::NodeArg(physicsWorld->GetBodyNode(otherBodyId));
        }
    }
    return nlohmann::json(nullptr);
}

void AreaTrigger3DComponent::DefineSignals() {
    RegisterSignal({"body_entered",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body enters this area."});
    RegisterSignal({"body_exited",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body leaves this area."});
}

}
}
