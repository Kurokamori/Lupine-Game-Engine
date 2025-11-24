#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace core {

Node::Node()
    : m_UUID(), m_Name("Node"), m_Parent(nullptr), m_Scene(nullptr),
      m_Active(true), m_Visible(true), m_Ready(false) {
}

Node::Node(const std::string& name)
    : m_UUID(), m_Name(name), m_Parent(nullptr), m_Scene(nullptr),
      m_Active(true), m_Visible(true), m_Ready(false) {
}

Node::~Node() {

    for (auto& child : m_Children) {
        child->m_Parent = nullptr;
    }
    m_Children.clear();

    for (auto& component : m_Components) {
        component->OnDestroy();
    }
    m_Components.clear();
}

void Node::RegisterProperties() {

    m_Properties.clear();

    RegisterProperty<std::string>("name", core::PropertyType::String,
        [this]() { return m_Name; },
        [this](const std::string& value) { m_Name = value; });

    RegisterProperty<bool>("active", core::PropertyType::Bool,
        [this]() { return m_Active; },
        [this](const bool& value) { m_Active = value; });

    RegisterProperty<bool>("visible", core::PropertyType::Bool,
        [this]() { return m_Visible; },
        [this](const bool& value) { m_Visible = value; });
}

nlohmann::json Node::Serialize() const {
    nlohmann::json json = core::ISerializable::Serialize();

    json["uuid"] = m_UUID.ToString();

    nlohmann::json componentsJson = nlohmann::json::array();
    for (const auto& component : m_Components) {
        componentsJson.push_back(component->Serialize());
    }
    json["components"] = componentsJson;

    nlohmann::json childrenJson = nlohmann::json::array();
    for (const auto& child : m_Children) {
        childrenJson.push_back(child->Serialize());
    }
    json["children"] = childrenJson;

    return json;
}

void Node::Deserialize(const nlohmann::json& json) {

    RegisterProperties();

    core::ISerializable::Deserialize(json);

    if (json.contains("uuid")) {
        m_UUID = core::UUID::FromString(json["uuid"].get<std::string>());
    }

    if (json.contains("components") && json["components"].is_array()) {

        for (auto& component : m_Components) {
            component->OnDestroy();
        }
        m_Components.clear();

        for (const auto& componentJson : json["components"]) {
            if (componentJson.contains("type")) {
                std::string typeName = componentJson["type"].get<std::string>();
                auto component = std::dynamic_pointer_cast<Component>(
                    core::TypeRegistry::GetInstance().CreateInstance(typeName));
                if (component) {
                    component->RegisterProperties();
                    component->Deserialize(componentJson);
                    AddComponent(component);
                }
            }
        }
    }

    if (json.contains("children") && json["children"].is_array()) {

        for (auto& child : m_Children) {
            child->m_Parent = nullptr;
            child->SetScene(nullptr);
        }
        m_Children.clear();

        for (const auto& childJson : json["children"]) {
            if (childJson.contains("type")) {
                std::string typeName = childJson["type"].get<std::string>();
                auto child = std::dynamic_pointer_cast<Node>(
                    core::TypeRegistry::GetInstance().CreateInstance(typeName));
                if (child) {
                    child->Deserialize(childJson);
                    AddChild(child);
                }
            }
        }
    }
}

void Node::AddChild(std::shared_ptr<Node> child) {
    if (!child) {

        return;
    }

    if (child->m_Parent) {

        child->m_Parent->RemoveChild(child);
    }

    m_Children.push_back(child);
    child->m_Parent = this;
    child->SetScene(m_Scene);

}

void Node::InsertChild(std::shared_ptr<Node> child, size_t index) {
    if (!child) {

        return;
    }

    if (child->m_Parent) {
        child->m_Parent->RemoveChild(child);
    }

    if (index > m_Children.size()) {
        index = m_Children.size();
    }

    m_Children.insert(m_Children.begin() + index, child);
    child->m_Parent = this;
    child->SetScene(m_Scene);

}

void Node::RemoveChild(std::shared_ptr<Node> child) {
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end()) {
        (*it)->m_Parent = nullptr;
        (*it)->SetScene(nullptr);
        m_Children.erase(it);
    }
}

void Node::RemoveChild(const std::string& name) {
    auto child = GetChild(name);
    if (child) {
        RemoveChild(child);
    }
}

void Node::ReparentTo(Node* newParent) {

    if (m_Parent == newParent) {
        return;
    }

    std::shared_ptr<Node> thisShared = nullptr;
    if (m_Parent) {

        for (const auto& child : m_Parent->m_Children) {
            if (child.get() == this) {
                thisShared = child;
                break;
            }
        }

        auto it = std::find(m_Parent->m_Children.begin(), m_Parent->m_Children.end(), thisShared);
        if (it != m_Parent->m_Children.end()) {
            m_Parent->m_Children.erase(it);
        }
    }

    m_Parent = newParent;

    if (newParent) {
        if (thisShared) {
            newParent->m_Children.push_back(thisShared);
        }
        SetScene(newParent->m_Scene);
    } else {
        SetScene(nullptr);
    }

}

std::shared_ptr<Node> Node::GetChild(const std::string& name) const {
    for (const auto& child : m_Children) {
        if (child->m_Name == name) {
            return child;
        }
    }
    return nullptr;
}

std::shared_ptr<Node> Node::GetChild(size_t index) const {
    if (index < m_Children.size()) {
        return m_Children[index];
    }
    return nullptr;
}

void Node::SetParent(Node* parent) {
    if (m_Parent == parent) return;

    if (m_Parent) {

        auto& siblings = m_Parent->m_Children;
        auto it = std::find_if(siblings.begin(), siblings.end(),
            [this](const std::shared_ptr<Node>& node) { return node.get() == this; });
        if (it != siblings.end()) {
            siblings.erase(it);
        }
    }

    m_Parent = parent;
}

std::shared_ptr<Node> Node::FindNode(const std::string& path) const {
    if (path.empty()) return nullptr;

    if (path.find('/') == std::string::npos) {
        return GetChild(path);
    }

    size_t pos = 0;
    std::shared_ptr<Node> current = nullptr;

    while (pos < path.length()) {
        size_t nextSlash = path.find('/', pos);
        std::string segment = (nextSlash == std::string::npos)
            ? path.substr(pos)
            : path.substr(pos, nextSlash - pos);

        if (segment.empty()) {
            pos = nextSlash + 1;
            continue;
        }

        if (!current) {
            current = GetChild(segment);
        } else {
            current = current->GetChild(segment);
        }

        if (!current) return nullptr;

        if (nextSlash == std::string::npos) break;
        pos = nextSlash + 1;
    }

    return current;
}

std::string Node::GetPath() const {
    if (!m_Parent) {
        return "/" + m_Name;
    }
    return m_Parent->GetPath() + "/" + m_Name;
}

void Node::AddComponent(std::shared_ptr<Component> component) {
    if (!component) {

        return;
    }

    m_Components.push_back(component);
    component->SetOwner(this);

    if (m_Ready) {
        component->OnAwake();
        component->OnReady();
    }

}

void Node::RemoveComponent(std::shared_ptr<Component> component) {
    auto it = std::find(m_Components.begin(), m_Components.end(), component);
    if (it != m_Components.end()) {
        (*it)->OnDestroy();
        (*it)->SetOwner(nullptr);
        m_Components.erase(it);

    }
}

std::shared_ptr<Component> Node::GetComponent(const std::string& typeName) const {
    for (const auto& component : m_Components) {
        if (component->GetTypeName() == typeName) {
            return component;
        }
    }
    return nullptr;
}

void Node::SetActive(bool active) {
    if (m_Active == active) return;
    m_Active = active;

    for (auto& child : m_Children) {
        if (active && m_Ready) {
            child->OnReady();
        }
    }
}

bool Node::IsActiveInHierarchy() const {
    if (!m_Active) return false;
    if (!m_Parent) return m_Active;
    return m_Parent->IsActiveInHierarchy();
}

bool Node::IsVisibleInHierarchy() const {
    if (!m_Visible) return false;
    if (!m_Parent) return m_Visible;
    return m_Parent->IsVisibleInHierarchy();
}

void Node::SetScene(Scene* scene) {

    m_Scene = scene;
    for (auto& child : m_Children) {
        child->SetScene(scene);
    }

}

void Node::OnReady() {
    if (m_Ready) return;
    m_Ready = true;

    for (auto& component : m_Components) {

        component->OnAwake();

        component->OnReady();

    }

    for (auto& child : m_Children) {
        if (child->IsActive()) {
            child->OnReady();
        }
    }

}

void Node::OnDestroy() {

    for (auto& component : m_Components) {
        component->OnDestroy();
    }

    for (auto& child : m_Children) {
        child->OnDestroy();
    }
}

void Node::OnInput(float deltaTime) {
    if (!IsActiveInHierarchy()) return;

    for (auto& component : m_Components) {

        component->OnInput(deltaTime);
    }

    for (auto& child : m_Children) {

        child->OnInput(deltaTime);

    }

}

void Node::OnProcess(float deltaTime) {
    if (!IsActiveInHierarchy()) return;

    for (auto& component : m_Components) {

        component->OnUpdate(deltaTime);
    }

    for (auto& component : m_Components) {

        component->OnProcess(deltaTime);
    }

    for (auto& child : m_Children) {

        child->OnProcess(deltaTime);

    }

    for (auto& component : m_Components) {

        component->OnLateUpdate(deltaTime);
    }

}

void Node::OnPhysicsProcess(float deltaTime) {
    if (!IsActiveInHierarchy()) return;

    for (auto& component : m_Components) {

        component->OnPhysicsProcess(deltaTime);
    }

    for (auto& child : m_Children) {

        child->OnPhysicsProcess(deltaTime);

    }

}

void Node::OnRender() {
    if (!IsVisibleInHierarchy()) return;

    for (auto& component : m_Components) {

        component->OnRender();
    }

    for (auto& child : m_Children) {

        child->OnRender();

    }

}

Node2D::Node2D()
    : Node(), m_Position(0.0f, 0.0f), m_Rotation(0.0f),
      m_Scale(1.0f, 1.0f), m_ZIndex(0) {
}

Node2D::Node2D(const std::string& name)
    : Node(name), m_Position(0.0f, 0.0f), m_Rotation(0.0f),
      m_Scale(1.0f, 1.0f), m_ZIndex(0) {
}

void Node2D::RegisterProperties() {
    Node::RegisterProperties();

    RegisterProperty<math::Vec2>("position", core::PropertyType::Vec2,
        [this]() { return m_Position; },
        [this](const math::Vec2& value) { m_Position = value; });

    RegisterProperty<float>("rotation", core::PropertyType::Float,
        [this]() { return m_Rotation; },
        [this](const float& value) { m_Rotation = value; });

    RegisterProperty<math::Vec2>("scale", core::PropertyType::Vec2,
        [this]() { return m_Scale; },
        [this](const math::Vec2& value) { m_Scale = value; });

    RegisterProperty<int>("z_index", core::PropertyType::Int,
        [this]() { return m_ZIndex; },
        [this](const int& value) { m_ZIndex = value; });
}

math::Vec2 Node2D::GetGlobalPosition() const {
    if (!m_Parent) return m_Position;

    Node2D* parent2D = dynamic_cast<Node2D*>(m_Parent);
    if (parent2D) {

        math::Vec2 parentPos = parent2D->GetGlobalPosition();
        float parentRot = parent2D->GetGlobalRotation();
        math::Vec2 parentScale = parent2D->GetGlobalScale();

        float cosRot = std::cos(parentRot);
        float sinRot = std::sin(parentRot);
        math::Vec2 rotated(
            m_Position.x * cosRot - m_Position.y * sinRot,
            m_Position.x * sinRot + m_Position.y * cosRot
        );

        return parentPos + rotated * parentScale;
    }

    return m_Position;
}

float Node2D::GetGlobalRotation() const {
    if (!m_Parent) return m_Rotation;

    Node2D* parent2D = dynamic_cast<Node2D*>(m_Parent);
    if (parent2D) {
        return parent2D->GetGlobalRotation() + m_Rotation;
    }

    return m_Rotation;
}

math::Vec2 Node2D::GetGlobalScale() const {
    if (!m_Parent) return m_Scale;

    Node2D* parent2D = dynamic_cast<Node2D*>(m_Parent);
    if (parent2D) {
        math::Vec2 parentScale = parent2D->GetGlobalScale();
        return m_Scale * parentScale;
    }

    return m_Scale;
}

math::Mat4 Node2D::GetTransformMatrix() const {

    math::Mat4 translation = math::Mat4::Translate(math::Vec3(m_Position.x, m_Position.y, 0.0f));
    math::Mat4 rotation = math::Mat4::Rotate(m_Rotation, math::Vec3(0.0f, 0.0f, 1.0f));
    math::Mat4 scale = math::Mat4::Scale(math::Vec3(m_Scale.x, m_Scale.y, 1.0f));
    return translation * rotation * scale;
}

math::Mat4 Node2D::GetGlobalTransformMatrix() const {
    if (!m_Parent) return GetTransformMatrix();

    Node2D* parent2D = dynamic_cast<Node2D*>(m_Parent);
    if (parent2D) {
        return parent2D->GetGlobalTransformMatrix() * GetTransformMatrix();
    }

    return GetTransformMatrix();
}

Node3D::Node3D()
    : Node(), m_Position(0.0f, 0.0f, 0.0f), m_Rotation(),
      m_Scale(1.0f, 1.0f, 1.0f) {
}

Node3D::Node3D(const std::string& name)
    : Node(name), m_Position(0.0f, 0.0f, 0.0f), m_Rotation(),
      m_Scale(1.0f, 1.0f, 1.0f) {
}

void Node3D::RegisterProperties() {
    Node::RegisterProperties();

    RegisterProperty<math::Vec3>("position", core::PropertyType::Vec3,
        [this]() { return m_Position; },
        [this](const math::Vec3& value) { m_Position = value; });

    RegisterProperty<math::Quat>("rotation", core::PropertyType::Vec4,
        [this]() { return m_Rotation; },
        [this](const math::Quat& value) { m_Rotation = value; });

    RegisterProperty<math::Vec3>("scale", core::PropertyType::Vec3,
        [this]() { return m_Scale; },
        [this](const math::Vec3& value) { m_Scale = value; });
}

math::Vec3 Node3D::GetGlobalPosition() const {
    if (!m_Parent) return m_Position;

    Node3D* parent3D = dynamic_cast<Node3D*>(m_Parent);
    if (parent3D) {
        math::Mat4 parentTransform = parent3D->GetGlobalTransformMatrix();
        math::Vec4 pos4(m_Position.x, m_Position.y, m_Position.z, 1.0f);
        math::Vec4 globalPos4 = parentTransform * pos4;
        return math::Vec3(globalPos4.x, globalPos4.y, globalPos4.z);
    }

    return m_Position;
}

math::Quat Node3D::GetGlobalRotation() const {
    if (!m_Parent) return m_Rotation;

    Node3D* parent3D = dynamic_cast<Node3D*>(m_Parent);
    if (parent3D) {
        return parent3D->GetGlobalRotation() * m_Rotation;
    }

    return m_Rotation;
}

math::Vec3 Node3D::GetGlobalScale() const {
    if (!m_Parent) return m_Scale;

    Node3D* parent3D = dynamic_cast<Node3D*>(m_Parent);
    if (parent3D) {
        math::Vec3 parentScale = parent3D->GetGlobalScale();
        return m_Scale * parentScale;
    }

    return m_Scale;
}

math::Mat4 Node3D::GetTransformMatrix() const {
    math::Mat4 translation = math::Mat4::Translate(m_Position);
    math::Mat4 rotation = m_Rotation.ToMat4();
    math::Mat4 scale = math::Mat4::Scale(m_Scale);
    return translation * rotation * scale;
}

math::Mat4 Node3D::GetGlobalTransformMatrix() const {
    if (!m_Parent) return GetTransformMatrix();

    Node3D* parent3D = dynamic_cast<Node3D*>(m_Parent);
    if (parent3D) {
        return parent3D->GetGlobalTransformMatrix() * GetTransformMatrix();
    }

    return GetTransformMatrix();
}

}
}

