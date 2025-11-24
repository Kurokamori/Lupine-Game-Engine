#pragma once

#include "RenderView.hpp"
#include "RenderContext.hpp"
#include "DrawCommand.hpp"
#include "Material.hpp"
#include "Light.hpp"
#include "gfx/IGfxDevice.hpp"
#include "debug/DebugRenderer.hpp"
#include "lupine/math/AABB.hpp"
#include "lupine/math/OBB.hpp"
#include "lupine/math/Ray.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace lupine {

// Import math types into lupine namespace for rendering
using math::AABB;
using math::OBB;
using math::Mat4;
using math::Ray;

// Forward declarations
namespace core {
    class Scene;
    class Node;
    class Component;
}

/**
 * Renderable component interface.
 * Components that want to render implement this interface.
 */
class IRenderableComponent {
public:
    virtual ~IRenderableComponent() = default;

    /**
     * Build draw commands for this component.
     * Called during the gather stage.
     */
    virtual void buildDrawCommands(RenderContext& ctx) = 0;

    /**
     * Get the world-space bounding box for frustum culling.
     */
    virtual AABB getWorldBounds() const = 0;

    /**
     * Get the render layer for this component.
     */
    virtual RenderLayer getRenderLayer() const { return RenderLayer::Opaque; }

    /**
     * Get the spatial type for this component.
     * Determines which view modes (2D/3D) this component should be rendered in.
     */
    virtual SpatialType getSpatialType() const { return SpatialType::World3D; }

    /**
     * Perform precise ray intersection test if supported.
     * Override this for components that can do more accurate hit testing than AABB.
     * @param ray The ray in world space
     * @param outDistance The distance along the ray to the hit point (if hit)
     * @return true if this component was hit by the ray
     */
    virtual bool IntersectRay(const math::Ray& ray, float& outDistance) const {
        // Default: use AABB intersection
        AABB bounds = getWorldBounds();
        return ray.IntersectAABB(bounds, outDistance);
    }

    /**
     * Get oriented bounding box for visualization.
     * Override this for components that want tighter bounding box visualization.
     * Default returns an OBB created from the AABB.
     */
    virtual math::OBB getOrientedBounds() const {
        return math::OBB::FromAABB(getWorldBounds());
    }
};

/**
 * RenderWorld - the main rendering orchestrator.
 *
 * Responsibilities:
 * - Owns the graphics device (IGfxDevice)
 * - Manages multiple RenderViews (editor tabs, game windows, etc.)
 * - Manages global rendering resources (pipelines, materials, meshes, textures)
 * - Orchestrates the per-frame rendering pipeline:
 *   1. Gather renderables from scenes
 *   2. Build RenderContext for each view
 *   3. Batch and sort draw items
 *   4. Execute GPU commands via the device
 *   5. Present swapchains
 */
class RenderWorld {
public:
    RenderWorld();
    ~RenderWorld();

    // ===== Initialization =====

    /**
     * Initialize the render world with a specific graphics backend.
     * Creates its own graphics device.
     */
    bool initialize(GraphicsBackend backend);

    /**
     * Initialize the render world with an existing graphics device.
     * Takes ownership of the device.
     */
    bool initialize(std::unique_ptr<IGfxDevice> device);

    /**
     * Shutdown the render world and release all resources.
     */
    void shutdown();

    /**
     * Get the graphics device.
     */
    IGfxDevice* getDevice() const { return m_device.get(); }

    /**
     * Get the current graphics backend.
     */
    GraphicsBackend getBackend() const;

    // ===== RenderView Management =====

    /**
     * Create a new render view.
     */
    RenderViewID createRenderView(std::unique_ptr<RenderCamera> camera);

    /**
     * Destroy a render view.
     */
    void destroyRenderView(RenderViewID viewID);

    /**
     * Get a render view by ID.
     */
    RenderView* getRenderView(RenderViewID viewID);

    /**
     * Get all render views.
     */
    std::vector<RenderView*> getAllRenderViews();

    // ===== Per-Frame Rendering =====

    /**
     * Begin a new frame.
     * Call this once per frame before rendering.
     */
    void beginFrame();

    /**
     * Render all views.
     * This executes the full rendering pipeline for all active views.
     */
    void renderAllViews();

    /**
     * Render a specific view.
     */
    void renderView(RenderViewID viewID);

    /**
     * End the frame.
     * Call this once per frame after rendering.
     * @param presentAll If true, presents all swapchains. If false, you must manually present each view.
     */
    void endFrame(bool presentAll = true);

    /**
     * Present a specific view's swapchain.
     * Use this when rendering views independently (e.g., in editor with multiple viewports).
     */
    void presentView(RenderViewID viewID);

    // ===== Resource Management =====

    /**
     * Create a material.
     */
    MaterialHandle createMaterial(const Material& material);

    /**
     * Get a material.
     */
    const Material* getMaterial(MaterialHandle handle) const;

    /**
     * Update a material.
     */
    void updateMaterial(MaterialHandle handle, const Material& material);

    /**
     * Destroy a material.
     */
    void destroyMaterial(MaterialHandle handle);

    /**
     * Get the default PBR material.
     */
    MaterialHandle getDefaultPBRMaterial() const { return m_defaultPBRMaterial; }

    /**
     * Get the default skeletal material (PBR with skeletal animation support).
     */
    MaterialHandle getDefaultSkeletalMaterial() const { return m_defaultSkeletalMaterial; }

    /**
     * Get the default 3D text material.
     */
    MaterialHandle getDefaultText3DMaterial() const { return m_defaultText3DMaterial; }

    // ===== Statistics =====

    struct RenderStats {
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        uint32_t vertices = 0;
        uint32_t renderViews = 0;
        uint32_t renderables = 0;
        float frameTime = 0.0f;
    };

    const RenderStats& getStats() const { return m_stats; }

    // ===== Debug Rendering =====

    /**
     * Get the debug renderer instance.
     */
    DebugRenderer* getDebugRenderer() { return m_debugRenderer.get(); }

    /**
     * Enable/disable debug rendering for editor views.
     */
    void setDebugRenderingEnabled(bool enabled) { m_debugRenderingEnabled = enabled; }
    bool isDebugRenderingEnabled() const { return m_debugRenderingEnabled; }

    /**
     * Set project window size for camera bounds visualization.
     */
    void setProjectWindowSize(uint32_t width, uint32_t height) {
        m_projectWindowWidth = width;
        m_projectWindowHeight = height;
    }

    /**
     * Set global texture filtering mode.
     * This affects all newly created textures.
     */
    void setTextureFiltering(FilterMode minFilter, FilterMode magFilter) {
        m_textureMinFilter = minFilter;
        m_textureMagFilter = magFilter;
        if (m_device) {
            m_device->setDefaultTextureFiltering(minFilter, magFilter);
        }
    }

    /**
     * Get current texture filtering modes.
     */
    FilterMode getTextureMinFilter() const { return m_textureMinFilter; }
    FilterMode getTextureMagFilter() const { return m_textureMagFilter; }

private:
    // ===== Internal Pipeline Stages =====

    /**
     * Gather renderables for a specific view.
     * Fills the RenderContext with DrawItems.
     */
    void gatherRenderables(RenderView* view, RenderContext& ctx);

    /**
     * Gather lights from the scene.
     * Collects all active light components and prepares light data for rendering.
     */
    void gatherLights(RenderView* view);

    /**
     * Gather world environment from the scene.
     * Finds the WorldEnvironment component (skybox, fog, ambient light).
     */
    void gatherWorldEnvironment(RenderView* view);

    /**
     * Upload light data to GPU uniform buffer.
     * @param cmd Command list to upload to
     * @param view Render view containing the per-view light data
     */
    void uploadLightData(IGfxCommandList* cmd, RenderView* view);

    /**
     * Render shadow maps for all shadow-casting lights.
     */
    void renderShadowMaps(RenderView* view, RenderContext& ctx);

    /**
     * Render the skybox if a WorldEnvironment is present.
     * @param cmd Command list to render to
     * @param view Render view containing the world environment
     * @param viewProj View-projection matrix
     */
    void renderSkybox(IGfxCommandList* cmd, RenderView* view, const math::Mat4& viewProj);

    /**
     * Render a single shadow map for a specific light.
     */
    void renderShadowMapForLight(RenderView* view, RenderContext& ctx, int shadowMapIndex, const math::Mat4& lightSpaceMatrix, int shadowResolution);

    /**
     * Render a single cube map face for a point light shadow.
     */
    void renderShadowCubeMapFace(RenderView* view, RenderContext& ctx, int cubeMapIndex, int faceIndex, const math::Mat4& lightSpaceMatrix, int shadowResolution, const math::Vec3& lightPos, float lightRange);

    /**
     * Calculate cascade split distances using PSSM (Practical Split Scheme).
     * @param nearPlane Camera near plane
     * @param farPlane Camera far plane
     * @param cascadeCount Number of cascades
     * @param lambda PSSM lambda (0=uniform, 1=logarithmic)
     * @param outSplits Output array for split distances (size must be cascadeCount+1)
     */
    void calculateCascadeSplits(float nearPlane, float farPlane, uint32_t cascadeCount, float lambda, float* outSplits);

    /**
     * Calculate light space matrix for a cascade.
     */
    math::Mat4 calculateCascadeLightSpaceMatrix(const RenderView* view, const math::Vec3& lightDir, float nearPlane, float farPlane);

    /**
     * Build render batches from draw items.
     */
    void buildRenderBatches(
        const std::vector<DrawItem>& drawItems,
        std::vector<RenderPass>& outPasses
    );

    /**
     * Sort draw items for a specific pass.
     */
    void sortDrawItems(std::vector<const DrawItem*>& items, RenderPassType passType);

    /**
     * Execute render passes for a view.
     */
    void executeRenderPasses(
        RenderView* view,
        const std::vector<RenderPass>& passes,
        RenderContext& ctx
    );

    /**
     * Execute a single render batch.
     */
    void executeBatch(
        IGfxCommandList* cmd,
        const RenderBatch& batch,
        const Mat4& viewProj,
        RenderCamera* camera,
        RenderView* view
    );

    /**
     * Get debug name for a render pass type.
     */
    const char* getPassName(RenderPassType type) const;

    /**
     * Create default materials with shaders and pipelines.
     */
    bool createDefaultMaterials();

    /**
     * Ensure rendering resources (materials, debug renderer) are initialized.
     * This is called lazily on first render view creation.
     */
    bool ensureRenderingResourcesInitialized();

    /**
     * Render debug grid (before sprite passes so sprites appear on top).
     */
    void renderDebugGrid(IGfxCommandList* cmd, RenderView* view, const Mat4& viewProj);

    /**
     * Render debug overlay post-sprites (gizmos, camera bounds, scene debug).
     */
    void renderDebugOverlayPostSprites(IGfxCommandList* cmd, RenderView* view, const Mat4& viewProj);

    /**
     * Render debug overlay (grid, gizmos, camera bounds) for a view.
     * Legacy function - now calls renderDebugGrid + renderDebugOverlayPostSprites.
     */
    void renderDebugOverlay(IGfxCommandList* cmd, RenderView* view, const Mat4& viewProj);

    /**
     * Apply material property overrides (textures, colors, etc.) to the current pipeline.
     */
    void applyPropertyOverrides(IGfxCommandList* cmd, const MaterialPropertyBlock& overrides);

    // ===== State =====

    // Graphics device
    std::unique_ptr<IGfxDevice> m_device;

    // Debug renderer
    std::unique_ptr<DebugRenderer> m_debugRenderer;
    bool m_debugRenderingEnabled = true;
    bool m_renderingResourcesInitialized = false;

    // Project settings for camera bounds visualization
    uint32_t m_projectWindowWidth = 1920;
    uint32_t m_projectWindowHeight = 1080;

    // Texture filtering settings
    FilterMode m_textureMinFilter = FilterMode::Linear;
    FilterMode m_textureMagFilter = FilterMode::Linear;

    // Render views (indexed by ID)
    std::unordered_map<RenderViewID, std::unique_ptr<RenderView>> m_renderViews;
    RenderViewID m_nextViewID = 1;

    // Resources
    std::unordered_map<uint32_t, Material> m_materials;
    uint32_t m_nextMaterialID = 1;

    // Default materials for primitive rendering
    MaterialHandle m_defaultColoredMaterial;
    MaterialHandle m_defaultColored2DMaterial;    // 2D colored (no depth test)
    MaterialHandle m_defaultColoredDoubleSidedMaterial;  // Colored double-sided (no culling)
    MaterialHandle m_defaultTexturedMaterial;     // 3D textured (depth-tested)
    MaterialHandle m_defaultTextured2DMaterial;   // 2D textured (no depth test)
    MaterialHandle m_defaultTexturedDoubleSidedMaterial;  // 3D textured double-sided (no culling)
    MaterialHandle m_defaultWireframeMaterial;
    MaterialHandle m_defaultLineMaterial;
    MaterialHandle m_defaultLine2DMaterial;       // 2D line (no depth test)
    MaterialHandle m_defaultTextMaterial;
    MaterialHandle m_defaultText3DMaterial;       // 3D text (depth-tested)
    MaterialHandle m_defaultRoundedRectMaterial;  // Rounded rectangle UI material (alpha blend)
    MaterialHandle m_roundedRectAdditiveMaterial; // Rounded rectangle with additive blend
    MaterialHandle m_roundedRectMultiplyMaterial; // Rounded rectangle with multiply blend
    MaterialHandle m_roundedRectOpaqueMaterial;   // Rounded rectangle with no blend (opaque)
    MaterialHandle m_roundedRectOverlayMaterial;  // Rounded rectangle with overlay blend
    MaterialHandle m_roundedRectBorderMaterial;   // Rounded rectangle border (cutout inner area)
    MaterialHandle m_roundedRect3DMaterial;       // 3D rounded rectangle with depth testing
    MaterialHandle m_roundedRect3DBorderMaterial; // 3D rounded rectangle border with depth testing
    MaterialHandle m_defaultPBRMaterial;          // PBR material with lighting
    MaterialHandle m_defaultSkeletalMaterial;     // Skeletal animation material with PBR lighting

    // Per-frame state
    bool m_inFrame = false;
    uint32_t m_frameNumber = 0;

    // Lighting state (per-view light data is now stored in RenderView)
    // Global uniform buffer shared across all views (but uploaded with per-view data)
    UniformBufferHandle m_lightUniformBuffer;
    std::vector<TextureHandle> m_shadowMaps;              // 2D shadow maps for directional/spot lights
    std::vector<RenderTargetHandle> m_shadowMapFramebuffers;
    std::vector<uint32_t> m_shadowMapResolutions;         // Track resolution of each shadow map
    std::vector<TextureHandle> m_shadowCubeMaps;          // Cube map shadow maps for point lights
    std::vector<RenderTargetHandle> m_shadowCubeMapFramebuffers; // Framebuffers for cube map faces
    std::vector<uint32_t> m_shadowCubeMapResolutions;     // Track resolution of each cube map
    MaterialHandle m_shadowMapMaterial;                   // Material for 2D shadow maps
    MaterialHandle m_shadowCubeMaterial;                  // Material for cube map shadows (point lights)
    MaterialHandle m_shadowMapSkeletalMaterial;           // Material for skeletal mesh shadow maps
    // Toggle for experimental cascaded shadow maps (directional lights)
    // Disabled by default until basic single-map shadows are fully working.
    bool m_enableCascadedShadowMaps = false;

    // Statistics
    RenderStats m_stats;

    // Temporary storage (reused each frame)
    std::vector<RenderPass> m_renderPasses;
    std::vector<const DrawItem*> m_sortBuffer;
};

} // namespace lupine
