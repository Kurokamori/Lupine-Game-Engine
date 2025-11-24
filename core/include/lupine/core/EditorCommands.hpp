#pragma once

#include "lupine/core/Command.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Scene.hpp"
#include <memory>
#include <string>

namespace lupine {
namespace core {

// Forward declarations
class Scene;

/**
 * Command for adding a node to the scene
 */
class AddNodeCommand : public Command {
public:
    AddNodeCommand(std::shared_ptr<Scene> scene, std::shared_ptr<Node> node, std::shared_ptr<Node> parent);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "AddNodeCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Scene> m_Scene;
    std::shared_ptr<Node> m_Node;
    std::weak_ptr<Node> m_Parent;
    UUID m_ParentUUID;
    bool m_WasRoot;
};

/**
 * Command for removing a node from the scene
 */
class RemoveNodeCommand : public Command {
public:
    RemoveNodeCommand(std::shared_ptr<Scene> scene, std::shared_ptr<Node> node);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "RemoveNodeCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Scene> m_Scene;
    std::shared_ptr<Node> m_Node;
    UUID m_ParentUUID;
    bool m_WasRoot;
    size_t m_ChildIndex;  // Position in parent's children list
};

/**
 * Command for reparenting a node
 */
class ReparentNodeCommand : public Command {
public:
    ReparentNodeCommand(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "ReparentNodeCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Node> m_Node;
    std::weak_ptr<Node> m_OldParent;
    std::weak_ptr<Node> m_NewParent;
    UUID m_NodeUUID;
    UUID m_OldParentUUID;
    UUID m_NewParentUUID;
    size_t m_OldChildIndex;
};

/**
 * Command for adding a component to a node
 */
class AddComponentCommand : public Command {
public:
    AddComponentCommand(std::shared_ptr<Node> node, std::shared_ptr<Component> component);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "AddComponentCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Node> m_Node;
    std::shared_ptr<Component> m_Component;
    UUID m_NodeUUID;
};

/**
 * Command for removing a component from a node
 */
class RemoveComponentCommand : public Command {
public:
    RemoveComponentCommand(std::shared_ptr<Node> node, std::shared_ptr<Component> component);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "RemoveComponentCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Node> m_Node;
    std::shared_ptr<Component> m_Component;
    UUID m_NodeUUID;
    size_t m_ComponentIndex;
};

/**
 * Command for changing a node property
 */
class SetNodePropertyCommand : public Command {
public:
    SetNodePropertyCommand(std::shared_ptr<Node> node, const std::string& propertyName,
                          const nlohmann::json& newValue);

    // Constructor that accepts both old and new values (for gizmo operations where value is already changed)
    SetNodePropertyCommand(std::shared_ptr<Node> node, const std::string& propertyName,
                          const nlohmann::json& oldValue, const nlohmann::json& newValue);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "SetNodePropertyCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

    bool CanMergeWith(const Command* other) const override;
    void MergeWith(const Command* other) override;

private:
    std::weak_ptr<Node> m_Node;
    UUID m_NodeUUID;
    std::string m_PropertyName;
    nlohmann::json m_OldValue;
    nlohmann::json m_NewValue;
};

/**
 * Command for changing a component property
 */
class SetComponentPropertyCommand : public Command {
public:
    SetComponentPropertyCommand(std::shared_ptr<Component> component, const std::string& propertyName,
                               const nlohmann::json& newValue);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "SetComponentPropertyCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;
    
    bool CanMergeWith(const Command* other) const override;
    void MergeWith(const Command* other) override;

private:
    std::weak_ptr<Component> m_Component;
    UUID m_ComponentUUID;
    std::string m_PropertyName;
    nlohmann::json m_OldValue;
    nlohmann::json m_NewValue;
};

} // namespace core
} // namespace lupine

