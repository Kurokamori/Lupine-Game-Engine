#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/rendering/Rendering.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/Command.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/rendering/GizmoRenderer.hpp"
#include "lupine/rendering/backends/opengl/OpenGLGizmoRenderer.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "VoxelBuilder.hpp"
#include "TileMap25DBuilder.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>

namespace lupine {

// Forward declarations
namespace math {
    class Ray;
}

namespace engine {

// Import types from core namespace
using core::Scene;
using core::Node;
using core::Component;
using core::Prefab;

/**
 * TypeInfo - Information about a registered type (Node, Component, or Prefab)
 */
struct TypeInfo {
    std::string typeName;
    std::string category;      // "Node", "Component", "Prefab"
    std::string subcategory;   // "2D", "3D", "UI", "Physics", "Rendering", etc. (Unity-style)
    std::string filePath;      // For user-defined types
    bool isBuiltIn;

    TypeInfo() : isBuiltIn(false) {}
    TypeInfo(const std::string& name, const std::string& cat, bool builtIn = false)
        : typeName(name), category(cat), subcategory(""), isBuiltIn(builtIn) {}
    TypeInfo(const std::string& name, const std::string& cat, const std::string& subcat, bool builtIn = false)
        : typeName(name), category(cat), subcategory(subcat), isBuiltIn(builtIn) {}
};

/**
 * View mode for editor viewports
 */
enum class ViewMode {
    View2D,  // 2D orthographic view
    View3D   // 3D perspective view
};

/**
 * RenderViewInfo - Information about a render view for the editor
 */
struct RenderViewInfo {
    RenderViewID viewID;
    SwapchainHandle swapchain;
    uint32_t width;
    uint32_t height;
    bool isValid;
    ViewMode viewMode;
    std::shared_ptr<Scene> scene;  // Associated scene for this view
    bool debugRenderingEnabled;    // Grid, gizmos, etc.
    bool respectFloorEnabled;      // Whether Y=0 floor constraint is active
    bool respectFloorPositiveSide; // True if camera is on positive Y side when enabled

    RenderViewInfo() : viewID(0), width(0), height(0), isValid(false), viewMode(ViewMode::View3D), debugRenderingEnabled(true), 
                       respectFloorEnabled(false), respectFloorPositiveSide(true) {}
};

/**
 * EditorBridge - Bridge between Core/Engine and Editor
 * 
 * Provides functionality for:
 * - Rendering to PyQt widgets via window handles
 * - Scene tree loading and active scene management
 * - Type discovery (Nodes, Components, Prefabs)
 * - Node/Component editing with dirty tracking
 */
class EditorBridge {
public:
    EditorBridge();
    ~EditorBridge();
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /**
     * Initialize the editor bridge with a graphics backend
     */
    bool Initialize(GraphicsBackend backend = GraphicsBackend::OpenGL);
    
    /**
     * Shutdown the editor bridge
     */
    void Shutdown();
    
    bool IsInitialized() const { return m_Initialized; }

    /**
     * Get the current graphics backend
     */
    GraphicsBackend GetBackend() const { return m_Backend; }

    // ========================================================================
    // Rendering Management
    // ========================================================================
    
    /**
     * Create a render view attached to a window handle (e.g., PyQt widget)
     * @param windowHandle Platform-specific window handle (HWND, winId, etc.)
     * @param width Width of the render surface
     * @param height Height of the render surface
     * @return RenderViewID for the created view
     */
    RenderViewID CreateRenderView(void* windowHandle, uint32_t width, uint32_t height);
    
    /**
     * Destroy a render view
     */
    void DestroyRenderView(RenderViewID viewID);
    
    /**
     * Resize a render view
     */
    void ResizeRenderView(RenderViewID viewID, uint32_t width, uint32_t height);
    
    /**
     * Render a specific view
     */
    void RenderView(RenderViewID viewID);
    
    /**
     * Get render view information
     */
    RenderViewInfo GetRenderViewInfo(RenderViewID viewID) const;

    /**
     * Set the view mode for a render view (2D or 3D)
     */
    void SetViewMode(RenderViewID viewID, ViewMode mode);

    /**
     * Get the view mode for a render view
     */
    ViewMode GetViewMode(RenderViewID viewID) const;

    /**
     * Set the scene to render for a specific view
     */
    void SetViewScene(RenderViewID viewID, std::shared_ptr<Scene> scene);

    /**
     * Get the scene associated with a view
     */
    std::shared_ptr<Scene> GetViewScene(RenderViewID viewID) const;

    /**
     * Set clear color for a render view
     */
    void SetViewClearColor(RenderViewID viewID, const math::Color& color);

    /**
     * Enable/disable debug rendering (grid, gizmos) for a view
     */
    void SetViewDebugRenderingEnabled(RenderViewID viewID, bool enabled);

    /**
     * Enable/disable grid rendering specifically for a view
     */
    void SetViewGridRenderingEnabled(RenderViewID viewID, bool enabled);

    /**
     * Enable/disable camera preview (bounds) rendering for a view (2D mode)
     */
    void SetViewCameraPreviewEnabled(RenderViewID viewID, bool enabled);

    /**
     * Enable/disable collision shapes rendering for a view
     */
    void SetViewCollisionShapesEnabled(RenderViewID viewID, bool enabled);

    /**
     * Enable/disable previewing the scene camera's stacked CameraEffect components in
     * the editor viewport. When on, the active scene camera that carries effects is
     * linked to this editor view so its effect chain (blur/glow/outline/...) is applied
     * to the editor render. The editor still navigates with its own free camera.
     */
    void SetViewCameraEffectsPreview(RenderViewID viewID, bool enabled);
    bool GetViewCameraEffectsPreview(RenderViewID viewID) const;

    /**
     * Set project settings for camera bounds visualization
     */
    void SetProjectSettings(uint32_t windowWidth, uint32_t windowHeight);

    /**
     * Set texture filtering mode for the editor viewport
     */
    void SetTextureFiltering(FilterMode minFilter, FilterMode magFilter);

    /**
     * Get the RenderWorld instance
     */
    RenderWorld* GetRenderWorld() { return m_RenderWorld.get(); }

    // ========================================================================
    // Camera Manipulation (for editor viewport controls)
    // ========================================================================

    /**
     * Pan the camera in 2D view
     * @param viewID The render view ID
     * @param deltaX Pan delta in screen space X
     * @param deltaY Pan delta in screen space Y
     */
    void PanCamera2D(RenderViewID viewID, float deltaX, float deltaY);

    /**
     * Zoom the camera in 2D view
     * @param viewID The render view ID
     * @param delta Zoom delta (positive = zoom in, negative = zoom out)
     */
    void ZoomCamera2D(RenderViewID viewID, float delta);

    /**
     * Pan the camera in 3D view
     * @param viewID The render view ID
     * @param deltaX Pan delta in screen space X
     * @param deltaY Pan delta in screen space Y
     */
    void PanCamera3D(RenderViewID viewID, float deltaX, float deltaY);

    /**
     * Orbit the camera in 3D view around the target
     * @param viewID The render view ID
     * @param deltaX Horizontal orbit delta (yaw)
     * @param deltaY Vertical orbit delta (pitch)
     */
    void OrbitCamera3D(RenderViewID viewID, float deltaX, float deltaY);

    /**
     * Zoom the camera in 3D view (move closer/farther from target)
     * @param viewID The render view ID
     * @param delta Zoom delta (positive = zoom in, negative = zoom out)
     */
    void ZoomCamera3D(RenderViewID viewID, float delta);

    /**
     * Set whether the camera respects Y=0 as a floor
     * @param viewID The render view ID
     * @param enabled Whether to enable the floor constraint
     */
    void SetRespectFloor(RenderViewID viewID, bool enabled);

    /**
     * Get whether the camera respects Y=0 as a floor
     * @param viewID The render view ID
     * @return True if floor constraint is enabled
     */
    bool GetRespectFloor(RenderViewID viewID) const;
    
    // ========================================================================
    // Scene Management
    // ========================================================================
    
    /**
     * Load a scene from file
     */
    std::shared_ptr<Scene> LoadScene(const std::string& scenePath);
    
    /**
     * Set the active scene
     */
    void SetActiveScene(std::shared_ptr<Scene> scene);
    
    /**
     * Get the active scene
     */
    std::shared_ptr<Scene> GetActiveScene() const { return m_ActiveScene; }
    
    /**
     * Save the active scene
     */
    bool SaveActiveScene();
    
    /**
     * Check if active scene has unsaved changes
     */
    bool IsActiveSceneDirty() const { return m_SceneDirty; }
    
    /**
     * Mark the active scene as dirty
     */
    void MarkSceneDirty() { m_SceneDirty = true; }

    /**
     * Set the active scene dirty flag explicitly. Used by the editor to clear
     * the flag after a document-level save (which does not run through the
     * bridge) so the dirty state does not linger.
     */
    void SetSceneDirty(bool dirty) { m_SceneDirty = dirty; }

    // ========================================================================
    // Type Discovery and Registration
    // ========================================================================

    /**
     * Scan and register user-defined types from project folders
     * @param projectPath Path to the project root
     */
    void ScanProjectTypes(const std::string& projectPath);

    /**
     * Get all registered node types (built-in + user-defined)
     */
    std::vector<TypeInfo> GetNodeTypes() const;

    /**
     * Get all registered component types (built-in + user-defined)
     */
    std::vector<TypeInfo> GetComponentTypes() const;

    /**
     * Get all registered prefab types (built-in + user-defined)
     */
    std::vector<TypeInfo> GetPrefabTypes() const;

    /**
     * Load a prefab from file
     */
    std::shared_ptr<Prefab> LoadPrefab(const std::string& prefabPath);

    // ========================================================================
    // Archetypes
    // ========================================================================

    /**
     * Get a summary of all registered archetype definitions.
     * Returns a JSON array of objects:
     *   { archetype_class, menu_path, icon, description, source, source_path }
     * Used to build the editor's Create menu.
     */
    nlohmann::json GetArchetypeDefinitions();

    /**
     * Get the full schema (own PropertyDescriptor fields) of one archetype definition.
     * Returns the .archetype JSON shape, or null if not found.
     */
    nlohmann::json GetArchetypeDefinition(const std::string& className);

    /**
     * Get the effective (merged with inherited) field descriptors for an archetype,
     * as a JSON array of serialized PropertyDescriptors.
     */
    nlohmann::json GetArchetypeEffectiveFields(const std::string& className);

    /**
     * Create a new archetype instance asset (.ares) populated with the
     * definition's default field values.
     * @param className The archetype definition class name
     * @param path Filesystem (or res://) path for the new .ares file
     */
    bool CreateArchetypeInstance(const std::string& className, const std::string& path);

    /**
     * Load an archetype instance for editing.
     * Returns { archetype_class, definition, properties, property_metadata }
     * matching the shape consumed by the inspector.
     */
    nlohmann::json LoadArchetypeInstance(const std::string& path);

    /**
     * Save edited field values into an archetype instance, preserving any
     * stored values not described by the definition.
     * @param path Path to the .ares file
     * @param valuesJson JSON object mapping field name to value
     */
    bool SaveArchetypeInstance(const std::string& path, const nlohmann::json& valuesJson);

    /**
     * Write a data-defined archetype definition (.archetype) from a schema.
     * @param path Filesystem (or res://) path for the .archetype file
     * @param schemaJson The .archetype schema JSON
     */
    bool CreateArchetypeDefinition(const std::string& path, const nlohmann::json& schemaJson);

    /**
     * Read a data-defined archetype definition file (for the schema editor).
     */
    nlohmann::json LoadArchetypeDefinitionFile(const std::string& path);

    /**
     * Rescan the project for archetype definitions (hot-reload).
     */
    void RescanArchetypes();

    // ========================================================================
    // Interface types (capability contracts; see InterfaceRegistry)
    // ========================================================================

    /**
     * Summary of all registered interface definitions. JSON array of objects:
     *   { interface_name, description, extends, tags, source, source_path,
     *     method_count, signal_count }
     * Used to populate the interface browser and the archetype/inspector pickers.
     */
    nlohmann::json GetInterfaceDefinitions();

    /**
     * Full serialized definition of one interface (the .interface JSON shape),
     * or null if not found.
     */
    nlohmann::json GetInterfaceDefinition(const std::string& interfaceName);

    /**
     * Write a data-defined interface definition (.interface) from a schema.
     * @param path Filesystem (or res://) path for the .interface file
     * @param schemaJson The .interface schema JSON
     */
    bool CreateInterfaceDefinition(const std::string& path, const nlohmann::json& schemaJson);

    /**
     * Read a data-defined interface definition file (for the schema editor).
     */
    nlohmann::json LoadInterfaceDefinitionFile(const std::string& path);

    /**
     * Rescan the project for interface definitions (hot-reload).
     */
    void RescanInterfaces();

    /**
     * The interfaces a node implements plus a conformance report per interface:
     *   { declared:[...], implemented:[...],
     *     conformance:[ {interface, exists, conforms, missing_methods, missing_signals}, ... ] }
     */
    nlohmann::json GetNodeInterfaces(std::shared_ptr<Node> node);

    /**
     * Verify a node against one interface's contract:
     *   { interface, exists, conforms, missing_methods, missing_signals }
     */
    nlohmann::json VerifyNodeInterface(std::shared_ptr<Node> node, const std::string& interfaceName);

    /** The interfaces an archetype class implements (across its base chain). */
    nlohmann::json GetArchetypeInterfaces(const std::string& className);

    /** Names of every node/archetype/type implementing an interface, grouped:
     *   { nodes:[paths...], archetypes:[classes...], component_types:[types...] } */
    nlohmann::json GetInterfaceImplementers(const std::string& interfaceName);

    /**
     * Nodes in the current scene matching a class name, for the inspector's
     * NodeType / ScriptClass reference pickers. A node matches when its own type
     * name, any of its component type names, or a script component's class/display
     * name equals className (className empty -> every node).
     * Returns a JSON array of { path, name, type }.
     */
    nlohmann::json FindNodesByClass(const std::string& className);

    /**
     * Archetype instance assets (.ares) in the project whose archetype class is, or
     * derives from, className, for the inspector's ArchetypeType reference picker
     * (className empty -> every instance).
     * Returns a JSON array of { path, display, archetype_class }.
     */
    nlohmann::json FindArchetypeInstances(const std::string& className);

    // ========================================================================
    // Localization (editor preview)
    // ========================================================================

    /**
     * Reload localization config + tables from disk, preserving the current
     * preview locale. Call after the localization editor saves changes so the
     * viewport reflects them.
     */
    void ReloadLocalization();

    /**
     * Reload the active UI theme + palette from disk (clears the theme cache and
     * bumps the theme version so UI components re-resolve). Call after the UI Theme
     * editor saves changes so the viewport reflects them live.
     */
    void ReloadTheme();

    /**
     * Set the project's configured default theme (a res:// .uitheme path, or empty
     * for the convention fallback). EditorSession pushes this from project settings
     * so ScanProjectTypes/ReloadTheme load the right theme.
     */
    void SetDefaultThemePath(const std::string& resPath) { m_DefaultThemePath = resPath; }

    /**
     * Set the locale used to resolve Localization Key fields in the viewport.
     * Does not persist (editor preview only).
     */
    void SetPreviewLocale(const std::string& locale);

    /**
     * Get the current preview locale.
     */
    std::string GetLocalizationLocale();

    /**
     * Get the union of configured locales and locales found in tables.
     */
    std::vector<std::string> GetLocalizationLocales();

    // ========================================================================
    // Node and Component Editing
    // ========================================================================

    /**
     * Get a node from the active scene by UUID
     */
    std::shared_ptr<Node> GetNode(const core::UUID& nodeUUID);

    /**
     * Get a node from the active scene by path
     */
    std::shared_ptr<Node> GetNodeByPath(const std::string& path);

    /**
     * Get the root node of the active scene
     */
    std::shared_ptr<Node> GetRootNode();

    /**
     * Create a new node of the specified type
     */
    std::shared_ptr<Node> CreateNode(const std::string& typeName, const std::string& nodeName = "");

    /**
     * Add a node to the active scene
     */
    bool AddNode(std::shared_ptr<Node> node, std::shared_ptr<Node> parent = nullptr);

    /**
     * Remove a node from the active scene
     */
    bool RemoveNode(std::shared_ptr<Node> node);

    /**
     * Reparent a node
     */
    bool ReparentNode(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent);

    /**
     * Move a node to a new parent and/or position (drag-and-drop reorder).
     * targetIndex is the desired final index within newParent's child list;
     * pass -1 to append. Undoable; restores the original parent and index.
     */
    bool MoveNode(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent, int targetIndex);

    /**
     * Get all children of a node
     */
    std::vector<std::shared_ptr<Node>> GetChildren(std::shared_ptr<Node> node);

    /**
     * Create a component of the specified type
     */
    std::shared_ptr<Component> CreateComponent(const std::string& typeName);

    /**
     * Add a component to a node
     */
    bool AddComponent(std::shared_ptr<Node> node, std::shared_ptr<Component> component);

    /**
     * Add a component to a node directly without creating an undo command
     */
    bool AddComponentDirect(std::shared_ptr<Node> node, std::shared_ptr<Component> component);

    /**
     * Remove a component from a node
     */
    bool RemoveComponent(std::shared_ptr<Node> node, std::shared_ptr<Component> component);

    /**
     * Get all components of a node
     */
    std::vector<std::shared_ptr<Component>> GetComponents(std::shared_ptr<Node> node);

    /**
     * Get node property value as JSON
     */
    nlohmann::json GetNodeProperty(std::shared_ptr<Node> node, const std::string& propertyName);

    /**
     * Set node property value from JSON
     */
    bool SetNodeProperty(std::shared_ptr<Node> node, const std::string& propertyName, const nlohmann::json& value);

    /**
     * Get all properties of a node as JSON
     */
    nlohmann::json GetNodeProperties(std::shared_ptr<Node> node);

    /**
     * Get component property value as JSON
     */
    nlohmann::json GetComponentProperty(std::shared_ptr<Component> component, const std::string& propertyName);

    /**
     * Set component property value from JSON
     */
    bool SetComponentProperty(std::shared_ptr<Component> component, const std::string& propertyName, const nlohmann::json& value);

    /**
     * Set component property value directly without creating a command (for duplication/copy-paste)
     */
    bool SetComponentPropertyDirect(std::shared_ptr<Component> component, const std::string& propertyName, const nlohmann::json& value);

    /**
     * Set node property value directly without creating a command (for duplication/copy-paste)
     */
    bool SetNodePropertyDirect(std::shared_ptr<Node> node, const std::string& propertyName, const nlohmann::json& value);

    /**
     * Deserialize component data directly without triggering individual property changes (for duplication/copy-paste)
     */
    bool DeserializeComponentDirect(std::shared_ptr<Component> component, const nlohmann::json& componentData);

    /**
     * Get all properties of a component as JSON
     */
    nlohmann::json GetComponentProperties(std::shared_ptr<Component> component);

    // ===== Animation editor support =====

    /**
     * Invoke a generic component control method (Component::CallMethod). Drives the
     * AnimationPlayer / AnimationTree editor tools (play/sample_at/set params/...).
     */
    nlohmann::json CallComponentMethod(std::shared_ptr<Component> component,
                                       const std::string& method, const nlohmann::json& args);

    /**
     * Apply an in-memory animation clip's pose at an absolute time to a node, for
     * timeline scrubbing of unsaved edits. Returns the affected targets.
     */
    nlohmann::json PreviewAnimationClip(std::shared_ptr<Node> node, const nlohmann::json& clipJson, float time);

    /**
     * Capture the current values of a set of channel targets so a preview can be
     * restored non-destructively. Returns [{node, channel, valueType, value}].
     */
    nlohmann::json CaptureAnimationPose(std::shared_ptr<Node> node, const nlohmann::json& targetsJson);

    /**
     * Restore previously captured channel values to a node (end of preview).
     */
    void RestoreAnimationPose(std::shared_ptr<Node> node, const nlohmann::json& capturedJson);

    // ===== Signals =====

    /**
     * List all signals available on a node, aggregated from the node's built-in
     * signals and every attached component. Returns a JSON array of objects:
     *   { "source": "node"|"<component_uuid>", "source_label": "Node"|"<TypeName>",
     *     "signal": "<name>", "args": [ { "name": "...", "type": "..." }, ... ] }
     */
    nlohmann::json GetNodeSignals(std::shared_ptr<Node> node);

    /**
     * List the declarative (object+method) connections originating from a node
     * and its components. Returns a JSON array of objects:
     *   { "source": "node"|"<component_uuid>", "signal": "...",
     *     "target_uuid": "...", "target_path": "...", "method": "...", "flags": N }
     */
    nlohmann::json GetNodeConnections(std::shared_ptr<Node> node);

    /**
     * Create a signal connection. sourceComponentUuid is empty to connect a
     * node-level signal, or the UUID of one of the node's components. The handler
     * method resolves to a function on the target node's script(s). Marks the
     * scene dirty. Returns true on success.
     */
    bool AddConnection(std::shared_ptr<Node> sourceNode, const std::string& sourceComponentUuid,
                       const std::string& signal, std::shared_ptr<Node> targetNode,
                       const std::string& method, uint32_t flags);

    /**
     * Remove a previously created connection (matched by source/signal/target/method).
     * Marks the scene dirty. Returns true on success.
     */
    bool RemoveConnection(std::shared_ptr<Node> sourceNode, const std::string& sourceComponentUuid,
                          const std::string& signal, std::shared_ptr<Node> targetNode,
                          const std::string& method);

    /**
     * Reload a script component's script file and re-parse its export properties
     * Returns true if the script was successfully reloaded
     */
    bool ReloadScriptComponent(std::shared_ptr<Component> component);

    /**
     * Reload an asset from disk and invalidate all caches that use it.
     * This triggers hot-reload for textures, models, fonts, audio, etc.
     * @param assetPath The path to the asset (can be res:// or physical path)
     * @return Number of components/caches that were affected
     */
    int ReloadAsset(const std::string& assetPath);

    // ========================================================================
    // StaticMesh3D Material Slot Management
    // ========================================================================

    /**
     * Get the number of material slots for a StaticMesh3D component
     */
    uint32_t GetMaterialSlotCount(std::shared_ptr<Component> component);

    /**
     * Get material slot properties as JSON
     */
    nlohmann::json GetMaterialSlotProperties(std::shared_ptr<Component> component, uint32_t slotIndex);

    /**
     * Set material slot property value from JSON
     */
    bool SetMaterialSlotProperty(std::shared_ptr<Component> component, uint32_t slotIndex, const std::string& propertyName, const nlohmann::json& value);

    /**
     * Instantiate a prefab in the active scene
     */
    std::shared_ptr<Node> InstantiatePrefab(std::shared_ptr<Prefab> prefab, std::shared_ptr<Node> parent = nullptr);

    // ========================================================================
    // Viewport Picking
    // ========================================================================

    /**
     * Pick a node at the given screen coordinates
     * @param viewID The render view ID
     * @param screenX Screen X coordinate (in pixels)
     * @param screenY Screen Y coordinate (in pixels)
     * @return The topmost node at the given position, or nullptr if no node was found
     */
    std::shared_ptr<Node> PickNodeAtScreenPosition(RenderViewID viewID, float screenX, float screenY);

    // ========================================================================
    // Node Selection
    // ========================================================================

    /**
     * Set the selected node for a render view
     * @param viewID The render view ID
     * @param node The node to select (nullptr to clear selection)
     */
    void SetSelectedNode(RenderViewID viewID, std::shared_ptr<Node> node);

    /**
     * Get the selected node for a render view
     * @param viewID The render view ID
     * @return The currently selected node (nullptr if none)
     */
    std::shared_ptr<lupine::core::Node> GetSelectedNode(RenderViewID viewID) const;

    /**
     * Set multiple selected nodes for a render view
     * @param viewID The render view ID
     * @param nodes The nodes to select (empty to clear selection)
     */
    void SetSelectedNodes(RenderViewID viewID, const std::vector<std::shared_ptr<lupine::core::Node>>& nodes);

    /**
     * Get all selected nodes for a render view
     * @param viewID The render view ID
     * @return The currently selected nodes (empty if none)
     */
    std::vector<std::shared_ptr<lupine::core::Node>> GetSelectedNodes(RenderViewID viewID) const;

    /**
     * Enable/disable gizmo rendering for a render view
     * @param viewID The render view ID
     * @param enabled Whether gizmos should be rendered
     */
    void SetGizmoEnabled(RenderViewID viewID, bool enabled);

    /**
     * Check if gizmo rendering is enabled for a render view
     * @param viewID The render view ID
     * @return Whether gizmos are enabled
     */
    bool IsGizmoEnabled(RenderViewID viewID) const;

    /**
     * Set the gizmo type for a render view
     * @param viewID The render view ID
     * @param type The gizmo type (Translation, Rotation, Scale, All)
     */
    void SetGizmoType(RenderViewID viewID, GizmoType type);

    /**
     * Set the transform space for gizmo operations
     * @param viewID The render view ID
     * @param space The transform space (Local, Parent, World)
     */
    void SetTransformSpace(RenderViewID viewID, TransformSpace space);

    /**
     * Get the transform space for gizmo operations
     * @param viewID The render view ID
     * @return The current transform space
     */
    TransformSpace GetTransformSpace(RenderViewID viewID) const;

    // ========================================================================
    // Undo/Redo System
    // ========================================================================

    /**
     * Execute a command and add it to the undo history
     * @param command The command to execute
     * @return true if successful
     */
    bool ExecuteCommand(std::shared_ptr<core::Command> command);

    /**
     * Record a command in the undo history without executing it (for commands already executed)
     * @param command The command to record
     */
    void RecordCommand(std::shared_ptr<core::Command> command);

    /**
     * Enable or disable undo/redo command recording. When disabled, RecordCommand
     * becomes a no-op and the active scene is NOT marked dirty. Modal editor tools
     * (e.g. the UI Theme preview) drive the gizmo against a detached scene and must
     * not pollute the real undo history or falsely dirty the user's open scene.
     */
    void SetUndoRecordingEnabled(bool enabled) { m_UndoRecordingEnabled = enabled; }
    bool IsUndoRecordingEnabled() const { return m_UndoRecordingEnabled; }

    /**
     * Create a composite command for grouping multiple operations
     * @param description Human-readable description of the composite operation
     * @return A new composite command
     */
    std::shared_ptr<core::CompositeCommand> CreateCompositeCommand(const std::string& description);

    /**
     * Create an AddNodeCommand without executing it (for use in composite commands)
     * @param node The node to add
     * @param parent The parent node (nullptr for root)
     * @return The command (not executed)
     */
    std::shared_ptr<core::Command> CreateAddNodeCommand(std::shared_ptr<Node> node, std::shared_ptr<Node> parent = nullptr);

    /**
     * Create an AddComponentCommand without executing it (for use in composite commands)
     * @param node The node to add the component to
     * @param component The component to add
     * @return The command (not executed)
     */
    std::shared_ptr<core::Command> CreateAddComponentCommand(std::shared_ptr<Node> node, std::shared_ptr<Component> component);

    /**
     * Create a SetNodePropertyCommand without executing it (for use in composite commands)
     * @param node The node to modify
     * @param propertyName The property name
     * @param value The new value (as JSON)
     * @return The command (not executed)
     */
    std::shared_ptr<core::Command> CreateSetNodePropertyCommand(std::shared_ptr<Node> node, const std::string& propertyName, const nlohmann::json& value);

    /**
     * Create a SetComponentPropertyCommand without executing it (for use in composite commands)
     * @param component The component to modify
     * @param propertyName The property name
     * @param value The new value (as JSON)
     * @return The command (not executed)
     */
    std::shared_ptr<core::Command> CreateSetComponentPropertyCommand(std::shared_ptr<Component> component, const std::string& propertyName, const nlohmann::json& value);

    /**
     * Undo the last command
     * @return true if successful
     */
    bool Undo();

    /**
     * Redo the last undone command
     * @return true if successful
     */
    bool Redo();

    /**
     * Check if undo is available
     */
    bool CanUndo() const;

    /**
     * Check if redo is available
     */
    bool CanRedo() const;

    /**
     * Get description of the command that would be undone
     */
    std::string GetUndoDescription() const;

    /**
     * Get description of the command that would be redone
     */
    std::string GetRedoDescription() const;

    /**
     * Clear all undo/redo history
     */
    void ClearHistory();

    /**
     * Set the maximum number of undo steps
     */
    void SetMaxUndoSteps(size_t maxSteps);

    /**
     * Get the maximum number of undo steps
     */
    size_t GetMaxUndoSteps() const;

    /**
     * Get the gizmo type for a render view
     * @param viewID The render view ID
     * @return The current gizmo type
     */
    GizmoType GetGizmoType(RenderViewID viewID) const;

    // ========================================================================
    // Audio Management
    // ========================================================================

    /**
     * Play an audio file (for editor audio preview)
     * @param filePath Path to the audio file
     * @param busName Audio bus to play on (default: "Master")
     * @param loop Whether to loop the audio
     * @param volume Volume (0.0 - 1.0)
     * @return UUID of the playing sound (can be used to stop it)
     */
    std::string PlayAudioFile(const std::string& filePath, const std::string& busName = "Master", bool loop = false, float volume = 1.0f);

    /**
     * Stop a playing audio source by UUID
     * @param sourceUUID UUID of the audio source (as string)
     */
    void StopAudio(const std::string& sourceUUID);

    /**
     * Stop all playing audio
     */
    void StopAllAudio();

    /**
     * Play all AudioPlayer components in a scene
     * @param scene The scene to play audio from
     */
    void PlaySceneAudio(std::shared_ptr<Scene> scene);

    /**
     * Stop all AudioPlayer components in a scene
     * @param scene The scene to stop audio from
     */
    void StopSceneAudio(std::shared_ptr<Scene> scene);

    /**
     * Set master audio volume
     * @param volume Volume (0.0 - 1.0)
     */
    void SetMasterVolume(float volume);

    /**
     * Get master audio volume
     * @return Volume (0.0 - 1.0)
     */
    float GetMasterVolume() const;

    /**
     * Set audio bus volume
     * @param busName Name of the bus ("Master", "SFX", "Music", "Ambient")
     * @param volume Volume (0.0 - 1.0)
     */
    void SetBusVolume(const std::string& busName, float volume);

    /**
     * Get audio bus volume
     * @param busName Name of the bus
     * @return Volume (0.0 - 1.0)
     */
    float GetBusVolume(const std::string& busName) const;

    // ========================================================================
    // Audio Mixer (buses + DSP effects)
    // ========================================================================

    /** Bus mute / solo state. */
    void SetBusMuted(const std::string& busName, bool muted);
    bool IsBusMuted(const std::string& busName) const;
    void SetBusSolo(const std::string& busName, bool solo);
    bool IsBusSolo(const std::string& busName) const;

    /** Bus lifecycle. */
    void CreateAudioBus(const std::string& busName, const std::string& parentBus);
    void DestroyAudioBus(const std::string& busName);

    /** Bus DSP effects (effectType is a name like "Reverb"; see GetAudioEffectTypesJson). */
    int AddBusEffect(const std::string& busName, const std::string& effectType);
    void RemoveBusEffect(const std::string& busName, int index);
    void MoveBusEffect(const std::string& busName, int fromIndex, int toIndex);
    void ClearBusEffects(const std::string& busName);
    void SetBusEffectEnabled(const std::string& busName, int index, bool enabled);
    void SetBusEffectParameter(const std::string& busName, int index,
                               const std::string& parameter, float value);

    /**
     * A full snapshot of the mixer as a JSON string: an array of buses, each
     * {name, volume, muted, solo, parentBus, effects:[{type, enabled,
     * parameters:{name:value}}]}. The mixer panel rebuilds its UI from this.
     */
    std::string GetAudioBusesJson() const;

    /** Available effect type names as a JSON array of strings. */
    std::string GetAudioEffectTypesJson() const;

    /**
     * Live VU levels for every bus as a JSON object {busName: [left, right]}
     * (linear peak). Polled by the mixer's meter timer.
     */
    std::string GetAudioBusLevelsJson() const;

    /** Serialize / restore the whole mixer state (for project persistence). */
    std::string SerializeAudioBuses() const;
    void DeserializeAudioBuses(const std::string& json);

    // ========================================================================
    // Gizmo Interaction
    // ========================================================================

    /**
     * Test if the gizmo is hit at a screen position
     * @param viewID The render view ID
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @return True if gizmo was hit
     */
    bool TestGizmoHit(RenderViewID viewID, float screenX, float screenY);

    /**
     * Start gizmo drag interaction
     * @param viewID The render view ID
     * @param screenX Screen X coordinate where drag started
     * @param screenY Screen Y coordinate where drag started
     */
    void BeginGizmoDrag(RenderViewID viewID, float screenX, float screenY);

    /**
     * Update gizmo drag interaction
     * @param viewID The render view ID
     * @param screenX Current screen X coordinate
     * @param screenY Current screen Y coordinate
     */
    void UpdateGizmoDrag(RenderViewID viewID, float screenX, float screenY, bool keepAspect = false);

    /**
     * End gizmo drag interaction
     * @param viewID The render view ID
     */
    void EndGizmoDrag(RenderViewID viewID);

    /**
     * Check if gizmo is currently being dragged
     * @param viewID The render view ID
     * @return True if dragging
     */
    bool IsGizmoDragging(RenderViewID viewID) const;

    // ========================================================================
    // CollisionBody2D Polygon Editing
    // ========================================================================

    /**
     * Convert screen coordinates to world coordinates for 2D view
     * @param viewID The render view ID
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @return World position as Vec2
     */
    math::Vec2 ScreenToWorld2D(RenderViewID viewID, float screenX, float screenY);

    /**
     * Cast a ray from screen coordinates into the 3D world
     * @param viewID The render view ID
     * @param screenX Screen X coordinate (in pixels)
     * @param screenY Screen Y coordinate (in pixels)
     * @param planeY Y-plane to intersect with (default 0 for ground plane)
     * @return World position where ray intersects the plane
     */
    math::Vec3 ScreenToWorld3D(RenderViewID viewID, float screenX, float screenY, float planeY = 0.0f);

    /**
     * Cast a ray from screen coordinates into the 3D world and intersect with an arbitrary plane
     * @param viewID The render view ID
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param planeNormalX Plane normal X component
     * @param planeNormalY Plane normal Y component
     * @param planeNormalZ Plane normal Z component
     * @param planePointX A point on the plane (X)
     * @param planePointY A point on the plane (Y)
     * @param planePointZ A point on the plane (Z)
     * @return World position where ray intersects the plane, or (0,0,0) if no intersection
     */
    math::Vec3 ScreenToWorldOnPlane(RenderViewID viewID, float screenX, float screenY,
                                     float planeNormalX, float planeNormalY, float planeNormalZ,
                                     float planePointX, float planePointY, float planePointZ);

    /**
     * Get the camera ray origin and direction from screen coordinates
     * @param viewID The render view ID
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param outOrigin Output ray origin
     * @param outDirection Output ray direction (normalized)
     * @return true if successful
     */
    bool GetCameraRay(RenderViewID viewID, float screenX, float screenY,
                      math::Vec3& outOrigin, math::Vec3& outDirection);

    /**
     * Add a vertex to a CollisionBody2D polygon
     * @param component The CollisionBody2D component
     * @param x World X coordinate
     * @param y World Y coordinate
     */
    void AddCollisionVertex(std::shared_ptr<core::Component> component, float x, float y);

    /**
     * Remove a vertex from a CollisionBody2D polygon
     * @param component The CollisionBody2D component
     * @param index Vertex index to remove
     */
    void RemoveCollisionVertex(std::shared_ptr<core::Component> component, int index);

    /**
     * Update a vertex position in a CollisionBody2D polygon
     * @param component The CollisionBody2D component
     * @param index Vertex index
     * @param x New world X coordinate
     * @param y New world Y coordinate
     */
    void UpdateCollisionVertex(std::shared_ptr<core::Component> component, int index, float x, float y);

    /**
     * Get all vertices from a CollisionBody2D polygon
     * @param component The CollisionBody2D component
     * @return JSON string containing array of vertices [{x, y}, ...]
     */
    std::string GetCollisionVertices(std::shared_ptr<core::Component> component);

    /**
     * Clear all vertices from a CollisionBody2D polygon
     * @param component The CollisionBody2D component
     */
    void ClearCollisionVertices(std::shared_ptr<core::Component> component);

    // ========================================================================
    // Line2D Point Editing
    // ========================================================================

    /**
     * Get all Line2D points with Bezier data as JSON
     * @param component The Line2D component
     * @return JSON string containing array of points with bezier data
     */
    std::string GetLine2DPoints(std::shared_ptr<core::Component> component);

    /**
     * Add a point to Line2D
     * @param component The Line2D component
     * @param x X coordinate
     * @param y Y coordinate
     */
    void AddLine2DPoint(std::shared_ptr<core::Component> component, float x, float y);

    /**
     * Add a point with Bezier controls to Line2D
     * @param component The Line2D component
     * @param x X coordinate
     * @param y Y coordinate
     * @param ctrlInX Control in X (relative)
     * @param ctrlInY Control in Y (relative)
     * @param ctrlOutX Control out X (relative)
     * @param ctrlOutY Control out Y (relative)
     * @param useBezier Whether to use Bezier curves
     */
    void AddLine2DPointWithBezier(std::shared_ptr<core::Component> component,
                                   float x, float y,
                                   float ctrlInX, float ctrlInY,
                                   float ctrlOutX, float ctrlOutY,
                                   bool useBezier);

    /**
     * Update a Line2D point position
     * @param component The Line2D component
     * @param index Point index
     * @param x New X coordinate
     * @param y New Y coordinate
     */
    void UpdateLine2DPoint(std::shared_ptr<core::Component> component, int index, float x, float y);

    /**
     * Update a Line2D point's Bezier control handles
     * @param component The Line2D component
     * @param index Point index
     * @param ctrlInX Control in X (relative)
     * @param ctrlInY Control in Y (relative)
     * @param ctrlOutX Control out X (relative)
     * @param ctrlOutY Control out Y (relative)
     * @param symmetric Whether handles are symmetric
     */
    void UpdateLine2DPointBezier(std::shared_ptr<core::Component> component, int index,
                                  float ctrlInX, float ctrlInY,
                                  float ctrlOutX, float ctrlOutY,
                                  bool symmetric);

    /**
     * Set whether a Line2D point uses Bezier curves
     * @param component The Line2D component
     * @param index Point index
     * @param useBezier Whether to use Bezier curves
     */
    void SetLine2DPointUseBezier(std::shared_ptr<core::Component> component, int index, bool useBezier);

    /**
     * Remove a point from Line2D
     * @param component The Line2D component
     * @param index Point index to remove
     */
    void RemoveLine2DPoint(std::shared_ptr<core::Component> component, int index);

    /**
     * Clear all points from Line2D
     * @param component The Line2D component
     */
    void ClearLine2DPoints(std::shared_ptr<core::Component> component);

    /**
     * Set whether to show Bezier handles in editor
     * @param component The Line2D component
     * @param show Whether to show handles
     */
    void SetLine2DShowHandles(std::shared_ptr<core::Component> component, bool show);

    /**
     * Set the selected point index for handle display
     * @param component The Line2D component
     * @param index Point index (-1 for none)
     */
    void SetLine2DSelectedPoint(std::shared_ptr<core::Component> component, int index);

    /**
     * Hit test for Line2D control points
     * @param component The Line2D component
     * @param worldX World X coordinate
     * @param worldY World Y coordinate
     * @param radius Hit radius
     * @return Control point ID or -1 if no hit
     */
    int HitTestLine2DControlPoint(std::shared_ptr<core::Component> component, float worldX, float worldY, float radius);

    /**
     * Move a Line2D control point
     * @param component The Line2D component
     * @param controlPointId Control point ID from hit test
     * @param worldX New world X coordinate
     * @param worldY New world Y coordinate
     */
    void MoveLine2DControlPoint(std::shared_ptr<core::Component> component, int controlPointId, float worldX, float worldY);

    // ========================================================================
    // Curve2D Point Editing (also works for Path2D)
    // ========================================================================

    /**
     * Get all Curve2D points with Bezier data as JSON
     * @param component The Curve2D or Path2D component
     * @return JSON string containing array of points with bezier data
     */
    std::string GetCurve2DPoints(std::shared_ptr<core::Component> component);

    /**
     * Add a point to Curve2D
     * @param component The Curve2D or Path2D component
     * @param x X coordinate
     * @param y Y coordinate
     */
    void AddCurve2DPoint(std::shared_ptr<core::Component> component, float x, float y);

    /**
     * Add a point with Bezier controls to Curve2D
     */
    void AddCurve2DPointWithBezier(std::shared_ptr<core::Component> component,
                                   float x, float y,
                                   float ctrlInX, float ctrlInY,
                                   float ctrlOutX, float ctrlOutY,
                                   bool useBezier);

    /**
     * Update a Curve2D point position
     */
    void UpdateCurve2DPoint(std::shared_ptr<core::Component> component, int index, float x, float y);

    /**
     * Update a Curve2D point's Bezier control handles
     */
    void UpdateCurve2DPointBezier(std::shared_ptr<core::Component> component, int index,
                                  float ctrlInX, float ctrlInY,
                                  float ctrlOutX, float ctrlOutY,
                                  bool symmetric);

    /**
     * Set whether a Curve2D point uses Bezier curves
     */
    void SetCurve2DPointUseBezier(std::shared_ptr<core::Component> component, int index, bool useBezier);

    /**
     * Remove a point from Curve2D
     */
    void RemoveCurve2DPoint(std::shared_ptr<core::Component> component, int index);

    /**
     * Clear all points from Curve2D
     */
    void ClearCurve2DPoints(std::shared_ptr<core::Component> component);

    /**
     * Set whether to show Bezier handles in editor
     */
    void SetCurve2DShowHandles(std::shared_ptr<core::Component> component, bool show);

    /**
     * Set the selected point index for handle display
     */
    void SetCurve2DSelectedPoint(std::shared_ptr<core::Component> component, int index);

    /**
     * Hit test for Curve2D control points
     */
    int HitTestCurve2DControlPoint(std::shared_ptr<core::Component> component, float worldX, float worldY, float radius);

    /**
     * Move a Curve2D control point
     */
    void MoveCurve2DControlPoint(std::shared_ptr<core::Component> component, int controlPointId, float worldX, float worldY);

    // ========================================================================
    // Curve3D Point Editing (also works for Path3D / PathFollow3D's target)
    // ========================================================================

    /** Get all Curve3D points (with bezier + tilt) as a JSON array string */
    std::string GetCurve3DPoints(std::shared_ptr<core::Component> component);

    /** Add a point to a Curve3D at a local-space position */
    void AddCurve3DPoint(std::shared_ptr<core::Component> component, float x, float y, float z);

    /** Update a Curve3D point's local-space position */
    void UpdateCurve3DPoint(std::shared_ptr<core::Component> component, int index, float x, float y, float z);

    /** Update a Curve3D point's bezier control handles (local-space, relative to the anchor) */
    void UpdateCurve3DPointBezier(std::shared_ptr<core::Component> component, int index,
                                  float ctrlInX, float ctrlInY, float ctrlInZ,
                                  float ctrlOutX, float ctrlOutY, float ctrlOutZ,
                                  bool symmetric);

    /** Set whether a Curve3D point uses bezier curves */
    void SetCurve3DPointUseBezier(std::shared_ptr<core::Component> component, int index, bool useBezier);

    /** Set a Curve3D point's tilt (roll about the tangent, radians) */
    void SetCurve3DPointTilt(std::shared_ptr<core::Component> component, int index, float tilt);

    /** Remove a point from a Curve3D */
    void RemoveCurve3DPoint(std::shared_ptr<core::Component> component, int index);

    /** Clear all points from a Curve3D */
    void ClearCurve3DPoints(std::shared_ptr<core::Component> component);

    /** Set the selected point index (drives which bezier handles are interactive) */
    void SetCurve3DSelectedPoint(std::shared_ptr<core::Component> component, int index);

    /** Show/hide bezier handles in the editor */
    void SetCurve3DShowHandles(std::shared_ptr<core::Component> component, bool show);

    /**
     * Hit-test Curve3D control points against a screen position by projecting
     * each editable point/handle to screen space.
     * @return encoded control-point id (pointIndex*10 + handleType: 0 anchor, 1 in, 2 out), or -1
     */
    int HitTestCurve3DControlPoint(std::shared_ptr<core::Component> component, RenderViewID viewID,
                                   float screenX, float screenY, float pixelRadius);

    /**
     * Move a Curve3D control point by intersecting the camera ray with the
     * view-facing plane through the point. Local-space conversion is automatic.
     */
    void MoveCurve3DControlPoint(std::shared_ptr<core::Component> component, int controlPointId,
                                 RenderViewID viewID, float screenX, float screenY);

    /**
     * Get the global position of a node (2D or 3D)
     * @param node The node to query
     * @return JSON string containing position {x, y} or {x, y, z}
     */
    std::string GetNodeGlobalPosition(std::shared_ptr<Node> node);

    /**
     * Get the global rotation of a node (2D or 3D)
     * @param node The node to query
     * @return Rotation value (float for 2D, returns first component for 3D quaternion)
     */
    float GetNodeGlobalRotation(std::shared_ptr<Node> node);

    // ========================================================================
    // Voxel Builder
    // ========================================================================

    /**
     * Create a voxel builder instance
     * @return VoxelBuilder ID (pointer cast to uint64_t)
     */
    uint64_t CreateVoxelBuilder();

    /**
     * Destroy a voxel builder instance
     * @param builderID The VoxelBuilder ID
     */
    void DestroyVoxelBuilder(uint64_t builderID);

    /**
     * Place a voxel in the builder
     * @param builderID The VoxelBuilder ID
     * @param x Voxel X coordinate
     * @param y Voxel Y coordinate
     * @param z Voxel Z coordinate
     * @param r Red component (0-1)
     * @param g Green component (0-1)
     * @param b Blue component (0-1)
     * @param a Alpha component (0-1)
     */
    void VoxelBuilderPlaceVoxel(uint64_t builderID, int32_t x, int32_t y, int32_t z, float r, float g, float b, float a);

    /**
     * Erase a voxel from the builder
     * @param builderID The VoxelBuilder ID
     * @param x Voxel X coordinate
     * @param y Voxel Y coordinate
     * @param z Voxel Z coordinate
     */
    void VoxelBuilderEraseVoxel(uint64_t builderID, int32_t x, int32_t y, int32_t z);

    /**
     * Clear all voxels from the builder
     * @param builderID The VoxelBuilder ID
     */
    void VoxelBuilderClear(uint64_t builderID);

    /**
     * Check if a voxel exists at the given position
     * @param builderID The VoxelBuilder ID
     * @param x Voxel X coordinate
     * @param y Voxel Y coordinate
     * @param z Voxel Z coordinate
     * @return true if voxel exists
     */
    bool VoxelBuilderHasVoxel(uint64_t builderID, int32_t x, int32_t y, int32_t z);

    /**
     * Get the number of voxels in the builder
     * @param builderID The VoxelBuilder ID
     * @return Number of voxels
     */
    size_t VoxelBuilderGetVoxelCount(uint64_t builderID);

    /**
     * Render the voxel builder to a view
     * @param builderID The VoxelBuilder ID
     * @param viewID The render view ID
     */
    void VoxelBuilderRender(uint64_t builderID, RenderViewID viewID);

    /**
     * Save voxel builder to JSON string
     * @param builderID The VoxelBuilder ID
     * @return JSON string
     */
    std::string VoxelBuilderToJSON(uint64_t builderID);

    /**
     * Load voxel builder from JSON string
     * @param builderID The VoxelBuilder ID
     * @param json JSON string
     * @return true if successful
     */
    bool VoxelBuilderFromJSON(uint64_t builderID, const std::string& json);

    /**
     * Export voxel builder to OBJ format
     * @param builderID The VoxelBuilder ID
     * @param mergeFaces If true, merge internal faces (only export visible external faces)
     * @param textureAtlas If true, use texture atlas for colors; if false, use vertex colors
     * @return OBJ file contents as string
     */
    std::string VoxelBuilderExportOBJ(uint64_t builderID, bool mergeFaces = false, bool textureAtlas = false);

    /**
     * Export voxel builder to glTF format
     * @param builderID The VoxelBuilder ID
     * @param mergeFaces If true, merge internal faces (only export visible external faces)
     * @param textureAtlas If true, use texture atlas for colors; if false, use vertex colors
     * @return glTF JSON as string
     */
    std::string VoxelBuilderExportGLTF(uint64_t builderID, bool mergeFaces = false, bool textureAtlas = false);

    // ========================================================================
    // TileMap 2.5D Builder
    // ========================================================================

    /**
     * Create a TileMap25D builder instance
     * @return TileMap25DBuilder ID (pointer cast to uint64_t)
     */
    uint64_t CreateTileMap25DBuilder();

    /**
     * Destroy a TileMap25D builder instance
     * @param builderID The TileMap25DBuilder ID
     */
    void DestroyTileMap25DBuilder(uint64_t builderID);

    /**
     * Add a face to the tilemap
     * @param builderID The TileMap25DBuilder ID
     * @param v0-v3 Vertex positions (x, y, z)
     * @param uv0-uv3 UV coordinates (u, v)
     * @param tilesetIndex Tileset index
     * @param tileIndex Tile index within tileset
     * @param twoSided Whether the face is two-sided
     * @return Face ID string
     */
    std::string TileMap25DAddFace(uint64_t builderID,
        float v0x, float v0y, float v0z,
        float v1x, float v1y, float v1z,
        float v2x, float v2y, float v2z,
        float v3x, float v3y, float v3z,
        float uv0u, float uv0v,
        float uv1u, float uv1v,
        float uv2u, float uv2v,
        float uv3u, float uv3v,
        int32_t tilesetIndex, int32_t tileIndex,
        bool twoSided = false);

    /**
     * Remove a face from the tilemap
     * @param builderID The TileMap25DBuilder ID
     * @param faceId The face ID to remove
     */
    void TileMap25DRemoveFace(uint64_t builderID, const std::string& faceId);

    /**
     * Clear all faces from the tilemap
     * @param builderID The TileMap25DBuilder ID
     */
    void TileMap25DClear(uint64_t builderID);

    /**
     * Get the number of faces in the tilemap
     * @param builderID The TileMap25DBuilder ID
     * @return Number of faces
     */
    size_t TileMap25DGetFaceCount(uint64_t builderID);

    /**
     * Set a vertex position for a face
     * @param builderID The TileMap25DBuilder ID
     * @param faceId The face ID
     * @param vertexIndex Vertex index (0-3)
     * @param x, y, z New position
     */
    void TileMap25DSetFaceVertex(uint64_t builderID, const std::string& faceId, int vertexIndex, float x, float y, float z);

    /**
     * Render the tilemap to a view
     * @param builderID The TileMap25DBuilder ID
     * @param viewID The render view ID
     */
    void TileMap25DRender(uint64_t builderID, RenderViewID viewID);

    /**
     * Save tilemap to JSON string
     * @param builderID The TileMap25DBuilder ID
     * @return JSON string
     */
    std::string TileMap25DToJSON(uint64_t builderID);

    /**
     * Load tilemap from JSON string
     * @param builderID The TileMap25DBuilder ID
     * @param json JSON string
     * @return true if successful
     */
    bool TileMap25DFromJSON(uint64_t builderID, const std::string& json);

    /**
     * Export tilemap to OBJ format
     * @param builderID The TileMap25DBuilder ID
     * @param mergeVertices If true, merge duplicate vertices
     * @param useTextureAtlas If true, use texture atlas UVs
     * @return OBJ file contents as string
     */
    std::string TileMap25DExportOBJ(uint64_t builderID, bool mergeVertices = true, bool useTextureAtlas = true);

    /**
     * Export tilemap material to MTL format
     * @param builderID The TileMap25DBuilder ID
     * @param useTextureAtlas If true, reference atlas texture
     * @return MTL file contents as string
     */
    std::string TileMap25DExportMTL(uint64_t builderID, bool useTextureAtlas = true);

    /**
     * Load a tileset texture
     * @param builderID The TileMap25DBuilder ID
     * @param tilesetIndex Index of the tileset to load
     * @return true if texture loaded successfully
     */
    bool TileMap25DLoadTilesetTexture(uint64_t builderID, int tilesetIndex);

    /**
     * Set texture for TileMap25D directly using a texture path
     * @param builderID The TileMap25DBuilder ID
     * @param texturePath Path to the texture file
     * @return true if texture loaded and set successfully
     */
    bool TileMap25DSetTexture(uint64_t builderID, const std::string& texturePath);

    /**
     * Unregister a TileMap25D builder from a view
     * @param builderID The TileMap25DBuilder ID
     * @param viewID The render view ID
     */
    void TileMap25DUnregister(uint64_t builderID, RenderViewID viewID);

    // ========================================================================
    // Debug Drawing
    // ========================================================================

    /**
     * Draw a debug line
     * @param viewID The render view ID
     * @param x1 Start X coordinate
     * @param y1 Start Y coordinate
     * @param z1 Start Z coordinate
     * @param x2 End X coordinate
     * @param y2 End Y coordinate
     * @param z2 End Z coordinate
     * @param r Red component (0-1)
     * @param g Green component (0-1)
     * @param b Blue component (0-1)
     * @param a Alpha component (0-1)
     * @param depthTest If true, line is depth tested against scene (default true)
     */
    void DebugDrawLine(RenderViewID viewID, float x1, float y1, float z1, float x2, float y2, float z2, float r, float g, float b, float a, bool depthTest = true);

    /**
     * Draw a debug AABB (axis-aligned bounding box)
     * @param viewID The render view ID
     * @param minX Min X coordinate
     * @param minY Min Y coordinate
     * @param minZ Min Z coordinate
     * @param maxX Max X coordinate
     * @param maxY Max Y coordinate
     * @param maxZ Max Z coordinate
     * @param r Red component (0-1)
     * @param g Green component (0-1)
     * @param b Blue component (0-1)
     * @param a Alpha component (0-1)
     */
    void DebugDrawAABB(RenderViewID viewID, float minX, float minY, float minZ, float maxX, float maxY, float maxZ, float r, float g, float b, float a);

private:
    // Helper methods
    void RenderVoxelBuilder(RenderViewID viewID);
    void RenderTileMap25D(RenderViewID viewID);

    bool m_Initialized;
    std::unique_ptr<RenderWorld> m_RenderWorld;
    GraphicsBackend m_Backend;

    // Scene management
    std::unique_ptr<core::SceneManager> m_SceneManager;
    std::shared_ptr<Scene> m_ActiveScene;
    bool m_SceneDirty;

    // Render views
    std::map<RenderViewID, RenderViewInfo> m_RenderViews;
    std::map<RenderViewID, std::shared_ptr<Node>> m_SelectedNodes;  // Selected node per view
    std::set<RenderViewID> m_CameraEffectsPreviewViews;  // Views previewing scene camera effects

    // Find the active scene camera node that carries at least one enabled CameraEffect
    // component (depth-first), for editor effect preview. Returns null when none exists.
    Node* FindPreviewCameraNode(Scene* scene) const;

    // Voxel builders
    std::map<RenderViewID, std::vector<VoxelBuilder*>> m_VoxelBuilders;  // Active voxel builders per view (supports multiple builders)

    // TileMap25D builders
    std::map<RenderViewID, std::vector<TileMap25DBuilder*>> m_TileMap25DBuilders;  // Active tilemap 2.5D builders per view

    // Project settings for camera bounds
    uint32_t m_ProjectWindowWidth = 1280;
    uint32_t m_ProjectWindowHeight = 720;

    // Type registry
    std::vector<TypeInfo> m_NodeTypes;
    std::vector<TypeInfo> m_ComponentTypes;
    std::vector<TypeInfo> m_PrefabTypes;
    std::string m_ProjectPath;
    std::string m_DefaultThemePath;  // project's configured default theme (res://), or empty

    // Undo/Redo system
    std::unique_ptr<core::CommandHistory> m_CommandHistory;
    bool m_UndoRecordingEnabled = true;  // when false, RecordCommand is a no-op (modal preview tools)

    // Audio preview - keep assets alive while playing
    std::map<std::string, std::shared_ptr<asset::AudioAsset>> m_PreviewAudioAssets;

    // Helper methods
    void RegisterBuiltInNodeTypes();
    void RegisterBuiltInComponentTypes();
    void ScanNodesDirectory(const std::string& directory);
    void ScanComponentsDirectory(const std::string& directory);
    void ScanPrefabsDirectory(const std::string& directory);
    void ScanCustomScriptedComponents(const std::string& projectDir);
    void RefreshCustomScriptedComponents();
    // Load native C++ extensions (.lupineext) and add their component classes to
    // the Add-Component menu. See ExtensionManager / NativeExtensionRegistry.
    void ScanNativeExtensions(const std::string& projectDir);
    void UpdateCameraNodesRecursive(std::shared_ptr<Node> node, float aspectRatio, uint32_t width, uint32_t height);

    // Rendering helpers
    void RenderDebugOverlay(RenderViewID viewID, const RenderViewInfo& viewInfo);
    void UpdateCameraForViewMode(RenderViewID viewID, ViewMode mode);

    // Picking helpers
    math::Vec2 GetNodeBounds2D(std::shared_ptr<Node> node);
    math::Vec3 GetNodeBounds3D(std::shared_ptr<Node> node);
    bool TestNode2DPick(std::shared_ptr<Node> node, const math::Vec2& worldPos);
    bool TestNode3DPick(std::shared_ptr<Node> node, const math::Ray& ray, float& distance);
};

} // namespace engine
} // namespace lupine

