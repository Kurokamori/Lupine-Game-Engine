#include "lupine/components/AreaTrigger2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

AreaTrigger2DComponent::AreaTrigger2DComponent()
    : AreaTrigger2DComponent("AreaTrigger2DComponent") {
}

AreaTrigger2DComponent::AreaTrigger2DComponent(const std::string& name)
    : Component(name),
      m_PhysicsBody(nullptr),
      m_BodyCreated(false),
      m_ShapeType(CollisionShape2DType::Rectangle),
      m_Size(math::Vec2(100.0f, 100.0f)),
      m_Radius(50.0f),
      m_Offset(math::Vec2::Zero()),
      m_CollisionLayers(1),
      m_CollisionMask(0xFFFFFFFFu),
      m_LastPosition(0.0f, 0.0f),
      m_LastRotation(0.0f) {
}

AreaTrigger2DComponent::~AreaTrigger2DComponent() {
    DestroyPhysicsBody();
}

void AreaTrigger2DComponent::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(monitoring, Bool, true, "Trigger"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(monitorable, Bool, true, "Trigger"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(priority, 0, 0, 100, 1, "Trigger"));

    DefineProperty(PROPERTY_ENUM_GROUP(shapeType, 0, "Shape", Rectangle, Circle, Triangle, Pentagon, Hexagon, Polygon));
    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec2, math::Vec2(100.0f, 100.0f), "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 50.0f, 0.01f, 1000.0f, 1.0f, "Shape"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, math::Vec2::Zero(), "Shape"));

    DefineProperty(PROPERTY_GROUP(collisionLayers, Int, 1, Layers2D, "", "Collision"));
    DefineProperty(PROPERTY_GROUP(collisionMask, Int, static_cast<int>(0xFFFFFFFF), Layers2D, "", "Collision"));
}

nlohmann::json AreaTrigger2DComponent::Serialize() const {
    nlohmann::json json = Component::Serialize();

    nlohmann::json verticesArray = nlohmann::json::array();
    for (const auto& vertex : m_Vertices) {
        verticesArray.push_back({{"x", vertex.x}, {"y", vertex.y}});
    }

    if (json.contains("properties")) {
        json["properties"]["vertices"] = verticesArray;
    }

    return json;
}

void AreaTrigger2DComponent::Deserialize(const nlohmann::json& json) {
    Component::Deserialize(json);

    if (json.contains("properties") && json["properties"].contains("vertices")) {
        const auto& verticesJson = json["properties"]["vertices"];
        if (verticesJson.is_array()) {
            m_Vertices.clear();
            for (const auto& vertexJson : verticesJson) {
                if (vertexJson.contains("x") && vertexJson.contains("y")) {
                    math::Vec2 vertex;
                    vertex.x = vertexJson["x"].get<float>();
                    vertex.y = vertexJson["y"].get<float>();
                    m_Vertices.push_back(vertex);
                }
            }
        }
    }

    m_ShapeType = static_cast<CollisionShape2DType>(GetPropertyValue<int>("shapeType"));
    m_Size = GetPropertyValue<math::Vec2>("size");
    m_Radius = GetPropertyValue<float>("radius");
    m_Offset = GetPropertyValue<math::Vec2>("offset");
    m_CollisionLayers = static_cast<uint32_t>(GetPropertyValue<int>("collisionLayers"));

    bool hasMask = json.contains("properties") && json["properties"].contains("collisionMask");
    if (hasMask) {
        m_CollisionMask = static_cast<uint32_t>(GetPropertyValue<int>("collisionMask"));
    } else {
        m_CollisionMask = m_CollisionLayers;
        SetPropertyValue("collisionMask", static_cast<int>(m_CollisionMask));
    }
}

void AreaTrigger2DComponent::OnAwake() {
    if (m_ShapeType != CollisionShape2DType::Polygon || m_Vertices.empty()) {
        CollisionBody2DComponent::GenerateShapeVertices(m_ShapeType, m_Size, m_Radius, m_Vertices);
    }

    CreatePhysicsBody();
}

void AreaTrigger2DComponent::OnReady() {
    if (m_PhysicsBody) {
        CreateSensorCollider();
        SyncTransformToPhysics();
    }
}

void AreaTrigger2DComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void AreaTrigger2DComponent::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
    // See Component::PhysicsWorldRebuildPhase. A trigger owns both a body and its sensor
    // collider, so both are rebuilt here (mirroring OnAwake then OnReady).
    switch (phase) {
        case PhysicsWorldRebuildPhase::SaveState:
            break;

        case PhysicsWorldRebuildPhase::RecreateBodies:
            // Destroying the body already told the collider its body is gone (see
            // RigidBody2D's destructor), so resetting the unique_ptr is safe and does not
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

void AreaTrigger2DComponent::OnPhysicsProcess(float) {
    if (!m_PhysicsBody) return;

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        // Compared in world space: an ancestor may have moved even when this node's local
        // transform is unchanged, and the sensor has to follow.
        const math::Vec2 globalPosition = node2D->GetGlobalPosition();
        const float globalRotation = node2D->GetGlobalRotation();

        if (globalPosition != m_LastPosition || globalRotation != m_LastRotation) {
            SyncTransformToPhysics();
        }

        const math::Vec2 globalScale = node2D->GetGlobalScale();
        if (m_Collider && (globalScale.x != m_LastScale.x || globalScale.y != m_LastScale.y)) {
            m_Collider->SetScale(globalScale);
            m_LastScale = globalScale;
        }
    }
}

void AreaTrigger2DComponent::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "shapeType") {
        m_ShapeType = static_cast<CollisionShape2DType>(newValue.get<int>());
        CollisionBody2DComponent::GenerateShapeVertices(m_ShapeType, m_Size, m_Radius, m_Vertices);
        RebuildSensorCollider();
    }
    else if (propertyName == "size") {
        m_Size = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
        if (m_ShapeType == CollisionShape2DType::Rectangle) {
            CollisionBody2DComponent::GenerateShapeVertices(m_ShapeType, m_Size, m_Radius, m_Vertices);
            RebuildSensorCollider();
        }
    }
    else if (propertyName == "radius") {
        m_Radius = newValue.get<float>();
        if (m_ShapeType != CollisionShape2DType::Rectangle) {
            CollisionBody2DComponent::GenerateShapeVertices(m_ShapeType, m_Size, m_Radius, m_Vertices);
            RebuildSensorCollider();
        }
    }
    else if (propertyName == "offset") {
        m_Offset = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
        if (m_Collider) {
            m_Collider->SetOffset(m_Offset);
        }
    }
    else if (propertyName == "collisionLayers") {
        m_CollisionLayers = static_cast<uint32_t>(newValue.get<int>());
        if (m_Collider) {
            m_Collider->SetCollisionLayer(m_CollisionLayers);
        }
    }
    else if (propertyName == "collisionMask") {
        m_CollisionMask = static_cast<uint32_t>(newValue.get<int>());
        if (m_Collider) {
            m_Collider->SetCollisionMask(m_CollisionMask);
        }
    }
}

void AreaTrigger2DComponent::CreatePhysicsBody() {
    if (m_BodyCreated) return;

    auto* sceneManager = core::SceneManager::GetInstance();
    if (!sceneManager) {
        return;
    }

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) {
        return;
    }

    m_PhysicsBody = physicsWorld->CreateBody(m_PhysicsBodyId, physics2d::BodyType::Static);
    if (!m_PhysicsBody) {
        return;
    }

    // Associate the physics body with the owning node so trigger signals can
    // resolve the other overlapping body back to a node.
    physicsWorld->SetBodyNode(m_PhysicsBodyId, m_Owner);

    RegisterTriggerCallbacks();

    m_BodyCreated = true;
}

void AreaTrigger2DComponent::RegisterTriggerCallbacks() {
    auto* sceneManager = core::SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return;

    physicsWorld->RegisterTriggerEnter(m_PhysicsBodyId,
        [this](const physics2d::CollisionInfo& info) {
            OnTriggerEnterInternal(info);
        });

    physicsWorld->RegisterTriggerStay(m_PhysicsBodyId,
        [this](const physics2d::CollisionInfo& info) {
            OnTriggerStayInternal(info);
        });

    physicsWorld->RegisterTriggerExit(m_PhysicsBodyId,
        [this](const physics2d::CollisionInfo& info) {
            OnTriggerExitInternal(info);
        });
}

void AreaTrigger2DComponent::CreateSensorCollider() {
    if (!m_PhysicsBody) return;

    DestroySensorCollider();

    physics2d::PhysicsMaterial2D material(0.0f, 0.0f, 0.0f);

    switch (m_ShapeType) {
        case CollisionShape2DType::Circle: {
            core::UUID colliderId;
            auto circleCollider = std::make_unique<physics2d::CircleCollider2D>(
                m_PhysicsBody, colliderId, m_Radius);
            m_Collider = std::move(circleCollider);
            break;
        }

        case CollisionShape2DType::Rectangle: {
            core::UUID colliderId;
            auto boxCollider = std::make_unique<physics2d::BoxCollider2D>(
                m_PhysicsBody, colliderId, m_Size);
            m_Collider = std::move(boxCollider);
            break;
        }

        case CollisionShape2DType::Triangle:
        case CollisionShape2DType::Pentagon:
        case CollisionShape2DType::Hexagon:
        case CollisionShape2DType::Polygon: {
            if (m_Vertices.size() >= 3) {
                core::UUID colliderId;
                auto polygonCollider = std::make_unique<physics2d::PolygonCollider2D>(
                    m_PhysicsBody, colliderId, m_Vertices);
                m_Collider = std::move(polygonCollider);
            }
            break;
        }
    }

    const math::Vec2 globalScale = GetNodeGlobalScale();

    if (m_Collider) {
        m_Collider->SetMaterial(material);
        m_Collider->SetSensor(true);
        m_Collider->SetOffset(m_Offset);
        m_Collider->SetScale(globalScale);
        m_Collider->SetCollisionLayer(m_CollisionLayers);
        m_Collider->SetCollisionMask(m_CollisionMask);
    }

    m_LastScale = globalScale;
}

void AreaTrigger2DComponent::DestroySensorCollider() {
    m_Collider.reset();
}

math::Vec2 AreaTrigger2DComponent::GetNodeGlobalScale() const {
    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return math::Vec2(1.0f, 1.0f);
    }
    return node2D->GetGlobalScale();
}

void AreaTrigger2DComponent::RebuildSensorCollider() {
    if (m_PhysicsBody && m_BodyCreated) {
        CreateSensorCollider();
    }
}

void AreaTrigger2DComponent::DestroyPhysicsBody() {
    if (!m_BodyCreated) return;

    DestroySensorCollider();

    auto* sceneManager = core::SceneManager::GetInstance();

    bool isShuttingDown = (sceneManager && sceneManager->IsShuttingDown()) ||
                          (sceneManager && sceneManager->IsChangingScene()) ||
                          (m_Owner && m_Owner->GetScene() && m_Owner->GetScene()->IsShuttingDown());

    if (sceneManager && !isShuttingDown) {
        auto* physicsWorld = sceneManager->GetPhysics2DWorld();
        if (physicsWorld) {
            physicsWorld->DestroyBody(m_PhysicsBodyId);
        }
    }

    m_PhysicsBody = nullptr;
    m_BodyCreated = false;
    m_OverlappingBodies.clear();
}

void AreaTrigger2DComponent::SyncTransformToPhysics() {
    if (!m_PhysicsBody || !m_Owner) return;

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) return;

    const math::Vec2 globalPosition = node2D->GetGlobalPosition();
    const float globalRotation = node2D->GetGlobalRotation();

    m_PhysicsBody->SetPosition(globalPosition);
    m_PhysicsBody->SetRotation(globalRotation);

    m_LastPosition = globalPosition;
    m_LastRotation = globalRotation;
}

void AreaTrigger2DComponent::SyncTransformFromPhysics() {

}

bool AreaTrigger2DComponent::GetMonitoring() const {
    return GetPropertyValue<bool>("monitoring");
}

void AreaTrigger2DComponent::SetMonitoring(bool monitoring) {
    SetPropertyValue("monitoring", monitoring);
}

bool AreaTrigger2DComponent::GetMonitorable() const {
    return GetPropertyValue<bool>("monitorable");
}

void AreaTrigger2DComponent::SetMonitorable(bool monitorable) {
    SetPropertyValue("monitorable", monitorable);
}

int AreaTrigger2DComponent::GetPriority() const {
    return GetPropertyValue<int>("priority");
}

void AreaTrigger2DComponent::SetPriority(int priority) {
    SetPropertyValue("priority", priority);
}

CollisionShape2DType AreaTrigger2DComponent::GetShapeType() const {
    return static_cast<CollisionShape2DType>(GetPropertyValue<int>("shapeType"));
}

void AreaTrigger2DComponent::SetShapeType(CollisionShape2DType type) {
    SetPropertyValue("shapeType", static_cast<int>(type));
    m_ShapeType = type;
    CollisionBody2DComponent::GenerateShapeVertices(m_ShapeType, m_Size, m_Radius, m_Vertices);
    RebuildSensorCollider();
}

math::Vec2 AreaTrigger2DComponent::GetSize() const {
    return GetPropertyValue<math::Vec2>("size");
}

void AreaTrigger2DComponent::SetSize(const math::Vec2& size) {
    SetPropertyValue("size", size);
    m_Size = size;
    if (m_ShapeType == CollisionShape2DType::Rectangle) {
        CollisionBody2DComponent::GenerateShapeVertices(m_ShapeType, m_Size, m_Radius, m_Vertices);
        RebuildSensorCollider();
    }
}

float AreaTrigger2DComponent::GetRadius() const {
    return GetPropertyValue<float>("radius");
}

void AreaTrigger2DComponent::SetRadius(float radius) {
    SetPropertyValue("radius", radius);
    m_Radius = radius;
    if (m_ShapeType != CollisionShape2DType::Rectangle) {
        CollisionBody2DComponent::GenerateShapeVertices(m_ShapeType, m_Size, m_Radius, m_Vertices);
        RebuildSensorCollider();
    }
}

math::Vec2 AreaTrigger2DComponent::GetOffset() const {
    return GetPropertyValue<math::Vec2>("offset");
}

void AreaTrigger2DComponent::SetOffset(const math::Vec2& offset) {
    SetPropertyValue("offset", offset);
    m_Offset = offset;
    if (m_Collider) {
        m_Collider->SetOffset(offset);
    }
}

uint32_t AreaTrigger2DComponent::GetCollisionLayers() const {
    return static_cast<uint32_t>(GetPropertyValue<int>("collisionLayers"));
}

void AreaTrigger2DComponent::SetCollisionLayers(uint32_t layers) {
    SetPropertyValue("collisionLayers", static_cast<int>(layers));
    m_CollisionLayers = layers;
    if (m_Collider) {
        m_Collider->SetCollisionLayer(layers);
    }
}

uint32_t AreaTrigger2DComponent::GetCollisionMask() const {
    return static_cast<uint32_t>(GetPropertyValue<int>("collisionMask"));
}

void AreaTrigger2DComponent::SetCollisionMask(uint32_t mask) {
    SetPropertyValue("collisionMask", static_cast<int>(mask));
    m_CollisionMask = mask;
    if (m_Collider) {
        m_Collider->SetCollisionMask(mask);
    }
}

void AreaTrigger2DComponent::SetVertices(const std::vector<math::Vec2>& vertices) {
    m_Vertices = vertices;
    if (m_ShapeType == CollisionShape2DType::Polygon) {
        RebuildSensorCollider();
    }
}

bool AreaTrigger2DComponent::IsOverlapping(const core::UUID& bodyId) const {
    return std::find(m_OverlappingBodies.begin(), m_OverlappingBodies.end(), bodyId) != m_OverlappingBodies.end();
}

void AreaTrigger2DComponent::OnTriggerEnterInternal(const physics2d::CollisionInfo& info) {
    if (!GetMonitoring()) return;

    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;

    if (!IsOverlapping(otherBodyId)) {
        m_OverlappingBodies.push_back(otherBodyId);
    }

    if (m_OnTriggerEnter) {
        m_OnTriggerEnter(info);
    }
    Emit("body_entered", { ResolveOtherBodyNodeArg(otherBodyId) });
}

void AreaTrigger2DComponent::OnTriggerStayInternal(const physics2d::CollisionInfo& info) {
    if (!GetMonitoring()) return;

    if (m_OnTriggerStay) {
        m_OnTriggerStay(info);
    }
}

void AreaTrigger2DComponent::OnTriggerExitInternal(const physics2d::CollisionInfo& info) {
    if (!GetMonitoring()) return;

    core::UUID otherBodyId = (info.bodyA == m_PhysicsBodyId) ? info.bodyB : info.bodyA;

    auto it = std::find(m_OverlappingBodies.begin(), m_OverlappingBodies.end(), otherBodyId);
    if (it != m_OverlappingBodies.end()) {
        m_OverlappingBodies.erase(it);
    }

    if (m_OnTriggerExit) {
        m_OnTriggerExit(info);
    }
    Emit("body_exited", { ResolveOtherBodyNodeArg(otherBodyId) });
}

nlohmann::json AreaTrigger2DComponent::ResolveOtherBodyNodeArg(const core::UUID& otherBodyId) const {
    auto* sceneManager = core::SceneManager::GetInstance();
    if (sceneManager) {
        auto* physicsWorld = sceneManager->GetPhysics2DWorld();
        if (physicsWorld) {
            return core::Node::NodeArg(physicsWorld->GetBodyNode(otherBodyId));
        }
    }
    return nlohmann::json(nullptr);
}

void AreaTrigger2DComponent::DefineSignals() {
    RegisterSignal({"body_entered",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body enters this area."});
    RegisterSignal({"body_exited",
                    {{"body", core::PropertyValueType::NodePath}},
                    "Emitted when another physics body leaves this area."});
}

}
}
