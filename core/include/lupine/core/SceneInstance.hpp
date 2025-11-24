#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace core {

// Import types from core namespace
using core::Node;
using core::Scene;

/**
 * SceneInstance class - allows nesting scenes within scenes
 * 
 * A SceneInstance is a special Node type that references an external scene file
 * and instantiates its entire node hierarchy as children of this node.
 * 
 * Key features:
 * - References an external .scene file
 * - Automatically loads and instantiates the scene hierarchy
 * - Updates when the referenced scene changes
 * - Maintains the scene reference in serialization
 * - Shows nested hierarchy in the scene tree
 * - Can be nested recursively (scene within scene within scene)
 * 
 * Example usage:
 *   auto sceneInstance = std::make_shared<SceneInstance>("MyLevel");
 *   sceneInstance->SetSceneReference("assets/levels/level1.scene");
 *   parentNode->AddChild(sceneInstance);
 * 
 * When the scene instance is added to a scene tree, it will:
 * 1. Load the referenced scene file
 * 2. Instantiate all nodes from that scene as children
 * 3. Maintain the reference for reloading/updating
 */
class SceneInstance : public Node {
public:
    SceneInstance();
    explicit SceneInstance(const std::string& name);
    virtual ~SceneInstance();

    // ISerializable interface
    std::string GetTypeName() const override { return "SceneInstance"; }
    void RegisterProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Scene reference management
    /**
     * Set the scene file to reference
     * This will automatically load and instantiate the scene
     * @param filepath Path to the .scene file to reference
     * @return true if the scene was loaded successfully
     */
    bool SetSceneReference(const std::string& filepath);

    /**
     * Get the path to the referenced scene file
     * @return The filepath of the referenced scene
     */
    const std::string& GetSceneReference() const { return m_SceneFilePath; }

    /**
     * Reload the referenced scene
     * This clears all current children and re-instantiates from the scene file
     * Useful for updating when the scene file has changed
     * @return true if reload was successful
     */
    bool ReloadScene();

    /**
     * Clear the scene instance
     * Removes all instantiated nodes and clears the reference
     */
    void ClearInstance();

    /**
     * Check if the scene instance has a valid reference
     * @return true if a scene file is referenced and loaded
     */
    bool HasValidReference() const { return !m_SceneFilePath.empty() && m_IsLoaded; }

    /**
     * Get the instantiated root node from the referenced scene
     * @return The root node of the instantiated scene, or nullptr if not loaded
     */
    std::shared_ptr<Node> GetInstancedRoot() const { return m_InstancedRoot; }

    // Lifecycle overrides
    void OnReady() override;
    void OnDestroy() override;
    void OnProcess(float deltaTime) override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnRender() override;

private:
    /**
     * Load and instantiate the referenced scene
     * @return true if successful
     */
    bool LoadAndInstantiate();

    /**
     * Clone a node tree from JSON data with new UUIDs
     * Similar to Prefab instantiation but for entire scenes
     * @param nodeJson The JSON data of the node to clone
     * @return A new node instance
     */
    std::shared_ptr<Node> CloneNodeTree(const nlohmann::json& nodeJson);

    /**
     * Recursively strip UUIDs from JSON to force regeneration
     * @param json The JSON data to strip UUIDs from
     */
    void StripUUIDs(nlohmann::json& json);

    std::string m_SceneFilePath;           // Path to the referenced .scene file
    std::shared_ptr<Node> m_InstancedRoot; // The root node of the instantiated scene
    bool m_IsLoaded;                        // Whether the scene has been successfully loaded
    bool m_AutoReload;                      // Whether to automatically reload on file changes (future feature)
};

} // namespace core
} // namespace lupine
