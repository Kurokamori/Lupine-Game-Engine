#include "lupine/rendering/debug/DebugRendererOpenGL.hpp"
#include "lupine/rendering/GizmoUtils.hpp"
#include "lupine/rendering/debug/DebugTextGeometry.hpp"
#include "lupine/rendering/debug/DebugShaders.hpp"
#include "lupine/rendering/MeshBuilder.hpp"
#include "lupine/logger/Logger.hpp"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>

#ifdef LUPINE_HAS_VULKAN
#include "lupine/rendering/debug/DebugRendererVulkan.hpp"
#endif

#ifdef LUPINE_HAS_DIRECTX11
#include "lupine/rendering/debug/DebugRendererDX11.hpp"
#endif

#ifdef LUPINE_HAS_METAL
#include "lupine/rendering/debug/DebugRendererMetal.hpp"
#endif

#ifdef LUPINE_HAS_DIRECTX12
#include "lupine/rendering/debug/DebugRendererDX12.hpp"
#endif

namespace lupine {

struct LineVertex {
    Vec3 position;
    Color color;
};

struct DebugRendererOpenGL::Impl {
    GfxDeviceOpenGL* device = nullptr;
    bool initialized = false;

    ShaderHandle lineVertexShader;
    ShaderHandle lineFragmentShader;
    PipelineHandle linePipelineDepthTest;
    PipelineHandle linePipelineNoDepthTest;

    ShaderHandle gridVertexShader;
    ShaderHandle gridFragmentShader;
    PipelineHandle gridPipeline;

    MeshHandle grid2DMesh;
    MeshHandle grid3DMesh;

    MeshHandle arrowMesh;
    MeshHandle circleMesh;
    MeshHandle boxMesh;

    std::vector<LineVertex> lineVerticesDepthTest;
    std::vector<LineVertex> lineVerticesNoDepthTest;
    std::vector<uint32_t> lineIndicesDepthTest;
    std::vector<uint32_t> lineIndicesNoDepthTest;

    BufferHandle lineVertexBufferDepthTest;
    BufferHandle lineIndexBufferDepthTest;
    BufferHandle lineVertexBufferNoDepthTest;
    BufferHandle lineIndexBufferNoDepthTest;

    struct PersistentLine {
        Vec3 start;
        Vec3 end;
        Color color;
        float remainingTime;
        bool depthTest;
    };
    std::vector<PersistentLine> persistentLines;

    float deltaTime = 0.016f;
};

DebugRendererOpenGL::DebugRendererOpenGL()
    : m_impl(std::make_unique<Impl>()) {
}

DebugRendererOpenGL::~DebugRendererOpenGL() {
    if (m_impl->initialized) {
        shutdown();
    }
}

bool DebugRendererOpenGL::initialize(IGfxDevice* device) {
    if (m_impl->initialized) {
        return true;
    }

    m_impl->device = dynamic_cast<GfxDeviceOpenGL*>(device);
    if (!m_impl->device) {

        return false;
    }

    // Debug: Log shader source info
    
    if (DebugShaders::DebugLine_Vertex) {
        std::string preview(DebugShaders::DebugLine_Vertex, std::min(size_t(50), strlen(DebugShaders::DebugLine_Vertex)));
        
    }

    ShaderDesc vertDesc;
    vertDesc.stage = ShaderStage::Vertex;
    vertDesc.bytecode = DebugShaders::DebugLine_Vertex;
    vertDesc.bytecodeSize = strlen(DebugShaders::DebugLine_Vertex);
    m_impl->lineVertexShader = m_impl->device->createShader(vertDesc);

    ShaderDesc fragDesc;
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.bytecode = DebugShaders::DebugLine_Fragment;
    fragDesc.bytecodeSize = strlen(DebugShaders::DebugLine_Fragment);
    m_impl->lineFragmentShader = m_impl->device->createShader(fragDesc);

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
        {"a_Position", VertexFormat::Float3, offsetof(LineVertex, position), 0},
        {"a_Color", VertexFormat::Float4, offsetof(LineVertex, color), 0}
    };

    m_impl->linePipelineDepthTest = m_impl->device->createPipeline(linePipelineDescDepth);

    PipelineDesc linePipelineDescNoDepth;
    linePipelineDescNoDepth.shaders = {m_impl->lineVertexShader, m_impl->lineFragmentShader};
    linePipelineDescNoDepth.topology = PrimitiveTopology::LineList;
    linePipelineDescNoDepth.blendState = BlendState::alphaBlend();
    linePipelineDescNoDepth.depthStencilState = DepthStencilState::noDepth();
    linePipelineDescNoDepth.rasterizerState = RasterizerState::noCull();

    linePipelineDescNoDepth.vertexLayout.stride = sizeof(LineVertex);
    linePipelineDescNoDepth.vertexLayout.attributes = {
        {"a_Position", VertexFormat::Float3, offsetof(LineVertex, position), 0},
        {"a_Color", VertexFormat::Float4, offsetof(LineVertex, color), 0}
    };

    m_impl->linePipelineNoDepthTest = m_impl->device->createPipeline(linePipelineDescNoDepth);

    ShaderDesc gridVertDesc;
    gridVertDesc.stage = ShaderStage::Vertex;
    gridVertDesc.bytecode = DebugShaders::Grid_Vertex;
    gridVertDesc.bytecodeSize = strlen(DebugShaders::Grid_Vertex);
    m_impl->gridVertexShader = m_impl->device->createShader(gridVertDesc);

    ShaderDesc gridFragDesc;
    gridFragDesc.stage = ShaderStage::Fragment;
    gridFragDesc.bytecode = DebugShaders::Grid_Fragment;
    gridFragDesc.bytecodeSize = strlen(DebugShaders::Grid_Fragment);
    m_impl->gridFragmentShader = m_impl->device->createShader(gridFragDesc);

    PipelineDesc gridPipelineDesc;
    gridPipelineDesc.shaders = {m_impl->gridVertexShader, m_impl->gridFragmentShader};
    gridPipelineDesc.topology = PrimitiveTopology::TriangleList;
    gridPipelineDesc.blendState = BlendState::alphaBlend();
    gridPipelineDesc.depthStencilState = DepthStencilState::depthReadOnly();
    gridPipelineDesc.rasterizerState = RasterizerState::noCull();

    gridPipelineDesc.vertexLayout.stride = sizeof(Vertex);
    gridPipelineDesc.vertexLayout.attributes = {
        {"a_Position", VertexFormat::Float3, 0, 0}
    };

    m_impl->gridPipeline = m_impl->device->createPipeline(gridPipelineDesc);

    createGridMesh(false);
    createGridMesh(true);

    createGizmaMeshes();

    createPrimitiveMeshes();

    BufferDesc lineVBDesc;
    lineVBDesc.size = 10000 * sizeof(LineVertex);
    lineVBDesc.usage = BufferUsage::Vertex;
    m_impl->lineVertexBufferDepthTest = m_impl->device->createBuffer(lineVBDesc);
    m_impl->lineVertexBufferNoDepthTest = m_impl->device->createBuffer(lineVBDesc);

    BufferDesc lineIBDesc;
    lineIBDesc.size = 10000 * sizeof(uint32_t);
    lineIBDesc.usage = BufferUsage::Index;
    m_impl->lineIndexBufferDepthTest = m_impl->device->createBuffer(lineIBDesc);
    m_impl->lineIndexBufferNoDepthTest = m_impl->device->createBuffer(lineIBDesc);

    m_impl->initialized = true;

    return true;
}

void DebugRendererOpenGL::shutdown() {
    if (!m_impl->initialized) {
        return;
    }

    m_impl->device->destroyShader(m_impl->lineVertexShader);
    m_impl->device->destroyShader(m_impl->lineFragmentShader);
    m_impl->device->destroyShader(m_impl->gridVertexShader);
    m_impl->device->destroyShader(m_impl->gridFragmentShader);

    m_impl->device->destroyPipeline(m_impl->linePipelineDepthTest);
    m_impl->device->destroyPipeline(m_impl->linePipelineNoDepthTest);
    m_impl->device->destroyPipeline(m_impl->gridPipeline);

    m_impl->device->destroyMesh(m_impl->grid2DMesh);
    m_impl->device->destroyMesh(m_impl->grid3DMesh);
    m_impl->device->destroyMesh(m_impl->arrowMesh);
    m_impl->device->destroyMesh(m_impl->circleMesh);
    m_impl->device->destroyMesh(m_impl->boxMesh);

    m_impl->device->destroyBuffer(m_impl->lineVertexBufferDepthTest);
    m_impl->device->destroyBuffer(m_impl->lineIndexBufferDepthTest);
    m_impl->device->destroyBuffer(m_impl->lineVertexBufferNoDepthTest);
    m_impl->device->destroyBuffer(m_impl->lineIndexBufferNoDepthTest);

    m_impl->initialized = false;

}

void DebugRendererOpenGL::beginFrame() {

    m_impl->lineVerticesDepthTest.clear();
    m_impl->lineVerticesNoDepthTest.clear();
    m_impl->lineIndicesDepthTest.clear();
    m_impl->lineIndicesNoDepthTest.clear();

    auto it = m_impl->persistentLines.begin();
    while (it != m_impl->persistentLines.end()) {
        it->remainingTime -= m_impl->deltaTime;
        if (it->remainingTime <= 0.0f) {
            it = m_impl->persistentLines.erase(it);
        } else {

            drawLine(it->start, it->end, it->color, 0.0f, it->depthTest);
            ++it;
        }
    }
}

void DebugRendererOpenGL::endFrame() {

    updateLineBuffer();
}

void DebugRendererOpenGL::render(IGfxCommandList* cmd, const Mat4& viewProjection, float lineWidth) {
    if (!m_impl->initialized || !cmd) {
        return;
    }

    if (!m_impl->lineVerticesDepthTest.empty()) {
        renderLines(cmd, viewProjection, true, lineWidth);
    }

    if (!m_impl->lineVerticesNoDepthTest.empty()) {
        renderLines(cmd, viewProjection, false, lineWidth);
    }
}

void DebugRendererOpenGL::clear() {
    m_impl->lineVerticesDepthTest.clear();
    m_impl->lineVerticesNoDepthTest.clear();
    m_impl->lineIndicesDepthTest.clear();
    m_impl->lineIndicesNoDepthTest.clear();
    m_impl->persistentLines.clear();
}

void DebugRendererOpenGL::draw2DGrid(const DebugGrid& grid) {
    DebugGrid grid2D = grid;
    grid2D.is3D = false;
    renderGrid(grid2D, Mat4::Identity());
}

void DebugRendererOpenGL::draw3DGrid(const DebugGrid& grid) {
    DebugGrid grid3D = grid;
    grid3D.is3D = true;
    renderGrid(grid3D, Mat4::Identity());
}

void DebugRendererOpenGL::draw2DBoundingBox(const AABB& bounds, const Color& color, float duration) {

    Vec3 min = bounds.min;
    Vec3 max = bounds.max;

    drawLine(Vec3(min.x, min.y, 0), Vec3(max.x, min.y, 0), color, duration, false);

    drawLine(Vec3(max.x, min.y, 0), Vec3(max.x, max.y, 0), color, duration, false);

    drawLine(Vec3(max.x, max.y, 0), Vec3(min.x, max.y, 0), color, duration, false);

    drawLine(Vec3(min.x, max.y, 0), Vec3(min.x, min.y, 0), color, duration, false);
}

void DebugRendererOpenGL::draw2DOrientedBoundingBox(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration) {

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

    drawLine(worldCorners[0], worldCorners[1], color, duration, false);
    drawLine(worldCorners[1], worldCorners[2], color, duration, false);
    drawLine(worldCorners[2], worldCorners[3], color, duration, false);
    drawLine(worldCorners[3], worldCorners[0], color, duration, false);
}

void DebugRendererOpenGL::draw3DBoundingBox(const AABB& bounds, const Color& color, float duration) {

    Vec3 min = bounds.min;
    Vec3 max = bounds.max;

    drawLine(Vec3(min.x, min.y, min.z), Vec3(max.x, min.y, min.z), color, duration, true);
    drawLine(Vec3(max.x, min.y, min.z), Vec3(max.x, min.y, max.z), color, duration, true);
    drawLine(Vec3(max.x, min.y, max.z), Vec3(min.x, min.y, max.z), color, duration, true);
    drawLine(Vec3(min.x, min.y, max.z), Vec3(min.x, min.y, min.z), color, duration, true);

    drawLine(Vec3(min.x, max.y, min.z), Vec3(max.x, max.y, min.z), color, duration, true);
    drawLine(Vec3(max.x, max.y, min.z), Vec3(max.x, max.y, max.z), color, duration, true);
    drawLine(Vec3(max.x, max.y, max.z), Vec3(min.x, max.y, max.z), color, duration, true);
    drawLine(Vec3(min.x, max.y, max.z), Vec3(min.x, max.y, min.z), color, duration, true);

    drawLine(Vec3(min.x, min.y, min.z), Vec3(min.x, max.y, min.z), color, duration, true);
    drawLine(Vec3(max.x, min.y, min.z), Vec3(max.x, max.y, min.z), color, duration, true);
    drawLine(Vec3(max.x, min.y, max.z), Vec3(max.x, max.y, max.z), color, duration, true);
    drawLine(Vec3(min.x, min.y, max.z), Vec3(min.x, max.y, max.z), color, duration, true);
}

void DebugRendererOpenGL::draw3DOrientedBoundingBox(const OBB& obb, const Color& col, float dur) {

    Vec3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i] = obb.GetCorner(i);
    }

    drawLine(corners[0], corners[1], col, dur, true);
    drawLine(corners[1], corners[3], col, dur, true);
    drawLine(corners[3], corners[2], col, dur, true);
    drawLine(corners[2], corners[0], col, dur, true);

    drawLine(corners[4], corners[5], col, dur, true);
    drawLine(corners[5], corners[7], col, dur, true);
    drawLine(corners[7], corners[6], col, dur, true);
    drawLine(corners[6], corners[4], col, dur, true);

    drawLine(corners[0], corners[4], col, dur, true);
    drawLine(corners[1], corners[5], col, dur, true);
    drawLine(corners[2], corners[6], col, dur, true);
    drawLine(corners[3], corners[7], col, dur, true);
}

void DebugRendererOpenGL::draw2DGizmo(const DebugGizmo& gizmo) {
    std::vector<GizmoSegment> segments = GizmoUtils::BuildGizmo2DGeometry(gizmo);
    for (const GizmoSegment& seg : segments) {
        drawLine(seg.start, seg.end, seg.color, 0.0f, false);
    }
}

void DebugRendererOpenGL::draw3DGizmo(const DebugGizmo& gizmo) {
    renderGizmo(gizmo, Mat4::Identity(), true);
}

void DebugRendererOpenGL::drawLine(const Vec3& start, const Vec3& end, const Color& color,
                                    float duration, bool depthTest) {
    if (duration > 0.0f) {

        Impl::PersistentLine persistentLine;
        persistentLine.start = start;
        persistentLine.end = end;
        persistentLine.color = color;
        persistentLine.remainingTime = duration;
        persistentLine.depthTest = depthTest;
        m_impl->persistentLines.push_back(persistentLine);
    }

    auto& vertices = depthTest ? m_impl->lineVerticesDepthTest : m_impl->lineVerticesNoDepthTest;
    auto& indices = depthTest ? m_impl->lineIndicesDepthTest : m_impl->lineIndicesNoDepthTest;

    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    vertices.push_back({start, color});
    vertices.push_back({end, color});

    indices.push_back(baseIndex);
    indices.push_back(baseIndex + 1);
}

void DebugRendererOpenGL::drawLineStrip(const std::vector<Vec3>& points, const Color& color,
                                        float duration, bool depthTest) {
    if (points.size() < 2) {
        return;
    }

    for (size_t i = 0; i < points.size() - 1; ++i) {
        drawLine(points[i], points[i + 1], color, duration, depthTest);
    }
}

void DebugRendererOpenGL::drawRay(const Vec3& origin, const Vec3& direction, const Color& color,
                                  float length, float duration, bool depthTest) {
    Vec3 end = origin + direction.Normalized() * length;
    drawLine(origin, end, color, duration, depthTest);
}

void DebugRendererOpenGL::drawBox(const Vec3& center, const Vec3& size, const Color& color,
                                  bool wireframe, float duration, bool depthTest) {
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

        drawLine(corners[0], corners[1], color, duration, depthTest);
        drawLine(corners[1], corners[2], color, duration, depthTest);
        drawLine(corners[2], corners[3], color, duration, depthTest);
        drawLine(corners[3], corners[0], color, duration, depthTest);

        drawLine(corners[4], corners[5], color, duration, depthTest);
        drawLine(corners[5], corners[6], color, duration, depthTest);
        drawLine(corners[6], corners[7], color, duration, depthTest);
        drawLine(corners[7], corners[4], color, duration, depthTest);

        drawLine(corners[0], corners[4], color, duration, depthTest);
        drawLine(corners[1], corners[5], color, duration, depthTest);
        drawLine(corners[2], corners[6], color, duration, depthTest);
        drawLine(corners[3], corners[7], color, duration, depthTest);
    }
}

void DebugRendererOpenGL::drawSphere(const Vec3& center, float radius, const Color& color,
                                     bool wireframe, int segments, float duration, bool depthTest) {
    if (!wireframe) {
        return;
    }

    drawCircle(center, Vec3(1, 0, 0), radius, color, segments, duration, depthTest);
    drawCircle(center, Vec3(0, 1, 0), radius, color, segments, duration, depthTest);
    drawCircle(center, Vec3(0, 0, 1), radius, color, segments, duration, depthTest);
}

void DebugRendererOpenGL::drawCircle(const Vec3& center, const Vec3& normal, float radius,
                                     const Color& color, int segments, float duration, bool depthTest) {

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

    drawLineStrip(points, color, duration, depthTest);
}

void DebugRendererOpenGL::drawArrow(const Vec3& start, const Vec3& end, const Color& color,
                                    float arrowSize, float duration, bool depthTest) {

    drawLine(start, end, color, duration, depthTest);

    Vec3 direction = (end - start).Normalized();
    Vec3 perpendicular1 = std::abs(direction.y) < 0.9f ?
                         math::Cross(direction, Vec3(0, 1, 0)).Normalized() :
                         math::Cross(direction, Vec3(1, 0, 0)).Normalized();
    Vec3 perpendicular2 = math::Cross(direction, perpendicular1).Normalized();

    Vec3 arrowTip = end - direction * arrowSize;

    drawLine(end, arrowTip + perpendicular1 * arrowSize * 0.5f, color, duration, depthTest);
    drawLine(end, arrowTip - perpendicular1 * arrowSize * 0.5f, color, duration, depthTest);
    drawLine(end, arrowTip + perpendicular2 * arrowSize * 0.5f, color, duration, depthTest);
    drawLine(end, arrowTip - perpendicular2 * arrowSize * 0.5f, color, duration, depthTest);
}

void DebugRendererOpenGL::drawText(const Vec3& position, const std::string& text,
                                   const Color& color, float size, float duration) {
    debug::EmitDebugText(*this, position, text, color, size, duration);
}

void DebugRendererOpenGL::drawAxes(const Vec3& position, float size, float duration) {

    drawArrow(position, position + Vec3(size, 0, 0), Color::Red(), size * 0.1f, duration, true);
    drawArrow(position, position + Vec3(0, size, 0), Color::Green(), size * 0.1f, duration, true);
    drawArrow(position, position + Vec3(0, 0, size), Color::Blue(), size * 0.1f, duration, true);
}

void DebugRendererOpenGL::drawTransform(const Vec3& position, const Mat4& rotation, float size, float duration) {

    Vec3 right = Vec3(rotation[0][0], rotation[1][0], rotation[2][0]).Normalized();
    Vec3 up = Vec3(rotation[0][1], rotation[1][1], rotation[2][1]).Normalized();
    Vec3 forward = Vec3(rotation[0][2], rotation[1][2], rotation[2][2]).Normalized();

    drawArrow(position, position + right * size, Color::Red(), size * 0.1f, duration, true);
    drawArrow(position, position + up * size, Color::Green(), size * 0.1f, duration, true);
    drawArrow(position, position + forward * size, Color::Blue(), size * 0.1f, duration, true);
}

size_t DebugRendererOpenGL::getLineCount() const {
    return (m_impl->lineVerticesDepthTest.size() + m_impl->lineVerticesNoDepthTest.size()) / 2;
}

size_t DebugRendererOpenGL::getPrimitiveCount() const {
    return m_impl->lineIndicesDepthTest.size() / 2 + m_impl->lineIndicesNoDepthTest.size() / 2;
}

void DebugRendererOpenGL::createGridMesh(bool is3D) {

    MeshData gridMesh;

    float gridSize = 100.0f;

    if (is3D) {

        gridMesh.vertices = {
            {{-gridSize, 0, -gridSize}, {0, 1, 0}, {0, 0}},
            {{ gridSize, 0, -gridSize}, {0, 1, 0}, {1, 0}},
            {{ gridSize, 0,  gridSize}, {0, 1, 0}, {1, 1}},
            {{-gridSize, 0,  gridSize}, {0, 1, 0}, {0, 1}}
        };
    } else {

        gridMesh.vertices = {
            {{-gridSize, -gridSize, 0}, {0, 0, 1}, {0, 0}},
            {{ gridSize, -gridSize, 0}, {0, 0, 1}, {1, 0}},
            {{ gridSize,  gridSize, 0}, {0, 0, 1}, {1, 1}},
            {{-gridSize,  gridSize, 0}, {0, 0, 1}, {0, 1}}
        };
    }

    gridMesh.indices = {0, 1, 2, 0, 2, 3};
    gridMesh.calculateBounds();
    gridMesh.addSubmesh(6, 4);

    if (is3D) {
        m_impl->grid3DMesh = m_impl->device->createMesh(gridMesh);
    } else {
        m_impl->grid2DMesh = m_impl->device->createMesh(gridMesh);
    }
}

void DebugRendererOpenGL::createGizmaMeshes() {

    m_impl->arrowMesh = m_impl->device->createMesh(MeshBuilder::createCylinder(0.05f, 1.0f, 8));

    m_impl->circleMesh = m_impl->device->createMesh(MeshBuilder::createCircle(1.0f, 64));

    m_impl->boxMesh = m_impl->device->createMesh(MeshBuilder::createCube(0.1f));
}

void DebugRendererOpenGL::createPrimitiveMeshes() {

}

void DebugRendererOpenGL::updateLineBuffer() {

    if (!m_impl->lineVerticesDepthTest.empty()) {
        size_t vertexDataSize = m_impl->lineVerticesDepthTest.size() * sizeof(LineVertex);
        size_t indexDataSize = m_impl->lineIndicesDepthTest.size() * sizeof(uint32_t);

        m_impl->device->updateBuffer(m_impl->lineVertexBufferDepthTest,
                                     m_impl->lineVerticesDepthTest.data(),
                                     vertexDataSize, 0);

        m_impl->device->updateBuffer(m_impl->lineIndexBufferDepthTest,
                                     m_impl->lineIndicesDepthTest.data(),
                                     indexDataSize, 0);
    }

    if (!m_impl->lineVerticesNoDepthTest.empty()) {
        size_t vertexDataSize = m_impl->lineVerticesNoDepthTest.size() * sizeof(LineVertex);
        size_t indexDataSize = m_impl->lineIndicesNoDepthTest.size() * sizeof(uint32_t);

        m_impl->device->updateBuffer(m_impl->lineVertexBufferNoDepthTest,
                                     m_impl->lineVerticesNoDepthTest.data(),
                                     vertexDataSize, 0);

        m_impl->device->updateBuffer(m_impl->lineIndexBufferNoDepthTest,
                                     m_impl->lineIndicesNoDepthTest.data(),
                                     indexDataSize, 0);
    }
}

void DebugRendererOpenGL::renderLines(IGfxCommandList* cmd, const Mat4& viewProjection, bool depthTest, float lineWidth) {
    auto& vertices = depthTest ? m_impl->lineVerticesDepthTest : m_impl->lineVerticesNoDepthTest;
    auto& indices = depthTest ? m_impl->lineIndicesDepthTest : m_impl->lineIndicesNoDepthTest;
    auto& vertexBuffer = depthTest ? m_impl->lineVertexBufferDepthTest : m_impl->lineVertexBufferNoDepthTest;
    auto& indexBuffer = depthTest ? m_impl->lineIndexBufferDepthTest : m_impl->lineIndexBufferNoDepthTest;

    if (vertices.empty() || !cmd) {
        return;
    }

    PipelineHandle pipeline = depthTest ? m_impl->linePipelineDepthTest : m_impl->linePipelineNoDepthTest;

    cmd->bindPipeline(pipeline);

    cmd->setLineWidth(lineWidth);

    cmd->bindVertexBuffer(vertexBuffer, 0, 0);

    cmd->bindIndexBuffer(indexBuffer, IndexFormat::UInt32, 0);

    cmd->pushConstants(ShaderStage::Vertex, &viewProjection, sizeof(Mat4), 0);

    cmd->drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

}

void DebugRendererOpenGL::renderGrid(const DebugGrid& grid, const Mat4&) {

    float cellSize = grid.cellSize;
    int halfCells = grid.cellCount / 2;

    if (grid.is3D) {

        for (int i = -halfCells; i <= halfCells; ++i) {
            float offset = i * cellSize;

            Color color = (i == 0) ? grid.axisColor : grid.gridColor;
            Vec3 start(grid.center.x - halfCells * cellSize, grid.center.y, grid.center.z + offset);
            Vec3 end(grid.center.x + halfCells * cellSize, grid.center.y, grid.center.z + offset);

            drawLine(start, end, color, 0.0f, true);

            color = (i == 0) ? grid.axisColor : grid.gridColor;
            start = Vec3(grid.center.x + offset, grid.center.y, grid.center.z - halfCells * cellSize);
            end = Vec3(grid.center.x + offset, grid.center.y, grid.center.z + halfCells * cellSize);
            drawLine(start, end, color, 0.0f, true);
        }

        if (grid.drawAxes) {
            float axisLength = halfCells * cellSize;
            drawLine(grid.center, grid.center + Vec3(0, axisLength, 0), Color::Green(), 0.0f, true);
        }
    } else {

        for (int i = -halfCells; i <= halfCells; ++i) {
            float offset = i * cellSize;

            Color color = (i == 0) ? grid.axisColor : grid.gridColor;
            Vec3 start(grid.center.x - halfCells * cellSize, grid.center.y + offset, grid.center.z);
            Vec3 end(grid.center.x + halfCells * cellSize, grid.center.y + offset, grid.center.z);
            drawLine(start, end, color, 0.0f, false);

            color = (i == 0) ? grid.axisColor : grid.gridColor;
            start = Vec3(grid.center.x + offset, grid.center.y - halfCells * cellSize, grid.center.z);
            end = Vec3(grid.center.x + offset, grid.center.y + halfCells * cellSize, grid.center.z);
            drawLine(start, end, color, 0.0f, false);
        }
    }
}

void DebugRendererOpenGL::renderGizmo(const DebugGizmo& gizmo, const Mat4&, bool is3D) {
    float size = gizmo.size;

    if (is3D) {

        Vec3 xAxis = gizmo.rotation3D * Vec3(1, 0, 0);
        Vec3 yAxis = gizmo.rotation3D * Vec3(0, 1, 0);
        Vec3 zAxis = gizmo.rotation3D * Vec3(0, 0, 1);

        switch (gizmo.type) {
            case GizmoType::Translation: {

                Color xColor = (gizmo.highlightAxis == GizmoAxis::X) ? Color::Yellow() : Color::Red();
                Color yColor = (gizmo.highlightAxis == GizmoAxis::Y) ? Color::Yellow() : Color::Green();
                Color zColor = (gizmo.highlightAxis == GizmoAxis::Z) ? Color::Yellow() : Color::Blue();

                drawArrow(gizmo.position, gizmo.position + xAxis * size, xColor, size * 0.1f, 0.0f, false);
                drawArrow(gizmo.position, gizmo.position + yAxis * size, yColor, size * 0.1f, 0.0f, false);
                drawArrow(gizmo.position, gizmo.position + zAxis * size, zColor, size * 0.1f, 0.0f, false);
                break;
            }

            case GizmoType::Rotation: {

                Color xColor = (gizmo.highlightAxis == GizmoAxis::X) ? Color::Yellow() : Color::Red();
                Color yColor = (gizmo.highlightAxis == GizmoAxis::Y) ? Color::Yellow() : Color::Green();
                Color zColor = (gizmo.highlightAxis == GizmoAxis::Z) ? Color::Yellow() : Color::Blue();

                drawCircle(gizmo.position, xAxis, size, xColor, 64, 0.0f, false);
                drawCircle(gizmo.position, yAxis, size, yColor, 64, 0.0f, false);
                drawCircle(gizmo.position, zAxis, size, zColor, 64, 0.0f, false);
                break;
            }

            case GizmoType::Scale: {

                Color xColor = (gizmo.highlightAxis == GizmoAxis::X) ? Color::Yellow() : Color::Red();
                Color yColor = (gizmo.highlightAxis == GizmoAxis::Y) ? Color::Yellow() : Color::Green();
                Color zColor = (gizmo.highlightAxis == GizmoAxis::Z) ? Color::Yellow() : Color::Blue();

                drawLine(gizmo.position, gizmo.position + xAxis * size, xColor, 0.0f, false);
                drawLine(gizmo.position, gizmo.position + yAxis * size, yColor, 0.0f, false);
                drawLine(gizmo.position, gizmo.position + zAxis * size, zColor, 0.0f, false);

                drawBox(gizmo.position + xAxis * size, Vec3(size * 0.1f), xColor, false, 0.0f, false);
                drawBox(gizmo.position + yAxis * size, Vec3(size * 0.1f), yColor, false, 0.0f, false);
                drawBox(gizmo.position + zAxis * size, Vec3(size * 0.1f), zColor, false, 0.0f, false);
                break;
            }

            case GizmoType::All: {

                draw3DGizmo({gizmo.position, GizmoType::Translation, size * 0.8f, gizmo.highlightAxis, 0.0f, gizmo.rotation3D});
                draw3DGizmo({gizmo.position, GizmoType::Rotation, size * 1.2f, gizmo.highlightAxis, 0.0f, gizmo.rotation3D});
                break;
            }
        }
    } else {

        float cosR = std::cos(gizmo.rotation2D);
        float sinR = std::sin(gizmo.rotation2D);
        Vec3 xAxis(cosR, sinR, 0.0f);
        Vec3 yAxis(-sinR, cosR, 0.0f);

        switch (gizmo.type) {
            case GizmoType::Translation: {
                Color xColor = (gizmo.highlightAxis == GizmoAxis::X) ? Color::Yellow() : Color::Red();
                Color yColor = (gizmo.highlightAxis == GizmoAxis::Y) ? Color::Yellow() : Color::Green();

                drawArrow(gizmo.position, gizmo.position + xAxis * size, xColor, size * 0.1f, 0.0f, false);
                drawArrow(gizmo.position, gizmo.position + yAxis * size, yColor, size * 0.1f, 0.0f, false);
                break;
            }

            case GizmoType::Rotation: {
                drawCircle(gizmo.position, Vec3(0, 0, 1), size, Color::Blue(), 64, 0.0f, false);
                break;
            }

            case GizmoType::Scale: {
                Color xColor = (gizmo.highlightAxis == GizmoAxis::X) ? Color::Yellow() : Color::Red();
                Color yColor = (gizmo.highlightAxis == GizmoAxis::Y) ? Color::Yellow() : Color::Green();

                drawLine(gizmo.position, gizmo.position + xAxis * size, xColor, 0.0f, false);
                drawLine(gizmo.position, gizmo.position + yAxis * size, yColor, 0.0f, false);

                drawBox(gizmo.position + xAxis * size, Vec3(size * 0.1f), xColor, false, 0.0f, false);
                drawBox(gizmo.position + yAxis * size, Vec3(size * 0.1f), yColor, false, 0.0f, false);
                break;
            }

            case GizmoType::All: {

                draw2DGizmo({gizmo.position, GizmoType::Translation, size * 0.8f, gizmo.highlightAxis, gizmo.rotation2D});
                draw2DGizmo({gizmo.position, GizmoType::Rotation, size * 1.0f, gizmo.highlightAxis, gizmo.rotation2D});
                draw2DGizmo({gizmo.position, GizmoType::Scale, size * 0.6f, gizmo.highlightAxis, gizmo.rotation2D});
                break;
            }
        }
    }
}

}

namespace lupine {

std::unique_ptr<DebugRenderer> createDebugRenderer(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::OpenGL:
            return std::make_unique<DebugRendererOpenGL>();

#ifdef LUPINE_HAS_VULKAN
        case GraphicsBackend::Vulkan:
            return std::make_unique<DebugRendererVulkan>();
#endif

#ifdef LUPINE_HAS_DIRECTX11
        case GraphicsBackend::DirectX11:
            return std::make_unique<DebugRendererDX11>();
#endif

#ifdef LUPINE_HAS_DIRECTX12
        case GraphicsBackend::DirectX12:
            return std::make_unique<DebugRendererDX12>();
#endif

#ifdef LUPINE_HAS_METAL
        case GraphicsBackend::Metal:
            return std::make_unique<DebugRendererMetal>();
#endif

        default:
            return nullptr;
    }
}

}
