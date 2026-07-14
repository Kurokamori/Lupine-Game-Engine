#pragma once

#include "RenderView.hpp"
#include "RenderContext.hpp"
#include "DrawCommand.hpp"
#include "Material.hpp"
#include "Light.hpp"
#include "gfx/IGfxDevice.hpp"
#include "TextureUpload.hpp"
#include "debug/DebugRenderer.hpp"
#include "lupine/math/AABB.hpp"
#include "lupine/math/OBB.hpp"
#include "lupine/math/Ray.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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

class PostProcessStack;
struct PostProcessSettings;
class CameraEffectStack;
struct CameraEffectChain;

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
     * Whether this component's rendered output can change from frame to frame on its
     * own - i.e. WITHOUT a node transform/visibility change or a component-property
     * change (both of which advance Node::GetRenderEpoch()). Examples that ARE dynamic:
     * skeletal poses, particle systems, animated sprites, CPU-animated materials.
     *
     * The renderer reuses a cached draw list / shadow maps for a view only while the
     * render epoch, tree-structure version and camera are all unchanged AND no gathered
     * renderable reports itself dynamic. The default is therefore the SAFE answer
     * (true): a component is assumed dynamic - and thus its view is never cached - until
     * it explicitly declares its content static by overriding this to return false.
     * Returning false is only correct when every way the component can change its output
     * goes through the render epoch (transform/visibility/SetPropertyValue/
     * NotifyRenderStateChanged).
     */
    virtual bool isRenderContentDynamic() const { return true; }

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

    /**
     * Prepare GPU resources for this component.
     * Called before the first render to upload meshes, textures, etc. to the GPU.
     * This avoids the first-frame stutter caused by lazy initialization.
     * Override this for components that have GPU resources to upload.
     */
    virtual void prepareGPUResources(IGfxDevice* device) { (void)device; }
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

    /**
     * Get the current frame number.
     * Incremented once per beginFrame(). Used by embedded SubViewports to render
     * their content at most once per frame even when multiple views are drawn.
     */
    uint32_t getFrameNumber() const { return m_frameNumber; }

    /**
     * Authoritative per-frame elapsed time in seconds, derived from the frame counter
     * (60 FPS reference). Single source for the engine-fed shader `u_Time` across the
     * batch path, the post-process stack, and the camera-effect stack.
     */
    float elapsedSeconds() const { return static_cast<float>(m_frameNumber) * 0.0166667f; }

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
     * Render a simple textured quad (for splash screens, loading screens, etc.)
     * This bypasses the scene system and renders directly.
     * @param texture The texture to render
     * @param swapchain The swapchain to render to
     * @param tint Color tint (use alpha for fade effects)
     * @param position Screen position (pixels, top-left origin)
     * @param size Size in pixels
     * @param windowWidth Window width for orthographic projection
     * @param windowHeight Window height for orthographic projection
     */
    void renderTexturedQuad(
        TextureHandle texture,
        SwapchainHandle swapchain,
        const math::Color& tint,
        const math::Vec2& position,
        const math::Vec2& size,
        int windowWidth,
        int windowHeight);

    /**
     * Render a simple textured quad from raw image data (for splash screens, loading screens, etc.)
     * This version creates a temporary texture from the provided image data.
     * Use this when you need to display an image before the render loop starts.
     * The texture is created after the graphics context is made current and destroyed after rendering.
     * @param imageData Pointer to raw RGBA image data
     * @param imageWidth Image width in pixels
     * @param imageHeight Image height in pixels
     * @param swapchain The swapchain to render to
     * @param tint Color tint (use alpha for fade effects)
     * @param position Screen position (pixels, top-left origin)
     * @param size Size in pixels
     * @param windowWidth Window width for orthographic projection
     * @param windowHeight Window height for orthographic projection
     */
    void renderTexturedQuadFromImageData(
        const uint8_t* imageData,
        uint32_t imageWidth,
        uint32_t imageHeight,
        SwapchainHandle swapchain,
        const math::Color& tint,
        const math::Vec2& position,
        const math::Vec2& size,
        int windowWidth,
        int windowHeight);

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

    /**
     * Prepare a scene for rendering by pre-uploading GPU resources.
     * Call this after loading a scene but before the first render to avoid
     * first-frame stutter caused by lazy mesh/texture uploads.
     * This is automatically called on first render if not called manually.
     */
    void prepareScene(core::Scene* scene);

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
     * Get the default colored double-sided material (for custom 3D meshes with vertex colors).
     */
    MaterialHandle getDefaultColoredDoubleSidedMaterial() const { return m_defaultColoredDoubleSidedMaterial; }

    /**
     * Get the default toon material (cel shading).
     */
    MaterialHandle getDefaultToonMaterial() const { return m_defaultToonMaterial; }

    /**
     * Get the default skeletal toon material (cel shading with skeletal animation).
     */
    MaterialHandle getDefaultSkeletalToonMaterial() const { return m_defaultSkeletalToonMaterial; }

    /**
     * Get the default stylized material (fantasy-style soft shading with shadow ramps).
     */
    MaterialHandle getDefaultStylizedMaterial() const { return m_defaultStylizedMaterial; }

    /**
     * Get the default skeletal stylized material (stylized rendering with skeletal animation).
     */
    MaterialHandle getDefaultSkeletalStylizedMaterial() const { return m_defaultSkeletalStylizedMaterial; }

    /**
     * Get the default transparent/glass material (refraction and fresnel effects).
     */
    MaterialHandle getDefaultTransparentMaterial() const { return m_defaultTransparentMaterial; }

    /**
     * Get the default glow material (emissive effects for stars, lights, etc).
     */
    MaterialHandle getDefaultGlowMaterial() const { return m_defaultGlowMaterial; }

    /**
     * Get the default 3D text material.
     */
    MaterialHandle getDefaultText3DMaterial() const { return m_defaultText3DMaterial; }

    // ===== Custom Shader Support =====

    /**
     * Get or create a custom material from shader file paths.
     * Materials are cached by their path combination for efficiency.
     * @param vertPath Path to the vertex shader file
     * @param fragPath Path to the fragment shader file
     * @param isSkeletal If true, uses skeletal vertex layout with bone weights
     * @return The material handle, or invalid handle if shader compilation fails
     */
    MaterialHandle getOrCreateCustomMaterial(const std::string& vertPath, const std::string& fragPath, bool isSkeletal = false);

    /**
     * Get or create a 2D UI material from a Lupine Shader (.lsh) file.
     *
     * The .lsh source is loaded, translated to the active backend at runtime via
     * ShaderTranslator, then compiled into a pipeline using the standard 2D UI render
     * state (alpha/additive/multiply/opaque/overlay blend per blendMode, no depth test,
     * no culling, UI render layer). The result is cached per (path, blendMode). When the
     * source file changes on disk (non-pack mode) the material is recompiled automatically
     * and the previous GPU resources are released, enabling live shader editing in the editor.
     *
     * The shader may declare a `#render_mode` to override blend/cull/depth state, and the
     * `layout` selects the vertex input layout and the default render state for the host
     * (2D UI vs 3D mesh vs skeletal mesh). The result is cached per (path, blendMode, layout).
     *
     * @param lshPath res:// path (or physical/absolute path) to the .lsh file
     * @param blendMode Blend mode (0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay)
     * @param layout Vertex-layout / host kind (defaults to 2D UI for the existing callers)
     * @return Material handle, or invalid handle if loading/translation/compilation fails
     */
    MaterialHandle getOrCreateLshMaterial(const std::string& lshPath, int blendMode,
                                          LshMaterialLayout layout = LshMaterialLayout::UI2D);

    /**
     * Clear the custom shader cache (useful when shaders are recompiled).
     */
    void clearCustomShaderCache();

    /**
     * Clear the cached .lsh UI material(s) and release their GPU resources.
     * If lshPath is empty, clears every cached .lsh material; otherwise clears all
     * blend-mode variants compiled from that path.
     */
    void clearLshMaterialCache(const std::string& lshPath = "");

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
        // Tie mip-chain generation for 2D content textures to the filter mode:
        // Nearest = crisp pixel-art (no mips), Linear = smooth mipmapped
        // minification so sprites/images downscale cleanly below the design size.
        SetGenerateTextureMipmaps(minFilter != FilterMode::Nearest);
    }

    /**
     * Get current texture filtering modes.
     */
    FilterMode getTextureMinFilter() const { return m_textureMinFilter; }
    FilterMode getTextureMagFilter() const { return m_textureMagFilter; }

    // --- Supersampling / render-scale frame target ---
    // When the window is smaller than the design resolution, the runtime renders all
    // cameras into this shared offscreen target at the (higher) design resolution and
    // then downscales it to the window. That supersamples the whole frame - sprites,
    // UI and text all stay crisp instead of being rendered natively at the low window
    // resolution. The colour format matches the swapchain so colours round-trip
    // exactly; a depth attachment lets the 3D/2D passes depth-test as usual.
    bool ensureFrameTarget(uint32_t width, uint32_t height);
    RenderTargetHandle getFrameTarget() const { return m_frameTarget; }
    bool hasFrameTarget() const { return m_frameTarget.isValid(); }

    // Clear the swapchain backbuffer (letterbox bars) and blit the frame target into
    // dstViewport with linear filtering. fullWidth/fullHeight are the backbuffer size.
    void presentFrameTargetScaled(SwapchainHandle swapchain, const Viewport& dstViewport,
                                  uint32_t fullWidth, uint32_t fullHeight);

private:
    // ===== Internal Pipeline Stages =====

    /**
     * Render all enabled SubViewport components found within a view's render scope
     * into their own off-screen targets before the view itself is gathered.
     * This is a no-op for scenes that contain no SubViewport components.
     * Recursion (nested SubViewports) and multi-view frames are handled via a
     * per-frame guard inside each SubViewport.
     */
    void renderEmbeddedSubViewports(RenderView* view);

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
     * A per-view pool of recyclable dynamic vertex/index buffers backing 2D geometry
     * batches. buildRenderBatches resets `used` to 0 and hands out slots in order,
     * growing a slot's buffers on demand. Because the per-view draw-list cache may
     * retain batches that reference these buffers across cache-hit frames, the pool is
     * owned per view and only recycled when that same view rebuilds its draw list.
     */
    struct GeometryBatchBuffers {
        BufferHandle vertexBuffer;
        uint32_t vertexCapacity = 0;   // capacity in Vertex records
        BufferHandle indexBuffer;
        uint32_t indexCapacity = 0;    // capacity in uint32_t indices
    };
    struct GeometryBatchPool {
        std::vector<GeometryBatchBuffers> slots;
        size_t used = 0;
    };

    /**
     * Build render batches from draw items. When `geomPool` is non-null, consecutive
     * runs of bakeable 2D quad items (sprites / colored quads) sharing pipeline +
     * material + texture + clip are combined into single-draw geometry batches using
     * buffers drawn from that pool; pass null to disable geometry batching (utility
     * one-off renders).
     */
    void buildRenderBatches(
        const std::vector<DrawItem>& drawItems,
        std::vector<RenderPass>& outPasses,
        GeometryBatchPool* geomPool = nullptr
    );

    /**
     * Order-preserving batch builder for the z-ordered 2D / Canvas passes. Combines
     * bakeable quad runs into geometry batches (one draw) when `geomPool` is non-null,
     * and falls back to consecutive state-merge (one bind, per-item draws) for
     * everything else. Preserves exact submission order and per-item clip rects.
     */
    void buildOrderedBatches2D(
        const std::vector<const DrawItem*>& items,
        std::vector<RenderBatch>& outBatches,
        GeometryBatchPool* geomPool
    );

    /**
     * True if `material` is one of the four built-in bakeable 2D materials (textured /
     * colored, alpha / additive) whose only per-object state — transform, tint, uv rect,
     * texture — can be folded into per-vertex data for geometry batching.
     */
    bool is2DBakeableMaterial(MaterialHandle material) const;

    /**
     * Combine items[begin, end) (all bakeable quads sharing pipeline/material/texture/
     * clip) into a single geometry batch, filling a dynamic buffer slot from `pool`.
     */
    RenderBatch buildGeometryBatch(
        const std::vector<const DrawItem*>& items,
        size_t begin,
        size_t end,
        PipelineHandle pipeline,
        TextureHandle texture,
        bool useTexture,
        GeometryBatchPool& pool
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
        RenderContext& ctx,
        bool skipClear = false
    );

    /**
     * Copy a texture to a render target with a fullscreen quad (a "blit"). Self-contained:
     * issues its own command list to `dst`. `flipV` inverts the sampled V (for swapchain
     * presentation of an offscreen target). Used by the ordered mid-scene capture path.
     */
    void blitTexture(TextureHandle src, RenderTargetHandle dst, uint32_t width, uint32_t height,
                     bool flipV = false);
    bool ensureBlitResources();
    bool ensureSceneCapturePong(uint32_t width, uint32_t height);

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
     * Execute a 2D geometry batch: a single drawIndexed over the batch's pre-combined
     * dynamic buffers (world-space positions, baked tint/uv), binding the shared texture
     * and shared 2D Unlit uniforms once. Used only for batches with isGeometryBatched.
     */
    void executeGeometryBatch(
        IGfxCommandList* cmd,
        const RenderBatch& batch,
        const Mat4& viewProj,
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

    /**
     * Get the texture binding slot for a given texture name based on the current graphics backend.
     * This provides backend-agnostic texture binding that works across OpenGL, Vulkan, etc.
     * @param textureName The uniform name of the texture (e.g., "u_AlbedoTexture")
     * @return The binding slot to use for this texture
     */
    uint32_t getTextureBinding(const std::string& textureName) const;

    /**
     * Get the base binding slot for shadow maps based on the current graphics backend.
     * @return The starting binding slot for shadow maps
     */
    uint32_t getShadowMapBaseBinding() const;

    /**
     * Get the base binding slot for shadow cube maps based on the current graphics backend.
     * @return The starting binding slot for shadow cube maps
     */
    uint32_t getShadowCubeMapBaseBinding() const;

    // ===== Shader Loading Helpers =====

    /**
     * Result of loading shaders - contains handles for vertex, fragment, and optionally geometry shaders.
     */
    struct ShaderLoadResult {
        ShaderHandle vertex;
        ShaderHandle fragment;
        ShaderHandle geometry;  // Optional
        bool success = false;

        bool hasGeometry() const { return geometry.isValid(); }
    };

    /**
     * Load shaders for a given shader name, automatically using precompiled bytecode
     * when available for the current backend (e.g., DXIL for DX12, SPIR-V for Vulkan).
     * Falls back to source code compilation for backends without precompiled shaders.
     *
     * This is the preferred method for loading built-in shaders as it's extensible
     * for future backends (DX11, Vulkan, Metal, etc.) that may support offline compilation.
     *
     * @param shaderName Name of the shader (e.g., "Unlit", "PBR", "Wireframe")
     * @return ShaderLoadResult containing shader handles and success status
     */
    ShaderLoadResult loadShaders(const char* shaderName);

    /**
     * Helper to destroy shaders from a ShaderLoadResult (cleanup on failure).
     */
    void destroyLoadedShaders(ShaderLoadResult& result);

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
    MaterialHandle m_defaultColored2DAdditiveMaterial; // 2D colored with additive blend
    MaterialHandle m_defaultColoredDoubleSidedMaterial;  // Colored double-sided (no culling)
    MaterialHandle m_defaultTexturedMaterial;     // 3D textured (depth-tested)
    MaterialHandle m_defaultTextured2DMaterial;   // 2D textured (no depth test)
    MaterialHandle m_defaultTextured2DAdditiveMaterial; // 2D textured with additive blend (sprites/particles)
    MaterialHandle m_particle3DAlphaMaterial;     // 3D billboard particles, alpha blend (depth read-only)
    MaterialHandle m_particle3DAdditiveMaterial;  // 3D billboard particles, additive blend (depth read-only)
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
    MaterialHandle m_radialGradientMaterial;      // Radial gradient (alpha blend)
    MaterialHandle m_radialGradientAdditiveMaterial; // Radial gradient (additive blend)
    MaterialHandle m_radialGradientMultiplyMaterial; // Radial gradient (multiply blend)
    MaterialHandle m_radialGradientOpaqueMaterial;   // Radial gradient (opaque)
    MaterialHandle m_polygonMaterial;             // Regular polygon (alpha blend)
    MaterialHandle m_polygonAdditiveMaterial;     // Regular polygon (additive blend)
    MaterialHandle m_polygonMultiplyMaterial;     // Regular polygon (multiply blend)
    MaterialHandle m_polygonOpaqueMaterial;       // Regular polygon (opaque)
    MaterialHandle m_defaultPBRMaterial;          // PBR material with lighting
    MaterialHandle m_defaultPBRInstancedMaterial; // GPU-instanced PBR (MultiMesh)
    MaterialHandle m_defaultSkeletalMaterial;     // Skeletal animation material with PBR lighting
    MaterialHandle m_defaultToonMaterial;         // Toon/cel shading material
    MaterialHandle m_defaultSkeletalToonMaterial; // Skeletal toon shading material
    MaterialHandle m_defaultStylizedMaterial;     // Stylized fantasy-style material
    MaterialHandle m_defaultSkeletalStylizedMaterial; // Skeletal stylized material
    MaterialHandle m_defaultTransparentMaterial;  // Transparent/glass material with refraction
    MaterialHandle m_defaultGlowMaterial;         // Glow/emissive material for stars, lights

    // Default sampler for material rendering (Linear filtering, Wrap mode)
    SamplerHandle m_defaultSampler;

    // Quad mesh for splash screen rendering
    MeshHandle m_splashQuadMesh;

    // Shadow sampler for shadow map sampling (used at sampler binding 1)
    // Uses Clamp-to-border addressing with nearest filtering for depth comparison
    SamplerHandle m_shadowSampler;

    // Custom shader cache (key = "vertPath|fragPath|skeletal")
    std::unordered_map<std::string, MaterialHandle> m_customShaderCache;

    // Cached 2D UI material compiled from a .lsh file. Retains the underlying GPU
    // resources so they can be released when the source file changes on disk.
    struct LshMaterialEntry {
        MaterialHandle material;
        ShaderHandle vertexShader;
        ShaderHandle fragmentShader;
        PipelineHandle pipeline;
        std::string sourceFingerprint;  // last-write-time stamp; empty in pack mode
    };
    // Key = "lshPath|blendMode|layout"
    std::unordered_map<std::string, LshMaterialEntry> m_lshMaterialCache;

    // Cache of samplers built from .lsh @filter/@repeat hints (Gap G), keyed by
    // "filter|wrap"; a handful at most, reused across all custom-shader textures.
    std::unordered_map<std::string, SamplerHandle> m_lshSamplerCache;
    SamplerHandle getOrCreateLshSampler(FilterMode filter, WrapMode wrap);

    // ----- Scene-texture post-processing (grab pass) -----
    // Offscreen target that captures the scene color so a post-process UI shader (a ColorRect
    // whose .lsh declares `u_SceneTexture`) can sample what's rendered behind it. Reused across
    // views and resized to match the current view.
    RenderTargetHandle m_sceneCaptureTarget;
    uint32_t m_sceneCaptureWidth = 0;
    uint32_t m_sceneCaptureHeight = 0;
    // Bound as u_SceneTexture during the post-process phase; invalid otherwise.
    TextureHandle m_postProcessSceneTexture;

    // Second capture target (color only) used as the per-grab-object snapshot in the
    // ordered mid-scene capture path (Gap C); reused/resized like m_sceneCaptureTarget.
    RenderTargetHandle m_sceneCapturePong;
    uint32_t m_sceneCapturePongWidth = 0;
    uint32_t m_sceneCapturePongHeight = 0;

    // Reusable fullscreen-blit resources (lazily created): a clip-space quad mesh and a
    // texture-copy pipeline (standard vertex layout, no depth, no cull, opaque).
    MeshHandle m_blitMesh;
    PipelineHandle m_blitPipeline;
    ShaderHandle m_blitVertexShader;
    ShaderHandle m_blitFragmentShader;

    /**
     * If the current view's batches contain any post-process (u_SceneTexture) material, render
     * the scene (excluding the post-process draws) into m_sceneCaptureTarget first, then re-render
     * the full scene + post-process draws into the real target with the captured scene bound as
     * u_SceneTexture. Returns true if it handled rendering (caller skips the normal path).
     */
    bool renderViewPostProcess(RenderView* view, RenderContext& ctx);

    /** Create/resize the scene-capture render target to (width, height). */
    bool ensureSceneCaptureTarget(uint32_t width, uint32_t height);

    // ----- WorldEnvironment-driven post-processing stack -----
    // The composable post-process chain (bloom, tonemap, color grading, SSAO,
    // vignette, overlays, ...). Driven by the view's active WorldEnvironment, so
    // each SubViewport subtree gets its own independent chain automatically.
    std::unique_ptr<PostProcessStack> m_postProcess;

    // HDR off-screen target the scene is rendered into when the post-process stack
    // is active for a view (RGBA16F color + depth, so SSAO can read scene depth).
    RenderTargetHandle m_hdrSceneTarget;
    uint32_t m_hdrSceneWidth = 0;
    uint32_t m_hdrSceneHeight = 0;

    /** Create/resize the HDR scene-capture target to (width, height). */
    bool ensureHdrSceneTarget(uint32_t width, uint32_t height);

    // Shared supersampling frame target (RGBA8 color + depth). All cameras composite
    // into this when render-scale supersampling is active for the window.
    //
    // The colour format is plain UNORM, NOT sRGB. The scene shaders already write
    // display-ready values and the swapchain applies the single sRGB encode on present;
    // an sRGB intermediate here would encode a second time (crushed/over-contrasty
    // "hard boiled" output). UNORM stores the values verbatim so the blit to the
    // swapchain reproduces the direct-render result exactly. This mirrors the existing
    // scene-capture target, which blits to screen with correct colours.
    RenderTargetHandle m_frameTarget;
    uint32_t m_frameTargetWidth = 0;
    uint32_t m_frameTargetHeight = 0;
    TextureFormat m_frameTargetFormat = TextureFormat::RGBA8_UNORM;

    /**
     * Populate post-process settings from a view's active WorldEnvironment (plus the
     * camera/timing data the renderer owns). Returns false when there is no
     * WorldEnvironment or post-processing is disabled.
     */
    bool gatherPostProcessSettings(RenderView* view, PostProcessSettings& out);

    /**
     * Render the view's scene (honoring the u_SceneTexture grab pass) into an
     * arbitrary render target instead of the view's real backing. Used to capture
     * the scene into the HDR target before the post-process chain runs.
     */
    void renderSceneInto(RenderView* view, RenderContext& ctx, RenderTargetHandle target);

    // ----- Per-camera stackable effect chain -----
    // Runs after the WorldEnvironment post-process, applying the ordered CameraEffect
    // components attached to the view's source camera node (blur/glow/outline/color
    // grade/...). Each camera gets its own chain; effects ping-pong between HDR targets.
    std::unique_ptr<CameraEffectStack> m_cameraEffects;

    // Intermediate HDR target used only when BOTH the post-process stack and a camera
    // effect chain are active for a view: the post-process writes here and the effect
    // chain reads it (RGBA16F so HDR survives into the effects).
    RenderTargetHandle m_effectInputTarget;
    uint32_t m_effectInputWidth = 0;
    uint32_t m_effectInputHeight = 0;
    bool ensureEffectInputTarget(uint32_t width, uint32_t height);

    /**
     * Build the resolved camera-effect chain for a view from the CameraEffect components
     * on its source camera node (plus camera/timing data the renderer owns). Returns false
     * when there is no source camera node, effects are disabled, or none are present/enabled.
     */
    bool gatherCameraEffects(RenderView* view, CameraEffectChain& out);

    // Per-frame state
    bool m_inFrame = false;
    uint32_t m_frameNumber = 0;
    std::unordered_set<RenderViewID> m_viewsWithActiveFrame;  // Views that had successful beginFrame this frame

    // Lighting state (per-view light data is now stored in RenderView)
    // Ring-buffered light UBOs to prevent CPU-GPU race conditions
    // Each frame in flight gets its own buffer to avoid writing while GPU is reading
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    UniformBufferHandle m_lightUniformBuffers[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_currentLightBufferIndex = 0;
    std::vector<TextureHandle> m_shadowMaps;              // 2D shadow maps for directional/spot lights
    std::vector<RenderTargetHandle> m_shadowMapFramebuffers;
    std::vector<uint32_t> m_shadowMapResolutions;         // Track resolution of each shadow map
    std::vector<TextureHandle> m_shadowCubeMaps;          // Cube map shadow maps for point lights
    std::vector<RenderTargetHandle> m_shadowCubeMapFramebuffers; // Framebuffers for cube map faces
    std::vector<uint32_t> m_shadowCubeMapResolutions;     // Track resolution of each cube map
    MaterialHandle m_shadowMapMaterial;                   // Material for 2D shadow maps
    MaterialHandle m_shadowMapInstancedMaterial;          // Instanced 2D shadow maps (MultiMesh)
    MaterialHandle m_shadowCubeMaterial;                  // Material for cube map shadows (point lights)
    MaterialHandle m_shadowCubeInstancedMaterial;         // Instanced cube map shadows (MultiMesh)
    MaterialHandle m_shadowMapSkeletalMaterial;           // Material for skeletal mesh shadow maps
    // Toggle for experimental cascaded shadow maps (directional lights)
    // Disabled by default until basic single-map shadows are fully working.
    bool m_enableCascadedShadowMaps = false;

    // Statistics
    RenderStats m_stats;

    // Temporary storage (reused each frame)
    std::vector<RenderPass> m_renderPasses;
    std::vector<const DrawItem*> m_sortBuffer;

    // Set during gatherRenderables() to true if any gathered (visible) renderable
    // reports isRenderContentDynamic(). Read by the shadow cache: a dynamic caster's
    // depth silhouette can change without advancing the render epoch, so its presence
    // forces shadow maps to be re-rendered every frame.
    bool m_AnyDynamicRenderableGathered = false;

    // Per-view cache of the gathered + batched draw list. Reused (gather +
    // buildRenderBatches skipped) while the render epoch, tree-structure version and
    // camera view-projection are all unchanged and the last gather contained no dynamic
    // renderable. Erased in destroyRenderView.
    struct DrawListCache {
        // Owned copy of the gathered DrawItems. RenderBatch::items are const DrawItem*
        // into the per-frame RenderContext storage (destroyed each frame), so the cache
        // keeps its own stable copy and rebases the batch pointers onto it.
        std::vector<DrawItem> drawItems;
        std::vector<RenderPass> passes;
        uint64_t epoch = 0;
        uint64_t treeVersion = 0;
        math::Mat4 cameraVP;
        bool valid = false;
    };
    std::unordered_map<RenderViewID, DrawListCache> m_drawListCache;

    // Per-view pool of recyclable dynamic buffers backing 2D geometry batches (see
    // GeometryBatchPool). Owned per view so cached draw lists can keep referencing a
    // view's combined-quad buffers across cache-hit frames; rebuilt only when that view
    // rebuilds. Buffers freed in destroyRenderView and shutdown.
    std::unordered_map<RenderViewID, GeometryBatchPool> m_geometryBatchPools;

    // Track which scenes have been prepared for GPU resource upload
    std::unordered_set<core::Scene*> m_preparedScenes;
};

} // namespace lupine
