#include "lupine/rendering/debug/DebugRendererDX11.hpp"
#include "lupine/rendering/GizmoUtils.hpp"
#include "lupine/rendering/debug/DebugTextGeometry.hpp"

#ifdef LUPINE_HAS_DIRECTX11

#include "lupine/rendering/backends/directx11/HLSLCompiler.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>

namespace lupine {

// DirectX 11-compatible HLSL shaders
namespace DX11DebugShaders {

const char* const DebugLine_Vertex = R"(
cbuffer ViewProjection : register(b0) {
    matrix u_ViewProjection;
};

struct VS_INPUT {
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct VS_OUTPUT {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.Position = mul(u_ViewProjection, float4(input.Position, 1.0));
    output.Color = input.Color;
    return output;
}
)";

const char* const DebugLine_Fragment = R"(
struct PS_INPUT {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(PS_INPUT input) : SV_TARGET {
    return input.Color;
}
)";

// Thick line vertex shader - passes data to geometry shader
const char* const ThickLine_Vertex = R"(
cbuffer LineParams : register(b0) {
    matrix u_ViewProjection;    // offset 0, size 64
    float u_LineWidth;          // offset 64, size 4
    float _padding;             // offset 68, size 4 (alignment padding)
    float2 u_ScreenSize;        // offset 72, size 8
    float _pad0;                // offset 80, size 4
    float _pad1;                // offset 84, size 4
    float _pad2;                // offset 88, size 4
    float _pad3;                // offset 92, size 4
};

struct VS_INPUT {
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct VS_OUTPUT {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.Position = mul(u_ViewProjection, float4(input.Position, 1.0));
    output.Color = input.Color;
    return output;
}
)";

// Thick line geometry shader - expands lines into quads
// Approach: Calculate perpendicular in NDC space for constant screen thickness
const char* const ThickLine_Geometry = R"(
cbuffer LineParams : register(b0) {
    matrix u_ViewProjection;    // offset 0, size 64
    float u_LineWidth;          // offset 64, size 4
    float _padding;             // offset 68, size 4 (alignment padding)
    float2 u_ScreenSize;        // offset 72, size 8
    float _pad0;                // offset 80, size 4
    float _pad1;                // offset 84, size 4
    float _pad2;                // offset 88, size 4
    float _pad3;                // offset 92, size 4
};

struct GS_INPUT {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

struct GS_OUTPUT {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 LineCoord : TEXCOORD0;
};

[maxvertexcount(4)]
void main(line GS_INPUT input[2], inout TriangleStream<GS_OUTPUT> outputStream) {
    float4 clipPos0 = input[0].Position;
    float4 clipPos1 = input[1].Position;

    // Convert to NDC (this is what we see on screen after perspective divide)
    float2 ndc0 = clipPos0.xy / clipPos0.w;
    float2 ndc1 = clipPos1.xy / clipPos1.w;

    // Line direction in NDC space
    float2 lineDir = ndc1 - ndc0;

    // Aspect ratio correction for proper perpendicular in screen space
    float aspect = u_ScreenSize.x / u_ScreenSize.y;
    float2 lineDirCorrected = float2(lineDir.x * aspect, lineDir.y);
    float len = length(lineDirCorrected);

    // Avoid division by zero for degenerate lines
    if (len < 0.0001) {
        return;
    }

    // Perpendicular direction in aspect-corrected space
    float2 perpCorrected = float2(-lineDirCorrected.y, lineDirCorrected.x) / len;

    // Convert perpendicular back to NDC space
    float2 perpNDC = float2(perpCorrected.x / aspect, perpCorrected.y);

    // Calculate offset in NDC units (line width in pixels -> NDC)
    // 2.0 / screenHeight gives the NDC size of one pixel in Y
    // We use Y as reference since perpNDC is normalized in screen space
    float pixelToNDC = 2.0 / u_ScreenSize.y;
    float2 offsetNDC = perpNDC * (u_LineWidth * 0.5 * pixelToNDC);

    GS_OUTPUT output;

    // Emit a triangle strip forming a quad
    // Apply offset in clip space: since ndc = clip.xy/w,
    // to offset ndc we need to offset clip.xy by offsetNDC * w

    // Vertex 0: Start point, negative offset
    output.Position = float4(clipPos0.xy - offsetNDC * clipPos0.w, clipPos0.z, clipPos0.w);
    output.Color = input[0].Color;
    output.LineCoord = float2(0.0, -1.0);
    outputStream.Append(output);

    // Vertex 1: Start point, positive offset
    output.Position = float4(clipPos0.xy + offsetNDC * clipPos0.w, clipPos0.z, clipPos0.w);
    output.Color = input[0].Color;
    output.LineCoord = float2(0.0, 1.0);
    outputStream.Append(output);

    // Vertex 2: End point, negative offset
    output.Position = float4(clipPos1.xy - offsetNDC * clipPos1.w, clipPos1.z, clipPos1.w);
    output.Color = input[1].Color;
    output.LineCoord = float2(1.0, -1.0);
    outputStream.Append(output);

    // Vertex 3: End point, positive offset
    output.Position = float4(clipPos1.xy + offsetNDC * clipPos1.w, clipPos1.z, clipPos1.w);
    output.Color = input[1].Color;
    output.LineCoord = float2(1.0, 1.0);
    outputStream.Append(output);

    outputStream.RestartStrip();
}
)";

// Thick line fragment shader with anti-aliasing
const char* const ThickLine_Fragment = R"(
struct PS_INPUT {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 LineCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET {
    float4 color = input.Color;

    // Anti-aliasing based on distance from line center
    float dist = abs(input.LineCoord.y);
    float aa = 1.0 - smoothstep(0.7, 1.0, dist);

    color.a *= aa;

    // Discard fully transparent pixels
    if (color.a < 0.01) {
        discard;
    }

    return color;
}
)";

} // namespace DX11DebugShaders

struct LineVertex {
    Vec3 position;
    Color color;
};

struct DebugRendererDX11::Impl {
    GfxDeviceDX11* device = nullptr;
    bool initialized = false;

    // Thin line shaders (1px, for fallback)
    ShaderHandle lineVertexShader;
    ShaderHandle lineFragmentShader;
    PipelineHandle linePipelineDepthTest;
    PipelineHandle linePipelineNoDepthTest;

    // Thick line shaders (geometry shader based)
    ShaderHandle thickLineVertexShader;
    ShaderHandle thickLineGeometryShader;
    ShaderHandle thickLineFragmentShader;
    PipelineHandle thickLinePipelineDepthTest;
    PipelineHandle thickLinePipelineNoDepthTest;

    // Screen size for thick line rendering
    float screenWidth = 1920.0f;
    float screenHeight = 1080.0f;
    float lineWidth = 2.0f;  // Default line width in pixels

    // Line data organized by category and depth test mode
    // [category][depthTest: 0=no, 1=yes]
    static constexpr int NUM_CATEGORIES = 4;
    std::vector<LineVertex> lineVertices[NUM_CATEGORIES][2];
    std::vector<uint32_t> lineIndices[NUM_CATEGORIES][2];

    BufferHandle lineVertexBuffers[NUM_CATEGORIES][2];
    BufferHandle lineIndexBuffers[NUM_CATEGORIES][2];

    struct PersistentLine {
        Vec3 start;
        Vec3 end;
        Color color;
        float remainingTime;
        bool depthTest;
        DebugRendererDX11::LineCategory category;
    };
    std::vector<PersistentLine> persistentLines;

    float deltaTime = 0.016f;
    bool useThickLines = true;  // Enable thick line rendering by default
};

DebugRendererDX11::DebugRendererDX11()
    : m_impl(std::make_unique<Impl>()) {
}

DebugRendererDX11::~DebugRendererDX11() {
    if (m_impl->initialized) {
        shutdown();
    }
}

bool DebugRendererDX11::initialize(IGfxDevice* device) {
    if (m_impl->initialized) {
        return true;
    }

    m_impl->device = dynamic_cast<GfxDeviceDX11*>(device);
    if (!m_impl->device) {
        LOG_ERROR(LogCategory::Render, "DebugRendererDX11: Device is not a GfxDeviceDX11");
        return false;
    }

    // Compile shaders using HLSLCompiler
    std::vector<uint8_t> vertexBytecode;
    std::string vertexErrors;
    if (!HLSLCompiler::compile(DX11DebugShaders::DebugLine_Vertex, ShaderStage::Vertex, "main", vertexBytecode, vertexErrors)) {
        LOG_ERROR(LogCategory::Render, "Failed to compile debug line vertex shader: {}", vertexErrors);
        return false;
    }

    ShaderDesc vertexShaderDesc;
    vertexShaderDesc.stage = ShaderStage::Vertex;
    vertexShaderDesc.bytecode = vertexBytecode.data();
    vertexShaderDesc.bytecodeSize = vertexBytecode.size();
    vertexShaderDesc.entryPoint = "main";

    m_impl->lineVertexShader = m_impl->device->createShader(vertexShaderDesc);
    if (!m_impl->lineVertexShader.isValid()) {
        LOG_ERROR(LogCategory::Render, "Failed to create debug line vertex shader");
        return false;
    }

    std::vector<uint8_t> fragmentBytecode;
    std::string fragmentErrors;
    if (!HLSLCompiler::compile(DX11DebugShaders::DebugLine_Fragment, ShaderStage::Fragment, "main", fragmentBytecode, fragmentErrors)) {
        LOG_ERROR(LogCategory::Render, "Failed to compile debug line fragment shader: {}", fragmentErrors);
        return false;
    }

    ShaderDesc fragmentShaderDesc;
    fragmentShaderDesc.stage = ShaderStage::Fragment;
    fragmentShaderDesc.bytecode = fragmentBytecode.data();
    fragmentShaderDesc.bytecodeSize = fragmentBytecode.size();
    fragmentShaderDesc.entryPoint = "main";

    m_impl->lineFragmentShader = m_impl->device->createShader(fragmentShaderDesc);
    if (!m_impl->lineFragmentShader.isValid()) {
        LOG_ERROR(LogCategory::Render, "Failed to create debug line fragment shader");
        return false;
    }

    // Create line pipeline with depth test
    PipelineDesc linePipelineDescDepth;
    linePipelineDescDepth.shaders = {m_impl->lineVertexShader, m_impl->lineFragmentShader};
    linePipelineDescDepth.topology = PrimitiveTopology::LineList;
    linePipelineDescDepth.blendState = BlendState::alphaBlend();
    linePipelineDescDepth.depthStencilState.depthTestEnable = true;
    linePipelineDescDepth.depthStencilState.depthWriteEnable = false;
    linePipelineDescDepth.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
    linePipelineDescDepth.rasterizerState = RasterizerState::noCull();

    linePipelineDescDepth.vertexLayout.stride = sizeof(LineVertex);
    linePipelineDescDepth.vertexLayout.attributes = {
        {"a_Position", VertexFormat::Float3, offsetof(LineVertex, position), 0, 0},
        {"a_Color", VertexFormat::Float4, offsetof(LineVertex, color), 0, 1}
    };

    m_impl->linePipelineDepthTest = m_impl->device->createPipeline(linePipelineDescDepth);
    if (!m_impl->linePipelineDepthTest.isValid()) {
        LOG_ERROR(LogCategory::Render, "Failed to create debug line pipeline with depth test");
        shutdown();
        return false;
    }

    // Create line pipeline without depth test
    PipelineDesc linePipelineDescNoDepth;
    linePipelineDescNoDepth.shaders = {m_impl->lineVertexShader, m_impl->lineFragmentShader};
    linePipelineDescNoDepth.topology = PrimitiveTopology::LineList;
    linePipelineDescNoDepth.blendState = BlendState::alphaBlend();
    linePipelineDescNoDepth.depthStencilState = DepthStencilState::noDepth();
    linePipelineDescNoDepth.rasterizerState = RasterizerState::noCull();

    linePipelineDescNoDepth.vertexLayout.stride = sizeof(LineVertex);
    linePipelineDescNoDepth.vertexLayout.attributes = {
        {"a_Position", VertexFormat::Float3, offsetof(LineVertex, position), 0, 0},
        {"a_Color", VertexFormat::Float4, offsetof(LineVertex, color), 0, 1}
    };

    m_impl->linePipelineNoDepthTest = m_impl->device->createPipeline(linePipelineDescNoDepth);
    if (!m_impl->linePipelineNoDepthTest.isValid()) {
        LOG_ERROR(LogCategory::Render, "Failed to create debug line pipeline without depth test");
        shutdown();
        return false;
    }

    // Compile thick line shaders
    std::vector<uint8_t> thickVertexBytecode;
    std::string thickVertexErrors;
    if (!HLSLCompiler::compile(DX11DebugShaders::ThickLine_Vertex, ShaderStage::Vertex, "main", thickVertexBytecode, thickVertexErrors)) {
        
        m_impl->useThickLines = false;
    } else {
        ShaderDesc thickVertexShaderDesc;
        thickVertexShaderDesc.stage = ShaderStage::Vertex;
        thickVertexShaderDesc.bytecode = thickVertexBytecode.data();
        thickVertexShaderDesc.bytecodeSize = thickVertexBytecode.size();
        thickVertexShaderDesc.entryPoint = "main";
        m_impl->thickLineVertexShader = m_impl->device->createShader(thickVertexShaderDesc);

        std::vector<uint8_t> thickGeometryBytecode;
        std::string thickGeometryErrors;
        if (!HLSLCompiler::compile(DX11DebugShaders::ThickLine_Geometry, ShaderStage::Geometry, "main", thickGeometryBytecode, thickGeometryErrors)) {
            
            m_impl->useThickLines = false;
        } else {
            ShaderDesc thickGeometryShaderDesc;
            thickGeometryShaderDesc.stage = ShaderStage::Geometry;
            thickGeometryShaderDesc.bytecode = thickGeometryBytecode.data();
            thickGeometryShaderDesc.bytecodeSize = thickGeometryBytecode.size();
            thickGeometryShaderDesc.entryPoint = "main";
            m_impl->thickLineGeometryShader = m_impl->device->createShader(thickGeometryShaderDesc);

            std::vector<uint8_t> thickFragmentBytecode;
            std::string thickFragmentErrors;
            if (!HLSLCompiler::compile(DX11DebugShaders::ThickLine_Fragment, ShaderStage::Fragment, "main", thickFragmentBytecode, thickFragmentErrors)) {
                
                m_impl->useThickLines = false;
            } else {
                ShaderDesc thickFragmentShaderDesc;
                thickFragmentShaderDesc.stage = ShaderStage::Fragment;
                thickFragmentShaderDesc.bytecode = thickFragmentBytecode.data();
                thickFragmentShaderDesc.bytecodeSize = thickFragmentBytecode.size();
                thickFragmentShaderDesc.entryPoint = "main";
                m_impl->thickLineFragmentShader = m_impl->device->createShader(thickFragmentShaderDesc);

                // Create thick line pipeline with depth test
                PipelineDesc thickLinePipelineDescDepth;
                thickLinePipelineDescDepth.shaders = {m_impl->thickLineVertexShader, m_impl->thickLineGeometryShader, m_impl->thickLineFragmentShader};
                thickLinePipelineDescDepth.topology = PrimitiveTopology::LineList;
                thickLinePipelineDescDepth.blendState = BlendState::alphaBlend();
                thickLinePipelineDescDepth.depthStencilState.depthTestEnable = true;
                thickLinePipelineDescDepth.depthStencilState.depthWriteEnable = false;
                thickLinePipelineDescDepth.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
                thickLinePipelineDescDepth.rasterizerState = RasterizerState::noCull();
                thickLinePipelineDescDepth.vertexLayout.stride = sizeof(LineVertex);
                thickLinePipelineDescDepth.vertexLayout.attributes = {
                    {"a_Position", VertexFormat::Float3, offsetof(LineVertex, position), 0, 0},
                    {"a_Color", VertexFormat::Float4, offsetof(LineVertex, color), 0, 1}
                };

                m_impl->thickLinePipelineDepthTest = m_impl->device->createPipeline(thickLinePipelineDescDepth);

                // Create thick line pipeline without depth test
                PipelineDesc thickLinePipelineDescNoDepth;
                thickLinePipelineDescNoDepth.shaders = {m_impl->thickLineVertexShader, m_impl->thickLineGeometryShader, m_impl->thickLineFragmentShader};
                thickLinePipelineDescNoDepth.topology = PrimitiveTopology::LineList;
                thickLinePipelineDescNoDepth.blendState = BlendState::alphaBlend();
                thickLinePipelineDescNoDepth.depthStencilState = DepthStencilState::noDepth();
                thickLinePipelineDescNoDepth.rasterizerState = RasterizerState::noCull();
                thickLinePipelineDescNoDepth.vertexLayout.stride = sizeof(LineVertex);
                thickLinePipelineDescNoDepth.vertexLayout.attributes = {
                    {"a_Position", VertexFormat::Float3, offsetof(LineVertex, position), 0, 0},
                    {"a_Color", VertexFormat::Float4, offsetof(LineVertex, color), 0, 1}
                };

                m_impl->thickLinePipelineNoDepthTest = m_impl->device->createPipeline(thickLinePipelineDescNoDepth);

                if (!m_impl->thickLinePipelineDepthTest.isValid() || !m_impl->thickLinePipelineNoDepthTest.isValid()) {
                    
                    m_impl->useThickLines = false;
                } else {
                    
                }
            }
        }
    }

    // Create line buffers for each category and depth mode
    BufferDesc lineVBDesc;
    lineVBDesc.size = 10000 * sizeof(LineVertex);
    lineVBDesc.usage = BufferUsage::Vertex;

    BufferDesc lineIBDesc;
    lineIBDesc.size = 10000 * sizeof(uint32_t);
    lineIBDesc.usage = BufferUsage::Index;

    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            m_impl->lineVertexBuffers[cat][depth] = m_impl->device->createBuffer(lineVBDesc);
            m_impl->lineIndexBuffers[cat][depth] = m_impl->device->createBuffer(lineIBDesc);
        }
    }

    m_impl->initialized = true;

    return true;
}

void DebugRendererDX11::shutdown() {
    if (!m_impl->initialized || !m_impl->device) {
        return;
    }

    // Destroy thin line shaders
    if (m_impl->lineVertexShader.isValid()) {
        m_impl->device->destroyShader(m_impl->lineVertexShader);
    }
    if (m_impl->lineFragmentShader.isValid()) {
        m_impl->device->destroyShader(m_impl->lineFragmentShader);
    }

    // Destroy thick line shaders
    if (m_impl->thickLineVertexShader.isValid()) {
        m_impl->device->destroyShader(m_impl->thickLineVertexShader);
    }
    if (m_impl->thickLineGeometryShader.isValid()) {
        m_impl->device->destroyShader(m_impl->thickLineGeometryShader);
    }
    if (m_impl->thickLineFragmentShader.isValid()) {
        m_impl->device->destroyShader(m_impl->thickLineFragmentShader);
    }

    // Destroy thin line pipelines
    if (m_impl->linePipelineDepthTest.isValid()) {
        m_impl->device->destroyPipeline(m_impl->linePipelineDepthTest);
    }
    if (m_impl->linePipelineNoDepthTest.isValid()) {
        m_impl->device->destroyPipeline(m_impl->linePipelineNoDepthTest);
    }

    // Destroy thick line pipelines
    if (m_impl->thickLinePipelineDepthTest.isValid()) {
        m_impl->device->destroyPipeline(m_impl->thickLinePipelineDepthTest);
    }
    if (m_impl->thickLinePipelineNoDepthTest.isValid()) {
        m_impl->device->destroyPipeline(m_impl->thickLinePipelineNoDepthTest);
    }

    // Destroy buffers
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            if (m_impl->lineVertexBuffers[cat][depth].isValid()) {
                m_impl->device->destroyBuffer(m_impl->lineVertexBuffers[cat][depth]);
            }
            if (m_impl->lineIndexBuffers[cat][depth].isValid()) {
                m_impl->device->destroyBuffer(m_impl->lineIndexBuffers[cat][depth]);
            }
        }
    }

    m_impl->initialized = false;
}

void DebugRendererDX11::beginFrame() {
    m_impl->deltaTime = 0.016f; // Default delta time

    // Clear all line data at the start of each frame
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            m_impl->lineVertices[cat][depth].clear();
            m_impl->lineIndices[cat][depth].clear();
        }
    }

    // Update persistent lines and remove expired ones
    auto it = m_impl->persistentLines.begin();
    while (it != m_impl->persistentLines.end()) {
        it->remainingTime -= m_impl->deltaTime;
        if (it->remainingTime <= 0.0f) {
            it = m_impl->persistentLines.erase(it);
        } else {
            // Re-add persistent lines to vertex buffers
            drawLineInternal(it->start, it->end, it->color, it->depthTest, it->category);
            ++it;
        }
    }
}

void DebugRendererDX11::endFrame() {
    // Nothing to do
}

void DebugRendererDX11::render(IGfxCommandList* cmd, const Mat4& viewProjection, float lineWidth) {
    if (!m_impl->initialized || !cmd) {
        return;
    }

    // Update line width if provided
    if (lineWidth > 0.0f) {
        m_impl->lineWidth = lineWidth;
    }

    // Determine if we should use thick lines (but not for Grid category)
    bool thickLinesAvailable = m_impl->useThickLines && m_impl->lineWidth > 1.0f;

    // Render lines for each category and depth mode
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            auto& vertices = m_impl->lineVertices[cat][depth];
            auto& indices = m_impl->lineIndices[cat][depth];

            if (vertices.empty() || indices.empty()) {
                continue;
            }

            // Update vertex buffer
            uint32_t vertexDataSize = static_cast<uint32_t>(vertices.size() * sizeof(LineVertex));
            m_impl->device->updateBuffer(m_impl->lineVertexBuffers[cat][depth], vertices.data(), vertexDataSize, 0);

            // Update index buffer
            uint32_t indexDataSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
            m_impl->device->updateBuffer(m_impl->lineIndexBuffers[cat][depth], indices.data(), indexDataSize, 0);

            // Use thin lines for Grid category (cat == 0), thick lines for everything else
            bool useThickLinesForCategory = thickLinesAvailable && (cat != static_cast<int>(LineCategory::Grid));

            // Bind appropriate pipeline
            bool useDepthTest = (depth == 1);
            PipelineHandle pipeline;

            if (useThickLinesForCategory) {
                pipeline = useDepthTest ? m_impl->thickLinePipelineDepthTest : m_impl->thickLinePipelineNoDepthTest;
            } else {
                pipeline = useDepthTest ? m_impl->linePipelineDepthTest : m_impl->linePipelineNoDepthTest;
            }
            cmd->bindPipeline(pipeline);

            if (useThickLinesForCategory) {
                // Set LineParams cbuffer for thick lines
                // HLSL cbuffer packing rules:
                // - matrix u_ViewProjection: offset 0, size 64 (aligned to 16)
                // - float u_LineWidth: offset 64, size 4
                // - PADDING: 4 bytes (float2 must be aligned to 8 bytes)
                // - float2 u_ScreenSize: offset 72, size 8
                // - float _pad0: offset 80, size 4
                // Total: 84 bytes (rounded to 96 for 16-byte alignment)
                struct alignas(16) LineParams {
                    Mat4 viewProjection;    // offset 0, size 64
                    float lineWidth;        // offset 64, size 4
                    float _padding;         // offset 68, size 4 (alignment padding for float2)
                    float screenWidth;      // offset 72, size 4 (u_ScreenSize.x)
                    float screenHeight;     // offset 76, size 4 (u_ScreenSize.y)
                    float _pad0;            // offset 80, size 4
                    float _pad1;            // offset 84, size 4
                    float _pad2;            // offset 88, size 4
                    float _pad3;            // offset 92, size 4
                };
                LineParams params;
                params.viewProjection = viewProjection;
                params.lineWidth = m_impl->lineWidth;
                params._padding = 0.0f;
                params.screenWidth = m_impl->screenWidth;
                params.screenHeight = m_impl->screenHeight;
                params._pad0 = 0.0f;
                params._pad1 = 0.0f;
                params._pad2 = 0.0f;
                params._pad3 = 0.0f;
                cmd->pushConstants(ShaderStage::Vertex, &params, sizeof(LineParams), 0);
            } else {
                // Set view-projection matrix via push constants for thin lines
                cmd->pushConstants(ShaderStage::Vertex, &viewProjection, sizeof(Mat4), 0);
            }

            // Bind vertex and index buffers
            cmd->bindVertexBuffer(m_impl->lineVertexBuffers[cat][depth], 0, 0);
            cmd->bindIndexBuffer(m_impl->lineIndexBuffers[cat][depth], IndexFormat::UInt32, 0);

            // Draw
            uint32_t indexCount = static_cast<uint32_t>(indices.size());
            cmd->drawIndexed(indexCount, 1, 0, 0, 0);
        }
    }
}

void DebugRendererDX11::clear() {
    // Clear all line data
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            m_impl->lineVertices[cat][depth].clear();
            m_impl->lineIndices[cat][depth].clear();
        }
    }
}

void DebugRendererDX11::setScreenSize(float width, float height) {
    m_impl->screenWidth = width;
    m_impl->screenHeight = height;
}

void DebugRendererDX11::setLineWidth(float width) {
    m_impl->lineWidth = width;
}

void DebugRendererDX11::setUseThickLines(bool enable) {
    // Only enable if thick line shaders were successfully compiled
    m_impl->useThickLines = enable && m_impl->thickLinePipelineDepthTest.isValid();
}

// Internal line drawing
void DebugRendererDX11::drawLineInternal(const Vec3& start, const Vec3& end, const Color& color,
                                          bool depthTest, LineCategory category) {
    int catIndex = static_cast<int>(category);
    int depthIndex = depthTest ? 1 : 0;

    auto& vertices = m_impl->lineVertices[catIndex][depthIndex];
    auto& indices = m_impl->lineIndices[catIndex][depthIndex];

    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    vertices.push_back({start, color});
    vertices.push_back({end, color});

    indices.push_back(baseIndex);
    indices.push_back(baseIndex + 1);
}

void DebugRendererDX11::drawLineWithCategory(const Vec3& start, const Vec3& end, const Color& color,
                                              float duration, bool depthTest, LineCategory category) {
    if (duration > 0.0f) {
        Impl::PersistentLine persistentLine;
        persistentLine.start = start;
        persistentLine.end = end;
        persistentLine.color = color;
        persistentLine.remainingTime = duration;
        persistentLine.depthTest = depthTest;
        persistentLine.category = category;
        m_impl->persistentLines.push_back(persistentLine);
    }

    drawLineInternal(start, end, color, depthTest, category);
}

void DebugRendererDX11::drawLineStripWithCategory(const std::vector<Vec3>& points, const Color& color,
                                                   float duration, bool depthTest, LineCategory category) {
    if (points.size() < 2) {
        return;
    }

    for (size_t i = 0; i < points.size() - 1; ++i) {
        drawLineWithCategory(points[i], points[i + 1], color, duration, depthTest, category);
    }
}

// Grid rendering
void DebugRendererDX11::draw2DGrid(const DebugGrid& grid) {
    renderGrid(nullptr, grid, Mat4::Identity());
}

void DebugRendererDX11::draw3DGrid(const DebugGrid& grid) {
    renderGrid(nullptr, grid, Mat4::Identity());
}

void DebugRendererDX11::renderGrid(IGfxCommandList*, const DebugGrid& grid, const Mat4&) {
    float cellSize = grid.cellSize;
    int halfCells = grid.cellCount / 2;

    if (grid.is3D) {
        // 3D grid on XZ plane
        for (int i = -halfCells; i <= halfCells; ++i) {
            float offset = i * cellSize;

            Color color = (i == 0) ? grid.axisColor : grid.gridColor;
            Vec3 start(grid.center.x - halfCells * cellSize, grid.center.y, grid.center.z + offset);
            Vec3 end(grid.center.x + halfCells * cellSize, grid.center.y, grid.center.z + offset);
            drawLineWithCategory(start, end, color, 0.0f, true, LineCategory::Grid);

            color = (i == 0) ? grid.axisColor : grid.gridColor;
            start = Vec3(grid.center.x + offset, grid.center.y, grid.center.z - halfCells * cellSize);
            end = Vec3(grid.center.x + offset, grid.center.y, grid.center.z + halfCells * cellSize);
            drawLineWithCategory(start, end, color, 0.0f, true, LineCategory::Grid);
        }

        // Draw Y axis
        if (grid.drawAxes) {
            float axisLength = halfCells * cellSize;
            drawLineWithCategory(grid.center, grid.center + Vec3(0, axisLength, 0), Color::Green(), 0.0f, true, LineCategory::Grid);
        }
    } else {
        // 2D grid on XY plane
        for (int i = -halfCells; i <= halfCells; ++i) {
            float offset = i * cellSize;

            Color color = (i == 0) ? grid.axisColor : grid.gridColor;
            Vec3 start(grid.center.x - halfCells * cellSize, grid.center.y + offset, grid.center.z);
            Vec3 end(grid.center.x + halfCells * cellSize, grid.center.y + offset, grid.center.z);
            drawLineWithCategory(start, end, color, 0.0f, false, LineCategory::Grid);

            color = (i == 0) ? grid.axisColor : grid.gridColor;
            start = Vec3(grid.center.x + offset, grid.center.y - halfCells * cellSize, grid.center.z);
            end = Vec3(grid.center.x + offset, grid.center.y + halfCells * cellSize, grid.center.z);
            drawLineWithCategory(start, end, color, 0.0f, false, LineCategory::Grid);
        }
    }
}

// Bounding boxes
void DebugRendererDX11::draw2DBoundingBox(const AABB& bounds, const Color& color, float duration) {
    Vec3 min = bounds.min;
    Vec3 max = bounds.max;

    // Draw 4 edges of the 2D bounding box (on XY plane)
    drawLineWithCategory(Vec3(min.x, min.y, 0), Vec3(max.x, min.y, 0), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, min.y, 0), Vec3(max.x, max.y, 0), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, max.y, 0), Vec3(min.x, max.y, 0), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(min.x, max.y, 0), Vec3(min.x, min.y, 0), color, duration, false, LineCategory::BoundingBox);
}

void DebugRendererDX11::draw2DOrientedBoundingBox(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration) {
    float halfW = size.x * 0.5f;
    float halfH = size.y * 0.5f;

    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);

    // Calculate rotated corners
    Vec2 corners[4] = {
        Vec2(-halfW, -halfH),
        Vec2( halfW, -halfH),
        Vec2( halfW,  halfH),
        Vec2(-halfW,  halfH)
    };

    for (int i = 0; i < 4; ++i) {
        float x = corners[i].x * cosR - corners[i].y * sinR + center.x;
        float y = corners[i].x * sinR + corners[i].y * cosR + center.y;
        corners[i] = Vec2(x, y);
    }

    // Draw edges
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        drawLineWithCategory(Vec3(corners[i].x, corners[i].y, 0), Vec3(corners[next].x, corners[next].y, 0),
                             color, duration, false, LineCategory::BoundingBox);
    }
}

void DebugRendererDX11::draw3DBoundingBox(const AABB& bounds, const Color& color, float duration) {
    Vec3 min = bounds.min;
    Vec3 max = bounds.max;

    Vec3 corners[8] = {
        Vec3(min.x, min.y, min.z),
        Vec3(max.x, min.y, min.z),
        Vec3(max.x, max.y, min.z),
        Vec3(min.x, max.y, min.z),
        Vec3(min.x, min.y, max.z),
        Vec3(max.x, min.y, max.z),
        Vec3(max.x, max.y, max.z),
        Vec3(min.x, max.y, max.z)
    };

    // Bottom face
    drawLineWithCategory(corners[0], corners[1], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[1], corners[2], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[2], corners[3], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[3], corners[0], color, duration, true, LineCategory::BoundingBox);

    // Top face
    drawLineWithCategory(corners[4], corners[5], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[5], corners[6], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[6], corners[7], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[7], corners[4], color, duration, true, LineCategory::BoundingBox);

    // Vertical edges
    drawLineWithCategory(corners[0], corners[4], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[1], corners[5], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[2], corners[6], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[3], corners[7], color, duration, true, LineCategory::BoundingBox);
}

void DebugRendererDX11::draw3DOrientedBoundingBox(const OBB& obb, const Color& color, float duration) {
    Vec3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i] = obb.GetCorner(i);
    }

    // OBB::GetCorner uses bit indexing:
    // Corner 0: (-x, -y, -z), 1: (+x, -y, -z), 2: (-x, +y, -z), 3: (+x, +y, -z)
    // Corner 4: (-x, -y, +z), 5: (+x, -y, +z), 6: (-x, +y, +z), 7: (+x, +y, +z)
    // So winding order is 0→1→3→2 (not 0→1→2→3)

    // Bottom face (z = -extents.z)
    drawLineWithCategory(corners[0], corners[1], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[1], corners[3], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[3], corners[2], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[2], corners[0], color, duration, true, LineCategory::BoundingBox);

    // Top face (z = +extents.z)
    drawLineWithCategory(corners[4], corners[5], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[5], corners[7], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[7], corners[6], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[6], corners[4], color, duration, true, LineCategory::BoundingBox);

    // Vertical edges
    drawLineWithCategory(corners[0], corners[4], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[1], corners[5], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[2], corners[6], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[3], corners[7], color, duration, true, LineCategory::BoundingBox);
}

// Gizmos
void DebugRendererDX11::draw2DGizmo(const DebugGizmo& gizmo) {
    std::vector<GizmoSegment> segments = GizmoUtils::BuildGizmo2DGeometry(gizmo);
    for (const GizmoSegment& seg : segments) {
        drawLineWithCategory(seg.start, seg.end, seg.color, 0.0f, false, LineCategory::Gizmo);
    }
}

void DebugRendererDX11::draw3DGizmo(const DebugGizmo& gizmo) {
    float size = gizmo.size;
    Vec3 pos = gizmo.position;

    // Draw based on gizmo type
    if (gizmo.type == GizmoType::Translation || gizmo.type == GizmoType::All) {
        // X axis (red)
        drawGizmoArrow(pos, Vec3(1, 0, 0), size, Color::Red(), true);
        // Y axis (green)
        drawGizmoArrow(pos, Vec3(0, 1, 0), size, Color::Green(), true);
        // Z axis (blue)
        drawGizmoArrow(pos, Vec3(0, 0, 1), size, Color::Blue(), true);
    }

    if (gizmo.type == GizmoType::Rotation || gizmo.type == GizmoType::All) {
        // Draw rotation circles
        float circleRadius = size * 0.8f;
        drawGizmoCircle(pos, Vec3(1, 0, 0), circleRadius, Color::Red(), true);
        drawGizmoCircle(pos, Vec3(0, 1, 0), circleRadius, Color::Green(), true);
        drawGizmoCircle(pos, Vec3(0, 0, 1), circleRadius, Color::Blue(), true);
    }

    if (gizmo.type == GizmoType::Scale || gizmo.type == GizmoType::All) {
        // Draw scale boxes at the end of each axis
        float boxSize = size * 0.1f;
        drawGizmoBox(pos + Vec3(size, 0, 0), boxSize, Color::Red(), true);
        drawGizmoBox(pos + Vec3(0, size, 0), boxSize, Color::Green(), true);
        drawGizmoBox(pos + Vec3(0, 0, size), boxSize, Color::Blue(), true);
    }
}

// Basic primitives
void DebugRendererDX11::drawLine(const Vec3& start, const Vec3& end, const Color& color,
                                  float duration, bool depthTest) {
    drawLineWithCategory(start, end, color, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX11::drawLineStrip(const std::vector<Vec3>& points, const Color& color,
                                       float duration, bool depthTest) {
    drawLineStripWithCategory(points, color, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX11::drawRay(const Vec3& origin, const Vec3& direction, const Color& color,
                                 float length, float duration, bool depthTest) {
    Vec3 end = origin + direction.Normalized() * length;
    drawLine(origin, end, color, duration, depthTest);
}

void DebugRendererDX11::drawBox(const Vec3& center, const Vec3& size, const Color& color,
                                 bool wireframe, float duration, bool depthTest) {
    drawBoxWithCategory(center, size, color, wireframe, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX11::drawBoxWithCategory(const Vec3& center, const Vec3& size, const Color& color,
                                             bool wireframe, float duration, bool depthTest, LineCategory category) {
    Vec3 halfSize = size * 0.5f;

    if (wireframe) {
        Vec3 corners[8] = {
            center + Vec3(-halfSize.x, -halfSize.y, -halfSize.z),
            center + Vec3( halfSize.x, -halfSize.y, -halfSize.z),
            center + Vec3( halfSize.x,  halfSize.y, -halfSize.z),
            center + Vec3(-halfSize.x,  halfSize.y, -halfSize.z),
            center + Vec3(-halfSize.x, -halfSize.y,  halfSize.z),
            center + Vec3( halfSize.x, -halfSize.y,  halfSize.z),
            center + Vec3( halfSize.x,  halfSize.y,  halfSize.z),
            center + Vec3(-halfSize.x,  halfSize.y,  halfSize.z),
        };

        // Bottom face
        drawLineWithCategory(corners[0], corners[1], color, duration, depthTest, category);
        drawLineWithCategory(corners[1], corners[2], color, duration, depthTest, category);
        drawLineWithCategory(corners[2], corners[3], color, duration, depthTest, category);
        drawLineWithCategory(corners[3], corners[0], color, duration, depthTest, category);

        // Top face
        drawLineWithCategory(corners[4], corners[5], color, duration, depthTest, category);
        drawLineWithCategory(corners[5], corners[6], color, duration, depthTest, category);
        drawLineWithCategory(corners[6], corners[7], color, duration, depthTest, category);
        drawLineWithCategory(corners[7], corners[4], color, duration, depthTest, category);

        // Vertical edges
        drawLineWithCategory(corners[0], corners[4], color, duration, depthTest, category);
        drawLineWithCategory(corners[1], corners[5], color, duration, depthTest, category);
        drawLineWithCategory(corners[2], corners[6], color, duration, depthTest, category);
        drawLineWithCategory(corners[3], corners[7], color, duration, depthTest, category);
    }
}

void DebugRendererDX11::drawSphere(const Vec3& center, float radius, const Color& color,
                                    bool wireframe, int segments, float duration, bool depthTest) {
    if (!wireframe) {
        return;
    }

    // Draw 3 circles for each axis
    drawCircle(center, Vec3(1, 0, 0), radius, color, segments, duration, depthTest);
    drawCircle(center, Vec3(0, 1, 0), radius, color, segments, duration, depthTest);
    drawCircle(center, Vec3(0, 0, 1), radius, color, segments, duration, depthTest);
}

void DebugRendererDX11::drawCircle(const Vec3& center, const Vec3& normal, float radius,
                                    const Color& color, int segments, float duration, bool depthTest) {
    drawCircleWithCategory(center, normal, radius, color, segments, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX11::drawCircleWithCategory(const Vec3& center, const Vec3& normal, float radius,
                                                const Color& color, int segments, float duration,
                                                bool depthTest, LineCategory category) {
    Vec3 up = std::abs(normal.y) < 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 right = math::Cross(up, normal).Normalized();
    Vec3 forward = math::Cross(normal, right).Normalized();

    std::vector<Vec3> points;
    points.reserve(segments + 1);

    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * math::PI;
        float x = std::cos(angle) * radius;
        float y = std::sin(angle) * radius;

        Vec3 point = center + right * x + forward * y;
        points.push_back(point);
    }

    drawLineStripWithCategory(points, color, duration, depthTest, category);
}

void DebugRendererDX11::drawArrow(const Vec3& start, const Vec3& end, const Color& color,
                                   float arrowSize, float duration, bool depthTest) {
    drawArrowWithCategory(start, end, color, arrowSize, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX11::drawArrowWithCategory(const Vec3& start, const Vec3& end, const Color& color,
                                               float arrowSize, float duration, bool depthTest, LineCategory category) {
    drawLineWithCategory(start, end, color, duration, depthTest, category);

    Vec3 direction = (end - start).Normalized();
    Vec3 perpendicular1 = std::abs(direction.y) < 0.9f ?
                         math::Cross(direction, Vec3(0, 1, 0)).Normalized() :
                         math::Cross(direction, Vec3(1, 0, 0)).Normalized();
    Vec3 perpendicular2 = math::Cross(direction, perpendicular1).Normalized();

    Vec3 arrowTip = end - direction * arrowSize;

    drawLineWithCategory(end, arrowTip + perpendicular1 * arrowSize * 0.5f, color, duration, depthTest, category);
    drawLineWithCategory(end, arrowTip - perpendicular1 * arrowSize * 0.5f, color, duration, depthTest, category);
    drawLineWithCategory(end, arrowTip + perpendicular2 * arrowSize * 0.5f, color, duration, depthTest, category);
    drawLineWithCategory(end, arrowTip - perpendicular2 * arrowSize * 0.5f, color, duration, depthTest, category);
}

void DebugRendererDX11::drawText(const Vec3& position, const std::string& text, const Color& color,
                                  float size, float duration) {
    debug::EmitDebugText(*this, position, text, color, size, duration);
}

// Axes
void DebugRendererDX11::drawAxes(const Vec3& position, float size, float duration) {
    drawArrow(position, position + Vec3(size, 0, 0), Color::Red(), size * 0.1f, duration, true);
    drawArrow(position, position + Vec3(0, size, 0), Color::Green(), size * 0.1f, duration, true);
    drawArrow(position, position + Vec3(0, 0, size), Color::Blue(), size * 0.1f, duration, true);
}

void DebugRendererDX11::drawTransform(const Vec3& position, const Mat4& rotation, float size, float duration) {
    Vec3 right = Vec3(rotation[0][0], rotation[1][0], rotation[2][0]).Normalized();
    Vec3 up = Vec3(rotation[0][1], rotation[1][1], rotation[2][1]).Normalized();
    Vec3 forward = Vec3(rotation[0][2], rotation[1][2], rotation[2][2]).Normalized();

    drawArrow(position, position + right * size, Color::Red(), size * 0.1f, duration, true);
    drawArrow(position, position + up * size, Color::Green(), size * 0.1f, duration, true);
    drawArrow(position, position + forward * size, Color::Blue(), size * 0.1f, duration, true);
}

// Statistics
size_t DebugRendererDX11::getLineCount() const {
    size_t count = 0;
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            count += m_impl->lineVertices[cat][depth].size();
        }
    }
    return count / 2;
}

size_t DebugRendererDX11::getPrimitiveCount() const {
    size_t count = 0;
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            count += m_impl->lineIndices[cat][depth].size();
        }
    }
    return count / 2;
}

// Gizmo helpers
void DebugRendererDX11::drawGizmoArrow(const Vec3& origin, const Vec3& direction, float length, const Color& color, bool depthTest) {
    Vec3 end = origin + direction * length;
    drawArrowWithCategory(origin, end, color, length * 0.1f, 0.0f, depthTest, LineCategory::Gizmo);
}

void DebugRendererDX11::drawGizmoCircle(const Vec3& center, const Vec3& normal, float radius, const Color& color, bool depthTest) {
    drawCircleWithCategory(center, normal, radius, color, 64, 0.0f, depthTest, LineCategory::Gizmo);
}

void DebugRendererDX11::drawGizmoBox(const Vec3& center, float size, const Color& color, bool depthTest) {
    drawBoxWithCategory(center, Vec3(size), color, true, 0.0f, depthTest, LineCategory::Gizmo);
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX11
