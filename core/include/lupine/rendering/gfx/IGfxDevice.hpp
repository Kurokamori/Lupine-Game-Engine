#pragma once

#include "../ResourceHandles.hpp"
#include "GfxTypes.hpp"
#include "GfxDescriptors.hpp"
#include "GfxCommandList.hpp"
#include <memory>
#include <string>

namespace lupine {

// Forward declarations
struct MeshData;
struct GPUMesh;
struct FontDesc;
struct FontAtlas;

/**
 * Graphics device capabilities
 */
struct GfxDeviceCaps {
    GraphicsBackend backend;
    std::string deviceName;
    std::string apiVersion;

    uint32_t maxTextureSize;
    uint32_t maxTextureLayers;
    uint32_t maxVertexAttributes;
    uint32_t maxUniformBufferSize;

    bool supportsCompute;
    bool supportsGeometryShader;
    bool supportsTessellation;
    bool supportsBindless;
    bool supportsRayTracing;
};

/**
 * Abstract graphics device interface.
 * All rendering backends (OpenGL, Vulkan, DirectX, Metal, WebGL) implement this interface.
 *
 * Core rendering code interacts only with IGfxDevice - it never calls GL/Vulkan/DirectX functions directly.
 */
class IGfxDevice {
public:
    virtual ~IGfxDevice() = default;

    // ===== Initialization & Capabilities =====

    /**
     * Initialize the graphics device.
     * Called once during startup.
     */
    virtual bool initialize() = 0;

    /**
     * Shutdown the graphics device.
     * Called once during cleanup.
     */
    virtual void shutdown() = 0;

    /**
     * Get device capabilities.
     */
    virtual const GfxDeviceCaps& getCapabilities() const = 0;

    /**
     * Get the backend type.
     */
    virtual GraphicsBackend getBackend() const = 0;

    /**
     * Set default texture filtering mode for newly created textures.
     */
    virtual void setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) = 0;

    // ===== Swapchain Management =====

    /**
     * Create a swapchain for a window/surface.
     * Used for PyQt widgets, SDL windows, standalone windows, etc.
     */
    virtual SwapchainHandle createSwapchain(const SwapchainDesc& desc) = 0;

    /**
     * Destroy a swapchain.
     */
    virtual void destroySwapchain(SwapchainHandle swapchain) = 0;

    /**
     * Resize a swapchain (e.g., window was resized).
     */
    virtual void resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) = 0;

    /**
     * Present the swapchain to the screen.
     */
    virtual void present(SwapchainHandle swapchain) = 0;

    /**
     * Get the current backbuffer render target for a swapchain.
     */
    virtual RenderTargetHandle getSwapchainBackbuffer(SwapchainHandle swapchain) = 0;

    /**
     * Make the graphics context current for a swapchain.
     * This ensures the GPU context is active for resource creation and rendering.
     * For OpenGL: calls wglMakeCurrent/glXMakeCurrent/eglMakeCurrent
     * For Vulkan/DX11/DX12: may be a no-op or ensure device is ready
     * For WebGL: ensures the correct canvas context is active
     */
    virtual void makeContextCurrent(SwapchainHandle swapchain) = 0;

    /**
     * Set the swapchain to use for synchronization when rendering to off-screen targets.
     * This is critical for Vulkan when multiple views are rendering - each view's off-screen
     * rendering (like shadow maps) should use that view's swapchain for proper sync.
     * Call this before beginFrame() for off-screen targets.
     */
    virtual void setSwapchainHintForOffscreen(SwapchainHandle swapchain) = 0;

    // ===== Resource Creation =====

    /**
     * Create a texture.
     */
    virtual TextureHandle createTexture(const TextureDesc& desc) = 0;

    /**
     * Destroy a texture.
     */
    virtual void destroyTexture(TextureHandle texture) = 0;

    /**
     * Create a buffer (vertex, index, uniform, etc.).
     */
    virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;

    /**
     * Destroy a buffer.
     */
    virtual void destroyBuffer(BufferHandle buffer) = 0;

    /**
     * Create a sampler.
     */
    virtual SamplerHandle createSampler(const SamplerDesc& desc) = 0;

    /**
     * Destroy a sampler.
     */
    virtual void destroySampler(SamplerHandle sampler) = 0;

    /**
     * Create a shader from bytecode.
     */
    virtual ShaderHandle createShader(const ShaderDesc& desc) = 0;

    /**
     * Destroy a shader.
     */
    virtual void destroyShader(ShaderHandle shader) = 0;

    /**
     * Create a graphics pipeline.
     */
    virtual PipelineHandle createPipeline(const PipelineDesc& desc) = 0;

    /**
     * Destroy a pipeline.
     */
    virtual void destroyPipeline(PipelineHandle pipeline) = 0;

    /**
     * Return a variant of `base` whose color (render-target) format is `colorFormat`,
     * creating and caching it on first request. Backends that bake the render-target
     * format into the pipeline state (DirectX 12) require the bound RTV format to match
     * the pipeline exactly, so a pipeline created for the swapchain (e.g. RGBA8) cannot
     * be used to render into an HDR target (RGBA16F) without a matching variant.
     *
     * Default: returns `base` unchanged (backends that don't bake the format - OpenGL,
     * Vulkan, WebGL, Metal, DirectX 11 - need no variant). Returns `base` when
     * colorFormat is Unknown or already matches the base pipeline's format.
     */
    virtual PipelineHandle getColorFormatVariant(PipelineHandle base, TextureFormat colorFormat) {
        (void)colorFormat;
        return base;
    }

    /**
     * Create a render target (off-screen rendering).
     */
    virtual RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) = 0;

    /**
     * Destroy a render target.
     */
    virtual void destroyRenderTarget(RenderTargetHandle target) = 0;

    /**
     * Get the color texture from a render target.
     * Returns an invalid handle if the render target doesn't exist or has no color attachment.
     */
    virtual TextureHandle getRenderTargetColorTexture(RenderTargetHandle target) = 0;

    /**
     * Get the depth texture from a render target.
     * Returns an invalid handle if the render target doesn't exist or has no depth attachment.
     */
    virtual TextureHandle getRenderTargetDepthTexture(RenderTargetHandle target) = 0;

    /**
     * Get the color (render-target) format of a render target, including swapchain
     * backbuffers. Used to pick the matching pipeline variant (see getColorFormatVariant)
     * for the target currently being rendered into. Default returns Unknown for backends
     * that don't need format-matched pipelines.
     */
    virtual TextureFormat getRenderTargetColorFormat(RenderTargetHandle target) {
        (void)target;
        return TextureFormat::Unknown;
    }

    /**
     * Attach a specific cube map face to a render target's framebuffer.
     * This allows rendering to individual cube map faces without recreating framebuffers.
     * Only valid for render targets created with isCubeMap=true.
     *
     * @param target The render target handle
     * @param faceIndex The cube map face index (0-5: +X, -X, +Y, -Y, +Z, -Z)
     */
    virtual void attachCubeMapFace(RenderTargetHandle target, uint32_t faceIndex) = 0;

    /**
     * Unbind all framebuffers and bind the default framebuffer.
     * This is necessary after rendering to textures to ensure they can be sampled.
     */
    virtual void unbindFramebuffer() = 0;

    /**
     * Create a uniform buffer.
     */
    virtual UniformBufferHandle createUniformBuffer(uint32_t size) = 0;

    /**
     * Destroy a uniform buffer.
     */
    virtual void destroyUniformBuffer(UniformBufferHandle buffer) = 0;

    // ===== Resource Updates =====

    /**
     * Update buffer data.
     */
    virtual void updateBuffer(BufferHandle buffer, const void* data, uint64_t size, uint64_t offset = 0) = 0;

    /**
     * Update texture data.
     */
    virtual void updateTexture(TextureHandle texture, const void* data, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) = 0;

    /**
     * Update uniform buffer data.
     */
    virtual void updateUniformBuffer(UniformBufferHandle buffer, const void* data, uint32_t size, uint32_t offset = 0) = 0;

    // ===== Command Recording & Execution =====

    /**
     * Begin a new frame.
     * Returns a command list for recording GPU commands.
     */
    virtual std::unique_ptr<IGfxCommandList> beginFrame(RenderTargetHandle target) = 0;

    /**
     * Submit a command list for execution.
     */
    virtual void submit(std::unique_ptr<IGfxCommandList> commandList) = 0;

    /**
     * Wait for all GPU work to complete.
     * Used for synchronization, but should be avoided in hot paths.
     */
    virtual void waitIdle() = 0;

    // ===== Mesh Management =====

    /**
     * Create a mesh from mesh data.
     */
    virtual MeshHandle createMesh(const MeshData& meshData) = 0;

    /**
     * Get GPU mesh data.
     */
    virtual const GPUMesh* getMesh(MeshHandle handle) const = 0;

    /**
     * Destroy a mesh.
     */
    virtual void destroyMesh(MeshHandle handle) = 0;

    // ===== Font Management =====

    /**
     * Create a font atlas from a font file.
     * Loads the font, generates glyphs, and uploads the atlas texture to GPU.
     */
    virtual FontHandle createFontAtlas(const FontDesc& desc) = 0;

    /**
     * Get a font atlas by handle.
     * Returns nullptr if the handle is invalid.
     */
    virtual const FontAtlas* getFontAtlas(FontHandle handle) const = 0;

    /**
     * Destroy a font atlas and free GPU resources.
     */
    virtual void destroyFontAtlas(FontHandle handle) = 0;

    /**
     * Re-bake every live font atlas at the current global font oversampling factor
     * (see SetFontOversample). Each atlas is regenerated in place under the same
     * FontHandle, so existing handles stay valid; only the underlying texture and
     * glyph metrics change. Called by the runtime after a resolution change so UI
     * text stays crisp. The default implementation is a no-op for backends without
     * oversampling support.
     */
    virtual void refreshFontAtlases() {}

    // ===== Utility =====

    /**
     * Get a human-readable name for debugging.
     */
    virtual const char* getName() const = 0;

    /**
     * Check if the graphics context is valid and ready for rendering.
     * For WebGL: returns false if the context was lost (e.g., GPU driver reset, tab switched).
     * For other backends: typically returns true unless device is lost.
     *
     * Call this before rendering to gracefully handle context loss scenarios.
     * If false is returned, avoid making GL calls until context is restored.
     */
    virtual bool isContextValid() const { return true; }

    /**
     * Attempt to recover from context loss.
     * For WebGL: attempts to recreate the context and reload resources.
     * For other backends: may attempt device reset.
     *
     * @return true if recovery was successful, false otherwise
     */
    virtual bool tryRecoverContext() { return true; }
};

/**
 * Factory function to create a graphics device.
 * Implemented in each backend's source file.
 */
std::unique_ptr<IGfxDevice> createGraphicsDevice(GraphicsBackend backend);

} // namespace lupine
