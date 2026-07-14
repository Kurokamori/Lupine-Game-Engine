#include "lupine/rendering/debug/DebugRendererMetal.hpp"
#include "lupine/rendering/GizmoUtils.hpp"
#include "lupine/rendering/debug/DebugTextGeometry.hpp"
#include "lupine/rendering/MeshBuilder.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>

namespace lupine {

#ifdef LUPINE_HAS_METAL

// Metal Shading Language (MSL) shaders for debug line rendering
namespace MetalDebugShaders {

const char* const DebugLine_Vertex = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 position [[attribute(0)]];
    float4 color [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

struct Uniforms {
    float4x4 u_ViewProjection;
};

vertex VertexOut debug_line_vertex(
    VertexIn in [[stage_in]],
    constant Uniforms& uniforms [[buffer(16)]],
    uint vertexID [[vertex_id]]
) {
    VertexOut out;
    out.position = uniforms.u_ViewProjection * float4(in.position, 1.0);
    out.color = in.color;
    return out;
}
)";

const char* const DebugLine_Fragment = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

fragment float4 debug_line_fragment(VertexOut in [[stage_in]]) {
    return in.color;
}
)";

} // namespace MetalDebugShaders

struct LineVertex {
    Vec3 position;
    Color color;
};

// Line width multipliers relative to base line width
// If base is 4: Grid=1, BoundingBox=2, Gizmo=3, Other=2
constexpr float LINE_WIDTH_MULTIPLIER_GRID = 0.25f;
constexpr float LINE_WIDTH_MULTIPLIER_BBOX = 0.5f;
constexpr float LINE_WIDTH_MULTIPLIER_GIZMO = 0.75f;
constexpr float LINE_WIDTH_MULTIPLIER_OTHER = 0.5f;

struct DebugRendererMetal::Impl {
    GfxDeviceMetal* device = nullptr;
    bool initialized = false;

    ShaderHandle lineVertexShader;
    ShaderHandle lineFragmentShader;
    PipelineHandle linePipelineDepthTest;
    PipelineHandle linePipelineNoDepthTest;

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
        LineCategory category;
    };
    std::vector<PersistentLine> persistentLines;

    float deltaTime = 0.016f;

    static float getLineWidthMultiplier(LineCategory category) {
        switch (category) {
            case LineCategory::Grid: return LINE_WIDTH_MULTIPLIER_GRID;
            case LineCategory::BoundingBox: return LINE_WIDTH_MULTIPLIER_BBOX;
            case LineCategory::Gizmo: return LINE_WIDTH_MULTIPLIER_GIZMO;
            case LineCategory::Other: return LINE_WIDTH_MULTIPLIER_OTHER;
        }
        return LINE_WIDTH_MULTIPLIER_OTHER;
    }
};

DebugRendererMetal::DebugRendererMetal()
    : m_impl(std::make_unique<Impl>()) {
}

DebugRendererMetal::~DebugRendererMetal() {
    if (m_impl->initialized) {
        shutdown();
    }
}

bool DebugRendererMetal::initialize(IGfxDevice* device) {
    if (m_impl->initialized) {
        return true;
    }

    m_impl->device = dynamic_cast<GfxDeviceMetal*>(device);
    if (!m_impl->device) {
        return false;
    }

    // Create line vertex shader
    ShaderDesc vertDesc;
    vertDesc.stage = ShaderStage::Vertex;
    vertDesc.bytecode = MetalDebugShaders::DebugLine_Vertex;
    vertDesc.bytecodeSize = strlen(MetalDebugShaders::DebugLine_Vertex);
    vertDesc.entryPoint = "debug_line_vertex";
    m_impl->lineVertexShader = m_impl->device->createShader(vertDesc);

    if (!m_impl->lineVertexShader.isValid()) {
        return false;
    }

    // Create line fragment shader
    ShaderDesc fragDesc;
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.bytecode = MetalDebugShaders::DebugLine_Fragment;
    fragDesc.bytecodeSize = strlen(MetalDebugShaders::DebugLine_Fragment);
    fragDesc.entryPoint = "debug_line_fragment";
    m_impl->lineFragmentShader = m_impl->device->createShader(fragDesc);

    if (!m_impl->lineFragmentShader.isValid()) {
        m_impl->device->destroyShader(m_impl->lineVertexShader);
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
        {"position", VertexFormat::Float3, offsetof(LineVertex, position), 0, 0},
        {"color", VertexFormat::Float4, offsetof(LineVertex, color), 0, 1}
    };

    m_impl->linePipelineDepthTest = m_impl->device->createPipeline(linePipelineDescDepth);

    if (!m_impl->linePipelineDepthTest.isValid()) {
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
        {"position", VertexFormat::Float3, offsetof(LineVertex, position), 0, 0},
        {"color", VertexFormat::Float4, offsetof(LineVertex, color), 0, 1}
    };

    m_impl->linePipelineNoDepthTest = m_impl->device->createPipeline(linePipelineDescNoDepth);

    if (!m_impl->linePipelineNoDepthTest.isValid()) {
        shutdown();
        return false;
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

void DebugRendererMetal::shutdown() {
    if (!m_impl->initialized) {
        return;
    }

    if (m_impl->device) {
        m_impl->device->destroyShader(m_impl->lineVertexShader);
        m_impl->device->destroyShader(m_impl->lineFragmentShader);

        m_impl->device->destroyPipeline(m_impl->linePipelineDepthTest);
        m_impl->device->destroyPipeline(m_impl->linePipelineNoDepthTest);

        for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
            for (int depth = 0; depth < 2; ++depth) {
                m_impl->device->destroyBuffer(m_impl->lineVertexBuffers[cat][depth]);
                m_impl->device->destroyBuffer(m_impl->lineIndexBuffers[cat][depth]);
            }
        }
    }

    m_impl->initialized = false;
}

void DebugRendererMetal::beginFrame() {
    // Clear all category buffers
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            m_impl->lineVertices[cat][depth].clear();
            m_impl->lineIndices[cat][depth].clear();
        }
    }

    // Process persistent lines
    auto it = m_impl->persistentLines.begin();
    while (it != m_impl->persistentLines.end()) {
        it->remainingTime -= m_impl->deltaTime;
        if (it->remainingTime <= 0.0f) {
            it = m_impl->persistentLines.erase(it);
        } else {
            drawLineInternal(it->start, it->end, it->color, it->depthTest, it->category);
            ++it;
        }
    }
}

void DebugRendererMetal::endFrame() {
    // Update GPU buffers for all categories and depth modes
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            if (!m_impl->lineVertices[cat][depth].empty()) {
                size_t vertexDataSize = m_impl->lineVertices[cat][depth].size() * sizeof(LineVertex);
                size_t indexDataSize = m_impl->lineIndices[cat][depth].size() * sizeof(uint32_t);

                // Debug: print first few vertices
                auto& verts = m_impl->lineVertices[cat][depth];
                if (verts.size() >= 2) {
                    LOG_INFO(LogCategory::Render, "DebugMetal: cat={} depth={} v0=({},{},{}) color=({},{},{},{})",
                             cat, depth,
                             verts[0].position.x, verts[0].position.y, verts[0].position.z,
                             verts[0].color.r, verts[0].color.g, verts[0].color.b, verts[0].color.a);
                    LOG_INFO(LogCategory::Render, "DebugMetal: v1=({},{},{}) color=({},{},{},{})",
                             verts[1].position.x, verts[1].position.y, verts[1].position.z,
                             verts[1].color.r, verts[1].color.g, verts[1].color.b, verts[1].color.a);
                }

                m_impl->device->updateBuffer(m_impl->lineVertexBuffers[cat][depth],
                                             m_impl->lineVertices[cat][depth].data(),
                                             vertexDataSize, 0);

                m_impl->device->updateBuffer(m_impl->lineIndexBuffers[cat][depth],
                                             m_impl->lineIndices[cat][depth].data(),
                                             indexDataSize, 0);
            }
        }
    }
}

void DebugRendererMetal::render(IGfxCommandList* cmd, const Mat4& viewProjection, float lineWidth) {
    if (!m_impl->initialized || !cmd) {
        LOG_WARN(LogCategory::Render, "DebugRendererMetal::render - not initialized or no cmd");
        return;
    }

    // Count total lines
    size_t totalLines = 0;
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            totalLines += m_impl->lineVertices[cat][depth].size();
        }
    }
    LOG_INFO(LogCategory::Render, "DebugRendererMetal::render - totalLines={}", totalLines);

    // Render each category with its appropriate line width
    // Order: Grid, BoundingBox, Other, Gizmo (gizmos on top)
    const LineCategory renderOrder[] = {
        LineCategory::Grid,
        LineCategory::BoundingBox,
        LineCategory::Other,
        LineCategory::Gizmo
    };

    for (LineCategory category : renderOrder) {
        int cat = static_cast<int>(category);
        float categoryLineWidth = lineWidth * Impl::getLineWidthMultiplier(category);

        // Render with depth test first
        if (!m_impl->lineVertices[cat][1].empty()) {
            LOG_DEBUG(LogCategory::Render, "DebugRendererMetal: cat={} depth=1 vertices={} indices={}",
                      cat, m_impl->lineVertices[cat][1].size(), m_impl->lineIndices[cat][1].size());
            cmd->bindPipeline(m_impl->linePipelineDepthTest);
            cmd->setLineWidth(categoryLineWidth);
            cmd->bindVertexBuffer(m_impl->lineVertexBuffers[cat][1], 0, 0);
            cmd->bindIndexBuffer(m_impl->lineIndexBuffers[cat][1], IndexFormat::UInt32, 0);

            // Set view projection matrix
            cmd->setUniformMat4("u_ViewProjection", viewProjection);
            cmd->drawIndexed(static_cast<uint32_t>(m_impl->lineIndices[cat][1].size()), 1, 0, 0, 0);
        }

        // Render without depth test
        if (!m_impl->lineVertices[cat][0].empty()) {
            LOG_DEBUG(LogCategory::Render, "DebugRendererMetal: cat={} depth=0 vertices={} indices={}",
                      cat, m_impl->lineVertices[cat][0].size(), m_impl->lineIndices[cat][0].size());
            cmd->bindPipeline(m_impl->linePipelineNoDepthTest);
            cmd->setLineWidth(categoryLineWidth);
            cmd->bindVertexBuffer(m_impl->lineVertexBuffers[cat][0], 0, 0);
            cmd->bindIndexBuffer(m_impl->lineIndexBuffers[cat][0], IndexFormat::UInt32, 0);

            // Set view projection matrix
            cmd->setUniformMat4("u_ViewProjection", viewProjection);
            cmd->drawIndexed(static_cast<uint32_t>(m_impl->lineIndices[cat][0].size()), 1, 0, 0, 0);
        }
    }
}

void DebugRendererMetal::clear() {
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            m_impl->lineVertices[cat][depth].clear();
            m_impl->lineIndices[cat][depth].clear();
        }
    }
    m_impl->persistentLines.clear();
}

void DebugRendererMetal::draw2DGrid(const DebugGrid& grid) {
    DebugGrid grid2D = grid;
    grid2D.is3D = false;
    renderGrid(nullptr, grid2D, Mat4::Identity());
}

void DebugRendererMetal::draw3DGrid(const DebugGrid& grid) {
    DebugGrid grid3D = grid;
    grid3D.is3D = true;
    renderGrid(nullptr, grid3D, Mat4::Identity());
}

void DebugRendererMetal::draw2DBoundingBox(const AABB& bounds, const Color& color, float duration) {
    Vec3 min = bounds.min;
    Vec3 max = bounds.max;

    drawLineWithCategory(Vec3(min.x, min.y, 0), Vec3(max.x, min.y, 0), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, min.y, 0), Vec3(max.x, max.y, 0), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, max.y, 0), Vec3(min.x, max.y, 0), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(min.x, max.y, 0), Vec3(min.x, min.y, 0), color, duration, false, LineCategory::BoundingBox);
}

void DebugRendererMetal::draw2DOrientedBoundingBox(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration) {
    Vec2 halfSize = size * 0.5f;

    Vec2 localCorners[4] = {
        Vec2(-halfSize.x, -halfSize.y),
        Vec2( halfSize.x, -halfSize.y),
        Vec2( halfSize.x,  halfSize.y),
        Vec2(-halfSize.x,  halfSize.y)
    };

    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);
    Vec3 worldCorners[4];

    for (int i = 0; i < 4; ++i) {
        Vec2 rotated(
            localCorners[i].x * cosR - localCorners[i].y * sinR,
            localCorners[i].x * sinR + localCorners[i].y * cosR
        );
        worldCorners[i] = Vec3(center.x + rotated.x, center.y + rotated.y, 0.0f);
    }

    drawLineWithCategory(worldCorners[0], worldCorners[1], color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(worldCorners[1], worldCorners[2], color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(worldCorners[2], worldCorners[3], color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(worldCorners[3], worldCorners[0], color, duration, false, LineCategory::BoundingBox);
}

void DebugRendererMetal::draw3DBoundingBox(const AABB& bounds, const Color& color, float duration) {
    Vec3 min = bounds.min;
    Vec3 max = bounds.max;

    // Bottom face
    drawLineWithCategory(Vec3(min.x, min.y, min.z), Vec3(max.x, min.y, min.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, min.y, min.z), Vec3(max.x, min.y, max.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, min.y, max.z), Vec3(min.x, min.y, max.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(min.x, min.y, max.z), Vec3(min.x, min.y, min.z), color, duration, true, LineCategory::BoundingBox);

    // Top face
    drawLineWithCategory(Vec3(min.x, max.y, min.z), Vec3(max.x, max.y, min.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, max.y, min.z), Vec3(max.x, max.y, max.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, max.y, max.z), Vec3(min.x, max.y, max.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(min.x, max.y, max.z), Vec3(min.x, max.y, min.z), color, duration, true, LineCategory::BoundingBox);

    // Vertical edges
    drawLineWithCategory(Vec3(min.x, min.y, min.z), Vec3(min.x, max.y, min.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, min.y, min.z), Vec3(max.x, max.y, min.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, min.y, max.z), Vec3(max.x, max.y, max.z), color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(min.x, min.y, max.z), Vec3(min.x, max.y, max.z), color, duration, true, LineCategory::BoundingBox);
}

void DebugRendererMetal::draw3DOrientedBoundingBox(const OBB& obb, const Color& color, float duration) {
    Vec3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i] = obb.GetCorner(i);
    }

    // Bottom face
    drawLineWithCategory(corners[0], corners[1], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[1], corners[3], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[3], corners[2], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[2], corners[0], color, duration, true, LineCategory::BoundingBox);

    // Top face
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

void DebugRendererMetal::draw2DGizmo(const DebugGizmo& gizmo) {
    std::vector<GizmoSegment> segments = GizmoUtils::BuildGizmo2DGeometry(gizmo);
    for (const GizmoSegment& seg : segments) {
        drawLineWithCategory(seg.start, seg.end, seg.color, 0.0f, false, LineCategory::Gizmo);
    }
}

void DebugRendererMetal::draw3DGizmo(const DebugGizmo& gizmo) {
    float size = gizmo.size;

    Vec3 xAxis = gizmo.rotation3D * Vec3(1, 0, 0);
    Vec3 yAxis = gizmo.rotation3D * Vec3(0, 1, 0);
    Vec3 zAxis = gizmo.rotation3D * Vec3(0, 0, 1);

    switch (gizmo.type) {
        case GizmoType::Translation: {
            Color xColor = (gizmo.highlightAxis == GizmoAxis::X) ? Color::Yellow() : Color::Red();
            Color yColor = (gizmo.highlightAxis == GizmoAxis::Y) ? Color::Yellow() : Color::Green();
            Color zColor = (gizmo.highlightAxis == GizmoAxis::Z) ? Color::Yellow() : Color::Blue();

            drawArrowWithCategory(gizmo.position, gizmo.position + xAxis * size, xColor, size * 0.1f, 0.0f, false, LineCategory::Gizmo);
            drawArrowWithCategory(gizmo.position, gizmo.position + yAxis * size, yColor, size * 0.1f, 0.0f, false, LineCategory::Gizmo);
            drawArrowWithCategory(gizmo.position, gizmo.position + zAxis * size, zColor, size * 0.1f, 0.0f, false, LineCategory::Gizmo);
            break;
        }

        case GizmoType::Rotation: {
            Color xColor = (gizmo.highlightAxis == GizmoAxis::X) ? Color::Yellow() : Color::Red();
            Color yColor = (gizmo.highlightAxis == GizmoAxis::Y) ? Color::Yellow() : Color::Green();
            Color zColor = (gizmo.highlightAxis == GizmoAxis::Z) ? Color::Yellow() : Color::Blue();

            drawCircleWithCategory(gizmo.position, xAxis, size, xColor, 64, 0.0f, false, LineCategory::Gizmo);
            drawCircleWithCategory(gizmo.position, yAxis, size, yColor, 64, 0.0f, false, LineCategory::Gizmo);
            drawCircleWithCategory(gizmo.position, zAxis, size, zColor, 64, 0.0f, false, LineCategory::Gizmo);
            break;
        }

        case GizmoType::Scale: {
            Color xColor = (gizmo.highlightAxis == GizmoAxis::X) ? Color::Yellow() : Color::Red();
            Color yColor = (gizmo.highlightAxis == GizmoAxis::Y) ? Color::Yellow() : Color::Green();
            Color zColor = (gizmo.highlightAxis == GizmoAxis::Z) ? Color::Yellow() : Color::Blue();

            drawLineWithCategory(gizmo.position, gizmo.position + xAxis * size, xColor, 0.0f, false, LineCategory::Gizmo);
            drawLineWithCategory(gizmo.position, gizmo.position + yAxis * size, yColor, 0.0f, false, LineCategory::Gizmo);
            drawLineWithCategory(gizmo.position, gizmo.position + zAxis * size, zColor, 0.0f, false, LineCategory::Gizmo);

            drawBoxWithCategory(gizmo.position + xAxis * size, Vec3(size * 0.1f), xColor, true, 0.0f, false, LineCategory::Gizmo);
            drawBoxWithCategory(gizmo.position + yAxis * size, Vec3(size * 0.1f), yColor, true, 0.0f, false, LineCategory::Gizmo);
            drawBoxWithCategory(gizmo.position + zAxis * size, Vec3(size * 0.1f), zColor, true, 0.0f, false, LineCategory::Gizmo);
            break;
        }

        case GizmoType::All: {
            draw3DGizmo({gizmo.position, GizmoType::Translation, size * 0.8f, gizmo.highlightAxis, 0.0f, gizmo.rotation3D});
            draw3DGizmo({gizmo.position, GizmoType::Rotation, size * 1.2f, gizmo.highlightAxis, 0.0f, gizmo.rotation3D});
            break;
        }
    }
}

void DebugRendererMetal::drawLineInternal(const Vec3& start, const Vec3& end, const Color& color,
                                           bool depthTest, LineCategory category) {
    int cat = static_cast<int>(category);
    int depthIdx = depthTest ? 1 : 0;

    auto& vertices = m_impl->lineVertices[cat][depthIdx];
    auto& indices = m_impl->lineIndices[cat][depthIdx];

    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    vertices.push_back({start, color});
    vertices.push_back({end, color});

    indices.push_back(baseIndex);
    indices.push_back(baseIndex + 1);
}

void DebugRendererMetal::drawLine(const Vec3& start, const Vec3& end, const Color& color,
                                   float duration, bool depthTest) {
    drawLineWithCategory(start, end, color, duration, depthTest, LineCategory::Other);
}

void DebugRendererMetal::drawLineWithCategory(const Vec3& start, const Vec3& end, const Color& color,
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

void DebugRendererMetal::drawLineStrip(const std::vector<Vec3>& points, const Color& color,
                                        float duration, bool depthTest) {
    drawLineStripWithCategory(points, color, duration, depthTest, LineCategory::Other);
}

void DebugRendererMetal::drawLineStripWithCategory(const std::vector<Vec3>& points, const Color& color,
                                                    float duration, bool depthTest, LineCategory category) {
    if (points.size() < 2) {
        return;
    }

    for (size_t i = 0; i < points.size() - 1; ++i) {
        drawLineWithCategory(points[i], points[i + 1], color, duration, depthTest, category);
    }
}

void DebugRendererMetal::drawRay(const Vec3& origin, const Vec3& direction, const Color& color,
                                  float length, float duration, bool depthTest) {
    Vec3 end = origin + direction.Normalized() * length;
    drawLine(origin, end, color, duration, depthTest);
}

void DebugRendererMetal::drawBox(const Vec3& center, const Vec3& size, const Color& color,
                                  bool wireframe, float duration, bool depthTest) {
    drawBoxWithCategory(center, size, color, wireframe, duration, depthTest, LineCategory::Other);
}

void DebugRendererMetal::drawBoxWithCategory(const Vec3& center, const Vec3& size, const Color& color,
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

void DebugRendererMetal::drawSphere(const Vec3& center, float radius, const Color& color,
                                     bool wireframe, int segments, float duration, bool depthTest) {
    if (!wireframe) {
        return;
    }

    // Draw 3 circles for each axis
    drawCircle(center, Vec3(1, 0, 0), radius, color, segments, duration, depthTest);
    drawCircle(center, Vec3(0, 1, 0), radius, color, segments, duration, depthTest);
    drawCircle(center, Vec3(0, 0, 1), radius, color, segments, duration, depthTest);
}

void DebugRendererMetal::drawCircle(const Vec3& center, const Vec3& normal, float radius,
                                     const Color& color, int segments, float duration, bool depthTest) {
    drawCircleWithCategory(center, normal, radius, color, segments, duration, depthTest, LineCategory::Other);
}

void DebugRendererMetal::drawCircleWithCategory(const Vec3& center, const Vec3& normal, float radius,
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

void DebugRendererMetal::drawArrow(const Vec3& start, const Vec3& end, const Color& color,
                                    float arrowSize, float duration, bool depthTest) {
    drawArrowWithCategory(start, end, color, arrowSize, duration, depthTest, LineCategory::Other);
}

void DebugRendererMetal::drawArrowWithCategory(const Vec3& start, const Vec3& end, const Color& color,
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

void DebugRendererMetal::drawText(const Vec3& position, const std::string& text,
                                   const Color& color, float size, float duration) {
    debug::EmitDebugText(*this, position, text, color, size, duration);
}

void DebugRendererMetal::drawAxes(const Vec3& position, float size, float duration) {
    drawArrow(position, position + Vec3(size, 0, 0), Color::Red(), size * 0.1f, duration, true);
    drawArrow(position, position + Vec3(0, size, 0), Color::Green(), size * 0.1f, duration, true);
    drawArrow(position, position + Vec3(0, 0, size), Color::Blue(), size * 0.1f, duration, true);
}

void DebugRendererMetal::drawTransform(const Vec3& position, const Mat4& rotation, float size, float duration) {
    Vec3 right = Vec3(rotation[0][0], rotation[1][0], rotation[2][0]).Normalized();
    Vec3 up = Vec3(rotation[0][1], rotation[1][1], rotation[2][1]).Normalized();
    Vec3 forward = Vec3(rotation[0][2], rotation[1][2], rotation[2][2]).Normalized();

    drawArrow(position, position + right * size, Color::Red(), size * 0.1f, duration, true);
    drawArrow(position, position + up * size, Color::Green(), size * 0.1f, duration, true);
    drawArrow(position, position + forward * size, Color::Blue(), size * 0.1f, duration, true);
}

size_t DebugRendererMetal::getLineCount() const {
    size_t count = 0;
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            count += m_impl->lineVertices[cat][depth].size();
        }
    }
    return count / 2;
}

size_t DebugRendererMetal::getPrimitiveCount() const {
    size_t count = 0;
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            count += m_impl->lineIndices[cat][depth].size();
        }
    }
    return count / 2;
}

void DebugRendererMetal::renderGrid(IGfxCommandList* cmd, const DebugGrid& grid, const Mat4& viewProjection) {
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

void DebugRendererMetal::drawGizmoArrow(const Vec3& origin, const Vec3& direction, float length, const Color& color, bool depthTest) {
    Vec3 end = origin + direction * length;
    drawArrowWithCategory(origin, end, color, length * 0.1f, 0.0f, depthTest, LineCategory::Gizmo);
}

void DebugRendererMetal::drawGizmoCircle(const Vec3& center, const Vec3& normal, float radius, const Color& color, bool depthTest) {
    drawCircleWithCategory(center, normal, radius, color, 64, 0.0f, depthTest, LineCategory::Gizmo);
}

void DebugRendererMetal::drawGizmoBox(const Vec3& center, float size, const Color& color, bool depthTest) {
    drawBoxWithCategory(center, Vec3(size), color, true, 0.0f, depthTest, LineCategory::Gizmo);
}

#endif // LUPINE_HAS_METAL

} // namespace lupine
