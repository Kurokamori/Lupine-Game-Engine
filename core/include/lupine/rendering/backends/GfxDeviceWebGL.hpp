#pragma once

#include "../gfx/IGfxDevice.hpp"
#include "../Mesh.hpp"
#include "../Font.hpp"

#ifdef __EMSCRIPTEN__

namespace lupine {

/**
 * WebGL graphics device implementation.
 * Supports WebGL 2.0 (OpenGL ES 3.0) compiled via Emscripten.
 *
 * WebGL 2.0 provides most OpenGL ES 3.0 features including:
 * - Vertex Array Objects (VAOs)
 * - Uniform Buffer Objects (UBOs)
 * - Multiple render targets
 * - 3D textures
 * - Sampler objects
 * - Transform feedback
 * - Instanced rendering
 *
 * Limitations compared to desktop OpenGL:
 * - No geometry shaders
 * - No tessellation shaders
 * - No compute shaders
 * - Limited texture formats (no BC compression, limited float formats)
 * - Single context only
 */
class GfxDeviceWebGL : public IGfxDevice {
public:
    GfxDeviceWebGL();
    ~GfxDeviceWebGL() override;

    // IGfxDevice interface
    bool initialize() override;
    void shutdown() override;
    const GfxDeviceCaps& getCapabilities() const override;
    GraphicsBackend getBackend() const override;
    void setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) override;

    // Swapchain Management
    SwapchainHandle createSwapchain(const SwapchainDesc& desc) override;
    void destroySwapchain(SwapchainHandle swapchain) override;
    void resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) override;
    void present(SwapchainHandle swapchain) override;
    RenderTargetHandle getSwapchainBackbuffer(SwapchainHandle swapchain) override;
    void makeContextCurrent(SwapchainHandle swapchain) override;
    void setSwapchainHintForOffscreen(SwapchainHandle swapchain) override;

    // Resource Creation
    TextureHandle createTexture(const TextureDesc& desc) override;
    void destroyTexture(TextureHandle texture) override;

    BufferHandle createBuffer(const BufferDesc& desc) override;
    void destroyBuffer(BufferHandle buffer) override;

    SamplerHandle createSampler(const SamplerDesc& desc) override;
    void destroySampler(SamplerHandle sampler) override;

    ShaderHandle createShader(const ShaderDesc& desc) override;
    void destroyShader(ShaderHandle shader) override;

    PipelineHandle createPipeline(const PipelineDesc& desc) override;
    void destroyPipeline(PipelineHandle pipeline) override;

    RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) override;
    void destroyRenderTarget(RenderTargetHandle target) override;
    TextureHandle getRenderTargetColorTexture(RenderTargetHandle target) override;
    TextureHandle getRenderTargetDepthTexture(RenderTargetHandle target) override;
    void attachCubeMapFace(RenderTargetHandle target, uint32_t faceIndex) override;
    void unbindFramebuffer() override;

    UniformBufferHandle createUniformBuffer(uint32_t size) override;
    void destroyUniformBuffer(UniformBufferHandle buffer) override;

    // Resource Updates
    void updateBuffer(BufferHandle buffer, const void* data, uint64_t size, uint64_t offset) override;
    void updateTexture(TextureHandle texture, const void* data, uint32_t mipLevel, uint32_t arrayLayer) override;
    void updateUniformBuffer(UniformBufferHandle buffer, const void* data, uint32_t size, uint32_t offset) override;

    // Command Recording & Execution
    std::unique_ptr<IGfxCommandList> beginFrame(RenderTargetHandle target) override;
    void submit(std::unique_ptr<IGfxCommandList> commandList) override;
    void waitIdle() override;

    // Mesh Management
    MeshHandle createMesh(const MeshData& meshData) override;
    const GPUMesh* getMesh(MeshHandle handle) const override;
    void destroyMesh(MeshHandle handle) override;

    // Font Management
    FontHandle createFontAtlas(const FontDesc& desc) override;
    const FontAtlas* getFontAtlas(FontHandle handle) const override;
    void destroyFontAtlas(FontHandle handle) override;
    void refreshFontAtlases() override;

    const char* getName() const override { return "WebGL 2.0"; }

    // Context validity and recovery
    bool isContextValid() const override;
    bool tryRecoverContext() override;

    /**
     * Called when WebGL context is lost (from JavaScript callback).
     * Marks all resources as invalid and stops rendering.
     */
    void onContextLost();

    /**
     * Called when WebGL context is restored (from JavaScript callback).
     * Attempts to recreate critical resources.
     */
    void onContextRestored();

private:
    FontAtlas buildBakedAtlas(const struct BakedFontAtlas& baked);

    /**
     * Initialize WebGL context via Emscripten
     */
    bool initializeWebGLContext();

    /**
     * Query WebGL capabilities
     */
    void queryCapabilities();

    /**
     * Register Emscripten callbacks for context loss events
     */
    void registerContextLossCallbacks();

    /**
     * Invalidate all GPU resource handles after context loss
     */
    void invalidateAllResources();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lupine

#else // !__EMSCRIPTEN__

// Stub implementation for non-Emscripten builds
namespace lupine {

class GfxDeviceWebGL : public IGfxDevice {
public:
    GfxDeviceWebGL() = default;
    ~GfxDeviceWebGL() override = default;

    bool initialize() override { return false; }
    void shutdown() override {}
    const GfxDeviceCaps& getCapabilities() const override { static GfxDeviceCaps caps; return caps; }
    GraphicsBackend getBackend() const override { return GraphicsBackend::WebGL; }
    void setDefaultTextureFiltering(FilterMode, FilterMode) override {}

    SwapchainHandle createSwapchain(const SwapchainDesc&) override { return SwapchainHandle(); }
    void destroySwapchain(SwapchainHandle) override {}
    void resizeSwapchain(SwapchainHandle, uint32_t, uint32_t) override {}
    void present(SwapchainHandle) override {}
    RenderTargetHandle getSwapchainBackbuffer(SwapchainHandle) override { return RenderTargetHandle(); }
    void makeContextCurrent(SwapchainHandle) override {}
    void setSwapchainHintForOffscreen(SwapchainHandle) override {}

    TextureHandle createTexture(const TextureDesc&) override { return TextureHandle(); }
    void destroyTexture(TextureHandle) override {}

    BufferHandle createBuffer(const BufferDesc&) override { return BufferHandle(); }
    void destroyBuffer(BufferHandle) override {}

    SamplerHandle createSampler(const SamplerDesc&) override { return SamplerHandle(); }
    void destroySampler(SamplerHandle) override {}

    ShaderHandle createShader(const ShaderDesc&) override { return ShaderHandle(); }
    void destroyShader(ShaderHandle) override {}

    PipelineHandle createPipeline(const PipelineDesc&) override { return PipelineHandle(); }
    void destroyPipeline(PipelineHandle) override {}

    RenderTargetHandle createRenderTarget(const RenderTargetDesc&) override { return RenderTargetHandle(); }
    void destroyRenderTarget(RenderTargetHandle) override {}
    TextureHandle getRenderTargetColorTexture(RenderTargetHandle) override { return TextureHandle(); }
    TextureHandle getRenderTargetDepthTexture(RenderTargetHandle) override { return TextureHandle(); }
    void attachCubeMapFace(RenderTargetHandle, uint32_t) override {}
    void unbindFramebuffer() override {}

    UniformBufferHandle createUniformBuffer(uint32_t) override { return UniformBufferHandle(); }
    void destroyUniformBuffer(UniformBufferHandle) override {}

    void updateBuffer(BufferHandle, const void*, uint64_t, uint64_t) override {}
    void updateTexture(TextureHandle, const void*, uint32_t, uint32_t) override {}
    void updateUniformBuffer(UniformBufferHandle, const void*, uint32_t, uint32_t) override {}

    std::unique_ptr<IGfxCommandList> beginFrame(RenderTargetHandle) override { return nullptr; }
    void submit(std::unique_ptr<IGfxCommandList>) override {}
    void waitIdle() override {}

    MeshHandle createMesh(const MeshData&) override { return MeshHandle(); }
    const GPUMesh* getMesh(MeshHandle) const override { return nullptr; }
    void destroyMesh(MeshHandle) override {}

    FontHandle createFontAtlas(const FontDesc&) override { return FontHandle(); }
    const FontAtlas* getFontAtlas(FontHandle) const override { return nullptr; }
    void destroyFontAtlas(FontHandle) override {}

    const char* getName() const override { return "WebGL (unavailable)"; }
};

} // namespace lupine

#endif // __EMSCRIPTEN__
