#include "lupine/rendering/backends/directx11/DirectX11State.hpp"

#ifdef LUPINE_HAS_DIRECTX11

#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cctype>

namespace lupine {

DirectX11State::DirectX11State() = default;

DirectX11State::~DirectX11State() {
    flushDeferredDestructions();
    destroyDummyResources();
}

void DirectX11State::createDummyResources() {
    if (!device) {
        return;
    }

    // Create dummy constant buffer (256 bytes)
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = 256;
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&cbDesc, nullptr, &dummyConstantBuffer);

    // Create dummy 1x1 white texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    uint32_t whitePixel = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &whitePixel;
    initData.SysMemPitch = 4;

    device->CreateTexture2D(&texDesc, &initData, &dummyTexture);

    // Create shader resource view
    if (dummyTexture) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(dummyTexture.Get(), &srvDesc, &dummyTextureSRV);
    }

    // Create dummy sampler (linear filtering, clamp wrap mode)
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    device->CreateSamplerState(&samplerDesc, &dummySampler);
}

void DirectX11State::destroyDummyResources() {
    dummyConstantBuffer.Reset();
    dummyTexture.Reset();
    dummyTextureSRV.Reset();
    dummySampler.Reset();
}

void DirectX11State::deferPipelineDestroy(const DX11Pipeline& pipeline) {
    deferredPipelineDestroys.push_back({pipeline, DEFERRED_DESTROY_FRAMES});
}

void DirectX11State::deferShaderDestroy(const DX11Shader& shader) {
    deferredShaderDestroys.push_back({shader, DEFERRED_DESTROY_FRAMES});
}

void DirectX11State::deferBufferDestroy(const DX11Buffer& buffer) {
    deferredBufferDestroys.push_back({buffer, DEFERRED_DESTROY_FRAMES});
}

void DirectX11State::deferTextureDestroy(const DX11Texture& texture) {
    deferredTextureDestroys.push_back({texture, DEFERRED_DESTROY_FRAMES});
}

void DirectX11State::processDeferredDestructions() {
    // Process pipelines
    for (auto& deferred : deferredPipelineDestroys) {
        if (deferred.framesRemaining > 0) {
            deferred.framesRemaining--;
        }
    }
    deferredPipelineDestroys.erase(
        std::remove_if(deferredPipelineDestroys.begin(), deferredPipelineDestroys.end(),
            [](const DeferredPipelineDestroy& d) { return d.framesRemaining == 0; }),
        deferredPipelineDestroys.end()
    );

    // Process shaders
    for (auto& deferred : deferredShaderDestroys) {
        if (deferred.framesRemaining > 0) {
            deferred.framesRemaining--;
        }
    }
    deferredShaderDestroys.erase(
        std::remove_if(deferredShaderDestroys.begin(), deferredShaderDestroys.end(),
            [](const DeferredShaderDestroy& d) { return d.framesRemaining == 0; }),
        deferredShaderDestroys.end()
    );

    // Process buffers
    for (auto& deferred : deferredBufferDestroys) {
        if (deferred.framesRemaining > 0) {
            deferred.framesRemaining--;
        }
    }
    deferredBufferDestroys.erase(
        std::remove_if(deferredBufferDestroys.begin(), deferredBufferDestroys.end(),
            [](const DeferredBufferDestroy& d) { return d.framesRemaining == 0; }),
        deferredBufferDestroys.end()
    );

    // Process textures
    for (auto& deferred : deferredTextureDestroys) {
        if (deferred.framesRemaining > 0) {
            deferred.framesRemaining--;
        }
    }
    deferredTextureDestroys.erase(
        std::remove_if(deferredTextureDestroys.begin(), deferredTextureDestroys.end(),
            [](const DeferredTextureDestroy& d) { return d.framesRemaining == 0; }),
        deferredTextureDestroys.end()
    );
}

void DirectX11State::flushDeferredDestructions() {
    deferredPipelineDestroys.clear();
    deferredShaderDestroys.clear();
    deferredBufferDestroys.clear();
    deferredTextureDestroys.clear();
}

// ============================================================================
// DirectX 11 Utility Functions
// ============================================================================

namespace DX11Utils {

DXGI_FORMAT toDXGIFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::RG8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::RGBA8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case TextureFormat::BGRA8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::BGRA8_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        case TextureFormat::R16_FLOAT: return DXGI_FORMAT_R16_FLOAT;
        case TextureFormat::RG16_FLOAT: return DXGI_FORMAT_R16G16_FLOAT;
        case TextureFormat::RGBA16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case TextureFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::RG32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
        case TextureFormat::RGB32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
        case TextureFormat::RGBA32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;

        case TextureFormat::DEPTH16: return DXGI_FORMAT_D16_UNORM;
        case TextureFormat::DEPTH24: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::DEPTH32F: return DXGI_FORMAT_D32_FLOAT;
        case TextureFormat::DEPTH24_STENCIL8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::DEPTH32F_STENCIL8: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        case TextureFormat::BC1_RGB: return DXGI_FORMAT_BC1_UNORM;
        case TextureFormat::BC1_RGBA: return DXGI_FORMAT_BC1_UNORM;
        case TextureFormat::BC3_RGBA: return DXGI_FORMAT_BC3_UNORM;
        case TextureFormat::BC5_RG: return DXGI_FORMAT_BC5_UNORM;
        case TextureFormat::BC7_RGBA: return DXGI_FORMAT_BC7_UNORM;

        default: return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT toDXGIFormatTypeless(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH16: return DXGI_FORMAT_R16_TYPELESS;
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH24_STENCIL8: return DXGI_FORMAT_R24G8_TYPELESS;
        case TextureFormat::DEPTH32F: return DXGI_FORMAT_R32_TYPELESS;
        case TextureFormat::DEPTH32F_STENCIL8: return DXGI_FORMAT_R32G8X24_TYPELESS;
        default: return toDXGIFormat(format);
    }
}

DXGI_FORMAT toDXGIFormatDSV(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH16: return DXGI_FORMAT_D16_UNORM;
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH24_STENCIL8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::DEPTH32F: return DXGI_FORMAT_D32_FLOAT;
        case TextureFormat::DEPTH32F_STENCIL8: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        default: return toDXGIFormat(format);
    }
}

DXGI_FORMAT toDXGIFormatSRV(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH16: return DXGI_FORMAT_R16_UNORM;
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH24_STENCIL8: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case TextureFormat::DEPTH32F: return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::DEPTH32F_STENCIL8: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        default: return toDXGIFormat(format);
    }
}

D3D11_PRIMITIVE_TOPOLOGY toDX11Topology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::PointList: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveTopology::LineList: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::LineStrip: return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveTopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

D3D11_COMPARISON_FUNC toDX11CompareFunc(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never: return D3D11_COMPARISON_NEVER;
        case CompareFunc::Less: return D3D11_COMPARISON_LESS;
        case CompareFunc::Equal: return D3D11_COMPARISON_EQUAL;
        case CompareFunc::LessEqual: return D3D11_COMPARISON_LESS_EQUAL;
        case CompareFunc::Greater: return D3D11_COMPARISON_GREATER;
        case CompareFunc::NotEqual: return D3D11_COMPARISON_NOT_EQUAL;
        case CompareFunc::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        case CompareFunc::Always: return D3D11_COMPARISON_ALWAYS;
        default: return D3D11_COMPARISON_LESS;
    }
}

D3D11_BLEND toDX11Blend(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero: return D3D11_BLEND_ZERO;
        case BlendFactor::One: return D3D11_BLEND_ONE;
        case BlendFactor::SrcColor: return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::DstColor: return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::OneMinusDstColor: return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlpha: return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha: return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
        default: return D3D11_BLEND_ONE;
    }
}

D3D11_BLEND_OP toDX11BlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::Add: return D3D11_BLEND_OP_ADD;
        case BlendOp::Subtract: return D3D11_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min: return D3D11_BLEND_OP_MIN;
        case BlendOp::Max: return D3D11_BLEND_OP_MAX;
        default: return D3D11_BLEND_OP_ADD;
    }
}

D3D11_FILTER toDX11Filter(FilterMode minFilter, FilterMode magFilter, FilterMode mipFilter) {
    bool minLinear = (minFilter == FilterMode::Linear);
    bool magLinear = (magFilter == FilterMode::Linear);
    bool mipLinear = (mipFilter == FilterMode::Linear);

    if (minLinear && magLinear && mipLinear) {
        return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    } else if (minLinear && magLinear && !mipLinear) {
        return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    } else if (minLinear && !magLinear && mipLinear) {
        return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    } else if (minLinear && !magLinear && !mipLinear) {
        return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    } else if (!minLinear && magLinear && mipLinear) {
        return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    } else if (!minLinear && magLinear && !mipLinear) {
        return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    } else if (!minLinear && !magLinear && mipLinear) {
        return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    } else {
        return D3D11_FILTER_MIN_MAG_MIP_POINT;
    }
}

D3D11_TEXTURE_ADDRESS_MODE toDX11AddressMode(WrapMode mode) {
    switch (mode) {
        case WrapMode::Repeat: return D3D11_TEXTURE_ADDRESS_WRAP;
        case WrapMode::MirroredRepeat: return D3D11_TEXTURE_ADDRESS_MIRROR;
        case WrapMode::ClampToEdge: return D3D11_TEXTURE_ADDRESS_CLAMP;
        case WrapMode::ClampToBorder: return D3D11_TEXTURE_ADDRESS_BORDER;
        default: return D3D11_TEXTURE_ADDRESS_WRAP;
    }
}

D3D11_CULL_MODE toDX11CullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None: return D3D11_CULL_NONE;
        case CullMode::Front: return D3D11_CULL_FRONT;
        case CullMode::Back: return D3D11_CULL_BACK;
        default: return D3D11_CULL_BACK;
    }
}

D3D11_FILL_MODE toDX11FillMode(FillMode mode) {
    switch (mode) {
        case FillMode::Solid: return D3D11_FILL_SOLID;
        case FillMode::Wireframe: return D3D11_FILL_WIREFRAME;
        default: return D3D11_FILL_SOLID;
    }
}

DXGI_FORMAT toDX11VertexFormat(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float: return DXGI_FORMAT_R32_FLOAT;
        case VertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case VertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexFormat::Int: return DXGI_FORMAT_R32_SINT;
        case VertexFormat::Int2: return DXGI_FORMAT_R32G32_SINT;
        case VertexFormat::Int3: return DXGI_FORMAT_R32G32B32_SINT;
        case VertexFormat::Int4: return DXGI_FORMAT_R32G32B32A32_SINT;
        case VertexFormat::UInt: return DXGI_FORMAT_R32_UINT;
        case VertexFormat::UInt2: return DXGI_FORMAT_R32G32_UINT;
        case VertexFormat::UInt3: return DXGI_FORMAT_R32G32B32_UINT;
        case VertexFormat::UInt4: return DXGI_FORMAT_R32G32B32A32_UINT;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}

const char* getSemanticNameFromAttributeName(const std::string& name) {
    // Convert attribute name to HLSL semantic name
    // This attempts to map common attribute names to DirectX semantics
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    // GPU-instancing per-instance attributes (a_InstanceModel0..3, a_InstanceColor,
    // a_InstanceCustom) always map to TEXCOORD semantics. Resolve them before the
    // generic colour/texcoord checks so a_InstanceColor does not match COLOR.
    if (lowerName.find("instance") != std::string::npos) return "TEXCOORD";

    if (lowerName.find("position") != std::string::npos || lowerName == "pos") return "POSITION";
    if (lowerName.find("normal") != std::string::npos) return "NORMAL";
    if (lowerName.find("tangent") != std::string::npos && lowerName.find("bi") == std::string::npos) return "TANGENT";
    if (lowerName.find("bitangent") != std::string::npos) return "BINORMAL";
    if (lowerName.find("color") != std::string::npos || lowerName.find("colour") != std::string::npos) return "COLOR";

    // Bone/skeletal animation attributes - check before texcoord since "boneid" shouldn't match texcoord
    if (lowerName.find("boneid") != std::string::npos ||
        lowerName.find("boneindex") != std::string::npos ||
        lowerName.find("boneindices") != std::string::npos ||
        lowerName.find("blendindices") != std::string::npos) return "BLENDINDICES";
    if (lowerName.find("boneweight") != std::string::npos ||
        lowerName.find("blendweight") != std::string::npos) return "BLENDWEIGHT";

    if (lowerName.find("texcoord") != std::string::npos || lowerName.find("uv") != std::string::npos) return "TEXCOORD";

    // Default to TEXCOORD for unknown attributes
    return "TEXCOORD";
}

uint32_t getVertexFormatSize(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float: return 4;
        case VertexFormat::Float2: return 8;
        case VertexFormat::Float3: return 12;
        case VertexFormat::Float4: return 16;
        case VertexFormat::Int: return 4;
        case VertexFormat::Int2: return 8;
        case VertexFormat::Int3: return 12;
        case VertexFormat::Int4: return 16;
        case VertexFormat::UInt: return 4;
        case VertexFormat::UInt2: return 8;
        case VertexFormat::UInt3: return 12;
        case VertexFormat::UInt4: return 16;
        default: return 0;
    }
}

bool hasStencilComponent(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
           format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
}

bool isDepthFormat(TextureFormat format) {
    return format == TextureFormat::DEPTH16 ||
           format == TextureFormat::DEPTH24 ||
           format == TextureFormat::DEPTH32F ||
           format == TextureFormat::DEPTH24_STENCIL8 ||
           format == TextureFormat::DEPTH32F_STENCIL8;
}

uint32_t getBytesPerPixel(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_R8_UNORM: return 1;
        case DXGI_FORMAT_R8G8_UNORM: return 2;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return 4;
        case DXGI_FORMAT_R16_FLOAT: return 2;
        case DXGI_FORMAT_R16G16_FLOAT: return 4;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
        case DXGI_FORMAT_R32_FLOAT: return 4;
        case DXGI_FORMAT_R32G32_FLOAT: return 8;
        case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
        case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
        default: return 4;
    }
}

} // namespace DX11Utils

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX11
