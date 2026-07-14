#pragma once

#include "../gfx/IGfxDevice.hpp"

namespace lupine {

#ifdef LUPINE_HAS_METAL

/**
 * Metal graphics device implementation.
 * Supports Metal 2.0+ on macOS and iOS.
 *
 * Metal is Apple's low-level graphics API, providing:
 * - Direct GPU access with minimal driver overhead
 * - Unified memory architecture on Apple Silicon
 * - Automatic resource tracking and memory management via ARC
 * - First-class support for compute shaders
 *
 * Key differences from other backends:
 * - Uses top-left origin and [0,1] depth range (like DirectX/Vulkan)
 * - Deferred rendering model with command buffers and encoders
 * - Metal Shading Language (MSL) for shaders
 * - Triple buffering is standard for frame synchronization
 */
class GfxDeviceMetal : public IGfxDevice {
public:
    GfxDeviceMetal();
    ~GfxDeviceMetal() override;

    // ===== Initialization & Capabilities =====
    bool initialize() override;
    void shutdown() override;
    const GfxDeviceCaps& getCapabilities() const override;
    GraphicsBackend getBackend() const override;
    void setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) override;

    // ===== Swapchain Management =====
    SwapchainHandle createSwapchain(const SwapchainDesc& desc) override;
    void destroySwapchain(SwapchainHandle swapchain) override;
    void resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) override;
    void present(SwapchainHandle swapchain) override;
    RenderTargetHandle getSwapchainBackbuffer(SwapchainHandle swapchain) override;
    void setSwapchainHintForOffscreen(SwapchainHandle swapchain) override;

    // ===== Resource Creation =====
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

    // ===== Resource Updates =====
    void updateBuffer(BufferHandle buffer, const void* data, uint64_t size, uint64_t offset) override;
    void updateTexture(TextureHandle texture, const void* data, uint32_t mipLevel, uint32_t arrayLayer) override;
    void updateUniformBuffer(UniformBufferHandle buffer, const void* data, uint32_t size, uint32_t offset) override;

    // ===== Command Recording & Execution =====
    std::unique_ptr<IGfxCommandList> beginFrame(RenderTargetHandle target) override;
    void submit(std::unique_ptr<IGfxCommandList> commandList) override;
    void waitIdle() override;

    // ===== Mesh Management =====
    MeshHandle createMesh(const MeshData& meshData) override;
    const GPUMesh* getMesh(MeshHandle handle) const override;
    void destroyMesh(MeshHandle handle) override;

    // ===== Font Management =====
    FontHandle createFontAtlas(const FontDesc& desc) override;
    const FontAtlas* getFontAtlas(FontHandle handle) const override;
    void destroyFontAtlas(FontHandle handle) override;
    void refreshFontAtlases() override;

    // ===== Utility =====
    const char* getName() const override { return "Metal"; }

    // ===== Compute Pipeline (Metal-specific) =====
    /**
     * Create a compute pipeline for GPU compute operations.
     * @param computeShader Shader handle for the compute kernel
     * @param threadGroupSizeX Thread group size in X dimension
     * @param threadGroupSizeY Thread group size in Y dimension
     * @param threadGroupSizeZ Thread group size in Z dimension
     * @return Compute pipeline ID, or 0 on failure
     */
    uint32_t createComputePipeline(ShaderHandle computeShader,
                                   uint32_t threadGroupSizeX = 8,
                                   uint32_t threadGroupSizeY = 8,
                                   uint32_t threadGroupSizeZ = 1);

    /**
     * Destroy a compute pipeline.
     * @param pipelineId The compute pipeline ID to destroy
     */
    void destroyComputePipeline(uint32_t pipelineId);

private:
    FontAtlas buildBakedAtlas(const struct BakedFontAtlas& baked);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#else // !LUPINE_HAS_METAL

/**
 * Stub Metal graphics device for non-Apple platforms.
 * Returns nullptr/false for all operations.
 */
class GfxDeviceMetal : public IGfxDevice {
public:
    GfxDeviceMetal() = default;
    ~GfxDeviceMetal() override = default;

    bool initialize() override { return false; }
    void shutdown() override {}
    const GfxDeviceCaps& getCapabilities() const override { static GfxDeviceCaps caps; return caps; }
    GraphicsBackend getBackend() const override { return GraphicsBackend::Metal; }
    void setDefaultTextureFiltering(FilterMode, FilterMode) override {}

    SwapchainHandle createSwapchain(const SwapchainDesc&) override { return SwapchainHandle(); }
    void destroySwapchain(SwapchainHandle) override {}
    void resizeSwapchain(SwapchainHandle, uint32_t, uint32_t) override {}
    void present(SwapchainHandle) override {}
    RenderTargetHandle getSwapchainBackbuffer(SwapchainHandle) override { return RenderTargetHandle(); }
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

    const char* getName() const override { return "Metal (Stub)"; }
};

#endif // LUPINE_HAS_METAL

} // namespace lupine
