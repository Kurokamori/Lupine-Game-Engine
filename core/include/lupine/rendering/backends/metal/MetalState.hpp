#pragma once

#include "../../ResourceHandles.hpp"
#include "../../gfx/GfxTypes.hpp"
#include "../../gfx/GfxDescriptors.hpp"

#ifdef LUPINE_HAS_METAL

// Metal framework includes - only on Apple platforms
#ifdef __APPLE__
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

namespace lupine {

#ifdef __APPLE__

// Forward declarations for Objective-C types
// These are actually id<MTL*> types but we use void* for C++ compatibility
// The actual Metal objects are managed via ARC or manual retain/release

/**
 * Metal buffer structure
 */
struct MetalBuffer {
    id<MTLBuffer> buffer = nil;
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    MTLResourceOptions resourceOptions = MTLResourceStorageModeShared;
};

/**
 * Metal texture structure
 */
struct MetalTexture {
    id<MTLTexture> texture = nil;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    TextureFormat format = TextureFormat::Unknown;
    TextureType type = TextureType::Texture2D;
    MTLPixelFormat mtlFormat = MTLPixelFormatInvalid;
    bool isSwapchainImage = false;  // Swapchain images are not owned by us
};

/**
 * Metal sampler structure
 */
struct MetalSampler {
    id<MTLSamplerState> sampler = nil;
    SamplerDesc desc;
};

/**
 * Metal shader structure
 */
struct MetalShader {
    id<MTLFunction> function = nil;
    id<MTLLibrary> library = nil;
    std::vector<uint8_t> sourceCode;  // Store MSL source code
    ShaderStage stage;
    std::string entryPoint = "main0";  // Metal uses main0 by default for cross-compiled shaders

    // Reflection data for uniform bindings
    struct UniformInfo {
        std::string name;
        uint32_t offset;
        uint32_t size;
        uint32_t bufferIndex;  // Which buffer this uniform is in
    };
    std::vector<UniformInfo> uniforms;

    // Texture binding info from reflection
    struct TextureBindingInfo {
        std::string name;
        uint32_t index;
        bool isWritable;  // For compute shader storage textures
    };
    std::vector<TextureBindingInfo> textureBindings;

    // Buffer binding info from reflection
    struct BufferBindingInfo {
        std::string name;
        uint32_t index;
        uint32_t size;
        bool isWritable;  // For compute shader storage buffers
    };
    std::vector<BufferBindingInfo> bufferBindings;
};

/**
 * Metal pipeline structure
 */
struct MetalPipeline {
    id<MTLRenderPipelineState> pipelineState = nil;
    id<MTLDepthStencilState> depthStencilState = nil;

    // Shader stages
    std::vector<ShaderHandle> shaders;

    // Pipeline configuration
    VertexBufferLayout vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    MTLPrimitiveType mtlPrimitiveType = MTLPrimitiveTypeTriangle;
    BlendState blendStateDesc;
    DepthStencilState depthStencilStateDesc;
    RasterizerState rasterizerStateDesc;

    // Uniform variable offsets within argument buffer slot 0
    // Maps uniform name -> byte offset in the constant buffer
    std::unordered_map<std::string, uint32_t> uniformOffsets;

    // Total size of all uniforms in slot 0 constant buffer
    uint32_t uniformBufferSize = 0;

    // Cull mode (stored separately since Metal handles this per-encoder)
    MTLCullMode cullMode = MTLCullModeBack;
    MTLWinding frontFace = MTLWindingCounterClockwise;
    MTLTriangleFillMode fillMode = MTLTriangleFillModeFill;

    // Depth bias for shadow mapping
    bool depthBiasEnable = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlope = 0.0f;
    float depthBiasClamp = 0.0f;
};

/**
 * Metal compute pipeline structure
 */
struct MetalComputePipeline {
    id<MTLComputePipelineState> pipelineState = nil;
    ShaderHandle shader;

    // Thread execution configuration
    MTLSize threadGroupSize = {1, 1, 1};
    uint32_t maxTotalThreadsPerThreadgroup = 0;

    // Uniform variable offsets
    std::unordered_map<std::string, uint32_t> uniformOffsets;
    uint32_t uniformBufferSize = 0;
};

/**
 * Metal render target structure
 */
struct MetalRenderTarget {
    id<MTLTexture> colorTexture = nil;
    id<MTLTexture> depthTexture = nil;

    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    TextureFormat depthFormat = TextureFormat::Unknown;
    MTLPixelFormat mtlColorFormat = MTLPixelFormatInvalid;
    MTLPixelFormat mtlDepthFormat = MTLPixelFormatInvalid;

    bool hasColor = true;
    bool hasDepth = true;
    bool hasStencil = false;
    bool isSwapchainBackbuffer = false;
    bool isCubeMap = false;
    int currentCubeFace = -1;

    // Handles to the textures (for API users)
    TextureHandle colorTextureHandle;
    TextureHandle depthTextureHandle;

    // If this is a swapchain backbuffer, store the swapchain ID
    uint32_t owningSwapchainId = 0;

    // Load/Store actions for the render pass
    MTLLoadAction colorLoadAction = MTLLoadActionClear;
    MTLStoreAction colorStoreAction = MTLStoreActionStore;
    MTLLoadAction depthLoadAction = MTLLoadActionClear;
    MTLStoreAction depthStoreAction = MTLStoreActionStore;

    // iOS optimization: memoryless storage for transient attachments
    // When true, depth/stencil data is discarded after render pass (saves bandwidth on TBDR GPUs)
    bool useMemorylessDepth = false;
};

/**
 * Metal swapchain structure
 */
struct MetalSwapchain {
    CAMetalLayer* metalLayer = nil;
    id<CAMetalDrawable> currentDrawable = nil;

    NativeWindowHandle window;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    MTLPixelFormat mtlColorFormat = MTLPixelFormatBGRA8Unorm;
    bool vsync = true;

    // Depth buffer
    id<MTLTexture> depthTexture = nil;

    // Associated render target
    RenderTargetHandle backbuffer;

    // Track if swapchain is valid
    bool isDead = false;
};

/**
 * Metal uniform buffer structure
 */
struct MetalUniformBuffer {
    id<MTLBuffer> buffer = nil;
    uint32_t size = 0;
    void* contents = nullptr;  // Mapped pointer (Metal buffers are always accessible)
};

/**
 * Metal state manager.
 * Manages all Metal resources and state tracking.
 */
class MetalState {
public:
    MetalState();
    ~MetalState();

    // Metal core objects
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;

    // Current command buffer and encoders
    id<MTLCommandBuffer> currentCommandBuffer = nil;
    id<MTLRenderCommandEncoder> currentEncoder = nil;
    id<MTLComputeCommandEncoder> currentComputeEncoder = nil;

    // Resource storage
    std::unordered_map<uint32_t, MetalBuffer> buffers;
    std::unordered_map<uint32_t, MetalTexture> textures;
    std::unordered_map<uint32_t, MetalSampler> samplers;
    std::unordered_map<uint32_t, MetalShader> shaders;
    std::unordered_map<uint32_t, MetalPipeline> pipelines;
    std::unordered_map<uint32_t, MetalComputePipeline> computePipelines;
    std::unordered_map<uint32_t, MetalRenderTarget> renderTargets;
    std::unordered_map<uint32_t, MetalSwapchain> swapchains;
    std::unordered_map<uint32_t, MetalUniformBuffer> uniformBuffers;

    // Resource ID generators
    uint32_t nextBufferID = 1;
    uint32_t nextTextureID = 1;
    uint32_t nextSamplerID = 1;
    uint32_t nextShaderID = 1;
    uint32_t nextPipelineID = 1;
    uint32_t nextComputePipelineID = 1;
    uint32_t nextRenderTargetID = 1;
    uint32_t nextSwapchainID = 1;
    uint32_t nextUniformBufferID = 1;

    // Current state tracking
    MetalSwapchain* currentSwapchain = nullptr;
    MetalRenderTarget* currentRenderTarget = nullptr;
    MetalPipeline* currentPipeline = nullptr;
    MetalComputePipeline* currentComputePipeline = nullptr;
    SwapchainHandle hintedSwapchain;  // For off-screen rendering synchronization

    // Dummy/default resources
    id<MTLTexture> dummyTexture = nil;
    id<MTLSamplerState> dummySampler = nil;
    id<MTLBuffer> dummyBuffer = nil;

    // Methods to create/destroy dummy resources
    void createDummyResources();
    void destroyDummyResources();

    // Frame synchronization
    dispatch_semaphore_t frameSemaphore = nullptr;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    uint32_t currentFrameIndex = 0;

    // Deferred resource destruction (for thread safety)
    // Resources are kept alive for a few frames before destruction
    struct DeferredPipelineDestroy {
        MetalPipeline pipeline;
        uint32_t framesRemaining;
    };
    struct DeferredShaderDestroy {
        MetalShader shader;
        uint32_t framesRemaining;
    };
    struct DeferredBufferDestroy {
        MetalBuffer buffer;
        uint32_t framesRemaining;
    };
    struct DeferredTextureDestroy {
        MetalTexture texture;
        uint32_t framesRemaining;
    };
    struct DeferredComputePipelineDestroy {
        MetalComputePipeline pipeline;
        uint32_t framesRemaining;
    };

    std::vector<DeferredPipelineDestroy> deferredPipelineDestroys;
    std::vector<DeferredComputePipelineDestroy> deferredComputePipelineDestroys;
    std::vector<DeferredShaderDestroy> deferredShaderDestroys;
    std::vector<DeferredBufferDestroy> deferredBufferDestroys;
    std::vector<DeferredTextureDestroy> deferredTextureDestroys;

    static constexpr uint32_t DEFERRED_DESTROY_FRAMES = 3;

    // Deferred destruction methods
    void deferPipelineDestroy(const MetalPipeline& pipeline);
    void deferComputePipelineDestroy(const MetalComputePipeline& pipeline);
    void deferShaderDestroy(const MetalShader& shader);
    void deferBufferDestroy(const MetalBuffer& buffer);
    void deferTextureDestroy(const MetalTexture& texture);

    // Process deferred destructions - call once per frame
    void processDeferredDestructions();

    // Force destroy all deferred resources immediately - call during shutdown
    void flushDeferredDestructions();
};

/**
 * Metal helper functions
 */
namespace MetalUtils {
    // Convert engine types to Metal types
    MTLPixelFormat toMTLPixelFormat(TextureFormat format);
    MTLPixelFormat toMTLPixelFormatDepth(TextureFormat format);
    MTLPrimitiveType toMTLPrimitiveType(PrimitiveTopology topology);
    MTLCompareFunction toMTLCompareFunction(CompareFunc func);
    MTLBlendFactor toMTLBlendFactor(BlendFactor factor);
    MTLBlendOperation toMTLBlendOperation(BlendOp op);
    MTLSamplerMinMagFilter toMTLFilter(FilterMode mode);
    MTLSamplerMipFilter toMTLMipFilter(FilterMode mode);
    MTLSamplerAddressMode toMTLAddressMode(WrapMode mode);
    MTLCullMode toMTLCullMode(CullMode mode);
    MTLWinding toMTLWinding(WindingOrder order);
    MTLTriangleFillMode toMTLFillMode(FillMode mode);
    MTLVertexFormat toMTLVertexFormat(VertexFormat format);
    MTLIndexType toMTLIndexType(IndexFormat format);

    // Format helpers
    bool hasStencilComponent(MTLPixelFormat format);
    bool isDepthFormat(TextureFormat format);
    uint32_t getBytesPerPixel(MTLPixelFormat format);
    uint32_t getVertexFormatSize(VertexFormat format);

    // Create default render pass descriptor
    MTLRenderPassDescriptor* createRenderPassDescriptor(
        id<MTLTexture> colorTexture,
        id<MTLTexture> depthTexture,
        MTLLoadAction colorLoadAction,
        MTLLoadAction depthLoadAction,
        MTLClearColor clearColor,
        double clearDepth
    );

    /**
     * Extract reflection data from a Metal function.
     * Populates the shader's uniform, texture, and buffer binding info.
     * @param shader The shader to extract reflection data for
     */
    void extractShaderReflection(MetalShader& shader);

    /**
     * Extract reflection data from a render pipeline for combined vertex/fragment uniforms.
     * @param pipeline The pipeline to extract reflection data for
     * @param reflection The pipeline reflection object from Metal
     */
    void extractPipelineReflection(MetalPipeline& pipeline, MTLRenderPipelineReflection* reflection);

    // ===== iOS/TBDR Optimizations =====

    /**
     * Get optimal storage mode for a texture based on platform and usage.
     * On iOS, uses MTLStorageModeMemoryless for transient render targets.
     * @param device The Metal device
     * @param usage Texture usage flags
     * @param isTransient True if texture content is discarded after render pass
     * @return Optimal storage mode for the platform
     */
    MTLStorageMode getOptimalStorageMode(id<MTLDevice> device, TextureUsage usage, bool isTransient);

    /**
     * Get optimal resource options for a buffer based on platform.
     * @param device The Metal device
     * @param cpuWritable True if CPU needs to write to buffer frequently
     * @return Optimal resource options
     */
    MTLResourceOptions getOptimalBufferOptions(id<MTLDevice> device, bool cpuWritable);

    /**
     * Check if device supports memoryless render targets (iOS/tvOS only).
     * @param device The Metal device
     * @return True if memoryless storage is supported
     */
    bool supportsMemorylessStorage(id<MTLDevice> device);

    /**
     * Check if device is an Apple Silicon (unified memory) device.
     * @param device The Metal device
     * @return True if device has unified memory
     */
    bool hasUnifiedMemory(id<MTLDevice> device);
}

#endif // __APPLE__

#endif // LUPINE_HAS_METAL

} // namespace lupine
