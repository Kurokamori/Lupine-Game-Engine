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
#include <memory>
#include <string>
#include <vector>
#include <map>
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
    void UpdateGizmoDrag(RenderViewID viewID, float screenX, float screenY);

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
     */
    void DebugDrawLine(RenderViewID viewID, float x1, float y1, float z1, float x2, float y2, float z2, float r, float g, float b, float a);

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

    // Voxel builders
    std::map<RenderViewID, std::vector<VoxelBuilder*>> m_VoxelBuilders;  // Active voxel builders per view (supports multiple builders)

    // Project settings for camera bounds
    uint32_t m_ProjectWindowWidth = 1280;
    uint32_t m_ProjectWindowHeight = 720;

    // Type registry
    std::vector<TypeInfo> m_NodeTypes;
    std::vector<TypeInfo> m_ComponentTypes;
    std::vector<TypeInfo> m_PrefabTypes;
    std::string m_ProjectPath;

    // Undo/Redo system
    std::unique_ptr<core::CommandHistory> m_CommandHistory;

    // Audio preview - keep assets alive while playing
    std::map<std::string, std::shared_ptr<asset::AudioAsset>> m_PreviewAudioAssets;

    // Helper methods
    void RegisterBuiltInNodeTypes();
    void RegisterBuiltInComponentTypes();
    void ScanNodesDirectory(const std::string& directory);
    void ScanComponentsDirectory(const std::string& directory);
    void ScanPrefabsDirectory(const std::string& directory);
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

