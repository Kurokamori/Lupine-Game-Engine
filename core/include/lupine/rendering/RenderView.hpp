#pragma once

#include "RenderCamera.hpp"
#include "ResourceHandles.hpp"
#include "gfx/GfxTypes.hpp"
#include "debug/DebugDrawCommands.hpp"
#include <memory>
#include <string>
#include <vector>

namespace lupine {

// Import math types for viewport/scissor
using math::Vec2;
using math::Vec3;

// Forward declarations
namespace core {
    class Scene;
    class Node;
}

namespace components {
    class WorldEnvironment;
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

    // Forward declare NodeInitialTransform for public access
    struct NodeInitialTransform {
        math::Vec2 position2D;
        float rotation2D;
        math::Vec2 scale2D;
        math::Vec3 position3D;
        math::Quat rotation3D;
        math::Vec3 scale3D;
        bool is2D;
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

    // Multi-selection gizmo state - store initial transforms for all selected nodes
    std::vector<NodeInitialTransform> m_gizmoDragInitialTransforms;

    // Custom draw commands (for editor tools like voxel builder)
    std::vector<CustomDrawMesh> m_customDrawMeshes;

    // Per-view lighting state (to prevent light data sharing between scenes)
    // These are managed by RenderWorld but stored per-view to ensure isolation
    friend class RenderWorld;
    std::vector<class LightDescriptor> m_activeLights;
    struct LightUniformBuffer* m_lightUniformData = nullptr;  // Allocated by RenderWorld

    // Per-view world environment (skybox, fog, ambient light)
    class components::WorldEnvironment* m_activeWorldEnvironment = nullptr;
};

} // namespace lupine
