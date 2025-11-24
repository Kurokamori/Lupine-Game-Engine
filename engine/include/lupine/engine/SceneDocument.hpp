#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/Scene.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace engine {

// Import Scene from core namespace
using core::Scene;

/**
 * Represents a scene document in the editor
 * Tracks a scene along with its file path, dirty state, and unique ID
 */
class SceneDocument {
public:
    SceneDocument();
    explicit SceneDocument(std::shared_ptr<Scene> scene);
    explicit SceneDocument(const std::string& scenePath);
    ~SceneDocument();

    /**
     * Gets the unique ID of this scene document
     * @return UUID of the document
     */
    const core::UUID& GetID() const { return m_ID; }

    /**
     * Gets the scene managed by this document
     * @return Shared pointer to the scene
     */
    std::shared_ptr<Scene> GetScene() const { return m_Scene; }

    /**
     * Sets the scene for this document
     * @param scene Shared pointer to the scene
     */
    void SetScene(std::shared_ptr<Scene> scene);

    /**
     * Gets the file path of the scene
     * @return File path (empty if not saved yet)
     */
    const std::string& GetFilePath() const { return m_FilePath; }

    /**
     * Sets the file path of the scene
     * @param filePath New file path
     */
    void SetFilePath(const std::string& filePath);

    /**
     * Gets the display name of the scene
     * @return Display name (derived from file path or scene name)
     */
    std::string GetDisplayName() const;

    /**
     * Checks if the scene has unsaved changes
     * @return True if dirty, false otherwise
     */
    bool IsDirty() const { return m_IsDirty; }

    /**
     * Sets the dirty flag
     * @param dirty New dirty state
     */
    void SetDirty(bool dirty) { m_IsDirty = dirty; }

    /**
     * Marks the document as dirty (has unsaved changes)
     */
    void MarkDirty() { m_IsDirty = true; }

    /**
     * Creates a new scene document
     * @param sceneName Name for the new scene
     * @return Shared pointer to the new scene document
     */
    static std::shared_ptr<SceneDocument> CreateNew(const std::string& sceneName = "New Scene");

    /**
     * Opens a scene from a file
     * @param scenePath Path to the scene file
     * @return Shared pointer to the scene document, or nullptr if failed
     */
    static std::shared_ptr<SceneDocument> Open(const std::string& scenePath);

    /**
     * Saves the scene to its current file path
     * @return True if saved successfully, false otherwise
     */
    bool Save();

    /**
     * Saves the scene to a new file path
     * @param newPath New file path
     * @return True if saved successfully, false otherwise
     */
    bool SaveAs(const std::string& newPath);

    /**
     * Closes the scene document
     * @return True if closed successfully, false otherwise
     */
    bool Close();

private:
    core::UUID m_ID;
    std::shared_ptr<Scene> m_Scene;
    std::string m_FilePath;
    bool m_IsDirty;
};

} // namespace engine
} // namespace lupine
