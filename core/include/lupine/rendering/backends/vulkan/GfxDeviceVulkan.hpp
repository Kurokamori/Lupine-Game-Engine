#pragma once

#include "../../gfx/IGfxDevice.hpp"
#include <memory>

#ifdef LUPINE_HAS_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace lupine {

#ifdef LUPINE_HAS_VULKAN

// Forward declarations
class VulkanState;

/**
 * Vulkan graphics device implementation.
 * Supports Vulkan 1.2+.
 */
class GfxDeviceVulkan : public IGfxDevice {
public:
    GfxDeviceVulkan();
    ~GfxDeviceVulkan() override;

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
    void makeContextCurrent(SwapchainHandle swapchain) override;
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
    void updateBuffer(BufferHandle buffer, const void* data, uint64_t size, uint64_t offset = 0) override;
    void updateTexture(TextureHandle texture, const void* data, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;
    void updateUniformBuffer(UniformBufferHandle buffer, const void* data, uint32_t size, uint32_t offset = 0) override;

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
    const char* getName() const override { return "Vulkan"; }

    // ===== Vulkan-specific access (for internal use) =====
    VulkanState* getVulkanState();
    VkDevice getDevice();
    VkPhysicalDevice getPhysicalDevice();
    VkInstance getInstance();
    VkQueue getGraphicsQueue();
    uint32_t getGraphicsQueueFamily();

    // Create a pipeline variant for specific render target formats
    // Returns the VkPipeline handle, or VK_NULL_HANDLE on failure
    VkPipeline createPipelineVariant(PipelineHandle handle, VkFormat colorFormat, VkFormat depthFormat);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    FontAtlas buildBakedAtlas(const struct BakedFontAtlas& baked);

    void createDefaultResources();
    void destroySwapchainResources(struct VulkanSwapchain& swapchain);
    void initializeDescriptorSets(struct VulkanPipeline& pipeline);
    VkPipeline createPipelineVariantInternal(VulkanPipeline& pipeline, VkFormat colorFormat, VkFormat depthFormat);

    // Helper methods
    void getSwapchainSize(SwapchainHandle handle, uint32_t& width, uint32_t& height);
    void* mapBuffer(BufferHandle handle);
    void unmapBuffer(BufferHandle handle);
    void updateTextureFace(TextureHandle handle, uint32_t face, const void* data, uint64_t size, uint32_t mipLevel);
    void generateMipmaps(TextureHandle handle);
    void getTextureSize(TextureHandle handle, uint32_t& width, uint32_t& height);
    void getRenderTargetSize(RenderTargetHandle handle, uint32_t& width, uint32_t& height);
    SamplerHandle getDefaultSampler();
    TextureHandle getWhiteTexture();
    TextureHandle getBlackTexture();
    TextureHandle getNormalTexture();

    // Device capabilities
    GfxDeviceCaps m_caps{};
    FilterMode m_defaultMinFilter = FilterMode::Linear;
    FilterMode m_defaultMagFilter = FilterMode::Linear;
};

#endif // LUPINE_HAS_VULKAN

} // namespace lupine
