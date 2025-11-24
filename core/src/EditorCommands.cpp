#include "lupine/core/EditorCommands.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace core {

AddNodeCommand::AddNodeCommand(std::shared_ptr<Scene> scene, std::shared_ptr<Node> node,
                               std::shared_ptr<Node> parent)
    : m_Scene(scene), m_Node(node), m_Parent(parent), m_WasRoot(false) {
    if (parent) {
        m_ParentUUID = parent->GetUUID();
    }
}

bool AddNodeCommand::Execute() {
    auto scene = m_Scene.lock();
    if (!scene || !m_Node) {
        return false;
    }

    auto parent = m_Parent.lock();
    if (parent) {
        parent->AddChild(m_Node);
    } else {

        m_WasRoot = (scene->GetRoot() == nullptr);
        scene->AddNode(m_Node);
    }

    return true;
}

bool AddNodeCommand::Undo() {
    auto scene = m_Scene.lock();
    if (!scene || !m_Node) {
        return false;
    }

    auto parent = m_Node->GetParent();
    if (parent) {
        parent->RemoveChild(m_Node);
    } else if (m_WasRoot) {
        scene->SetRoot(nullptr);
    }

    return true;
}

std::string AddNodeCommand::GetDescription() const {
    if (m_Node) {
        return "Add Node: " + m_Node->GetName();
    }
    return "Add Node";
}

nlohmann::json AddNodeCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["node_uuid"] = m_Node ? m_Node->GetUUID().ToString() : "";
    json["parent_uuid"] = m_ParentUUID.ToString();
    json["was_root"] = m_WasRoot;
    if (m_Node) {
        json["node_data"] = m_Node->Serialize();
    }
    return json;
}

bool AddNodeCommand::Deserialize(const nlohmann::json& json) {

    return false;
}

RemoveNodeCommand::RemoveNodeCommand(std::shared_ptr<Scene> scene, std::shared_ptr<Node> node)
    : m_Scene(scene), m_Node(node), m_WasRoot(false), m_ChildIndex(0) {
    if (node) {
        auto parent = node->GetParent();
        if (parent) {
            m_ParentUUID = parent->GetUUID();

            const auto& children = parent->GetChildren();
            auto it = std::find(children.begin(), children.end(), node);
            if (it != children.end()) {
                m_ChildIndex = std::distance(children.begin(), it);
            }
        } else {
            m_WasRoot = true;
        }
    }
}

bool RemoveNodeCommand::Execute() {
    auto scene = m_Scene.lock();
    if (!scene || !m_Node) {
        return false;
    }

    scene->RemoveNode(m_Node);
    return true;
}

bool RemoveNodeCommand::Undo() {
    auto scene = m_Scene.lock();
    if (!scene || !m_Node) {
        return false;
    }

    if (m_WasRoot) {

        scene->SetRoot(m_Node);
    } else {

        auto parent = scene->FindNodeByUUID(m_ParentUUID);
        if (parent) {

            parent->InsertChild(m_Node, m_ChildIndex);
        } else {

            return false;
        }
    }

    return true;
}

std::string RemoveNodeCommand::GetDescription() const {
    if (m_Node) {
        return "Remove Node: " + m_Node->GetName();
    }
    return "Remove Node";
}

nlohmann::json RemoveNodeCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["node_uuid"] = m_Node ? m_Node->GetUUID().ToString() : "";
    json["parent_uuid"] = m_ParentUUID.ToString();
    json["was_root"] = m_WasRoot;
    json["child_index"] = m_ChildIndex;
    if (m_Node) {
        json["node_data"] = m_Node->Serialize();
    }
    return json;
}

bool RemoveNodeCommand::Deserialize(const nlohmann::json& json) {
    return false;
}

ReparentNodeCommand::ReparentNodeCommand(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent)
    : m_Node(node), m_NewParent(newParent), m_OldChildIndex(0) {
    if (node) {
        m_NodeUUID = node->GetUUID();
        auto oldParent = node->GetParent();
        if (oldParent) {
            m_OldParent = std::shared_ptr<Node>(oldParent, [](Node*){});
            m_OldParentUUID = oldParent->GetUUID();

            const auto& children = oldParent->GetChildren();
            auto it = std::find(children.begin(), children.end(), node);
            if (it != children.end()) {
                m_OldChildIndex = std::distance(children.begin(), it);
            }
        }
    }
    if (newParent) {
        m_NewParentUUID = newParent->GetUUID();
    }
}

bool ReparentNodeCommand::Execute() {
    auto node = m_Node.lock();
    auto newParent = m_NewParent.lock();
    if (!node) {
        return false;
    }

    node->ReparentTo(newParent.get());

    return true;
}

bool ReparentNodeCommand::Undo() {
    auto node = m_Node.lock();
    auto oldParent = m_OldParent.lock();
    if (!node) {
        return false;
    }

    node->ReparentTo(oldParent.get());

    return true;
}

std::string ReparentNodeCommand::GetDescription() const {
    auto node = m_Node.lock();
    if (node) {
        return "Reparent Node: " + node->GetName();
    }
    return "Reparent Node";
}

nlohmann::json ReparentNodeCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["node_uuid"] = m_NodeUUID.ToString();
    json["old_parent_uuid"] = m_OldParentUUID.ToString();
    json["new_parent_uuid"] = m_NewParentUUID.ToString();
    json["old_child_index"] = m_OldChildIndex;
    return json;
}

bool ReparentNodeCommand::Deserialize(const nlohmann::json& json) {
    return false;
}

AddComponentCommand::AddComponentCommand(std::shared_ptr<Node> node, std::shared_ptr<Component> component)
    : m_Node(node), m_Component(component) {
    if (node) {
        m_NodeUUID = node->GetUUID();
    }
}

bool AddComponentCommand::Execute() {
    auto node = m_Node.lock();
    if (!node || !m_Component) {
        return false;
    }

    node->AddComponent(m_Component);
    return true;
}

bool AddComponentCommand::Undo() {
    auto node = m_Node.lock();
    if (!node || !m_Component) {
        return false;
    }

    node->RemoveComponent(m_Component);
    return true;
}

std::string AddComponentCommand::GetDescription() const {
    if (m_Component) {
        return "Add Component: " + m_Component->GetTypeName();
    }
    return "Add Component";
}

nlohmann::json AddComponentCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["node_uuid"] = m_NodeUUID.ToString();
    if (m_Component) {
        json["component_data"] = m_Component->Serialize();
    }
    return json;
}

bool AddComponentCommand::Deserialize(const nlohmann::json& json) {
    return false;
}

RemoveComponentCommand::RemoveComponentCommand(std::shared_ptr<Node> node, std::shared_ptr<Component> component)
    : m_Node(node), m_Component(component), m_ComponentIndex(0) {
    if (node) {
        m_NodeUUID = node->GetUUID();

        const auto& components = node->GetComponents();
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) {
            m_ComponentIndex = std::distance(components.begin(), it);
        }
    }
}

bool RemoveComponentCommand::Execute() {
    auto node = m_Node.lock();
    if (!node || !m_Component) {
        return false;
    }

    node->RemoveComponent(m_Component);
    return true;
}

bool RemoveComponentCommand::Undo() {
    auto node = m_Node.lock();
    if (!node || !m_Component) {
        return false;
    }

    node->AddComponent(m_Component);
    return true;
}

std::string RemoveComponentCommand::GetDescription() const {
    if (m_Component) {
        return "Remove Component: " + m_Component->GetTypeName();
    }
    return "Remove Component";
}

nlohmann::json RemoveComponentCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["node_uuid"] = m_NodeUUID.ToString();
    json["component_index"] = m_ComponentIndex;
    if (m_Component) {
        json["component_data"] = m_Component->Serialize();
    }
    return json;
}

bool RemoveComponentCommand::Deserialize(const nlohmann::json& json) {
    return false;
}

SetNodePropertyCommand::SetNodePropertyCommand(std::shared_ptr<Node> node, const std::string& propertyName,
                                             const nlohmann::json& newValue)
    : m_Node(node), m_PropertyName(propertyName), m_NewValue(newValue) {
    if (node) {
        m_NodeUUID = node->GetUUID();

        node->RegisterProperties();
        m_OldValue = node->Serialize()["properties"][propertyName];
    }
}

SetNodePropertyCommand::SetNodePropertyCommand(std::shared_ptr<Node> node, const std::string& propertyName,
                                             const nlohmann::json& oldValue, const nlohmann::json& newValue)
    : m_Node(node), m_PropertyName(propertyName), m_OldValue(oldValue), m_NewValue(newValue) {
    if (node) {
        m_NodeUUID = node->GetUUID();
    }
}

bool SetNodePropertyCommand::Execute() {
    auto node = m_Node.lock();
    if (!node) {
        return false;
    }

    node->RegisterProperties();

    nlohmann::json nodeJson;
    nodeJson["properties"][m_PropertyName] = m_NewValue;
    node->Deserialize(nodeJson);

    return true;
}

bool SetNodePropertyCommand::Undo() {
    auto node = m_Node.lock();
    if (!node) {

        return false;
    }

    node->RegisterProperties();

    nlohmann::json nodeJson;
    nodeJson["properties"][m_PropertyName] = m_OldValue;

    node->Deserialize(nodeJson);

    return true;
}

std::string SetNodePropertyCommand::GetDescription() const {
    auto node = m_Node.lock();
    if (node) {
        return "Set " + node->GetName() + "." + m_PropertyName;
    }
    return "Set Node Property: " + m_PropertyName;
}

nlohmann::json SetNodePropertyCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["node_uuid"] = m_NodeUUID.ToString();
    json["property_name"] = m_PropertyName;
    json["old_value"] = m_OldValue;
    json["new_value"] = m_NewValue;
    return json;
}

bool SetNodePropertyCommand::Deserialize(const nlohmann::json& json) {
    return false;
}

bool SetNodePropertyCommand::CanMergeWith(const Command* other) const {
    auto otherCmd = dynamic_cast<const SetNodePropertyCommand*>(other);
    if (!otherCmd) {
        return false;
    }

    return m_NodeUUID == otherCmd->m_NodeUUID && m_PropertyName == otherCmd->m_PropertyName;
}

void SetNodePropertyCommand::MergeWith(const Command* other) {
    auto otherCmd = dynamic_cast<const SetNodePropertyCommand*>(other);
    if (otherCmd) {

        m_NewValue = otherCmd->m_NewValue;
    }
}

SetComponentPropertyCommand::SetComponentPropertyCommand(std::shared_ptr<Component> component,
                                                       const std::string& propertyName,
                                                       const nlohmann::json& newValue)
    : m_Component(component), m_PropertyName(propertyName), m_NewValue(newValue) {
    if (component) {
        m_ComponentUUID = component->GetUUID();

        component->RegisterProperties();
        auto& registry = component->GetPropertyRegistry();
        auto prop = registry.GetProperty(propertyName);
        if (prop) {
            m_OldValue = prop->GetValueAsJson();
        }
    }
}

bool SetComponentPropertyCommand::Execute() {
    auto component = m_Component.lock();
    if (!component) {
        return false;
    }

    auto& registry = component->GetPropertyRegistry();
    auto prop = registry.GetProperty(m_PropertyName);
    if (prop) {
        prop->SetValueFromJson(m_NewValue);

        component->OnPropertyChanged(m_PropertyName, m_NewValue);

        return true;
    }

    return false;
}

bool SetComponentPropertyCommand::Undo() {
    auto component = m_Component.lock();
    if (!component) {
        return false;
    }

    auto& registry = component->GetPropertyRegistry();
    auto prop = registry.GetProperty(m_PropertyName);
    if (prop) {
        prop->SetValueFromJson(m_OldValue);

        component->OnPropertyChanged(m_PropertyName, m_OldValue);

        return true;
    }

    return false;
}

std::string SetComponentPropertyCommand::GetDescription() const {
    auto component = m_Component.lock();
    if (component) {
        return "Set " + component->GetTypeName() + "." + m_PropertyName;
    }
    return "Set Component Property: " + m_PropertyName;
}

nlohmann::json SetComponentPropertyCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["component_uuid"] = m_ComponentUUID.ToString();
    json["property_name"] = m_PropertyName;
    json["old_value"] = m_OldValue;
    json["new_value"] = m_NewValue;
    return json;
}

bool SetComponentPropertyCommand::Deserialize(const nlohmann::json& json) {
    return false;
}

bool SetComponentPropertyCommand::CanMergeWith(const Command* other) const {
    auto otherCmd = dynamic_cast<const SetComponentPropertyCommand*>(other);
    if (!otherCmd) {
        return false;
    }

    return m_ComponentUUID == otherCmd->m_ComponentUUID && m_PropertyName == otherCmd->m_PropertyName;
}

void SetComponentPropertyCommand::MergeWith(const Command* other) {
    auto otherCmd = dynamic_cast<const SetComponentPropertyCommand*>(other);
    if (otherCmd) {

        m_NewValue = otherCmd->m_NewValue;
    }
}

}
}

