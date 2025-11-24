#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/core/Scene.hpp"
#include "SceneDocument.hpp"
#include "RecentProjects.hpp"
#include "EditorBridge.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace lupine {
namespace engine {

// Import types from core namespace
using core::Scene;
using core::Project;

/**
 * Editor session - manages the current editor state
 * Tracks the open project, open scenes, and active scene
 */
class EditorSession {
public:
    EditorSession();
    ~EditorSession();

    // ========================================================================
    // Project Management
    // ========================================================================

    /**
     * Creates a new project
     * @param projectPath Path where the project file will be created
     * @param projectName Name of the project
     * @return True if created successfully, false otherwise
     */
    bool CreateProject(const std::string& projectPath, const std::string& projectName);

    /**
     * Opens an existing project
     * @param projectPath Path to the project file
     * @return True if opened successfully, false otherwise
     */
    bool OpenProject(const std::string& projectPath);

    /**
     * Closes the current project
     * @param saveOpenScenes If true, saves all open scenes before closing
     * @return True if closed successfully, false otherwise
     */
    bool CloseProject(bool saveOpenScenes = true);

    /**
     * Checks if a project is currently open
     * @return True if a project is open, false otherwise
     */
    bool HasOpenProject() const { return m_Project != nullptr; }

    /**
     * Gets the current open project
     * @return Shared pointer to the project, or nullptr if no project is open
     */
    std::shared_ptr<Project> GetProject() const { return m_Project; }

    /**
     * Gets the recent projects manager
     * @return Reference to the recent projects manager
     */
    RecentProjects& GetRecentProjects() { return m_RecentProjects; }
    const RecentProjects& GetRecentProjects() const { return m_RecentProjects; }

    // ========================================================================
    // Scene Management
    // ========================================================================

    /**
     * Creates a new scene document
     * @param sceneName Name for the new scene
     * @return Shared pointer to the new scene document, or nullptr if failed
     */
    std::shared_ptr<SceneDocument> CreateScene(const std::string& sceneName = "New Scene");

    /**
     * Opens a scene from a file
     * @param scenePath Path to the scene file
     * @return Shared pointer to the scene document, or nullptr if failed
     */
    std::shared_ptr<SceneDocument> OpenScene(const std::string& scenePath);

    /**
     * Closes a scene document
     * @param sceneID ID of the scene document to close
     * @param saveIfDirty If true, saves the scene if it has unsaved changes
     * @return True if closed successfully, false otherwise
     */
    bool CloseScene(const core::UUID& sceneID, bool saveIfDirty = true);

    /**
     * Closes all open scenes
     * @param saveIfDirty If true, saves scenes that have unsaved changes
     * @return True if all scenes closed successfully, false otherwise
     */
    bool CloseAllScenes(bool saveIfDirty = true);

    /**
     * Saves a scene document
     * @param sceneID ID of the scene document to save
     * @return True if saved successfully, false otherwise
     */
    bool SaveScene(const core::UUID& sceneID);

    /**
     * Saves a scene document to a new path
     * @param sceneID ID of the scene document to save
     * @param newPath New file path
     * @return True if saved successfully, false otherwise
     */
    bool SaveSceneAs(const core::UUID& sceneID, const std::string& newPath);

    /**
     * Saves all open scenes
     * @return True if all scenes saved successfully, false otherwise
     */
    bool SaveAllScenes();

    /**
     * Gets a scene document by ID
     * @param sceneID ID of the scene document
     * @return Shared pointer to the scene document, or nullptr if not found
     */
    std::shared_ptr<SceneDocument> GetSceneDocument(const core::UUID& sceneID) const;

    /**
     * Gets a scene document by file path
     * @param scenePath Path to the scene file
     * @return Shared pointer to the scene document, or nullptr if not found
     */
    std::shared_ptr<SceneDocument> GetSceneDocumentByPath(const std::string& scenePath) const;

    /**
     * Gets all open scene documents
     * @return Vector of shared pointers to scene documents
     */
    std::vector<std::shared_ptr<SceneDocument>> GetOpenScenes() const;

    /**
     * Checks if a scene is currently open
     * @param sceneID ID of the scene document
     * @return True if the scene is open, false otherwise
     */
    bool IsSceneOpen(const core::UUID& sceneID) const;

    /**
     * Checks if a scene file is currently open
     * @param scenePath Path to the scene file
     * @return True if the scene is open, false otherwise
     */
    bool IsSceneOpenByPath(const std::string& scenePath) const;

    // ========================================================================
    // Active Scene Management
    // ========================================================================

    /**
     * Sets the active scene
     * @param sceneID ID of the scene document to set as active
     * @return True if set successfully, false otherwise
     */
    bool SetActiveScene(const core::UUID& sceneID);

    /**
     * Gets the active scene document
     * @return Shared pointer to the active scene document, or nullptr if none
     */
    std::shared_ptr<SceneDocument> GetActiveSceneDocument() const;

    /**
     * Gets the active scene
     * @return Shared pointer to the active scene, or nullptr if none
     */
    std::shared_ptr<Scene> GetActiveScene() const;

    /**
     * Checks if there is an active scene
     * @return True if there is an active scene, false otherwise
     */
    bool HasActiveScene() const { return m_ActiveSceneID.IsValid(); }

    /**
     * Gets the ID of the active scene
     * @return UUID of the active scene, or invalid UUID if none
     */
    const core::UUID& GetActiveSceneID() const { return m_ActiveSceneID; }

    // ========================================================================
    // Session Management
    // ========================================================================

    /**
     * Initializes the editor session
     * @return True if initialized successfully, false otherwise
     */
    bool Initialize();

    /**
     * Shuts down the editor session
     */
    void Shutdown();

    /**
     * Checks if the session is initialized
     * @return True if initialized, false otherwise
     */
    bool IsInitialized() const { return m_Initialized; }

    // ========================================================================
    // Editor Bridge Access
    // ========================================================================

    /**
     * Gets the editor bridge instance
     * @return Pointer to the editor bridge, or nullptr if not initialized
     */
    EditorBridge* GetEditorBridge() { return m_EditorBridge.get(); }

    /**
     * Gets the editor bridge instance (const)
     * @return Pointer to the editor bridge, or nullptr if not initialized
     */
    const EditorBridge* GetEditorBridge() const { return m_EditorBridge.get(); }

private:
    // Project state
    std::shared_ptr<Project> m_Project;
    RecentProjects m_RecentProjects;

    // Scene state
    std::map<core::UUID, std::shared_ptr<SceneDocument>> m_OpenScenes;
    core::UUID m_ActiveSceneID;

    // Session state
    bool m_Initialized;

    // Editor bridge
    std::unique_ptr<EditorBridge> m_EditorBridge;

    // Helper methods
    void AddSceneDocument(std::shared_ptr<SceneDocument> sceneDoc);
    void RemoveSceneDocument(const core::UUID& sceneID);
};

} // namespace engine
} // namespace lupine
