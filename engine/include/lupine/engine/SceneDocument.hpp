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
 * The kind of document a SceneDocument represents.
 * A Scene document round-trips to a .scene file; a Prefab document edits a
 * single-root node tree that round-trips to a .prefab file. Both share the
 * exact same in-editor representation (a Scene with a root node), so all the
 * editing tooling is identical - only the load/save format differs.
 */
enum class DocumentKind {
    Scene,
    Prefab
};

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
     * Gets the kind of document (Scene or Prefab)
     * @return The document kind
     */
    DocumentKind GetKind() const { return m_Kind; }

    /**
     * Sets the kind of document (controls the load/save file format)
     * @param kind The document kind
     */
    void SetKind(DocumentKind kind) { m_Kind = kind; }

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
     * Opens a prefab from a .prefab file as an editable document.
     * The prefab's node tree is instantiated (preserving its UUIDs) into a
     * fresh Scene so it can be edited with the normal scene tooling. The
     * resulting document has kind Prefab, so Save() writes the .prefab format.
     * @param prefabPath Path to the .prefab file
     * @return Shared pointer to the scene document, or nullptr if failed
     */
    static std::shared_ptr<SceneDocument> OpenPrefab(const std::string& prefabPath);

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
    /**
     * Writes the document's root node tree to a .prefab file.
     * @param filePath Destination path
     * @return True if saved successfully, false otherwise
     */
    bool SaveAsPrefab(const std::string& filePath);

    core::UUID m_ID;
    std::shared_ptr<Scene> m_Scene;
    std::string m_FilePath;
    bool m_IsDirty;
    DocumentKind m_Kind = DocumentKind::Scene;
};

} // namespace engine
} // namespace lupine
