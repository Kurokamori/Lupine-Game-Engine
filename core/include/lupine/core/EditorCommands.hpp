#pragma once

#include "lupine/core/Command.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Scene.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace lupine {
namespace core {

// Forward declarations
class Scene;

/**
 * Command for adding a node to the scene
 */
class AddNodeCommand : public Command {
public:
    AddNodeCommand() = default;
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
    bool m_WasRoot = false;
};

/**
 * Command for removing a node from the scene
 */
class RemoveNodeCommand : public Command {
public:
    RemoveNodeCommand() = default;
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
    bool m_WasRoot = false;
    size_t m_ChildIndex = 0;  // Position in parent's children list
};

/**
 * Command for reparenting a node
 */
class ReparentNodeCommand : public Command {
public:
    ReparentNodeCommand() = default;
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
    size_t m_OldChildIndex = 0;
};

/**
 * Command for moving a node to a new parent and/or position (drag-and-drop
 * reorder). Unlike ReparentNodeCommand this targets a specific child index and
 * restores the original parent + index on undo. A target index < 0 means append.
 */
class MoveNodeCommand : public Command {
public:
    MoveNodeCommand() = default;
    MoveNodeCommand(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent, int targetIndex);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "MoveNodeCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Node> m_Node;
    std::weak_ptr<Node> m_OldParent;
    std::weak_ptr<Node> m_NewParent;
    UUID m_NodeUUID;
    UUID m_OldParentUUID;
    UUID m_NewParentUUID;
    size_t m_OldChildIndex = 0;
    int m_TargetIndex = -1;
};

/**
 * Command for adding a component to a node
 */
class AddComponentCommand : public Command {
public:
    AddComponentCommand() = default;
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
    RemoveComponentCommand() = default;
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
    size_t m_ComponentIndex = 0;
};

/**
 * Command for changing a node property
 */
class SetNodePropertyCommand : public Command {
public:
    SetNodePropertyCommand() = default;
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
    SetComponentPropertyCommand() = default;
    SetComponentPropertyCommand(std::shared_ptr<Component> component, const std::string& propertyName,
                               const nlohmann::json& newValue);

    // Constructor that accepts both old and new values (for gizmo operations where value is already changed)
    SetComponentPropertyCommand(std::shared_ptr<Component> component, const std::string& propertyName,
                               const nlohmann::json& oldValue, const nlohmann::json& newValue);

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

/**
 * Command for changing a component property that CASCADES into other properties.
 *
 * Some property setters trigger a component's OnPropertyChanged to derive several
 * other properties (e.g. a UIControl anchor preset rewrites anchorMin/anchorMax/
 * offsetMin/offsetMax/layoutMode). A plain SetComponentPropertyCommand only records
 * the single edited property, so undo leaves the derived properties changed. This
 * command snapshots the FULL affected property set before and after the edit and
 * restores those raw values on undo/redo (without re-running the derivation), so the
 * layout reverts exactly.
 */
class SetComponentPropertyGroupCommand : public Command {
public:
    SetComponentPropertyGroupCommand() = default;
    SetComponentPropertyGroupCommand(std::shared_ptr<Component> component,
                                     const std::string& propertyName,
                                     const nlohmann::json& newValue,
                                     const std::vector<std::string>& affectedProperties);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "SetComponentPropertyGroupCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    void CaptureState(std::map<std::string, nlohmann::json>& out) const;
    void RestoreState(const std::map<std::string, nlohmann::json>& state);

    std::weak_ptr<Component> m_Component;
    UUID m_ComponentUUID;
    std::string m_PropertyName;
    nlohmann::json m_NewValue;
    std::vector<std::string> m_AffectedProperties;
    std::map<std::string, nlohmann::json> m_OldState;
    std::map<std::string, nlohmann::json> m_NewState;
    bool m_Captured = false;
};

/**
 * Command for adding a point to Line2D/Curve2D/Path2D
 */
class AddPointCommand : public Command {
public:
    AddPointCommand() = default;
    AddPointCommand(std::shared_ptr<Component> component, int pointIndex, const nlohmann::json& pointData);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "AddPointCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Component> m_Component;
    UUID m_ComponentUUID;
    int m_PointIndex = 0;
    nlohmann::json m_PointData;
    std::string m_ComponentType;
};

/**
 * Command for removing a point from Line2D/Curve2D/Path2D
 */
class RemovePointCommand : public Command {
public:
    RemovePointCommand() = default;
    RemovePointCommand(std::shared_ptr<Component> component, int pointIndex, const nlohmann::json& pointData);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "RemovePointCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Component> m_Component;
    UUID m_ComponentUUID;
    int m_PointIndex = 0;
    nlohmann::json m_PointData;
    std::string m_ComponentType;
};

/**
 * Command for modifying a point in Line2D/Curve2D/Path2D (position, bezier controls, etc.)
 */
class ModifyPointCommand : public Command {
public:
    ModifyPointCommand() = default;
    ModifyPointCommand(std::shared_ptr<Component> component, int pointIndex,
                       const nlohmann::json& oldPointData, const nlohmann::json& newPointData);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "ModifyPointCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

    bool CanMergeWith(const Command* other) const override;
    void MergeWith(const Command* other) override;

private:
    std::weak_ptr<Component> m_Component;
    UUID m_ComponentUUID;
    int m_PointIndex = 0;
    nlohmann::json m_OldPointData;
    nlohmann::json m_NewPointData;
    std::string m_ComponentType;
};

/**
 * Command for clearing all points from Line2D/Curve2D/Path2D
 */
class ClearPointsCommand : public Command {
public:
    ClearPointsCommand() = default;
    ClearPointsCommand(std::shared_ptr<Component> component, const nlohmann::json& oldPointsData);

    bool Execute() override;
    bool Undo() override;
    std::string GetDescription() const override;
    std::string GetTypeName() const override { return "ClearPointsCommand"; }
    nlohmann::json Serialize() const override;
    bool Deserialize(const nlohmann::json& json) override;

private:
    std::weak_ptr<Component> m_Component;
    UUID m_ComponentUUID;
    nlohmann::json m_OldPointsData;
    std::string m_ComponentType;
};

// Construct an empty command whose GetTypeName() matches `typeName`, ready to be
// filled by Command::Deserialize. Returns nullptr for an unknown type. Used by
// CompositeCommand and CommandHistory when rebuilding a serialized history.
std::shared_ptr<Command> CreateCommandForType(const std::string& typeName);

} // namespace core
} // namespace lupine

