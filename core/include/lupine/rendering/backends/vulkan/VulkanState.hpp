#pragma once

#include "../../ResourceHandles.hpp"
#include "../../gfx/GfxTypes.hpp"
#include "../../gfx/GfxDescriptors.hpp"

#ifdef LUPINE_HAS_VULKAN
#include <vulkan/vulkan.h>
#endif

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

namespace lupine {

#ifdef LUPINE_HAS_VULKAN

// Forward declarations
struct VulkanBuffer;
struct VulkanTexture;
struct VulkanSampler;
struct VulkanShader;
struct VulkanPipeline;
struct VulkanRenderTarget;
struct VulkanSwapchain;
struct VulkanUniformBuffer;

/**
 * Vulkan buffer structure
 */
struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    void* mappedData = nullptr;
};

/**
 * Vulkan texture structure
 */
struct VulkanTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkImageView* cubeViews = nullptr;  // For cube maps, one view per face
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    TextureFormat format = TextureFormat::Unknown;
    TextureType type = TextureType::Texture2D;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool isSwapchainImage = false;  // Swapchain images are not owned
};

/**
 * Vulkan sampler structure
 */
struct VulkanSampler {
    VkSampler sampler = VK_NULL_HANDLE;
    SamplerDesc desc;
};

/**
 * A reflected uniform block member: its byte offset and size within the block.
 * Used to route engine uniforms (set by name, or carried in the canonical
 * push-constant blob) to the exact location each shader expects.
 */
struct VulkanUniformMember {
    uint32_t offset = 0;
    uint32_t size = 0;
};

/**
 * Vulkan shader structure
 */
struct VulkanShader {
    VkShaderModule module = VK_NULL_HANDLE;
    ShaderStage stage;
    std::string entryPoint = "main";
    std::vector<uint32_t> spirvCode;  // Store compiled SPIR-V code to keep it alive

    // Reflected uniform layout (parsed from GLSL source). These let the command
    // list write each named uniform / canonical push-constant-blob field to the
    // exact offset this shader declares, instead of assuming a fixed layout.
    std::unordered_map<std::string, VulkanUniformMember> pushConstantMembers;  // push_constant block
    std::unordered_map<std::string, VulkanUniformMember> materialUBOMembers;   // set=0, binding=2 block
    uint32_t materialUBOSize = 0;
};

/**
 * Pipeline format key for variant caching
 */
struct PipelineFormatKey {
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    bool operator==(const PipelineFormatKey& other) const {
        return colorFormat == other.colorFormat && depthFormat == other.depthFormat;
    }
};

struct PipelineFormatKeyHash {
    size_t operator()(const PipelineFormatKey& key) const {
        return std::hash<uint64_t>{}(static_cast<uint64_t>(key.colorFormat) |
                                     (static_cast<uint64_t>(key.depthFormat) << 32));
    }
};

/**
 * A single pipeline variant for a specific format combination
 */
struct VulkanPipelineVariant {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
};

/**
 * Vulkan pipeline structure with format variant support
 */
struct VulkanPipeline {
    // Shared resources (same across all variants)
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    // Dynamic descriptor set allocation for per-draw updates
    // Each frame has DESCRIPTOR_SETS_PER_FRAME sets to allow multiple draws with different textures
    // OPTIMIZATION: Reduced from 4096 to 512 - most scenes use far fewer draw calls
    // This dramatically reduces memory allocation and initialization time per pipeline
    static constexpr uint32_t MAX_FRAMES = 2;  // Must match VulkanState::MAX_FRAMES_IN_FLIGHT
    static constexpr uint32_t DESCRIPTOR_SETS_PER_FRAME = 512;  // Reduced from 4096 for better performance
    uint32_t nextDescriptorSetIndex[MAX_FRAMES] = {0, 0};

    // Pipeline configuration (for recreating variants)
    std::vector<ShaderHandle> shaders;
    VertexBufferLayout vertexLayout;
    std::vector<VertexBufferLayout> extraVertexBuffers;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    BlendState blendState;
    DepthStencilState depthStencilState;
    RasterizerState rasterizerState;

    // Uniform locations cache (for push constants), merged across this
    // pipeline's shaders. Populated from per-shader reflection in createPipeline.
    std::unordered_map<std::string, uint32_t> pushConstantOffsets;

    // Per-material uniform block (set=0, binding=2) member layout, merged across
    // this pipeline's shaders. Engine uniforms named here are written into the
    // per-draw material UBO at the reflected offset. materialUBOSize is the
    // std140 size of the largest material block among the pipeline's shaders.
    std::unordered_map<std::string, VulkanUniformMember> materialUBOOffsets;
    uint32_t materialUBOSize = 0;

    // Pipeline variants for different format combinations
    // Key: (colorFormat, depthFormat) -> VkPipeline
    std::unordered_map<PipelineFormatKey, VulkanPipelineVariant, PipelineFormatKeyHash> variants;

    // Default/fallback pipeline (first created variant)
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    // Get or indicate need to create variant for given formats
    VkPipeline getVariant(VkFormat color, VkFormat depth) const {
        PipelineFormatKey key{color, depth};
        auto it = variants.find(key);
        if (it != variants.end()) {
            return it->second.pipeline;
        }
        return VK_NULL_HANDLE;  // Variant needs to be created
    }

    // Get the next available descriptor set for this frame (for per-draw texture binding)
    uint32_t getNextDescriptorSetIndex(uint32_t frameIndex) {
        uint32_t localIndex = nextDescriptorSetIndex[frameIndex];
        uint32_t index = frameIndex * DESCRIPTOR_SETS_PER_FRAME + localIndex;
        nextDescriptorSetIndex[frameIndex] = (localIndex + 1) % DESCRIPTOR_SETS_PER_FRAME;

        // Warn if we're wrapping around (using more descriptor sets than allocated per frame)
        if (localIndex == DESCRIPTOR_SETS_PER_FRAME - 1) {
            // This would wrap around on the next call - may indicate too many draw calls
        }
        return index;
    }

    // Allocate and return the next available descriptor set for this frame
    VkDescriptorSet allocateDescriptorSet(uint32_t frameIndex) {
        uint32_t index = getNextDescriptorSetIndex(frameIndex);
        if (index < descriptorSets.size()) {
            return descriptorSets[index];
        }
        return VK_NULL_HANDLE;
    }

    // Reset descriptor set allocation for a frame (call at start of frame)
    void resetDescriptorSetAllocation(uint32_t frameIndex) {
        if (frameIndex < MAX_FRAMES) {
            nextDescriptorSetIndex[frameIndex] = 0;
        }
    }
};

/**
 * Vulkan render target structure
 */
struct VulkanRenderTarget {
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;  // Legacy - kept for compatibility, not used with dynamic rendering
    VkImageView colorView = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;

    // VkImage handles for dynamic rendering layout transitions
    VkImage colorImage = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;

    // Vulkan format info for dynamic rendering
    VkFormat vkColorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat vkDepthFormat = VK_FORMAT_D32_SFLOAT;

    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    TextureFormat depthFormat = TextureFormat::Unknown;
    bool hasColor = true;
    bool hasDepth = true;
    bool hasStencil = false;  // For combined depth-stencil formats
    bool isSwapchainBackbuffer = false;
    bool isCubeMap = false;
    int currentCubeFace = -1;

    // If this is a swapchain backbuffer, store the swapchain ID for render pass tracking
    uint32_t owningSwapchainId = 0;

    // Handles to the textures
    TextureHandle colorTextureHandle;
    TextureHandle depthTextureHandle;
};

/**
 * Vulkan swapchain structure
 */
struct VulkanSwapchain {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    NativeWindowHandle window;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    VkFormat vkColorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    bool vsync = true;

    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    uint32_t currentImageIndex = 0;

    VkRenderPass renderPass = VK_NULL_HANDLE;

    // Depth buffer
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;

    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    // Per-swapchain command buffers (one per frame in flight)
    std::vector<VkCommandBuffer> commandBuffers;

    // Track if a render pass was used this frame (per frame in flight)
    // If true, the render pass's finalLayout already transitioned to PRESENT_SRC_KHR
    // If false, we need an explicit barrier in present()
    std::vector<bool> renderPassUsed;

    // Track if an image was successfully acquired for this frame (per frame in flight)
    // If false, present() should not attempt to present
    std::vector<bool> imageAcquired;

    // Associated render target
    RenderTargetHandle backbuffer;

    // Mark swapchain as dead when surface becomes invalid (e.g., window destroyed)
    // Dead swapchains should not be used for rendering
    bool isDead = false;
};

/**
 * Vulkan uniform buffer structure
 */
struct VulkanUniformBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t size = 0;
    void* mappedData = nullptr;
};

/**
 * Vulkan state manager.
 * Manages all Vulkan resources and state tracking.
 */
class VulkanState {
public:
    VulkanState();
    ~VulkanState();

    // Vulkan core objects
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = UINT32_MAX;
    uint32_t presentQueueFamily = UINT32_MAX;

    // Command pool
    VkCommandPool commandPool = VK_NULL_HANDLE;

    // OPTIMIZATION: Pipeline cache for faster pipeline compilation
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;

    // Debug messenger (only in debug builds)
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    // Resource storage
    std::unordered_map<uint32_t, VulkanBuffer> buffers;
    std::unordered_map<uint32_t, VulkanTexture> textures;
    std::unordered_map<uint32_t, VulkanSampler> samplers;
    std::unordered_map<uint32_t, VulkanShader> shaders;
    std::unordered_map<uint32_t, VulkanPipeline> pipelines;
    std::unordered_map<uint32_t, VulkanRenderTarget> renderTargets;
    std::unordered_map<uint32_t, VulkanSwapchain> swapchains;
    std::unordered_map<uint32_t, VulkanUniformBuffer> uniformBuffers;

    // Resource ID generators
    uint32_t nextBufferID = 1;
    uint32_t nextTextureID = 1;
    uint32_t nextSamplerID = 1;
    uint32_t nextShaderID = 1;
    uint32_t nextPipelineID = 1;
    uint32_t nextRenderTargetID = 1;
    uint32_t nextSwapchainID = 1;
    uint32_t nextUniformBufferID = 1;

    // Physical device properties
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    // OPTIMIZATION: Cached device limits to avoid per-draw vkGetPhysicalDeviceProperties calls
    VkDeviceSize minUniformBufferOffsetAlignment = 256;  // Safe default, updated during init

    // Current state tracking
    VkCommandBuffer currentCommandBuffer = VK_NULL_HANDLE;
    VulkanSwapchain* currentSwapchain = nullptr;
    VulkanRenderTarget* currentRenderTarget = nullptr;
    VulkanPipeline* currentPipeline = nullptr;

    // Max frames in flight
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    // Dummy resources for descriptor set initialization
    // These are used to initialize all descriptor bindings to valid values
    VkBuffer dummyUniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory dummyUniformBufferMemory = VK_NULL_HANDLE;

    // Dedicated dummy light UBO - must be large enough for LightUniformBuffer (~7KB)
    // and initialized with lightCounts = 0 to prevent shader loops
    static constexpr size_t DUMMY_LIGHT_UBO_SIZE = 8192;  // 8KB to be safe
    VkBuffer dummyLightUBO = VK_NULL_HANDLE;
    VkDeviceMemory dummyLightUBOMemory = VK_NULL_HANDLE;

    VkImage dummyTexture = VK_NULL_HANDLE;
    VkDeviceMemory dummyTextureMemory = VK_NULL_HANDLE;
    VkImageView dummyTextureView = VK_NULL_HANDLE;
    VkSampler dummySampler = VK_NULL_HANDLE;

    // Dummy cube map for shadow cube map descriptor bindings
    VkImage dummyCubeMap = VK_NULL_HANDLE;
    VkDeviceMemory dummyCubeMapMemory = VK_NULL_HANDLE;
    VkImageView dummyCubeMapView = VK_NULL_HANDLE;

    // Methods to create/destroy dummy resources
    void createDummyResources();
    void destroyDummyResources();

    // ============================================================================
    // BINDLESS TEXTURE SYSTEM
    // ============================================================================
    // Bindless textures allow any shader to access any texture via an index,
    // eliminating per-draw descriptor set updates that cause the texture binding bug.

    static constexpr uint32_t MAX_BINDLESS_TEXTURES = 16384;  // Support up to 16K textures
    static constexpr uint32_t MAX_BINDLESS_SAMPLERS = 32;     // Reasonable sampler limit
    static constexpr uint32_t BINDLESS_TEXTURE_SET = 1;       // Descriptor set index for bindless

    // Bindless descriptor resources
    VkDescriptorSetLayout bindlessDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool bindlessDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet bindlessDescriptorSet = VK_NULL_HANDLE;  // Single global set

    // Texture index management - maps texture handle ID to bindless array index
    std::unordered_map<uint32_t, uint32_t> textureToBindlessIndex;
    std::vector<uint32_t> freeBindlessIndices;  // Recycled indices from destroyed textures
    uint32_t nextBindlessIndex = 0;  // Next index to allocate if freeBindlessIndices is empty

    // Sampler index management
    std::unordered_map<uint32_t, uint32_t> samplerToBindlessIndex;
    std::vector<uint32_t> freeBindlessSamplerIndices;
    uint32_t nextBindlessSamplerIndex = 0;

    // Feature support flags
    bool bindlessSupported = false;
    bool descriptorIndexingSupported = false;

    // Bindless system methods
    void createBindlessResources();
    void destroyBindlessResources();
    uint32_t allocateBindlessTextureIndex(uint32_t textureHandleId, VkImageView imageView);
    void freeBindlessTextureIndex(uint32_t textureHandleId);
    uint32_t getBindlessTextureIndex(uint32_t textureHandleId) const;
    uint32_t allocateBindlessSamplerIndex(uint32_t samplerHandleId, VkSampler sampler);
    void freeBindlessSamplerIndex(uint32_t samplerHandleId);
    uint32_t getBindlessSamplerIndex(uint32_t samplerHandleId) const;
    void updateBindlessTexture(uint32_t bindlessIndex, VkImageView imageView);
    void updateBindlessSampler(uint32_t bindlessIndex, VkSampler sampler);

    // Deferred resource destruction
    // Resources are kept alive for MAX_FRAMES_IN_FLIGHT frames before destruction
    struct DeferredPipelineDestroy {
        VulkanPipeline pipeline;
        uint32_t framesRemaining;
    };
    struct DeferredShaderDestroy {
        VulkanShader shader;
        uint32_t framesRemaining;
    };
    struct DeferredBufferDestroy {
        VulkanBuffer buffer;
        uint32_t bufferId;  // Store the ID so we can remove from map when actually destroyed
        uint32_t framesRemaining;
    };
    struct DeferredTextureDestroy {
        VulkanTexture texture;
        uint32_t framesRemaining;
    };

    std::vector<DeferredPipelineDestroy> deferredPipelineDestroys;
    std::vector<DeferredShaderDestroy> deferredShaderDestroys;
    std::vector<DeferredBufferDestroy> deferredBufferDestroys;
    std::vector<DeferredTextureDestroy> deferredTextureDestroys;

    // Helper methods
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void transitionImageLayout(VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t mipLevels = 1, uint32_t layerCount = 1,
                               uint32_t baseMipLevel = 0, uint32_t baseArrayLayer = 0);
    void copyBufferToImage(VkBuffer buffer, VkImage image,
                           uint32_t width, uint32_t height, uint32_t depth = 1);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory);
    void createImage(uint32_t width, uint32_t height, uint32_t depth,
                     uint32_t mipLevels, uint32_t arrayLayers,
                     VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkImageCreateFlags flags,
                     VkImage& image, VkDeviceMemory& memory);
    VkImageView createImageView(VkImage image, VkFormat format,
                                VkImageAspectFlags aspectFlags,
                                VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
                                uint32_t baseMipLevel = 0, uint32_t mipLevels = 1,
                                uint32_t baseArrayLayer = 0, uint32_t layerCount = 1);
    void generateMipmaps(VkImage image, VkFormat format,
                         uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels);

    // Deferred destruction methods
    void deferPipelineDestroy(const VulkanPipeline& pipeline);
    void deferShaderDestroy(const VulkanShader& shader);
    void deferBufferDestroy(const VulkanBuffer& buffer, uint32_t bufferId);
    void deferBufferDestroyNoMap(const VulkanBuffer& buffer);  // For buffers not in the buffers map
    void deferTextureDestroy(const VulkanTexture& texture);

    // Process deferred destructions - call once per frame after present
    void processDeferredDestructions();

    // Force destroy all deferred resources immediately - call during shutdown
    void flushDeferredDestructions();
};

/**
 * Vulkan helper functions
 */
namespace VkUtils {
    // Convert engine types to Vulkan types
    VkFormat toVkFormat(TextureFormat format);
    VkPrimitiveTopology toVkTopology(PrimitiveTopology topology);
    VkCompareOp toVkCompareOp(CompareFunc func);
    VkBlendFactor toVkBlendFactor(BlendFactor factor);
    VkBlendOp toVkBlendOp(BlendOp op);
    VkFilter toVkFilter(FilterMode mode);
    VkSamplerAddressMode toVkWrapMode(WrapMode mode);
    VkShaderStageFlagBits toVkShaderStage(ShaderStage stage);
    VkCullModeFlags toVkCullMode(CullMode mode);
    VkPolygonMode toVkPolygonMode(FillMode mode);
    VkFrontFace toVkFrontFace(WindingOrder order);
    VkIndexType toVkIndexType(IndexFormat format);
    VkFormat toVkVertexFormat(VertexFormat format);
    VkImageType toVkImageType(TextureType type);
    VkImageViewType toVkImageViewType(TextureType type);
    uint32_t getVertexFormatSize(VertexFormat format);

    // Format helpers
    bool hasStencilComponent(VkFormat format);
    bool isDepthFormat(TextureFormat format);
    VkImageAspectFlags getImageAspect(VkFormat format);
}

#endif // LUPINE_HAS_VULKAN

} // namespace lupine
