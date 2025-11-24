#pragma once

#include "GfxTypes.hpp"
#include <vector>
#include <string>

namespace lupine {

/**
 * Swapchain creation descriptor
 */
struct SwapchainDesc {
    NativeWindowHandle window;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::RGBA8_SRGB;
    bool vsync = true;
    uint32_t bufferCount = 2; // Double buffering by default
    bool isolated = false; // If true, don't share OpenGL context with other swapchains
};

/**
 * Texture creation descriptor
 */
struct TextureDesc {
    TextureType type = TextureType::Texture2D;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    TextureFormat format = TextureFormat::RGBA8_UNORM;
    TextureUsage usage = TextureUsage::Sampled;
    const void* initialData = nullptr;
};

/**
 * Buffer creation descriptor
 */
struct BufferDesc {
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    const void* initialData = nullptr;
};

/**
 * Sampler creation descriptor
 */
struct SamplerDesc {
    FilterMode minFilter = FilterMode::Linear;
    FilterMode magFilter = FilterMode::Linear;
    FilterMode mipFilter = FilterMode::Linear;
    WrapMode wrapU = WrapMode::Repeat;
    WrapMode wrapV = WrapMode::Repeat;
    WrapMode wrapW = WrapMode::Repeat;
    float maxAnisotropy = 1.0f;
    bool compareEnable = false;
    CompareFunc compareFunc = CompareFunc::Never;
};

/**
 * Vertex attribute description
 */
struct VertexAttribute {
    std::string name;
    VertexFormat format;
    uint32_t offset;
    uint32_t binding;
};

/**
 * Vertex buffer layout
 */
struct VertexBufferLayout {
    uint32_t stride;
    std::vector<VertexAttribute> attributes;
};

/**
 * Blend state description
 */
struct BlendState {
    bool blendEnable = false;
    BlendFactor srcColorBlend = BlendFactor::One;
    BlendFactor dstColorBlend = BlendFactor::Zero;
    BlendOp colorBlendOp = BlendOp::Add;
    BlendFactor srcAlphaBlend = BlendFactor::One;
    BlendFactor dstAlphaBlend = BlendFactor::Zero;
    BlendOp alphaBlendOp = BlendOp::Add;

    // Predefined blend states
    static BlendState opaque();
    static BlendState alphaBlend();
    static BlendState additive();
    static BlendState multiply();
    static BlendState overlay();
};

/**
 * Depth/stencil state description
 */
struct DepthStencilState {
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    CompareFunc depthCompareFunc = CompareFunc::Less;
    bool stencilEnable = false;

    static DepthStencilState depthReadWrite();
    static DepthStencilState depthReadOnly();
    static DepthStencilState noDepth();
};

/**
 * Rasterizer state description
 */
struct RasterizerState {
    CullMode cullMode = CullMode::Back;
    FillMode fillMode = FillMode::Solid;
    WindingOrder frontFace = WindingOrder::CounterClockwise;
    bool depthClampEnable = false;
    bool scissorEnable = false;

    static RasterizerState defaultState();
    static RasterizerState noCull();
    static RasterizerState wireframe();
};

/**
 * Shader module description
 */
struct ShaderDesc {
    ShaderStage stage;
    const void* bytecode = nullptr;
    size_t bytecodeSize = 0;
    std::string entryPoint = "main";
};

/**
 * Pipeline creation descriptor
 */
struct PipelineDesc {
    std::vector<ShaderHandle> shaders;
    VertexBufferLayout vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    BlendState blendState;
    DepthStencilState depthStencilState;
    RasterizerState rasterizerState;
    TextureFormat colorFormat = TextureFormat::RGBA8_UNORM;
    TextureFormat depthFormat = TextureFormat::DEPTH24_STENCIL8;
};

/**
 * Render target description
 */
struct RenderTargetDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::RGBA8_UNORM;
    TextureFormat depthFormat = TextureFormat::DEPTH24_STENCIL8;
    bool hasColor = true;     // If false, depth-only render target
    bool hasDepth = true;
    uint32_t sampleCount = 1; // MSAA sample count
    bool isCubeMap = false;   // If true, create a cube map render target (6 faces)
    int cubeMapFace = -1;     // Which cube map face to attach (-1 = all faces, 0-5 = specific face)
};

// Predefined blend state implementations
inline BlendState BlendState::opaque() {
    BlendState state;
    state.blendEnable = false;
    return state;
}

inline BlendState BlendState::alphaBlend() {
    BlendState state;
    state.blendEnable = true;
    state.srcColorBlend = BlendFactor::SrcAlpha;
    state.dstColorBlend = BlendFactor::OneMinusSrcAlpha;
    state.colorBlendOp = BlendOp::Add;
    state.srcAlphaBlend = BlendFactor::One;
    state.dstAlphaBlend = BlendFactor::OneMinusSrcAlpha;
    state.alphaBlendOp = BlendOp::Add;
    return state;
}

inline BlendState BlendState::additive() {
    BlendState state;
    state.blendEnable = true;
    state.srcColorBlend = BlendFactor::SrcAlpha;
    state.dstColorBlend = BlendFactor::One;
    state.colorBlendOp = BlendOp::Add;
    state.srcAlphaBlend = BlendFactor::One;
    state.dstAlphaBlend = BlendFactor::One;
    state.alphaBlendOp = BlendOp::Add;
    return state;
}

inline BlendState BlendState::multiply() {
    BlendState state;
    state.blendEnable = true;
    state.srcColorBlend = BlendFactor::DstColor;
    state.dstColorBlend = BlendFactor::Zero;
    state.colorBlendOp = BlendOp::Add;
    state.srcAlphaBlend = BlendFactor::DstAlpha;
    state.dstAlphaBlend = BlendFactor::Zero;
    state.alphaBlendOp = BlendOp::Add;
    return state;
}

inline BlendState BlendState::overlay() {
    // Overlay approximation using screen-like blending
    // Screen blend: 1 - (1-Src) * (1-Dst) = Src + Dst - Src*Dst
    // This brightens and creates contrast similar to overlay
    // Formula: Src * One + Dst * OneMinusSrcColor approximates this
    BlendState state;
    state.blendEnable = true;
    state.srcColorBlend = BlendFactor::One;               // Take full source
    state.dstColorBlend = BlendFactor::OneMinusSrcColor;  // Add dest * (1-src)
    state.colorBlendOp = BlendOp::Add;
    state.srcAlphaBlend = BlendFactor::One;
    state.dstAlphaBlend = BlendFactor::OneMinusSrcAlpha;
    state.alphaBlendOp = BlendOp::Add;
    return state;
}

inline DepthStencilState DepthStencilState::depthReadWrite() {
    DepthStencilState state;
    state.depthTestEnable = true;
    state.depthWriteEnable = true;
    state.depthCompareFunc = CompareFunc::Less;
    return state;
}

inline DepthStencilState DepthStencilState::depthReadOnly() {
    DepthStencilState state;
    state.depthTestEnable = true;
    state.depthWriteEnable = false;
    state.depthCompareFunc = CompareFunc::Less;
    return state;
}

inline DepthStencilState DepthStencilState::noDepth() {
    DepthStencilState state;
    state.depthTestEnable = false;
    state.depthWriteEnable = false;
    return state;
}

inline RasterizerState RasterizerState::defaultState() {
    RasterizerState state;
    state.cullMode = CullMode::Back;
    state.fillMode = FillMode::Solid;
    state.frontFace = WindingOrder::CounterClockwise;
    return state;
}

inline RasterizerState RasterizerState::noCull() {
    RasterizerState state;
    state.cullMode = CullMode::None;
    state.fillMode = FillMode::Solid;
    return state;
}

inline RasterizerState RasterizerState::wireframe() {
    RasterizerState state;
    state.cullMode = CullMode::Back;
    state.fillMode = FillMode::Wireframe;
    return state;
}

} // namespace lupine
