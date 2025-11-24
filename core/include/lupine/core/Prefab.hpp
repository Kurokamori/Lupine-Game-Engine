#pragma once

#include "lupine/core/Core.hpp"
#include "Node.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace core {

/**
 * Prefab class - serializes a segment of the node tree
 * Prefabs allow you to save a node hierarchy (with all components, properties,
 * parent/child relationships, visibility, etc.) and instantiate it multiple times
 * in different scenes.
 *
 * Example: Create a node tree with 2 Node2D nodes with specific components,
 * save it as a .prefab file, then instantiate it in any scene.
 */
class Prefab : public ISerializable {
public:
    Prefab();
    explicit Prefab(const std::string& name);
    ~Prefab();

    // ISerializable interface
    std::string GetTypeName() const override { return "Prefab"; }
    void RegisterProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Prefab management
    /**
     * Load a prefab from a .prefab file
     * @param filepath Path to the .prefab file
     * @return true if successful, false otherwise
     */
    bool Load(const std::string& filepath);

    /**
     * Save the prefab to a .prefab file
     * @param filepath Path to save the .prefab file
     * @return true if successful, false otherwise
     */
    bool Save(const std::string& filepath);

    /**
     * Save the prefab to its current filepath
     * @return true if successful, false otherwise
     */
    bool Save();

    /**
     * Create a prefab from an existing node tree
     * This serializes the node and all its children, components, etc.
     * @param rootNode The root node of the tree to save as a prefab
     */
    void CreateFromNode(std::shared_ptr<Node> rootNode);

    /**
     * Instantiate the prefab into a scene
     * This creates a new node tree from the prefab data with new UUIDs
     * @return A new node tree instance, or nullptr if prefab is invalid
     */
    std::shared_ptr<Node> Instantiate();

    /**
     * Instantiate the prefab as a child of a parent node
     * @param parent The parent node to attach the instantiated prefab to
     * @return A new node tree instance, or nullptr if prefab is invalid
     */
    std::shared_ptr<Node> InstantiateAsChild(std::shared_ptr<Node> parent);

    // Prefab properties
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    const std::string& GetFilePath() const { return m_FilePath; }

    /**
     * Check if the prefab has valid data
     * @return true if the prefab contains a valid node tree
     */
    bool IsValid() const { return m_RootNodeData != nullptr; }

    /**
     * Clear the prefab data
     */
    void Clear();

private:
    /**
     * Clone a node and its entire hierarchy with new UUIDs
     * This is used during instantiation to create independent copies
     * @param sourceNode The node to clone
     * @return A new node with a new UUID and cloned children/components
     */
    std::shared_ptr<Node> CloneNodeTree(const nlohmann::json& nodeJson);

    /**
     * Recursively regenerate UUIDs for a node tree
     * @param node The node to process
     */
    void RegenerateUUIDs(std::shared_ptr<Node> node);

    std::string m_Name;
    std::string m_FilePath;
    std::shared_ptr<nlohmann::json> m_RootNodeData;  // Serialized node tree data
};

} // namespace core
} // namespace lupine
