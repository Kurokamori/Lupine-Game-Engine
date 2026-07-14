#pragma once

#include "../../ResourceHandles.hpp"
#include "../../gfx/GfxTypes.hpp"
#include "../../gfx/GfxDescriptors.hpp"

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

#ifdef LUPINE_HAS_DIRECTX11

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11.h>
#include <d3d11_1.h>  // ID3DUserDefinedAnnotation (GPU debug markers)
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace lupine {

// Use Microsoft::WRL::ComPtr for automatic COM object lifetime management
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// Forward declarations
struct DX11Buffer;
struct DX11Texture;
struct DX11Sampler;
struct DX11Shader;
struct DX11Pipeline;
struct DX11RenderTarget;
struct DX11Swapchain;
struct DX11UniformBuffer;

/**
 * DirectX 11 buffer structure
 */
struct DX11Buffer {
    ComPtr<ID3D11Buffer> buffer;
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    D3D11_USAGE d3dUsage = D3D11_USAGE_DEFAULT;
    void* mappedData = nullptr;
};

/**
 * DirectX 11 texture structure
 */
struct DX11Texture {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;  // Shader resource view for sampling
    ComPtr<ID3D11UnorderedAccessView> uav; // Unordered access view for compute (optional)
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    TextureFormat format = TextureFormat::Unknown;
    TextureType type = TextureType::Texture2D;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
    bool isSwapchainImage = false;  // Swapchain images are not owned
};

/**
 * DirectX 11 sampler structure
 */
struct DX11Sampler {
    ComPtr<ID3D11SamplerState> sampler;
    SamplerDesc desc;
};

/**
 * DirectX 11 shader structure
 */
struct DX11Shader {
    ComPtr<ID3D11DeviceChild> shader;  // Base class for all shader types
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11GeometryShader> geometryShader;
    ComPtr<ID3D11ComputeShader> computeShader;
    ComPtr<ID3D11HullShader> hullShader;
    ComPtr<ID3D11DomainShader> domainShader;

    std::vector<uint8_t> bytecode;  // Store compiled bytecode
    ShaderStage stage;
    std::string entryPoint = "main";
};

/**
 * DirectX 11 pipeline structure
 */
struct DX11Pipeline {
    // Shader stages
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11GeometryShader> geometryShader;
    ComPtr<ID3D11HullShader> hullShader;
    ComPtr<ID3D11DomainShader> domainShader;

    // Input layout
    ComPtr<ID3D11InputLayout> inputLayout;

    // Pipeline state objects
    ComPtr<ID3D11RasterizerState> rasterizerState;
    ComPtr<ID3D11DepthStencilState> depthStencilState;
    ComPtr<ID3D11BlendState> blendState;

    // Pipeline configuration
    std::vector<ShaderHandle> shaders;
    VertexBufferLayout vertexLayout;
    // Additional per-binding layouts (e.g. binding 1 = per-instance data).
    std::vector<VertexBufferLayout> extraVertexBuffers;
    // Per-input-slot vertex strides, indexed by binding/input slot. Slot 0 is the
    // primary geometry layout stride; higher slots are per-instance/extra buffers.
    std::vector<uint32_t> vertexStrides;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    D3D11_PRIMITIVE_TOPOLOGY d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    BlendState blendStateDesc;
    DepthStencilState depthStencilStateDesc;
    RasterizerState rasterizerStateDesc;

    // Constant buffer reflection data
    struct ConstantBufferInfo {
        std::string name;
        uint32_t size;
        uint32_t slot;
        ShaderStage stage;
    };
    std::vector<ConstantBufferInfo> constantBuffers;

    // Texture/sampler reflection data
    struct TextureBindingInfo {
        std::string name;
        uint32_t slot;
        ShaderStage stage;
    };
    std::vector<TextureBindingInfo> textureBindings;
    std::vector<TextureBindingInfo> samplerBindings;

    // Uniform variable offsets within constant buffer slot 0 (like Vulkan push constants)
    // Maps uniform name -> byte offset in the constant buffer
    std::unordered_map<std::string, uint32_t> uniformOffsets;

    // Total size of all uniforms in slot 0 constant buffer
    uint32_t uniformBufferSize = 0;
};

/**
 * DirectX 11 render target structure
 */
struct DX11RenderTarget {
    ComPtr<ID3D11RenderTargetView> rtv;  // Render target view
    ComPtr<ID3D11DepthStencilView> dsv;  // Depth stencil view

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

    // Handles to the textures
    TextureHandle colorTextureHandle;
    TextureHandle depthTextureHandle;

    // If this is a swapchain backbuffer, store the swapchain ID
    uint32_t owningSwapchainId = 0;
};

/**
 * DirectX 11 swapchain structure
 */
struct DX11Swapchain {
    ComPtr<IDXGISwapChain> swapchain;

    NativeWindowHandle window;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    DXGI_FORMAT dxgiColorFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    bool vsync = true;

    // Backbuffer resources
    ComPtr<ID3D11Texture2D> backbufferTexture;
    ComPtr<ID3D11RenderTargetView> backbufferRTV;

    // Depth buffer
    ComPtr<ID3D11Texture2D> depthTexture;
    ComPtr<ID3D11DepthStencilView> depthDSV;

    // Associated render target
    RenderTargetHandle backbuffer;

    // Track if swapchain is valid
    bool isDead = false;
};

/**
 * DirectX 11 uniform buffer structure
 */
struct DX11UniformBuffer {
    ComPtr<ID3D11Buffer> buffer;
    uint32_t size = 0;
    void* mappedData = nullptr;
};

/**
 * DirectX 11 state manager.
 * Manages all DirectX 11 resources and state tracking.
 */
class DirectX11State {
public:
    DirectX11State();
    ~DirectX11State();

    // DirectX 11 core objects
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIFactory> dxgiFactory;
    ComPtr<IDXGIAdapter> dxgiAdapter;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    // Debug layer
    #ifdef _DEBUG
    ComPtr<ID3D11Debug> debugInterface;
    #endif

    // Resource storage
    std::unordered_map<uint32_t, DX11Buffer> buffers;
    std::unordered_map<uint32_t, DX11Texture> textures;
    std::unordered_map<uint32_t, DX11Sampler> samplers;
    std::unordered_map<uint32_t, DX11Shader> shaders;
    std::unordered_map<uint32_t, DX11Pipeline> pipelines;
    std::unordered_map<uint32_t, DX11RenderTarget> renderTargets;
    std::unordered_map<uint32_t, DX11Swapchain> swapchains;
    std::unordered_map<uint32_t, DX11UniformBuffer> uniformBuffers;

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
    DX11Swapchain* currentSwapchain = nullptr;
    DX11RenderTarget* currentRenderTarget = nullptr;
    DX11Pipeline* currentPipeline = nullptr;

    // Dummy/default resources
    ComPtr<ID3D11Buffer> dummyConstantBuffer;
    ComPtr<ID3D11Texture2D> dummyTexture;
    ComPtr<ID3D11ShaderResourceView> dummyTextureSRV;
    ComPtr<ID3D11SamplerState> dummySampler;

    // Methods to create/destroy dummy resources
    void createDummyResources();
    void destroyDummyResources();

    // Deferred resource destruction (for thread safety)
    // Resources are kept alive for a few frames before destruction
    struct DeferredPipelineDestroy {
        DX11Pipeline pipeline;
        uint32_t framesRemaining;
    };
    struct DeferredShaderDestroy {
        DX11Shader shader;
        uint32_t framesRemaining;
    };
    struct DeferredBufferDestroy {
        DX11Buffer buffer;
        uint32_t framesRemaining;
    };
    struct DeferredTextureDestroy {
        DX11Texture texture;
        uint32_t framesRemaining;
    };

    std::vector<DeferredPipelineDestroy> deferredPipelineDestroys;
    std::vector<DeferredShaderDestroy> deferredShaderDestroys;
    std::vector<DeferredBufferDestroy> deferredBufferDestroys;
    std::vector<DeferredTextureDestroy> deferredTextureDestroys;

    static constexpr uint32_t DEFERRED_DESTROY_FRAMES = 2;

    // Deferred destruction methods
    void deferPipelineDestroy(const DX11Pipeline& pipeline);
    void deferShaderDestroy(const DX11Shader& shader);
    void deferBufferDestroy(const DX11Buffer& buffer);
    void deferTextureDestroy(const DX11Texture& texture);

    // Process deferred destructions - call once per frame
    void processDeferredDestructions();

    // Force destroy all deferred resources immediately - call during shutdown
    void flushDeferredDestructions();
};

/**
 * DirectX 11 helper functions
 */
namespace DX11Utils {
    // Convert engine types to DirectX 11 types
    DXGI_FORMAT toDXGIFormat(TextureFormat format);
    DXGI_FORMAT toDXGIFormatTypeless(TextureFormat format);
    DXGI_FORMAT toDXGIFormatDSV(TextureFormat format);
    DXGI_FORMAT toDXGIFormatSRV(TextureFormat format);
    D3D11_PRIMITIVE_TOPOLOGY toDX11Topology(PrimitiveTopology topology);
    D3D11_COMPARISON_FUNC toDX11CompareFunc(CompareFunc func);
    D3D11_BLEND toDX11Blend(BlendFactor factor);
    D3D11_BLEND_OP toDX11BlendOp(BlendOp op);
    D3D11_FILTER toDX11Filter(FilterMode minFilter, FilterMode magFilter, FilterMode mipFilter);
    D3D11_TEXTURE_ADDRESS_MODE toDX11AddressMode(WrapMode mode);
    D3D11_CULL_MODE toDX11CullMode(CullMode mode);
    D3D11_FILL_MODE toDX11FillMode(FillMode mode);
    DXGI_FORMAT toDX11VertexFormat(VertexFormat format);
    const char* getSemanticNameFromAttributeName(const std::string& name);
    uint32_t getVertexFormatSize(VertexFormat format);

    // Format helpers
    bool hasStencilComponent(DXGI_FORMAT format);
    bool isDepthFormat(TextureFormat format);
    uint32_t getBytesPerPixel(DXGI_FORMAT format);
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX11

