#pragma once

#include "lupine/core/Core.hpp"
#include "Node.hpp"
#include <string>
#include <memory>
#include <vector>

namespace lupine {
namespace core {

/**
 * Scene class - manages a scene graph of nodes
 * Scenes can be saved to .scene files and loaded back
 */
class Scene : public ISerializable {
public:
    Scene();
    explicit Scene(const std::string& name);
    ~Scene();

    // ISerializable interface
    std::string GetTypeName() const override { return "Scene"; }
    void RegisterProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Scene management
    bool Load(const std::string& filepath);
    bool Save(const std::string& filepath);
    bool Save();  // Save to current filepath

    // Scene properties
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    const std::string& GetFilePath() const { return m_FilePath; }

    // Scene graph management
    void SetRoot(std::shared_ptr<Node> root);
    std::shared_ptr<Node> GetRoot() const { return m_Root; }

    // Node management
    void AddNode(std::shared_ptr<Node> node);
    void RemoveNode(std::shared_ptr<Node> node);
    std::shared_ptr<Node> FindNode(const std::string& path) const;
    std::shared_ptr<Node> FindNodeByUUID(const UUID& uuid) const;

    // Scene lifecycle
    void Initialize();
    void Shutdown();
    void Update(float deltaTime);
    void PhysicsUpdate(float deltaTime);
    void Render();
    void ProcessInput(float deltaTime);

    // Scene state
    bool IsInitialized() const { return m_Initialized; }
    bool IsActive() const { return m_Active; }
    void SetActive(bool active) { m_Active = active; }
    bool IsShuttingDown() const { return m_IsShuttingDown; }

    // Editor mode
    bool IsInEditor() const { return m_IsInEditor; }
    void SetInEditor(bool inEditor) { m_IsInEditor = inEditor; }

private:
    void FindNodeByUUIDRecursive(const std::shared_ptr<Node>& node,
                                  const UUID& uuid,
                                  std::shared_ptr<Node>& result) const;

    void RegisterNodePropertiesRecursive(std::shared_ptr<Node> node);

    std::string m_Name;
    std::string m_FilePath;
    std::shared_ptr<Node> m_Root;
    bool m_Initialized;
    bool m_Active;
    bool m_IsInEditor;
    bool m_IsShuttingDown;
};

} // namespace core
} // namespace lupine

