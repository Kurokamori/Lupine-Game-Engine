#pragma once

#include "RenderCamera.hpp"
#include "ResourceHandles.hpp"
#include "gfx/GfxTypes.hpp"
#include "debug/DebugDrawCommands.hpp"
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace lupine {

// Import math types for viewport/scissor
using math::Vec2;
using math::Vec3;

// Forward declarations
namespace core {
    class Scene;
    class Node;
    class Component;
}

namespace components {
    class WorldEnvironment;
    class UIControl;
}

struct LightDescriptor;
struct LightUniformBuffer;

/**
 * Render view identifier.
 * Each editor tab or game viewport has its own RenderView.
 */
using RenderViewID = uint32_t;

/**
 * RenderView combines a camera with a render target or swapchain.
 * Represents one viewport - could be an editor tab, game window, or off-screen render.
 *
 * Each RenderView:
 * - Has its own camera (2D, 3D, or Canvas)
 * - Targets a swapchain (window) or render target (off-screen)
 * - Has its own viewport and scissor settings
 * - Can render a specific scene or subset of a scene
 */
class RenderView {
public:
    RenderView(RenderViewID id, std::unique_ptr<RenderCamera> camera);
    ~RenderView();

    // ===== Identification =====

    RenderViewID getID() const { return m_id; }
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // ===== Camera =====

    RenderCamera* getCamera() const { return m_camera.get(); }

    template<typename T>
    T* getCameraAs() const {
        return dynamic_cast<T*>(m_camera.get());
    }

    void setCamera(std::unique_ptr<RenderCamera> camera) {
        m_camera = std::move(camera);
    }

    // ===== Render Target =====

    /**
     * Set the output target.
     * Can be a swapchain (for windows) or a render target (for off-screen rendering).
     */
    void setSwapchain(SwapchainHandle swapchain);
    void setRenderTarget(RenderTargetHandle target);

    bool hasSwapchain() const { return m_swapchain.isValid(); }
    bool hasRenderTarget() const { return m_renderTarget.isValid(); }

    SwapchainHandle getSwapchain() const { return m_swapchain; }
    RenderTargetHandle getRenderTarget() const { return m_renderTarget; }

    // ===== Viewport & Scissor =====

    const Viewport& getViewport() const { return m_viewport; }
    void setViewport(const Viewport& viewport) { m_viewport = viewport; }

    const ScissorRect& getScissor() const { return m_scissor; }
    void setScissor(const ScissorRect& scissor) { m_scissor = scissor; }

    // ===== Scene Association =====

    /**
     * Set which scene this view renders.
     * A view can render a different scene than others.
     */
    void setScene(core::Scene* scene) { m_scene = scene; }
    core::Scene* getScene() const { return m_scene; }

    // ===== Embedded SubViewport Support =====

    /**
     * Set the render-root node for this view.
     * When set, the view renders only the subtree rooted at this node instead of
     * the entire scene graph. Used by SubViewport components to render a node
     * subtree into an off-screen target (render-to-texture).
     *
     * The node must belong to the scene assigned via setScene(). Passing nullptr
     * (the default) renders the full scene from its root.
     */
    void setRenderRootNode(core::Node* node) { m_renderRootNode = node; }
    core::Node* getRenderRootNode() const { return m_renderRootNode; }

    /**
     * The scene node this view's camera was built from (a Camera2D / Camera3D / CameraUI),
     * when known. Used by the renderer to gather per-camera stackable effect components
     * (CameraEffect) attached to that node. Null for views whose camera is synthetic
     * (e.g. internal blits) — such views simply run no camera effects.
     */
    void setSourceCameraNode(core::Node* node) { m_sourceCameraNode = node; }
    core::Node* getSourceCameraNode() const { return m_sourceCameraNode; }

    // ===== Render Settings =====

    /**
     * Render layer mask - which layers to render.
     * Allows partial rendering (e.g., only 2D, only 3D).
     */
    uint32_t getRenderLayerMask() const { return m_renderLayerMask; }
    void setRenderLayerMask(uint32_t mask) { m_renderLayerMask = mask; }

    /**
     * Enable/disable specific render passes.
     */
    bool isRenderPass2DEnabled() const { return m_enable2D; }
    bool isRenderPass3DEnabled() const { return m_enable3D; }
    bool isRenderPassCanvasEnabled() const { return m_enableCanvas; }

    void setRenderPass2DEnabled(bool enabled) { m_enable2D = enabled; }
    void setRenderPass3DEnabled(bool enabled) { m_enable3D = enabled; }
    void setRenderPassCanvasEnabled(bool enabled) { m_enableCanvas = enabled; }

    /**
     * Enable/disable debug rendering (grid, gizmos, etc.)
     */
    bool isDebugRenderingEnabled() const { return m_debugRenderingEnabled; }
    void setDebugRenderingEnabled(bool enabled) { m_debugRenderingEnabled = enabled; }

    /**
     * Enable/disable grid rendering specifically
     */
    bool isGridRenderingEnabled() const { return m_gridRenderingEnabled; }
    void setGridRenderingEnabled(bool enabled) { m_gridRenderingEnabled = enabled; }

    /**
     * Enable/disable camera preview (bounds) rendering in 2D view
     */
    bool isCameraPreviewEnabled() const { return m_cameraPreviewEnabled; }
    void setCameraPreviewEnabled(bool enabled) { m_cameraPreviewEnabled = enabled; }

    /**
     * Enable/disable gizmo rendering
     */
    bool isGizmoEnabled() const { return m_gizmoEnabled; }
    void setGizmoEnabled(bool enabled) { m_gizmoEnabled = enabled; }

    /**
     * Enable/disable collision shapes rendering (debug visualization)
     */
    bool isCollisionShapesEnabled() const { return m_collisionShapesEnabled; }
    void setCollisionShapesEnabled(bool enabled) { m_collisionShapesEnabled = enabled; }

    // ===== Custom Draw Commands =====

    /**
     * Add a custom mesh to render in this view.
     * Used for editor tools that need to render geometry outside the scene graph.
     * @param mesh The mesh to render
     * @param material Material handle to use
     * @param transform Transform matrix for the mesh
     */
    void addCustomDrawMesh(MeshHandle mesh, MaterialHandle material, const math::Mat4& transform);

    /**
     * Clear all custom draw meshes.
     * Should be called each frame after rendering.
     */
    void clearCustomDrawMeshes();

    /**
     * Get custom draw meshes for rendering.
     */
    struct CustomDrawMesh {
        MeshHandle mesh;
        MaterialHandle material;
        math::Mat4 transform;
    };
    const std::vector<CustomDrawMesh>& getCustomDrawMeshes() const { return m_customDrawMeshes; }

    /**
     * Set/get the gizmo type (Translation, Rotation, Scale, All)
     */
    GizmoType getGizmoType() const { return m_gizmoType; }
    void setGizmoType(GizmoType type) { m_gizmoType = type; }

    /**
     * Set/get the transform space (Local, Parent, World)
     */
    TransformSpace getTransformSpace() const { return m_transformSpace; }
    void setTransformSpace(TransformSpace space) { m_transformSpace = space; }

    /**
     * Gizmo interaction state
     */
    bool isGizmoDragging() const { return m_gizmoDragging; }
    void setGizmoDragging(bool dragging) { m_gizmoDragging = dragging; }

    GizmoAxis getGizmoDragAxis() const { return m_gizmoDragAxis; }
    void setGizmoDragAxis(GizmoAxis axis) { m_gizmoDragAxis = axis; }

    const math::Vec3& getGizmoDragStartWorldPos() const { return m_gizmoDragStartWorldPos; }
    void setGizmoDragStartWorldPos(const math::Vec3& pos) { m_gizmoDragStartWorldPos = pos; }

    const math::Vec2& getGizmoDragStartScreenPos() const { return m_gizmoDragStartScreenPos; }
    void setGizmoDragStartScreenPos(const math::Vec2& pos) { m_gizmoDragStartScreenPos = pos; }

    GizmoType getGizmoDragOperation() const { return m_gizmoDragOperation; }
    void setGizmoDragOperation(GizmoType operation) { m_gizmoDragOperation = operation; }

    // Direction of the grabbed 2D scale-rectangle handle in the gizmo's local
    // frame (e.g. (1,1) = top-right corner, (-1,0) = left edge, (0,0) = uniform).
    const math::Vec2& getGizmoScaleHandleDir() const { return m_gizmoScaleHandleDir; }
    void setGizmoScaleHandleDir(const math::Vec2& dir) { m_gizmoScaleHandleDir = dir; }

    // Rotation (radians) the 2D scale rectangle was drawn with at drag start.
    float getGizmoDragRotation2D() const { return m_gizmoDragRotation2D; }
    void setGizmoDragRotation2D(float rotation) { m_gizmoDragRotation2D = rotation; }

    // Oriented half-extents (world units) of the 2D selection rectangle at drag start.
    const math::Vec2& getGizmoDragHalfExtents() const { return m_gizmoDragHalfExtents; }
    void setGizmoDragHalfExtents(const math::Vec2& extents) { m_gizmoDragHalfExtents = extents; }

    // Forward declare NodeInitialTransform for public access
    struct NodeInitialTransform {
        math::Vec2 position2D;
        float rotation2D;
        math::Vec2 scale2D;
        math::Vec3 position3D;
        math::Quat rotation3D;
        math::Vec3 scale3D;
        bool is2D;

        // Component scaling tracking (for components that override OnGizmoScale)
        std::weak_ptr<core::Component> scalingComponent;
        std::string componentTypeName;  // e.g., "Sprite2D", "Label", "Button"

        // Initial property values (stored as JSON for flexibility)
        nlohmann::json componentInitialProperties;

        // A UIControl on this node whose position is NOT its Node2D position: in Anchors
        // mode the rect is recomputed from anchors+offsets every frame and written back over
        // the node, so a move gizmo that set the node position was undone before the next
        // draw and the control snapped straight back. Dragging such a control translates its
        // OFFSETS, which is what actually places it -- these are the values at drag start.
        //
        // uiControlDrivesPosition is false for a Position-mode control (the node position is
        // real) and for a container-driven one (the parent owns the rect and the drag must be
        // refused outright).
        std::weak_ptr<components::UIControl> layoutControl;
        bool uiControlDrivesPosition = false;
        bool uiControlContainerDriven = false;
        math::Vec2 initialOffsetMin;
        math::Vec2 initialOffsetMax;
    };

    /**
     * Multi-selection gizmo initial transforms
     */
    std::vector<NodeInitialTransform>& getGizmoDragInitialTransforms() { return m_gizmoDragInitialTransforms; }
    const std::vector<NodeInitialTransform>& getGizmoDragInitialTransforms() const { return m_gizmoDragInitialTransforms; }

    /**
     * Set/get the selected node for editor visualization
     */
    void setSelectedNode(std::shared_ptr<core::Node> node) { m_selectedNode = node; }
    std::shared_ptr<core::Node> getSelectedNode() const { return m_selectedNode; }
    
    /**
     * Set/get multiple selected nodes for multi-selection visualization
     */
    void setSelectedNodes(const std::vector<std::shared_ptr<core::Node>>& nodes) { m_selectedNodes = nodes; }
    const std::vector<std::shared_ptr<core::Node>>& getSelectedNodes() const { return m_selectedNodes; }

    /**
     * Aspect ratio helper.
     */
    float getAspectRatio() const;

private:
    RenderViewID m_id;
    std::string m_name;

    // Camera
    std::unique_ptr<RenderCamera> m_camera;

    // Output target
    SwapchainHandle m_swapchain;
    RenderTargetHandle m_renderTarget;

    // Viewport settings
    Viewport m_viewport;
    ScissorRect m_scissor;

    // Scene
    core::Scene* m_scene = nullptr;

    // Optional render-root node. When non-null, gathering starts from this node's
    // subtree instead of the scene root (used by embedded SubViewports).
    core::Node* m_renderRootNode = nullptr;

    // Source camera node (Camera2D/Camera3D/CameraUI) this view was built from, when
    // known. Used to gather per-camera CameraEffect components. Not owned.
    core::Node* m_sourceCameraNode = nullptr;

    // Editor selection
    std::shared_ptr<core::Node> m_selectedNode;
    std::vector<std::shared_ptr<core::Node>> m_selectedNodes;

    // Render settings
    uint32_t m_renderLayerMask = 0xFFFFFFFF;
    bool m_enable2D = true;
    bool m_enable3D = true;
    bool m_enableCanvas = true;
    bool m_debugRenderingEnabled = true;
    bool m_gridRenderingEnabled = true;        // Grid rendering specifically
    bool m_cameraPreviewEnabled = true;        // Camera bounds preview in 2D
    bool m_collisionShapesEnabled = true;      // Collision shapes debug rendering

    // Gizmo settings
    bool m_gizmoEnabled = true;
    GizmoType m_gizmoType = GizmoType::All;
    TransformSpace m_transformSpace = TransformSpace::World;

    // Gizmo interaction state
    bool m_gizmoDragging = false;
    GizmoAxis m_gizmoDragAxis = GizmoAxis::None;
    math::Vec3 m_gizmoDragStartWorldPos;
    math::Vec2 m_gizmoDragStartScreenPos;
    GizmoType m_gizmoDragOperation = GizmoType::Translation;  // What operation is being performed
    math::Vec2 m_gizmoScaleHandleDir = math::Vec2(0.0f, 0.0f);  // Grabbed 2D scale handle direction
    float m_gizmoDragRotation2D = 0.0f;                        // Rotation of the 2D scale rect at drag start
    math::Vec2 m_gizmoDragHalfExtents = math::Vec2(0.0f, 0.0f); // 2D selection half-extents at drag start

    // Multi-selection gizmo state - store initial transforms for all selected nodes
    std::vector<NodeInitialTransform> m_gizmoDragInitialTransforms;

    // Custom draw commands (for editor tools like voxel builder)
    std::vector<CustomDrawMesh> m_customDrawMeshes;

    // Per-view lighting state (to prevent light data sharing between scenes)
    // These are managed by RenderWorld but stored per-view to ensure isolation
    friend class RenderWorld;
    std::vector<struct LightDescriptor> m_activeLights;
    struct LightUniformBuffer* m_lightUniformData = nullptr;  // Allocated by RenderWorld

    // Per-view GPU light UBO handles (ring-buffered to prevent CPU/GPU race conditions)
    // Each view needs its own GPU buffers to prevent conflicts when multiple scenes render concurrently
    static constexpr uint32_t MAX_LIGHT_UBO_FRAMES = 3;
    UniformBufferHandle m_lightUniformBuffers[MAX_LIGHT_UBO_FRAMES];
    uint32_t m_currentLightBufferIndex = 0;

    // Per-view shadow maps - each view needs its own shadow maps to prevent GPU conflicts
    // when multiple scenes with lights render concurrently (Vulkan-specific requirement)
    static constexpr uint32_t MAX_SHADOW_MAPS = 8;
    std::vector<TextureHandle> m_shadowMaps;              // 2D shadow maps for directional/spot lights
    std::vector<RenderTargetHandle> m_shadowMapFramebuffers;
    std::vector<uint32_t> m_shadowMapResolutions;
    std::vector<TextureHandle> m_shadowCubeMaps;          // Cube maps for point lights
    std::vector<RenderTargetHandle> m_shadowCubeMapFramebuffers;
    std::vector<uint32_t> m_shadowCubeMapResolutions;

    // Per-view world environment (skybox, fog, ambient light)
    class components::WorldEnvironment* m_activeWorldEnvironment = nullptr;

    // Static-shadow cache state. Shadow maps persist across frames (this view is
    // reused), so when nothing that affects them has changed the depth-map rasterization
    // is skipped entirely and the previously rendered maps + light shadow matrices are
    // reused. "Changed" = the render epoch (caster/light transform, visibility, content),
    // the tree-structure version (caster/light added/removed), a dynamic caster being
    // present, or - only when cascaded shadow maps are enabled, since cascades fit the
    // camera - the camera view-projection.
    uint64_t m_LastShadowEpoch = 0;
    uint64_t m_LastShadowTreeVersion = 0;
    bool m_HasRenderedShadows = false;
    math::Mat4 m_LastShadowCameraVP;
};

} // namespace lupine
