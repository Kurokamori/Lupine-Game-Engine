#pragma once

#ifdef LUPINE_HAS_DIRECTX12

#include "../gfx/IGfxDevice.hpp"

namespace lupine {

/**
 * DirectX 12 graphics device implementation.
 * Supports DirectX 12 on Windows 10+.
 *
 * DirectX 12 is a low-level, explicit graphics API that provides:
 * - Fine-grained control over GPU resources and memory
 * - Explicit synchronization and resource state management
 * - Multi-threaded command recording
 * - Better CPU/GPU parallelism
 *
 * This implementation follows the same patterns as other backends while
 * taking advantage of DX12's explicit nature for better performance.
 */
class GfxDeviceDX12 : public IGfxDevice {
public:
    GfxDeviceDX12();
    ~GfxDeviceDX12() override;

    // IGfxDevice interface
    bool initialize() override;
    void shutdown() override;
    const GfxDeviceCaps& getCapabilities() const override;
    GraphicsBackend getBackend() const override;
    void setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) override;

    SwapchainHandle createSwapchain(const SwapchainDesc& desc) override;
    void destroySwapchain(SwapchainHandle swapchain) override;
    void resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) override;
    void present(SwapchainHandle swapchain) override;
    RenderTargetHandle getSwapchainBackbuffer(SwapchainHandle swapchain) override;
    void makeContextCurrent(SwapchainHandle swapchain) override;
    void setSwapchainHintForOffscreen(SwapchainHandle swapchain) override;

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
    PipelineHandle getColorFormatVariant(PipelineHandle base, TextureFormat colorFormat) override;

    RenderTargetHandle createRenderTarget(const RenderTargetDesc& desc) override;
    void destroyRenderTarget(RenderTargetHandle target) override;
    TextureHandle getRenderTargetColorTexture(RenderTargetHandle target) override;
    TextureHandle getRenderTargetDepthTexture(RenderTargetHandle target) override;
    TextureFormat getRenderTargetColorFormat(RenderTargetHandle target) override;
    void attachCubeMapFace(RenderTargetHandle target, uint32_t face) override;
    void unbindFramebuffer() override;

    UniformBufferHandle createUniformBuffer(uint32_t size) override;
    void destroyUniformBuffer(UniformBufferHandle buffer) override;

    void updateBuffer(BufferHandle buffer, const void* data, uint64_t size, uint64_t offset) override;
    void updateTexture(TextureHandle texture, const void* data, uint32_t mipLevel, uint32_t arrayLayer) override;
    void updateUniformBuffer(UniformBufferHandle buffer, const void* data, uint32_t size, uint32_t offset) override;

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

    const char* getName() const override { return "DirectX 12"; }
    bool isContextValid() const override;

private:
    FontAtlas buildBakedAtlas(const struct BakedFontAtlas& baked);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12
