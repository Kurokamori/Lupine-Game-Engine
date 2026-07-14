#pragma once

#include "../../ResourceHandles.hpp"
#include "../../gfx/GfxTypes.hpp"
#include "../../gfx/GfxDescriptors.hpp"

#include <unordered_map>
#include <vector>
#include <queue>
#include <cstdint>
#include <string>
#include <mutex>

#ifdef LUPINE_HAS_DIRECTX12

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

namespace lupine {

// Use Microsoft::WRL::ComPtr for automatic COM object lifetime management
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// Forward declarations
struct DX12Buffer;
struct DX12Texture;
struct DX12Sampler;
struct DX12Shader;
struct DX12Pipeline;
struct DX12RenderTarget;
struct DX12Swapchain;
struct DX12UniformBuffer;

// Constants
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
static constexpr uint32_t MAX_RTV_DESCRIPTORS = 1024;
static constexpr uint32_t MAX_DSV_DESCRIPTORS = 256;
static constexpr uint32_t MAX_CBV_SRV_UAV_DESCRIPTORS = 524288;  // 512K descriptors (pool + textures)
static constexpr uint32_t MAX_SAMPLER_DESCRIPTORS = 2048;
static constexpr uint32_t DEFERRED_DESTROY_FRAMES = 3;

/**
 * Descriptor heap allocator for managing descriptor slots
 */
class DescriptorHeapAllocator {
public:
    void initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t maxDescriptors, bool shaderVisible);
    void shutdown();

    uint32_t allocate();
    uint32_t allocateRange(uint32_t count);  // Allocate contiguous range, returns start index
    void free(uint32_t index);
    void freeRange(uint32_t startIndex, uint32_t count);

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint32_t index) const;

    ID3D12DescriptorHeap* getHeap() const { return m_heap.Get(); }
    uint32_t getDescriptorSize() const { return m_descriptorSize; }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;
    uint32_t m_descriptorSize = 0;
    uint32_t m_maxDescriptors = 0;
    std::vector<uint32_t> m_freeList;
    uint32_t m_nextFreeIndex = 0;
    std::mutex m_mutex;
};

/**
 * Upload heap for staging data to GPU
 */
class UploadHeap {
public:
    void initialize(ID3D12Device* device, uint64_t size);
    void shutdown();

    // Allocate space in the upload heap
    // Returns the CPU pointer to write data and GPU virtual address
    struct Allocation {
        void* cpuAddress = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
        uint64_t offset = 0;
    };

    Allocation allocate(uint64_t size, uint64_t alignment = 256);
    void reset();

    ID3D12Resource* getResource() const { return m_resource.Get(); }

private:
    ComPtr<ID3D12Resource> m_resource;
    void* m_cpuAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress = 0;
    uint64_t m_size = 0;
    uint64_t m_offset = 0;
    bool m_overflowLogged = false;
    bool m_highUsageWarned = false;
};

/**
 * DirectX 12 buffer structure
 */
struct DX12Buffer {
    ComPtr<ID3D12Resource> resource;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    void* mappedData = nullptr;
    bool isMapped = false;
};

/**
 * DirectX 12 texture structure
 */
struct DX12Texture {
    ComPtr<ID3D12Resource> resource;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    TextureFormat format = TextureFormat::Unknown;
    TextureType type = TextureType::Texture2D;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    bool isSwapchainImage = false;

    // Descriptor indices
    uint32_t srvIndex = UINT32_MAX;  // Shader resource view
    uint32_t uavIndex = UINT32_MAX;  // Unordered access view
    uint32_t rtvIndex = UINT32_MAX;  // Render target view
    uint32_t dsvIndex = UINT32_MAX;  // Depth stencil view
};

/**
 * DirectX 12 sampler structure
 */
struct DX12Sampler {
    uint32_t samplerIndex = UINT32_MAX;  // Index in sampler heap
    SamplerDesc desc;
};

/**
 * DirectX 12 shader structure
 */
struct DX12Shader {
    std::vector<uint8_t> bytecode;
    ShaderStage stage;
    std::string entryPoint = "main";
};

/**
 * DirectX 12 root signature structure
 */
struct DX12RootSignature {
    ComPtr<ID3D12RootSignature> rootSignature;

    // Root parameter indices for quick lookup
    int32_t cbvRootParamIndex = -1;        // Root CBV for push constants
    int32_t descriptorTableIndex = -1;     // Descriptor table for SRVs/UAVs
    int32_t samplerTableIndex = -1;        // Sampler table

    // Constant buffer slots used
    std::vector<uint32_t> cbvSlots;
    std::vector<uint32_t> srvSlots;
    std::vector<uint32_t> samplerSlots;
};

/**
 * DirectX 12 pipeline structure
 */
struct DX12Pipeline {
    ComPtr<ID3D12PipelineState> pipelineState;
    DX12RootSignature rootSignature;

    // Pipeline configuration
    std::vector<ShaderHandle> shaders;
    VertexBufferLayout vertexLayout;
    std::vector<VertexBufferLayout> extraVertexBuffers;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    D3D12_PRIMITIVE_TOPOLOGY d3dTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    BlendState blendStateDesc;
    DepthStencilState depthStencilStateDesc;
    RasterizerState rasterizerStateDesc;

    // Uniform variable offsets within constant buffer slot 0
    std::unordered_map<std::string, uint32_t> uniformOffsets;
    uint32_t uniformBufferSize = 0;

    // The descriptor this pipeline was created from, retained so a color-format variant
    // can be recreated on demand (DX12 bakes the RTV format into the PSO, so rendering
    // into an HDR target needs a matching pipeline - see getColorFormatVariant).
    PipelineDesc sourceDesc;
};

/**
 * DirectX 12 render target structure
 */
struct DX12RenderTarget {
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    TextureFormat depthFormat = TextureFormat::Unknown;
    DXGI_FORMAT dxgiColorFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dxgiDepthFormat = DXGI_FORMAT_UNKNOWN;

    bool hasColor = true;
    bool hasDepth = true;
    bool hasStencil = false;
    bool isSwapchainBackbuffer = false;
    bool isCubeMap = false;
    int currentCubeFace = -1;

    // Descriptor indices
    uint32_t rtvIndex = UINT32_MAX;
    uint32_t dsvIndex = UINT32_MAX;

    // Handles to the textures
    TextureHandle colorTextureHandle;
    TextureHandle depthTextureHandle;

    // If this is a swapchain backbuffer, store the swapchain ID
    uint32_t owningSwapchainId = 0;
};

/**
 * DirectX 12 swapchain structure
 */
struct DX12Swapchain {
    ComPtr<IDXGISwapChain4> swapchain;

    NativeWindowHandle window;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    DXGI_FORMAT dxgiColorFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    bool vsync = true;
    uint32_t bufferCount = 2;

    // Backbuffer resources (one per buffer)
    std::vector<ComPtr<ID3D12Resource>> backbufferTextures;
    std::vector<uint32_t> backbufferRTVIndices;
    std::vector<D3D12_RESOURCE_STATES> backbufferStates;  // Track current state of each backbuffer
    uint32_t currentBackbufferIndex = 0;

    // Depth buffer
    ComPtr<ID3D12Resource> depthTexture;
    uint32_t depthDSVIndex = UINT32_MAX;

    // Associated render target handles (one per buffer)
    std::vector<RenderTargetHandle> backbufferHandles;

    // Synchronization
    std::vector<uint64_t> frameFenceValues;

    // Track if swapchain is valid
    bool isDead = false;
};

/**
 * DirectX 12 uniform buffer structure
 */
struct DX12UniformBuffer {
    ComPtr<ID3D12Resource> resource;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    uint32_t size = 0;
    void* mappedData = nullptr;
    uint32_t cbvIndex = UINT32_MAX;
};

/**
 * Per-frame resources for multi-buffering
 */
struct DX12FrameResources {
    // Pool of command allocators - one is consumed per render pass recorded this frame.
    // Giving each pass its own allocator lets consecutive passes pipeline on the GPU
    // instead of the CPU blocking on the previous pass's fence (which is otherwise
    // required before a single shared allocator could be reset). The pool grows on
    // demand and is reused every MAX_FRAMES_IN_FLIGHT frames; moveToNextFrame's fence
    // wait guarantees the GPU has finished with every allocator before the cursor resets.
    std::vector<ComPtr<ID3D12CommandAllocator>> commandAllocatorPool;
    uint32_t commandAllocatorCursor = 0;
    uint64_t fenceValue = 0;

    // Upload heap for this frame
    UploadHeap uploadHeap;
};

/**
 * DirectX 12 state manager.
 * Manages all DirectX 12 resources and state tracking.
 */
class DirectX12State {
public:
    DirectX12State();
    ~DirectX12State();

    // DirectX 12 core objects
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<IDXGIFactory6> dxgiFactory;
    ComPtr<IDXGIAdapter4> dxgiAdapter;

    // Command list (shared, reset per frame)
    ComPtr<ID3D12GraphicsCommandList> commandList;

    // Synchronization
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    uint64_t fenceValue = 0;

    // Per-frame resources
    DX12FrameResources frameResources[MAX_FRAMES_IN_FLIGHT];
    uint32_t currentFrameIndex = 0;

    // Descriptor heaps
    DescriptorHeapAllocator rtvHeap;      // Render target views
    DescriptorHeapAllocator dsvHeap;      // Depth stencil views
    DescriptorHeapAllocator cbvSrvUavHeap; // CBV/SRV/UAV (shader visible)
    DescriptorHeapAllocator samplerHeap;   // Samplers (shader visible)

    // Non-shader-visible heaps for staging
    DescriptorHeapAllocator cbvSrvUavStagingHeap;
    DescriptorHeapAllocator samplerStagingHeap;

    // Null SRV for empty descriptor slots (prevents GPU hang when sampling)
    uint32_t nullSrvIndex = UINT32_MAX;       // Null Texture2D SRV (returns 0 when sampled)
    uint32_t nullSrvCubeIndex = UINT32_MAX;   // Null TextureCube SRV (for cube map slots t12-t19)

    // Default constant buffers (prevents GPU hang when CBVs not explicitly bound)
    // These are bound in bindPipeline to ensure all root params are valid
    ComPtr<ID3D12Resource> defaultCBV1Buffer;  // Camera data (b1)
    ComPtr<ID3D12Resource> defaultCBV2Buffer;  // Bone data (b2)
    ComPtr<ID3D12Resource> defaultCBV3Buffer;  // Light data (b3)
    D3D12_GPU_VIRTUAL_ADDRESS defaultCBV1GPUAddress = 0;
    D3D12_GPU_VIRTUAL_ADDRESS defaultCBV2GPUAddress = 0;
    D3D12_GPU_VIRTUAL_ADDRESS defaultCBV3GPUAddress = 0;

    // Debug layer (always available for diagnostics)
    ComPtr<ID3D12Debug> debugInterface;
    ComPtr<ID3D12InfoQueue> infoQueue;

    // Resource storage
    std::unordered_map<uint32_t, DX12Buffer> buffers;
    std::unordered_map<uint32_t, DX12Texture> textures;
    std::unordered_map<uint32_t, DX12Sampler> samplers;
    std::unordered_map<uint32_t, DX12Shader> shaders;
    std::unordered_map<uint32_t, DX12Pipeline> pipelines;
    std::unordered_map<uint32_t, DX12RenderTarget> renderTargets;
    std::unordered_map<uint32_t, DX12Swapchain> swapchains;
    std::unordered_map<uint32_t, DX12UniformBuffer> uniformBuffers;

    // Cache of color-format pipeline variants. Key = (basePipelineId << 32) | colorFormat;
    // value = the pipeline id created for that base + format (see getColorFormatVariant).
    std::unordered_map<uint64_t, uint32_t> pipelineFormatVariants;

    // Resource ID generators
    uint32_t nextBufferID = 1;
    uint32_t nextTextureID = 1;
    uint32_t nextSamplerID = 1;
    uint32_t nextShaderID = 1;
    uint32_t nextPipelineID = 1;
    uint32_t nextRenderTargetID = 1;
    uint32_t nextSwapchainID = 1;
    uint32_t nextUniformBufferID = 1;

    // Current state tracking
    DX12Swapchain* currentSwapchain = nullptr;
    DX12RenderTarget* currentRenderTarget = nullptr;
    DX12Pipeline* currentPipeline = nullptr;
    uint32_t currentRenderTargetId = 0;

    // Pre-allocated contiguous sampler table (16 slots matching root signature range s0-s15).
    // Filled during init so every slot has a valid sampler — prevents GPU faults from
    // uninitialized sampler descriptors when transpiled shaders use sparse sampler registers
    // (e.g. material samplers at s0-s3, shadow sampler at s8).
    uint32_t samplerTableStart = UINT32_MAX;  // Start index in sampler heap (shader-visible)
    static constexpr uint32_t SAMPLER_TABLE_SIZE = 16;

    // Ring buffer for per-draw SRV descriptor tables (shared across command lists)
    // The pool is divided into per-frame segments to prevent overwriting descriptors still in use by the GPU
    // All passes within a frame (shadow + main) share the same pool segment.
    // With batch merging, identical-material draws share one SRV table, so 4096 is ample.
    // Total pool = 32 * 4096 * 2 = 262,144 descriptors (half the 524K heap, leaving
    // room for texture SRVs, null SRVs, and other resources).
    static constexpr uint32_t SRV_TABLE_SIZE = 32;
    static constexpr uint32_t SRV_TABLES_PER_FRAME = 4096;
    static constexpr uint32_t SRV_TABLE_POOL_SIZE = SRV_TABLES_PER_FRAME * MAX_FRAMES_IN_FLIGHT;
    uint32_t srvTablePoolStart = UINT32_MAX;  // Start of pre-allocated region
    uint32_t srvTablePoolIndex = 0;  // Current index within the current frame's segment
    uint32_t srvTablePoolHighWaterMark = 0;  // Peak usage this frame (for diagnostics)
    bool srvPoolAllocationFailed = false;  // Set once if pool allocation fails — stops retry spam

    // Command list state tracking
    bool commandListRecording = false;  // True if command list is currently recording
    bool deviceLost = false;  // Set on device removal or GPU hang — stops all rendering

    // Synchronization helpers
    void waitForGPU();
    void moveToNextFrame();
    void waitForFenceValue(uint64_t value);
    uint64_t signalFence();

    // Command list helpers
    void beginCommandList();
    void executeCommandList();

    // Drain the D3D12 debug/validation info queue into the engine log.
    // Captures GPU-based validation and debug-layer messages (the root-cause
    // detail behind DEVICE_HUNG / malformed-command errors) in all build types.
    // No-op when the info queue is unavailable (debug layer not installed).
    void drainDebugMessages();

    // Resource barrier helper
    void transitionResource(ID3D12Resource* resource,
                           D3D12_RESOURCE_STATES before,
                           D3D12_RESOURCE_STATES after);

    // Deferred resource destruction
    struct DeferredDestroy {
        enum class Type { Buffer, Texture, Pipeline, Shader, Sampler, UniformBuffer };
        Type type;
        uint32_t id;
        uint32_t framesRemaining;
    };
    std::vector<DeferredDestroy> deferredDestroys;

    void deferDestroy(DeferredDestroy::Type type, uint32_t id);
    void processDeferredDestructions();
    void flushDeferredDestructions();
};

/**
 * DirectX 12 helper functions
 */
namespace DX12Utils {
    // Convert engine types to DirectX 12 types
    DXGI_FORMAT toDXGIFormat(TextureFormat format);
    DXGI_FORMAT toDXGIFormatTypeless(TextureFormat format);
    DXGI_FORMAT toDXGIFormatDSV(TextureFormat format);
    DXGI_FORMAT toDXGIFormatSRV(TextureFormat format);
    D3D12_PRIMITIVE_TOPOLOGY toD3D12Topology(PrimitiveTopology topology);
    D3D12_PRIMITIVE_TOPOLOGY_TYPE toD3D12TopologyType(PrimitiveTopology topology);
    D3D12_COMPARISON_FUNC toD3D12CompareFunc(CompareFunc func);
    D3D12_BLEND toD3D12Blend(BlendFactor factor);
    D3D12_BLEND_OP toD3D12BlendOp(BlendOp op);
    D3D12_FILTER toD3D12Filter(FilterMode minFilter, FilterMode magFilter, FilterMode mipFilter, bool comparison = false);
    D3D12_TEXTURE_ADDRESS_MODE toD3D12AddressMode(WrapMode mode);
    D3D12_CULL_MODE toD3D12CullMode(CullMode mode);
    D3D12_FILL_MODE toD3D12FillMode(FillMode mode);
    DXGI_FORMAT toD3D12VertexFormat(VertexFormat format);
    const char* getSemanticNameFromAttributeName(const std::string& name);
    uint32_t getVertexFormatSize(VertexFormat format);

    // Format helpers
    bool hasStencilComponent(DXGI_FORMAT format);
    bool isDepthFormat(TextureFormat format);
    uint32_t getBytesPerPixel(DXGI_FORMAT format);
    uint32_t getRowPitch(uint32_t width, DXGI_FORMAT format);

    // Alignment helper
    inline uint64_t align(uint64_t value, uint64_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12

