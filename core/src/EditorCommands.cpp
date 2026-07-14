#include "lupine/core/EditorCommands.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/components/Line2D.hpp"
#include "lupine/components/Curve2D.hpp"
#include "lupine/components/Path2D.hpp"
#include <algorithm>

namespace lupine {
namespace core {

// Each Deserialize below restores exactly what the matching Serialize wrote: data
// fields, UUIDs, and (for the commands that own one) a reconstructed Node/Component
// payload. The weak back-references to the live Scene/Node/Component cannot be
// resolved here because Deserialize has no scene context; they stay empty and are
// resolved by UUID when the history is later rebound to a scene, so a restored
// command round-trips losslessly even though it is not immediately executable.

namespace {

// Rebuild a Node from its serialized blob (Node::Serialize writes a "type" tag that
// TypeRegistry resolves to a concrete node class). Returns nullptr on a missing type
// or an unregistered type.
std::shared_ptr<Node> DeserializeNodeBlob(const nlohmann::json& data) {
    if (!data.is_object() || !data.contains("type") || !data["type"].is_string()) {
        return nullptr;
    }
    auto node = std::dynamic_pointer_cast<Node>(
        TypeRegistry::GetInstance().CreateInstance(data["type"].get<std::string>()));
    if (node) {
        node->Deserialize(data);
    }
    return node;
}

// Rebuild a Component from its serialized blob, mirroring DeserializeNodeBlob.
std::shared_ptr<Component> DeserializeComponentBlob(const nlohmann::json& data) {
    if (!data.is_object() || !data.contains("type") || !data["type"].is_string()) {
        return nullptr;
    }
    auto component = std::dynamic_pointer_cast<Component>(
        TypeRegistry::GetInstance().CreateInstance(data["type"].get<std::string>()));
    if (component) {
        component->Deserialize(data);
    }
    return component;
}

// Read a UUID field, tolerating an empty/absent value (yields a default UUID).
UUID ReadUUID(const nlohmann::json& json, const char* key) {
    if (json.contains(key) && json[key].is_string()) {
        const std::string str = json[key].get<std::string>();
        if (!str.empty()) {
            return UUID::FromString(str);
        }
    }
    return UUID();
}

} // namespace

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
    m_ParentUUID = ReadUUID(json, "parent_uuid");
    m_WasRoot = json.value("was_root", false);
    if (json.contains("node_data")) {
        m_Node = DeserializeNodeBlob(json["node_data"]);
    }
    return true;
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
    m_ParentUUID = ReadUUID(json, "parent_uuid");
    m_WasRoot = json.value("was_root", false);
    m_ChildIndex = json.value("child_index", static_cast<size_t>(0));
    if (json.contains("node_data")) {
        m_Node = DeserializeNodeBlob(json["node_data"]);
    }
    return true;
}

ReparentNodeCommand::ReparentNodeCommand(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent)
    : m_Node(node), m_NewParent(newParent), m_OldChildIndex(0) {
    if (node) {
        m_NodeUUID = node->GetUUID();
        std::shared_ptr<Node> oldParent = node->GetParentShared();
        if (oldParent) {
            m_OldParent = oldParent;
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
    m_NodeUUID = ReadUUID(json, "node_uuid");
    m_OldParentUUID = ReadUUID(json, "old_parent_uuid");
    m_NewParentUUID = ReadUUID(json, "new_parent_uuid");
    m_OldChildIndex = json.value("old_child_index", static_cast<size_t>(0));
    return true;
}

MoveNodeCommand::MoveNodeCommand(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent, int targetIndex)
    : m_Node(node), m_NewParent(newParent), m_OldChildIndex(0), m_TargetIndex(targetIndex) {
    if (node) {
        m_NodeUUID = node->GetUUID();
        std::shared_ptr<Node> oldParent = node->GetParentShared();
        if (oldParent) {
            m_OldParent = oldParent;
            m_OldParentUUID = oldParent->GetUUID();
            int idx = oldParent->GetChildIndex(node.get());
            if (idx >= 0) {
                m_OldChildIndex = static_cast<size_t>(idx);
            }
        }
    }
    if (newParent) {
        m_NewParentUUID = newParent->GetUUID();
    }
}

bool MoveNodeCommand::Execute() {
    auto node = m_Node.lock();
    auto newParent = m_NewParent.lock();
    if (!node || !newParent) {
        return false;
    }

    Node* oldParent = node->GetParent();
    if (oldParent == newParent.get()) {
        size_t index = (m_TargetIndex < 0) ? newParent->GetChildCount()
                                           : static_cast<size_t>(m_TargetIndex);
        newParent->MoveChild(node, index);
    } else {
        node->ReparentTo(newParent.get());
        if (m_TargetIndex >= 0) {
            newParent->MoveChild(node, static_cast<size_t>(m_TargetIndex));
        }
    }
    return true;
}

bool MoveNodeCommand::Undo() {
    auto node = m_Node.lock();
    if (!node) {
        return false;
    }
    auto oldParent = m_OldParent.lock();

    if (oldParent) {
        if (node->GetParent() != oldParent.get()) {
            node->ReparentTo(oldParent.get());
        }
        oldParent->MoveChild(node, m_OldChildIndex);
    } else {
        node->ReparentTo(nullptr);
    }
    return true;
}

std::string MoveNodeCommand::GetDescription() const {
    auto node = m_Node.lock();
    if (node) {
        return "Move Node: " + node->GetName();
    }
    return "Move Node";
}

nlohmann::json MoveNodeCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["node_uuid"] = m_NodeUUID.ToString();
    json["old_parent_uuid"] = m_OldParentUUID.ToString();
    json["new_parent_uuid"] = m_NewParentUUID.ToString();
    json["old_child_index"] = m_OldChildIndex;
    json["target_index"] = m_TargetIndex;
    return json;
}

bool MoveNodeCommand::Deserialize(const nlohmann::json& json) {
    m_NodeUUID = ReadUUID(json, "node_uuid");
    m_OldParentUUID = ReadUUID(json, "old_parent_uuid");
    m_NewParentUUID = ReadUUID(json, "new_parent_uuid");
    m_OldChildIndex = json.value("old_child_index", static_cast<size_t>(0));
    m_TargetIndex = json.value("target_index", -1);
    return true;
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
    m_NodeUUID = ReadUUID(json, "node_uuid");
    if (json.contains("component_data")) {
        m_Component = DeserializeComponentBlob(json["component_data"]);
    }
    return true;
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
    m_NodeUUID = ReadUUID(json, "node_uuid");
    m_ComponentIndex = json.value("component_index", static_cast<size_t>(0));
    if (json.contains("component_data")) {
        m_Component = DeserializeComponentBlob(json["component_data"]);
    }
    return true;
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
    m_NodeUUID = ReadUUID(json, "node_uuid");
    m_PropertyName = json.value("property_name", std::string());
    m_OldValue = json.contains("old_value") ? json["old_value"] : nlohmann::json();
    m_NewValue = json.contains("new_value") ? json["new_value"] : nlohmann::json();
    return true;
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

SetComponentPropertyCommand::SetComponentPropertyCommand(std::shared_ptr<Component> component,
                                                       const std::string& propertyName,
                                                       const nlohmann::json& oldValue,
                                                       const nlohmann::json& newValue)
    : m_Component(component), m_PropertyName(propertyName), m_OldValue(oldValue), m_NewValue(newValue) {
    if (component) {
        m_ComponentUUID = component->GetUUID();
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
    m_ComponentUUID = ReadUUID(json, "component_uuid");
    m_PropertyName = json.value("property_name", std::string());
    m_OldValue = json.contains("old_value") ? json["old_value"] : nlohmann::json();
    m_NewValue = json.contains("new_value") ? json["new_value"] : nlohmann::json();
    return true;
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

// ===== SetComponentPropertyGroupCommand =====

SetComponentPropertyGroupCommand::SetComponentPropertyGroupCommand(
    std::shared_ptr<Component> component,
    const std::string& propertyName,
    const nlohmann::json& newValue,
    const std::vector<std::string>& affectedProperties)
    : m_Component(component), m_PropertyName(propertyName), m_NewValue(newValue),
      m_AffectedProperties(affectedProperties) {
    if (component) {
        m_ComponentUUID = component->GetUUID();
        component->RegisterProperties();
        CaptureState(m_OldState);
    }
}

void SetComponentPropertyGroupCommand::CaptureState(std::map<std::string, nlohmann::json>& out) const {
    auto component = m_Component.lock();
    if (!component) {
        return;
    }
    auto& registry = component->GetPropertyRegistry();
    out.clear();
    for (const std::string& name : m_AffectedProperties) {
        auto prop = registry.GetProperty(name);
        if (prop) {
            out[name] = prop->GetValueAsJson();
        }
    }
}

void SetComponentPropertyGroupCommand::RestoreState(const std::map<std::string, nlohmann::json>& state) {
    auto component = m_Component.lock();
    if (!component) {
        return;
    }
    auto& registry = component->GetPropertyRegistry();
    // Restore raw values without invoking OnPropertyChanged - the cascading
    // derivation (e.g. ApplyAnchorPreset) would otherwise overwrite the very
    // values we are restoring. The component re-resolves its layout next frame.
    for (const auto& entry : state) {
        auto prop = registry.GetProperty(entry.first);
        if (prop) {
            prop->SetValueFromJson(entry.second);
        }
    }
}

bool SetComponentPropertyGroupCommand::Execute() {
    auto component = m_Component.lock();
    if (!component) {
        return false;
    }

    if (!m_Captured) {
        // First execution: apply the user-triggered edit through OnPropertyChanged so
        // the component derives all dependent properties, then snapshot the result.
        auto& registry = component->GetPropertyRegistry();
        auto prop = registry.GetProperty(m_PropertyName);
        if (!prop) {
            return false;
        }
        prop->SetValueFromJson(m_NewValue);
        component->OnPropertyChanged(m_PropertyName, m_NewValue);
        CaptureState(m_NewState);
        m_Captured = true;
    } else {
        // Redo: restore the post-edit snapshot directly (no re-derivation).
        RestoreState(m_NewState);
    }
    return true;
}

bool SetComponentPropertyGroupCommand::Undo() {
    auto component = m_Component.lock();
    if (!component) {
        return false;
    }
    RestoreState(m_OldState);
    return true;
}

std::string SetComponentPropertyGroupCommand::GetDescription() const {
    auto component = m_Component.lock();
    if (component) {
        return "Set " + component->GetTypeName() + "." + m_PropertyName;
    }
    return "Set Component Property: " + m_PropertyName;
}

nlohmann::json SetComponentPropertyGroupCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["component_uuid"] = m_ComponentUUID.ToString();
    json["property_name"] = m_PropertyName;
    json["new_value"] = m_NewValue;
    json["affected_properties"] = m_AffectedProperties;
    return json;
}

bool SetComponentPropertyGroupCommand::Deserialize(const nlohmann::json& json) {
    m_ComponentUUID = ReadUUID(json, "component_uuid");
    m_PropertyName = json.value("property_name", std::string());
    m_NewValue = json.contains("new_value") ? json["new_value"] : nlohmann::json();
    m_AffectedProperties.clear();
    if (json.contains("affected_properties") && json["affected_properties"].is_array()) {
        for (const auto& name : json["affected_properties"]) {
            if (name.is_string()) {
                m_AffectedProperties.push_back(name.get<std::string>());
            }
        }
    }
    // Serialize does not persist the before/after state maps, so a restored command
    // re-derives them on first Execute (m_Captured stays false).
    m_Captured = false;
    return true;
}

// ===== AddPointCommand =====

AddPointCommand::AddPointCommand(std::shared_ptr<Component> component, int pointIndex, const nlohmann::json& pointData)
    : m_Component(component), m_PointIndex(pointIndex), m_PointData(pointData) {
    if (component) {
        m_ComponentUUID = component->GetUUID();
        m_ComponentType = component->GetTypeName();
    }
}

bool AddPointCommand::Execute() {
    auto component = m_Component.lock();
    if (!component) return false;

    float x = m_PointData.value("x", 0.0f);
    float y = m_PointData.value("y", 0.0f);
    math::Vec2 pos(x, y);

    if (m_ComponentType == "Line2D") {
        auto line = std::dynamic_pointer_cast<components::Line2D>(component);
        if (line) {
            line->AddPoint(pos);
            return true;
        }
    } else if (m_ComponentType == "Curve2D" || m_ComponentType == "Path2D") {
        auto curve = std::dynamic_pointer_cast<components::Curve2D>(component);
        if (curve) {
            curve->AddPoint(pos);
            return true;
        }
    }
    return false;
}

bool AddPointCommand::Undo() {
    auto component = m_Component.lock();
    if (!component) return false;

    if (m_ComponentType == "Line2D") {
        auto line = std::dynamic_pointer_cast<components::Line2D>(component);
        if (line && m_PointIndex >= 0 && m_PointIndex < (int)line->GetPointCount()) {
            line->RemovePoint(m_PointIndex);
            return true;
        }
    } else if (m_ComponentType == "Curve2D" || m_ComponentType == "Path2D") {
        auto curve = std::dynamic_pointer_cast<components::Curve2D>(component);
        if (curve && m_PointIndex >= 0 && m_PointIndex < (int)curve->GetPointCount()) {
            curve->RemovePoint(m_PointIndex);
            return true;
        }
    }
    return false;
}

std::string AddPointCommand::GetDescription() const {
    return "Add Point to " + m_ComponentType;
}

nlohmann::json AddPointCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["component_uuid"] = m_ComponentUUID.ToString();
    json["component_type"] = m_ComponentType;
    json["point_index"] = m_PointIndex;
    json["point_data"] = m_PointData;
    return json;
}

bool AddPointCommand::Deserialize(const nlohmann::json& json) {
    m_ComponentUUID = ReadUUID(json, "component_uuid");
    m_ComponentType = json.value("component_type", std::string());
    m_PointIndex = json.value("point_index", 0);
    m_PointData = json.contains("point_data") ? json["point_data"] : nlohmann::json();
    return true;
}

// ===== RemovePointCommand =====

RemovePointCommand::RemovePointCommand(std::shared_ptr<Component> component, int pointIndex, const nlohmann::json& pointData)
    : m_Component(component), m_PointIndex(pointIndex), m_PointData(pointData) {
    if (component) {
        m_ComponentUUID = component->GetUUID();
        m_ComponentType = component->GetTypeName();
    }
}

bool RemovePointCommand::Execute() {
    auto component = m_Component.lock();
    if (!component) return false;

    if (m_ComponentType == "Line2D") {
        auto line = std::dynamic_pointer_cast<components::Line2D>(component);
        if (line && m_PointIndex >= 0 && m_PointIndex < (int)line->GetPointCount()) {
            line->RemovePoint(m_PointIndex);
            return true;
        }
    } else if (m_ComponentType == "Curve2D" || m_ComponentType == "Path2D") {
        auto curve = std::dynamic_pointer_cast<components::Curve2D>(component);
        if (curve && m_PointIndex >= 0 && m_PointIndex < (int)curve->GetPointCount()) {
            curve->RemovePoint(m_PointIndex);
            return true;
        }
    }
    return false;
}

bool RemovePointCommand::Undo() {
    auto component = m_Component.lock();
    if (!component) return false;

    float x = m_PointData.value("x", 0.0f);
    float y = m_PointData.value("y", 0.0f);
    float ctrlInX = m_PointData.value("ctrlInX", 0.0f);
    float ctrlInY = m_PointData.value("ctrlInY", 0.0f);
    float ctrlOutX = m_PointData.value("ctrlOutX", 0.0f);
    float ctrlOutY = m_PointData.value("ctrlOutY", 0.0f);
    bool useBezier = m_PointData.value("useBezier", false);
    bool symmetric = m_PointData.value("symmetricHandles", true);

    if (m_ComponentType == "Line2D") {
        auto line = std::dynamic_pointer_cast<components::Line2D>(component);
        if (line) {
            components::Line2DPoint pt;
            pt.position = math::Vec2(x, y);
            pt.controlIn = math::Vec2(ctrlInX, ctrlInY);
            pt.controlOut = math::Vec2(ctrlOutX, ctrlOutY);
            pt.useBezier = useBezier;
            pt.symmetricHandles = symmetric;
            line->InsertLine2DPoint(static_cast<size_t>(m_PointIndex), pt);
            return true;
        }
    } else if (m_ComponentType == "Curve2D" || m_ComponentType == "Path2D") {
        auto curve = std::dynamic_pointer_cast<components::Curve2D>(component);
        if (curve) {
            components::Curve2DPoint pt;
            pt.position = math::Vec2(x, y);
            pt.controlIn = math::Vec2(ctrlInX, ctrlInY);
            pt.controlOut = math::Vec2(ctrlOutX, ctrlOutY);
            pt.useBezier = useBezier;
            pt.symmetricHandles = symmetric;
            curve->InsertCurvePoint(static_cast<size_t>(m_PointIndex), pt);
            return true;
        }
    }
    return false;
}

std::string RemovePointCommand::GetDescription() const {
    return "Remove Point from " + m_ComponentType;
}

nlohmann::json RemovePointCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["component_uuid"] = m_ComponentUUID.ToString();
    json["component_type"] = m_ComponentType;
    json["point_index"] = m_PointIndex;
    json["point_data"] = m_PointData;
    return json;
}

bool RemovePointCommand::Deserialize(const nlohmann::json& json) {
    m_ComponentUUID = ReadUUID(json, "component_uuid");
    m_ComponentType = json.value("component_type", std::string());
    m_PointIndex = json.value("point_index", 0);
    m_PointData = json.contains("point_data") ? json["point_data"] : nlohmann::json();
    return true;
}

// ===== ModifyPointCommand =====

ModifyPointCommand::ModifyPointCommand(std::shared_ptr<Component> component, int pointIndex,
                                       const nlohmann::json& oldPointData, const nlohmann::json& newPointData)
    : m_Component(component), m_PointIndex(pointIndex), m_OldPointData(oldPointData), m_NewPointData(newPointData) {
    if (component) {
        m_ComponentUUID = component->GetUUID();
        m_ComponentType = component->GetTypeName();
    }
}

bool ModifyPointCommand::Execute() {
    auto component = m_Component.lock();
    if (!component) return false;

    float x = m_NewPointData.value("x", 0.0f);
    float y = m_NewPointData.value("y", 0.0f);
    float ctrlInX = m_NewPointData.value("ctrlInX", 0.0f);
    float ctrlInY = m_NewPointData.value("ctrlInY", 0.0f);
    float ctrlOutX = m_NewPointData.value("ctrlOutX", 0.0f);
    float ctrlOutY = m_NewPointData.value("ctrlOutY", 0.0f);
    bool useBezier = m_NewPointData.value("useBezier", false);
    bool symmetric = m_NewPointData.value("symmetricHandles", true);

    if (m_ComponentType == "Line2D") {
        auto line = std::dynamic_pointer_cast<components::Line2D>(component);
        if (line && m_PointIndex >= 0 && m_PointIndex < (int)line->GetPointCount()) {
            line->SetPoint(m_PointIndex, math::Vec2(x, y));
            line->SetPointControlHandles(m_PointIndex, math::Vec2(ctrlInX, ctrlInY),
                                         math::Vec2(ctrlOutX, ctrlOutY), symmetric);
            line->SetPointUseBezier(m_PointIndex, useBezier);
            return true;
        }
    } else if (m_ComponentType == "Curve2D" || m_ComponentType == "Path2D") {
        auto curve = std::dynamic_pointer_cast<components::Curve2D>(component);
        if (curve && m_PointIndex >= 0 && m_PointIndex < (int)curve->GetPointCount()) {
            curve->SetPointPosition(m_PointIndex, math::Vec2(x, y));
            curve->SetPointControlHandles(m_PointIndex, math::Vec2(ctrlInX, ctrlInY),
                                          math::Vec2(ctrlOutX, ctrlOutY), symmetric);
            curve->SetPointUseBezier(m_PointIndex, useBezier);
            return true;
        }
    }
    return false;
}

bool ModifyPointCommand::Undo() {
    auto component = m_Component.lock();
    if (!component) return false;

    float x = m_OldPointData.value("x", 0.0f);
    float y = m_OldPointData.value("y", 0.0f);
    float ctrlInX = m_OldPointData.value("ctrlInX", 0.0f);
    float ctrlInY = m_OldPointData.value("ctrlInY", 0.0f);
    float ctrlOutX = m_OldPointData.value("ctrlOutX", 0.0f);
    float ctrlOutY = m_OldPointData.value("ctrlOutY", 0.0f);
    bool useBezier = m_OldPointData.value("useBezier", false);
    bool symmetric = m_OldPointData.value("symmetricHandles", true);

    if (m_ComponentType == "Line2D") {
        auto line = std::dynamic_pointer_cast<components::Line2D>(component);
        if (line && m_PointIndex >= 0 && m_PointIndex < (int)line->GetPointCount()) {
            line->SetPoint(m_PointIndex, math::Vec2(x, y));
            line->SetPointControlHandles(m_PointIndex, math::Vec2(ctrlInX, ctrlInY),
                                         math::Vec2(ctrlOutX, ctrlOutY), symmetric);
            line->SetPointUseBezier(m_PointIndex, useBezier);
            return true;
        }
    } else if (m_ComponentType == "Curve2D" || m_ComponentType == "Path2D") {
        auto curve = std::dynamic_pointer_cast<components::Curve2D>(component);
        if (curve && m_PointIndex >= 0 && m_PointIndex < (int)curve->GetPointCount()) {
            curve->SetPointPosition(m_PointIndex, math::Vec2(x, y));
            curve->SetPointControlHandles(m_PointIndex, math::Vec2(ctrlInX, ctrlInY),
                                          math::Vec2(ctrlOutX, ctrlOutY), symmetric);
            curve->SetPointUseBezier(m_PointIndex, useBezier);
            return true;
        }
    }
    return false;
}

std::string ModifyPointCommand::GetDescription() const {
    return "Modify Point in " + m_ComponentType;
}

nlohmann::json ModifyPointCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["component_uuid"] = m_ComponentUUID.ToString();
    json["component_type"] = m_ComponentType;
    json["point_index"] = m_PointIndex;
    json["old_point_data"] = m_OldPointData;
    json["new_point_data"] = m_NewPointData;
    return json;
}

bool ModifyPointCommand::Deserialize(const nlohmann::json& json) {
    m_ComponentUUID = ReadUUID(json, "component_uuid");
    m_ComponentType = json.value("component_type", std::string());
    m_PointIndex = json.value("point_index", 0);
    m_OldPointData = json.contains("old_point_data") ? json["old_point_data"] : nlohmann::json();
    m_NewPointData = json.contains("new_point_data") ? json["new_point_data"] : nlohmann::json();
    return true;
}

bool ModifyPointCommand::CanMergeWith(const Command* other) const {
    auto otherCmd = dynamic_cast<const ModifyPointCommand*>(other);
    if (!otherCmd) return false;
    return m_ComponentUUID == otherCmd->m_ComponentUUID && m_PointIndex == otherCmd->m_PointIndex;
}

void ModifyPointCommand::MergeWith(const Command* other) {
    auto otherCmd = dynamic_cast<const ModifyPointCommand*>(other);
    if (otherCmd) {
        m_NewPointData = otherCmd->m_NewPointData;
    }
}

// ===== ClearPointsCommand =====

ClearPointsCommand::ClearPointsCommand(std::shared_ptr<Component> component, const nlohmann::json& oldPointsData)
    : m_Component(component), m_OldPointsData(oldPointsData) {
    if (component) {
        m_ComponentUUID = component->GetUUID();
        m_ComponentType = component->GetTypeName();
    }
}

bool ClearPointsCommand::Execute() {
    auto component = m_Component.lock();
    if (!component) return false;

    if (m_ComponentType == "Line2D") {
        auto line = std::dynamic_pointer_cast<components::Line2D>(component);
        if (line) {
            line->ClearPoints();
            return true;
        }
    } else if (m_ComponentType == "Curve2D" || m_ComponentType == "Path2D") {
        auto curve = std::dynamic_pointer_cast<components::Curve2D>(component);
        if (curve) {
            curve->ClearPoints();
            return true;
        }
    }
    return false;
}

bool ClearPointsCommand::Undo() {
    auto component = m_Component.lock();
    if (!component) return false;

    if (!m_OldPointsData.is_array()) return false;

    if (m_ComponentType == "Line2D") {
        auto line = std::dynamic_pointer_cast<components::Line2D>(component);
        if (line) {
            for (const auto& ptData : m_OldPointsData) {
                components::Line2DPoint pt;
                pt.position = math::Vec2(ptData.value("x", 0.0f), ptData.value("y", 0.0f));
                pt.controlIn = math::Vec2(ptData.value("ctrlInX", 0.0f), ptData.value("ctrlInY", 0.0f));
                pt.controlOut = math::Vec2(ptData.value("ctrlOutX", 0.0f), ptData.value("ctrlOutY", 0.0f));
                pt.useBezier = ptData.value("useBezier", false);
                pt.symmetricHandles = ptData.value("symmetricHandles", true);
                line->AddLine2DPoint(pt);
            }
            return true;
        }
    } else if (m_ComponentType == "Curve2D" || m_ComponentType == "Path2D") {
        auto curve = std::dynamic_pointer_cast<components::Curve2D>(component);
        if (curve) {
            for (const auto& ptData : m_OldPointsData) {
                components::Curve2DPoint pt;
                pt.position = math::Vec2(ptData.value("x", 0.0f), ptData.value("y", 0.0f));
                pt.controlIn = math::Vec2(ptData.value("ctrlInX", 0.0f), ptData.value("ctrlInY", 0.0f));
                pt.controlOut = math::Vec2(ptData.value("ctrlOutX", 0.0f), ptData.value("ctrlOutY", 0.0f));
                pt.useBezier = ptData.value("useBezier", false);
                pt.symmetricHandles = ptData.value("symmetricHandles", true);
                curve->AddCurvePoint(pt);
            }
            return true;
        }
    }
    return false;
}

std::string ClearPointsCommand::GetDescription() const {
    return "Clear Points from " + m_ComponentType;
}

nlohmann::json ClearPointsCommand::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["component_uuid"] = m_ComponentUUID.ToString();
    json["component_type"] = m_ComponentType;
    json["old_points_data"] = m_OldPointsData;
    return json;
}

bool ClearPointsCommand::Deserialize(const nlohmann::json& json) {
    m_ComponentUUID = ReadUUID(json, "component_uuid");
    m_ComponentType = json.value("component_type", std::string());
    m_OldPointsData = json.contains("old_points_data") ? json["old_points_data"] : nlohmann::json();
    return true;
}

std::shared_ptr<Command> CreateCommandForType(const std::string& typeName) {
    if (typeName == "AddNodeCommand")                   return std::make_shared<AddNodeCommand>();
    if (typeName == "RemoveNodeCommand")                return std::make_shared<RemoveNodeCommand>();
    if (typeName == "ReparentNodeCommand")              return std::make_shared<ReparentNodeCommand>();
    if (typeName == "MoveNodeCommand")                  return std::make_shared<MoveNodeCommand>();
    if (typeName == "AddComponentCommand")              return std::make_shared<AddComponentCommand>();
    if (typeName == "RemoveComponentCommand")           return std::make_shared<RemoveComponentCommand>();
    if (typeName == "SetNodePropertyCommand")           return std::make_shared<SetNodePropertyCommand>();
    if (typeName == "SetComponentPropertyCommand")      return std::make_shared<SetComponentPropertyCommand>();
    if (typeName == "SetComponentPropertyGroupCommand") return std::make_shared<SetComponentPropertyGroupCommand>();
    if (typeName == "AddPointCommand")                  return std::make_shared<AddPointCommand>();
    if (typeName == "RemovePointCommand")               return std::make_shared<RemovePointCommand>();
    if (typeName == "ModifyPointCommand")               return std::make_shared<ModifyPointCommand>();
    if (typeName == "ClearPointsCommand")               return std::make_shared<ClearPointsCommand>();
    if (typeName == "CompositeCommand")                 return std::make_shared<CompositeCommand>();
    return nullptr;
}

}
}

