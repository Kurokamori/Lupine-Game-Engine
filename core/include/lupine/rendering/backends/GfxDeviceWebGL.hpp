#pragma once

#include "../gfx/IGfxDevice.hpp"

namespace lupine {

/**
 * WebGL graphics device implementation.
 * Supports WebGL 2.0 (compiled via Emscripten).
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

    SwapchainHandle createSwapchain(const SwapchainDesc& desc) override;
    void destroySwapchain(SwapchainHandle swapchain) override;
    void resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) override;
    void present(SwapchainHandle swapchain) override;
    RenderTargetHandle getSwapchainBackbuffer(SwapchainHandle swapchain) override;

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

    const char* getName() const override { return "WebGL"; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lupine
