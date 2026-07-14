#include "lupine/components/CollisionBody2DComponent.hpp"
#include "lupine/components/RigidBody2DComponent.hpp"
#include "lupine/components/StaticBody2DComponent.hpp"
#include "lupine/components/KinematicBody2DComponent.hpp"
#include "lupine/components/AreaTrigger2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/MathCommon.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

CollisionBody2DComponent::CollisionBody2DComponent()
    : CollisionBody2DComponent("CollisionBody2D")
{
}

CollisionBody2DComponent::CollisionBody2DComponent(const std::string& name)
    : Component(name)
    , m_ShapeType(CollisionShape2DType::Rectangle)
    , m_Size(math::Vec2(100.0f, 100.0f))
    , m_Radius(50.0f)
    , m_Offset(math::Vec2::Zero())
    , m_Density(1.0f)
    , m_Friction(0.3f)
    , m_Restitution(0.0f)
    , m_IsSensor(false)
    , m_DebugColor(math::Color(0.0f, 1.0f, 0.0f, 0.5f))
    , m_PhysicsBody(nullptr)
{
}

CollisionBody2DComponent::~CollisionBody2DComponent() {
    DestroyCollider();
}

void CollisionBody2DComponent::DefineProperties() {

    DefineProperty(PROPERTY_ENUM_GROUP(shapeType, 0, "Shape", Rectangle, Circle, Triangle, Pentagon, Hexagon, Polygon));
    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec2, math::Vec2(100.0f, 100.0f), "Shape"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 50.0f, 0.01f, 1000.0f, 1.0f, "Shape"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, math::Vec2::Zero(), "Shape"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(density, 1.0f, 0.0f, 100.0f, 0.1f, "Physics Material"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(friction, 0.3f, 0.0f, 1.0f, 0.01f, "Physics Material"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(restitution, 0.0f, 0.0f, 1.0f, 0.01f, "Physics Material"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(isSensor, Bool, false, "Physics Material"));
    DefineProperty(PROPERTY_GROUP(collisionLayers, Int, 1, Layers2D, "", "Physics Material"));
    DefineProperty(PROPERTY_GROUP(collisionMask, Int, static_cast<int>(0xFFFFFFFF), Layers2D, "", "Physics Material"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, math::Color(0.0f, 1.0f, 0.0f, 0.5f), "Debug"));
}

nlohmann::json CollisionBody2DComponent::Serialize() const {

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

void CollisionBody2DComponent::Deserialize(const nlohmann::json& json) {

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
    } else {

    }

    m_ShapeType = static_cast<CollisionShape2DType>(GetPropertyValue<int>("shapeType"));
    m_Size = GetPropertyValue<math::Vec2>("size");
    m_Radius = GetPropertyValue<float>("radius");
    m_Offset = GetPropertyValue<math::Vec2>("offset");
    m_Density = GetPropertyValue<float>("density");
    m_Friction = GetPropertyValue<float>("friction");
    m_Restitution = GetPropertyValue<float>("restitution");
    m_IsSensor = GetPropertyValue<bool>("isSensor");
    m_CollisionLayers = static_cast<uint32_t>(GetPropertyValue<int>("collisionLayers"));

    // Backward compatibility: scenes authored before layer/mask were split only
    // stored "collisionLayers" and relied on category == mask. Preserve that
    // behaviour by deriving the mask from the layer when it is absent.
    bool hasMask = json.contains("properties") && json["properties"].contains("collisionMask");
    if (hasMask) {
        m_CollisionMask = static_cast<uint32_t>(GetPropertyValue<int>("collisionMask"));
    } else {
        m_CollisionMask = m_CollisionLayers;
        SetPropertyValue("collisionMask", static_cast<int>(m_CollisionMask));
    }

    m_DebugColor = GetPropertyValue<math::Color>("debugColor");
}

void CollisionBody2DComponent::OnAwake() {

    if (m_ShapeType != CollisionShape2DType::Polygon || m_Vertices.empty()) {
        GenerateShapeVertices();
    } else {

    }
}

void CollisionBody2DComponent::OnReady() {

    m_PhysicsBody = FindPhysicsBody();

    if (m_PhysicsBody) {

        CreateCollider();

        std::string shapeInfo;
        switch (m_ShapeType) {
            case CollisionShape2DType::Rectangle:
                shapeInfo = fmt::format("Rectangle (size: {}, {})", m_Size.x, m_Size.y);
                break;
            case CollisionShape2DType::Circle:
                shapeInfo = fmt::format("Circle (radius: {})", m_Radius);
                break;
            default:
                shapeInfo = fmt::format("Shape type {}", static_cast<int>(m_ShapeType));
                break;
        }

    } else {

    }
}

void CollisionBody2DComponent::OnDestroy() {
    DestroyCollider();
}

void CollisionBody2DComponent::OnPhysicsWorldRebuild(PhysicsWorldRebuildPhase phase) {
    // See Component::PhysicsWorldRebuildPhase. This component owns no body - it attaches a
    // collider to a body found on its own node or its parent - so it does all of its work in
    // the AttachColliders phase, once those bodies have been recreated.
    switch (phase) {
        case PhysicsWorldRebuildPhase::SaveState:
            break;

        case PhysicsWorldRebuildPhase::RecreateBodies:
            // The body this collider was attached to has been destroyed, which already told
            // the collider its body is gone (RigidBody2D's destructor), so the reset is safe.
            // Clearing m_PhysicsBody matters: it is a dangling pointer into the old world and
            // CreateCollider would otherwise build a shape against it.
            DestroyCollider();
            m_PhysicsBody = nullptr;
            break;

        case PhysicsWorldRebuildPhase::AttachColliders:
            // Mirrors OnReady: the bodies exist again, so re-find ours and re-attach.
            m_PhysicsBody = FindPhysicsBody();
            if (m_PhysicsBody) {
                CreateCollider();
            }
            break;
    }
}

void CollisionBody2DComponent::OnPhysicsProcess(float) {
    if (!m_Collider) {
        return;
    }

    const math::Vec2 globalScale = GetNodeGlobalScale();
    if (globalScale.x != m_LastScale.x || globalScale.y != m_LastScale.y) {
        m_Collider->SetScale(globalScale);
        m_LastScale = globalScale;
    }
}

void CollisionBody2DComponent::OnRender() {

    DrawDebugShape();
}

void CollisionBody2DComponent::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "shapeType") {
        m_ShapeType = static_cast<CollisionShape2DType>(newValue.get<int>());
        GenerateShapeVertices();
        UpdateCollider();
    }
    else if (propertyName == "size") {
        m_Size = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
        if (m_ShapeType == CollisionShape2DType::Rectangle) {
            GenerateShapeVertices();
            UpdateCollider();
        }
    }
    else if (propertyName == "radius") {
        m_Radius = newValue.get<float>();
        if (m_ShapeType == CollisionShape2DType::Circle) {
            UpdateCollider();
        }
    }
    else if (propertyName == "offset") {
        m_Offset = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
        if (m_Collider) {
            m_Collider->SetOffset(m_Offset);
        }
    }
    else if (propertyName == "density") {
        m_Density = newValue.get<float>();
        if (m_Collider) {
            m_Collider->SetDensity(m_Density);
        }
    }
    else if (propertyName == "friction") {
        m_Friction = newValue.get<float>();
        if (m_Collider) {
            m_Collider->SetFriction(m_Friction);
        }
    }
    else if (propertyName == "restitution") {
        m_Restitution = newValue.get<float>();
        if (m_Collider) {
            m_Collider->SetRestitution(m_Restitution);
        }
    }
    else if (propertyName == "isSensor") {
        m_IsSensor = newValue.get<bool>();
        if (m_Collider) {
            m_Collider->SetSensor(m_IsSensor);
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
    else if (propertyName == "debugColor") {
        m_DebugColor = math::Color(
            newValue["r"].get<float>(),
            newValue["g"].get<float>(),
            newValue["b"].get<float>(),
            newValue["a"].get<float>()
        );
    }
}

CollisionShape2DType CollisionBody2DComponent::GetShapeType() const {
    return static_cast<CollisionShape2DType>(GetPropertyValue<int>("shapeType"));
}

void CollisionBody2DComponent::SetShapeType(CollisionShape2DType type) {
    SetPropertyValue("shapeType", static_cast<int>(type));
    m_ShapeType = type;
    GenerateShapeVertices();
    UpdateCollider();
}

math::Vec2 CollisionBody2DComponent::GetSize() const {
    return GetPropertyValue<math::Vec2>("size");
}

void CollisionBody2DComponent::SetSize(const math::Vec2& size) {
    SetPropertyValue("size", size);
    m_Size = size;
    if (m_ShapeType == CollisionShape2DType::Rectangle) {
        GenerateShapeVertices();
        UpdateCollider();
    }
}

float CollisionBody2DComponent::GetRadius() const {
    return GetPropertyValue<float>("radius");
}

void CollisionBody2DComponent::SetRadius(float radius) {
    SetPropertyValue("radius", radius);
    m_Radius = radius;
    if (m_ShapeType == CollisionShape2DType::Circle) {
        UpdateCollider();
    }
}

const std::vector<math::Vec2>& CollisionBody2DComponent::GetVertices() const {
    return m_Vertices;
}

void CollisionBody2DComponent::SetVertices(const std::vector<math::Vec2>& vertices) {
    m_Vertices = vertices;
    if (m_ShapeType == CollisionShape2DType::Polygon) {
        UpdateCollider();
    }
}

void CollisionBody2DComponent::AddVertex(const math::Vec2& vertex) {
    m_Vertices.push_back(vertex);

    if (m_ShapeType == CollisionShape2DType::Polygon) {
        UpdateCollider();
    }
}

void CollisionBody2DComponent::RemoveVertex(int index) {
    if (index >= 0 && index < static_cast<int>(m_Vertices.size())) {
        m_Vertices.erase(m_Vertices.begin() + index);
        if (m_ShapeType == CollisionShape2DType::Polygon) {
            UpdateCollider();
        }
    }
}

void CollisionBody2DComponent::UpdateVertex(int index, const math::Vec2& position) {
    if (index >= 0 && index < static_cast<int>(m_Vertices.size())) {
        m_Vertices[index] = position;
        if (m_ShapeType == CollisionShape2DType::Polygon) {
            UpdateCollider();
        }
    }
}

void CollisionBody2DComponent::ClearVertices() {
    m_Vertices.clear();
}

float CollisionBody2DComponent::GetDensity() const {
    return GetPropertyValue<float>("density");
}

void CollisionBody2DComponent::SetDensity(float density) {
    SetPropertyValue("density", density);
    m_Density = density;
    if (m_Collider) {
        m_Collider->SetDensity(density);
    }
}

float CollisionBody2DComponent::GetFriction() const {
    return GetPropertyValue<float>("friction");
}

void CollisionBody2DComponent::SetFriction(float friction) {
    SetPropertyValue("friction", friction);
    m_Friction = friction;
    if (m_Collider) {
        m_Collider->SetFriction(friction);
    }
}

float CollisionBody2DComponent::GetRestitution() const {
    return GetPropertyValue<float>("restitution");
}

void CollisionBody2DComponent::SetRestitution(float restitution) {
    SetPropertyValue("restitution", restitution);
    m_Restitution = restitution;
    if (m_Collider) {
        m_Collider->SetRestitution(restitution);
    }
}

bool CollisionBody2DComponent::IsSensor() const {
    return GetPropertyValue<bool>("isSensor");
}

void CollisionBody2DComponent::SetSensor(bool isSensor) {
    SetPropertyValue("isSensor", isSensor);
    m_IsSensor = isSensor;
    if (m_Collider) {
        m_Collider->SetSensor(isSensor);
    }
}

math::Vec2 CollisionBody2DComponent::GetOffset() const {
    return GetPropertyValue<math::Vec2>("offset");
}

void CollisionBody2DComponent::SetOffset(const math::Vec2& offset) {
    SetPropertyValue("offset", offset);
    m_Offset = offset;
    if (m_Collider) {
        m_Collider->SetOffset(offset);
    }
}

uint32_t CollisionBody2DComponent::GetCollisionLayers() const {
    return static_cast<uint32_t>(GetPropertyValue<int>("collisionLayers"));
}

void CollisionBody2DComponent::SetCollisionLayers(uint32_t layers) {
    SetPropertyValue("collisionLayers", static_cast<int>(layers));
    m_CollisionLayers = layers;
    if (m_Collider) {
        m_Collider->SetCollisionLayer(layers);
    }
}

uint32_t CollisionBody2DComponent::GetCollisionMask() const {
    return static_cast<uint32_t>(GetPropertyValue<int>("collisionMask"));
}

void CollisionBody2DComponent::SetCollisionMask(uint32_t mask) {
    SetPropertyValue("collisionMask", static_cast<int>(mask));
    m_CollisionMask = mask;
    if (m_Collider) {
        m_Collider->SetCollisionMask(mask);
    }
}

math::Color CollisionBody2DComponent::GetDebugColor() const {
    return GetPropertyValue<math::Color>("debugColor");
}

void CollisionBody2DComponent::SetDebugColor(const math::Color& color) {
    SetPropertyValue("debugColor", color);
    m_DebugColor = color;
}

void CollisionBody2DComponent::CreateCollider() {
    if (!m_PhysicsBody) {
        return;
    }

    DestroyCollider();

    physics2d::PhysicsMaterial2D material(m_Density, m_Friction, m_Restitution);
    const math::Vec2 globalScale = GetNodeGlobalScale();

    switch (m_ShapeType) {
        case CollisionShape2DType::Rectangle: {
            core::UUID colliderID;
            auto boxCollider = std::make_unique<physics2d::BoxCollider2D>(
                m_PhysicsBody, colliderID, m_Size);
            boxCollider->SetMaterial(material);
            boxCollider->SetSensor(m_IsSensor);
            boxCollider->SetOffset(m_Offset);
            boxCollider->SetScale(globalScale);
            boxCollider->SetCollisionLayer(m_CollisionLayers);
            boxCollider->SetCollisionMask(m_CollisionMask);
            m_Collider = std::move(boxCollider);
            break;
        }

        case CollisionShape2DType::Circle: {
            core::UUID colliderID;
            auto circleCollider = std::make_unique<physics2d::CircleCollider2D>(
                m_PhysicsBody, colliderID, m_Radius);
            circleCollider->SetMaterial(material);
            circleCollider->SetSensor(m_IsSensor);
            circleCollider->SetOffset(m_Offset);
            circleCollider->SetScale(globalScale);
            circleCollider->SetCollisionLayer(m_CollisionLayers);
            circleCollider->SetCollisionMask(m_CollisionMask);
            m_Collider = std::move(circleCollider);
            break;
        }

        case CollisionShape2DType::Triangle:
        case CollisionShape2DType::Pentagon:
        case CollisionShape2DType::Hexagon:
        case CollisionShape2DType::Polygon: {

            if (m_Vertices.size() >= 3) {
                core::UUID colliderID;
                auto polygonCollider = std::make_unique<physics2d::PolygonCollider2D>(
                    m_PhysicsBody, colliderID, m_Vertices);
                polygonCollider->SetMaterial(material);
                polygonCollider->SetSensor(m_IsSensor);
                polygonCollider->SetOffset(m_Offset);
                polygonCollider->SetScale(globalScale);
                polygonCollider->SetCollisionLayer(m_CollisionLayers);
                polygonCollider->SetCollisionMask(m_CollisionMask);
                m_Collider = std::move(polygonCollider);

            } else if (!m_Vertices.empty()) {

            }
            break;
        }
    }

    m_LastScale = globalScale;
}

math::Vec2 CollisionBody2DComponent::GetNodeGlobalScale() const {
    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return math::Vec2(1.0f, 1.0f);
    }
    return node2D->GetGlobalScale();
}

void CollisionBody2DComponent::DestroyCollider() {
    m_Collider.reset();
}

void CollisionBody2DComponent::UpdateCollider() {
    if (m_PhysicsBody) {
        CreateCollider();
    }
}

void CollisionBody2DComponent::GenerateShapeVertices() {
    GenerateShapeVertices(m_ShapeType, m_Size, m_Radius, m_Vertices);
}

void CollisionBody2DComponent::GenerateShapeVertices(CollisionShape2DType shapeType, const math::Vec2& size,
                                                     float radius, std::vector<math::Vec2>& outVertices) {
    outVertices.clear();

    switch (shapeType) {
        case CollisionShape2DType::Rectangle: {
            float halfW = size.x / 2.0f;
            float halfH = size.y / 2.0f;
            outVertices.push_back(math::Vec2(-halfW, -halfH));
            outVertices.push_back(math::Vec2(halfW, -halfH));
            outVertices.push_back(math::Vec2(halfW, halfH));
            outVertices.push_back(math::Vec2(-halfW, halfH));
            break;
        }

        case CollisionShape2DType::Circle: {
            break;
        }

        case CollisionShape2DType::Triangle: {
            float r = radius;
            outVertices.push_back(math::Vec2(0.0f, r));
            outVertices.push_back(math::Vec2(r * 0.866f, -r * 0.5f));
            outVertices.push_back(math::Vec2(-r * 0.866f, -r * 0.5f));
            break;
        }

        case CollisionShape2DType::Pentagon: {
            float r = radius;
            for (int i = 0; i < 5; ++i) {
                float angle = (float)i * (2.0f * math::PI / 5.0f) - math::PI / 2.0f;
                outVertices.push_back(math::Vec2(r * std::cos(angle), r * std::sin(angle)));
            }
            break;
        }

        case CollisionShape2DType::Hexagon: {
            float r = radius;
            for (int i = 0; i < 6; ++i) {
                float angle = (float)i * (2.0f * math::PI / 6.0f);
                outVertices.push_back(math::Vec2(r * std::cos(angle), r * std::sin(angle)));
            }
            break;
        }

        case CollisionShape2DType::Polygon: {
            break;
        }
    }
}

physics2d::RigidBody2D* CollisionBody2DComponent::FindPhysicsBody() {
    if (!m_Owner) {
        return nullptr;
    }

    auto rigidBody = m_Owner->GetComponent<RigidBody2DComponent>();
    if (rigidBody) {
        return rigidBody->GetPhysicsBody();
    }

    auto staticBody = m_Owner->GetComponent<StaticBody2DComponent>();
    if (staticBody) {
        return staticBody->GetPhysicsBody();
    }

    auto kinematicBody = m_Owner->GetComponent<KinematicBody2DComponent>();
    if (kinematicBody) {
        return kinematicBody->GetPhysicsBody();
    }

    auto areaTrigger = m_Owner->GetComponent<AreaTrigger2DComponent>();
    if (areaTrigger) {
        return areaTrigger->GetPhysicsBody();
    }

    auto parent = m_Owner->GetParent();
    if (parent) {
        rigidBody = parent->GetComponent<RigidBody2DComponent>();
        if (rigidBody) {
            return rigidBody->GetPhysicsBody();
        }

        staticBody = parent->GetComponent<StaticBody2DComponent>();
        if (staticBody) {
            return staticBody->GetPhysicsBody();
        }

        kinematicBody = parent->GetComponent<KinematicBody2DComponent>();
        if (kinematicBody) {
            return kinematicBody->GetPhysicsBody();
        }

        areaTrigger = parent->GetComponent<AreaTrigger2DComponent>();
        if (areaTrigger) {
            return areaTrigger->GetPhysicsBody();
        }
    }

    return nullptr;
}

void CollisionBody2DComponent::DrawDebugShape() {

    if (!DebugDraw::IsAvailable()) {
        return;
    }

    if (!DebugDraw::IsCollisionShapesEnabled()) {
        return;
    }

    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera2D) {
        return;
    }

    if (!m_Owner) {
        return;
    }

    auto node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    math::Vec2 position = node2D->GetGlobalPosition();
    float rotation = node2D->GetGlobalRotation();
    const math::Vec2 scale = node2D->GetGlobalScale();

    // Drawn from the same scaled geometry the Box2D shape is built from, so the debug outline
    // shows the collider that actually exists rather than its unscaled authoring size.
    math::Vec2 worldOffset = math::Vec2(m_Offset.x * scale.x, m_Offset.y * scale.y);

    if (rotation != 0.0f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        worldOffset = math::Vec2(
            worldOffset.x * cosR - worldOffset.y * sinR,
            worldOffset.x * sinR + worldOffset.y * cosR
        );
    }

    math::Vec3 center3D(position.x + worldOffset.x, position.y + worldOffset.y, 0.0f);
    math::Color color = m_DebugColor;

    switch (m_ShapeType) {
        case CollisionShape2DType::Circle: {
            const float radius = m_Radius * std::max(std::abs(scale.x), std::abs(scale.y));
            DebugDraw::Circle(center3D, math::Vec3(0, 0, 1), radius, color, 32, 0.0f, false);
            break;
        }

        case CollisionShape2DType::Rectangle:
        case CollisionShape2DType::Triangle:
        case CollisionShape2DType::Pentagon:
        case CollisionShape2DType::Hexagon:
        case CollisionShape2DType::Polygon: {

            if (m_Vertices.empty() && m_ShapeType != CollisionShape2DType::Polygon) {
                const_cast<CollisionBody2DComponent*>(this)->GenerateShapeVertices();
            }

            if (!m_Vertices.empty()) {

                std::vector<math::Vec3> points;
                points.reserve(m_Vertices.size() + 1);

                for (const auto& vertex : m_Vertices) {

                    const math::Vec2 scaled(vertex.x * scale.x, vertex.y * scale.y);
                    math::Vec2 rotated = scaled;
                    if (rotation != 0.0f) {
                        float cosR = std::cos(rotation);
                        float sinR = std::sin(rotation);
                        rotated = math::Vec2(
                            scaled.x * cosR - scaled.y * sinR,
                            scaled.x * sinR + scaled.y * cosR
                        );
                    }
                    points.push_back(math::Vec3(
                        position.x + worldOffset.x + rotated.x,
                        position.y + worldOffset.y + rotated.y,
                        0.0f
                    ));
                }

                for (const auto& point : points) {
                    DebugDraw::Circle(point, math::Vec3(0, 0, 1), 5.0f, color, 8, 0.0f, false);
                }

                if (points.size() >= 2) {

                    if (m_Vertices.size() < 3) {
                        DebugDraw::LineStrip(points, color, 0.0f, false);
                    } else {

                        points.push_back(points[0]);
                        DebugDraw::LineStrip(points, color, 0.0f, false);
                    }
                }
            }
            break;
        }
    }
}

}
}

