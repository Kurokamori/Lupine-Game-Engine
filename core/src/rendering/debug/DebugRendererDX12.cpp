#include "lupine/rendering/debug/DebugRendererDX12.hpp"
#include "lupine/rendering/GizmoUtils.hpp"
#include "lupine/rendering/debug/DebugTextGeometry.hpp"

#ifdef LUPINE_HAS_DIRECTX12

#include "lupine/rendering/DefaultShaders.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>

namespace lupine {

struct LineVertex {
    Vec3 position;
    Color color;
};

struct DebugRendererDX12::Impl {
    GfxDeviceDX12* device = nullptr;
    bool initialized = false;

    // Line shaders
    ShaderHandle lineVertexShader;
    ShaderHandle lineFragmentShader;
    PipelineHandle linePipelineDepthTest;
    PipelineHandle linePipelineNoDepthTest;

    // Screen size
    float screenWidth = 1920.0f;
    float screenHeight = 1080.0f;
    float lineWidth = 2.0f;

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
        DebugRendererDX12::LineCategory category;
    };
    std::vector<PersistentLine> persistentLines;

    float deltaTime = 0.016f;
};

DebugRendererDX12::DebugRendererDX12()
    : m_impl(std::make_unique<Impl>()) {
}

DebugRendererDX12::~DebugRendererDX12() {
    if (m_impl->initialized) {
        shutdown();
    }
}

bool DebugRendererDX12::initialize(IGfxDevice* device) {
    if (m_impl->initialized) {
        return true;
    }

    m_impl->device = dynamic_cast<GfxDeviceDX12*>(device);
    if (!m_impl->device) {
        LOG_ERROR(LogCategory::Render, "DebugRendererDX12: Device is not a GfxDeviceDX12");
        return false;
    }

    // Get precompiled Line shader bytecode from DefaultShaders
    DefaultShaders::ShaderDataPair shaderData;
    if (!DefaultShaders::getShaderData("Line", GraphicsBackend::DirectX12, &shaderData)) {
        LOG_ERROR(LogCategory::Render, "DebugRendererDX12: Failed to get Line shader data");
        return false;
    }

    if (!shaderData.vertex.isPrecompiled || !shaderData.fragment.isPrecompiled) {
        LOG_ERROR(LogCategory::Render, "DebugRendererDX12: Line shaders are not precompiled");
        return false;
    }

    // Create vertex shader from precompiled bytecode
    ShaderDesc vertexShaderDesc;
    vertexShaderDesc.stage = ShaderStage::Vertex;
    vertexShaderDesc.bytecode = static_cast<const uint8_t*>(shaderData.vertex.data);
    vertexShaderDesc.bytecodeSize = shaderData.vertex.size;
    vertexShaderDesc.entryPoint = "main";

    m_impl->lineVertexShader = m_impl->device->createShader(vertexShaderDesc);
    if (!m_impl->lineVertexShader.isValid()) {
        LOG_ERROR(LogCategory::Render, "Failed to create debug line vertex shader");
        return false;
    }

    // Create fragment shader from precompiled bytecode
    ShaderDesc fragmentShaderDesc;
    fragmentShaderDesc.stage = ShaderStage::Fragment;
    fragmentShaderDesc.bytecode = static_cast<const uint8_t*>(shaderData.fragment.data);
    fragmentShaderDesc.bytecodeSize = shaderData.fragment.size;
    fragmentShaderDesc.entryPoint = "main";

    m_impl->lineFragmentShader = m_impl->device->createShader(fragmentShaderDesc);
    if (!m_impl->lineFragmentShader.isValid()) {
        LOG_ERROR(LogCategory::Render, "Failed to create debug line fragment shader");
        return false;
    }

    // Create vertex layout for debug lines
    VertexBufferLayout lineLayout;
    lineLayout.stride = sizeof(LineVertex);
    lineLayout.attributes.push_back({"a_Position", VertexFormat::Float3, 0, 0, 0});
    lineLayout.attributes.push_back({"a_Color", VertexFormat::Float4, offsetof(LineVertex, color), 0, 1});

    // Create pipelines - one with depth test, one without
    {
        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {m_impl->lineVertexShader, m_impl->lineFragmentShader};
        pipelineDesc.vertexLayout = lineLayout;
        pipelineDesc.topology = PrimitiveTopology::LineList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = false;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        m_impl->linePipelineDepthTest = m_impl->device->createPipeline(pipelineDesc);
        if (!m_impl->linePipelineDepthTest.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create debug line pipeline with depth test");
            return false;
        }

        // Pipeline without depth test
        pipelineDesc.depthStencilState.depthTestEnable = false;
        m_impl->linePipelineNoDepthTest = m_impl->device->createPipeline(pipelineDesc);
        if (!m_impl->linePipelineNoDepthTest.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create debug line pipeline without depth test");
            return false;
        }
    }

    // Create vertex and index buffers for each category
    // Use TransferDst flag to create upload heap buffers that can be directly mapped
    constexpr size_t MAX_VERTICES = 65536;
    constexpr size_t MAX_INDICES = 65536;

    BufferDesc vertexBufferDesc;
    vertexBufferDesc.size = MAX_VERTICES * sizeof(LineVertex);
    vertexBufferDesc.usage = BufferUsage::Vertex | BufferUsage::TransferDst;

    BufferDesc indexBufferDesc;
    indexBufferDesc.size = MAX_INDICES * sizeof(uint32_t);
    indexBufferDesc.usage = BufferUsage::Index | BufferUsage::TransferDst;

    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            m_impl->lineVertexBuffers[cat][depth] = m_impl->device->createBuffer(vertexBufferDesc);
            m_impl->lineIndexBuffers[cat][depth] = m_impl->device->createBuffer(indexBufferDesc);

            if (!m_impl->lineVertexBuffers[cat][depth].isValid() ||
                !m_impl->lineIndexBuffers[cat][depth].isValid()) {
                LOG_ERROR(LogCategory::Render, "Failed to create debug line buffers");
                return false;
            }
        }
    }

    m_impl->initialized = true;

    return true;
}

void DebugRendererDX12::shutdown() {
    if (!m_impl->initialized) {
        return;
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

    // Destroy pipelines
    if (m_impl->linePipelineDepthTest.isValid()) {
        m_impl->device->destroyPipeline(m_impl->linePipelineDepthTest);
    }
    if (m_impl->linePipelineNoDepthTest.isValid()) {
        m_impl->device->destroyPipeline(m_impl->linePipelineNoDepthTest);
    }

    // Destroy shaders
    if (m_impl->lineVertexShader.isValid()) {
        m_impl->device->destroyShader(m_impl->lineVertexShader);
    }
    if (m_impl->lineFragmentShader.isValid()) {
        m_impl->device->destroyShader(m_impl->lineFragmentShader);
    }

    m_impl->initialized = false;
    
}

void DebugRendererDX12::beginFrame() {
    // Clear per-frame line data
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            m_impl->lineVertices[cat][depth].clear();
            m_impl->lineIndices[cat][depth].clear();
        }
    }

    // Add persistent lines
    for (auto& line : m_impl->persistentLines) {
        int catIdx = static_cast<int>(line.category);
        int depthIdx = line.depthTest ? 1 : 0;

        uint32_t startIndex = static_cast<uint32_t>(m_impl->lineVertices[catIdx][depthIdx].size());

        m_impl->lineVertices[catIdx][depthIdx].push_back({line.start, line.color});
        m_impl->lineVertices[catIdx][depthIdx].push_back({line.end, line.color});

        m_impl->lineIndices[catIdx][depthIdx].push_back(startIndex);
        m_impl->lineIndices[catIdx][depthIdx].push_back(startIndex + 1);
    }
}

void DebugRendererDX12::endFrame() {
    // Update persistent lines timing
    m_impl->persistentLines.erase(
        std::remove_if(m_impl->persistentLines.begin(), m_impl->persistentLines.end(),
            [this](Impl::PersistentLine& line) {
                line.remainingTime -= m_impl->deltaTime;
                return line.remainingTime <= 0.0f;
            }),
        m_impl->persistentLines.end()
    );
}

void DebugRendererDX12::render(IGfxCommandList* cmd, const Mat4& viewProjection, float /*lineWidth*/) {
    if (!m_impl->initialized) {
        return;
    }

    // Update buffers and render for each category/depth combination
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            const auto& vertices = m_impl->lineVertices[cat][depth];
            const auto& indices = m_impl->lineIndices[cat][depth];

            if (vertices.empty() || indices.empty()) {
                continue;
            }

            // Update vertex buffer
            m_impl->device->updateBuffer(
                m_impl->lineVertexBuffers[cat][depth],
                vertices.data(),
                vertices.size() * sizeof(LineVertex),
                0
            );

            // Update index buffer
            m_impl->device->updateBuffer(
                m_impl->lineIndexBuffers[cat][depth],
                indices.data(),
                indices.size() * sizeof(uint32_t),
                0
            );

            // Bind appropriate pipeline
            PipelineHandle pipeline = depth ? m_impl->linePipelineDepthTest : m_impl->linePipelineNoDepthTest;
            cmd->bindPipeline(pipeline);

            // Set view projection matrix
            cmd->setUniformMat4("u_ViewProjection", viewProjection);

            // Set model matrix to identity - essential for DX12 as the line shader transforms by u_Model
            // Without this, u_Model defaults to a zero matrix causing all vertices to collapse to origin
            cmd->setUniformMat4("u_Model", Mat4::Identity());

            // Set tint color to white - essential for DX12 as the line shader multiplies by this
            // Without this, u_TintColor defaults to (0,0,0,0) making all lines invisible
            cmd->setUniformVec4("u_TintColor", Vec4(1.0f, 1.0f, 1.0f, 1.0f));

            // Bind buffers
            cmd->bindVertexBuffer(m_impl->lineVertexBuffers[cat][depth], 0, 0);
            cmd->bindIndexBuffer(m_impl->lineIndexBuffers[cat][depth], IndexFormat::UInt32, 0);

            // Draw
            cmd->drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        }
    }
}

void DebugRendererDX12::clear() {
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            m_impl->lineVertices[cat][depth].clear();
            m_impl->lineIndices[cat][depth].clear();
        }
    }
    m_impl->persistentLines.clear();
}

void DebugRendererDX12::setScreenSize(float width, float height) {
    m_impl->screenWidth = width;
    m_impl->screenHeight = height;
}

void DebugRendererDX12::setLineWidth(float width) {
    m_impl->lineWidth = width;
}

// ===== Grid Rendering =====

void DebugRendererDX12::draw2DGrid(const DebugGrid& grid) {
    float halfSize = grid.cellSize * grid.cellCount;

    // Draw vertical lines (along Y axis)
    for (int i = -grid.cellCount; i <= grid.cellCount; ++i) {
        float x = grid.center.x + i * grid.cellSize;
        Vec3 start(x, grid.center.y - halfSize, grid.center.z);
        Vec3 end(x, grid.center.y + halfSize, grid.center.z);

        Color lineColor = (i == 0 && grid.drawAxes) ? grid.axisColor : grid.gridColor;
        drawLineInternal(start, end, lineColor, false, LineCategory::Grid);
    }

    // Draw horizontal lines (along X axis)
    for (int i = -grid.cellCount; i <= grid.cellCount; ++i) {
        float y = grid.center.y + i * grid.cellSize;
        Vec3 start(grid.center.x - halfSize, y, grid.center.z);
        Vec3 end(grid.center.x + halfSize, y, grid.center.z);

        Color lineColor = (i == 0 && grid.drawAxes) ? grid.axisColor : grid.gridColor;
        drawLineInternal(start, end, lineColor, false, LineCategory::Grid);
    }
}

void DebugRendererDX12::draw3DGrid(const DebugGrid& grid) {
    float halfSize = grid.cellSize * grid.cellCount;

    // Draw lines along X axis (perpendicular to Z)
    for (int i = -grid.cellCount; i <= grid.cellCount; ++i) {
        float z = grid.center.z + i * grid.cellSize;
        Vec3 start(grid.center.x - halfSize, grid.center.y, z);
        Vec3 end(grid.center.x + halfSize, grid.center.y, z);

        Color lineColor = (i == 0 && grid.drawAxes) ? grid.axisColor : grid.gridColor;
        drawLineInternal(start, end, lineColor, true, LineCategory::Grid);
    }

    // Draw lines along Z axis (perpendicular to X)
    for (int i = -grid.cellCount; i <= grid.cellCount; ++i) {
        float x = grid.center.x + i * grid.cellSize;
        Vec3 start(x, grid.center.y, grid.center.z - halfSize);
        Vec3 end(x, grid.center.y, grid.center.z + halfSize);

        Color lineColor = (i == 0 && grid.drawAxes) ? grid.axisColor : grid.gridColor;
        drawLineInternal(start, end, lineColor, true, LineCategory::Grid);
    }
}

// ===== Bounding Box Rendering =====

void DebugRendererDX12::draw2DBoundingBox(const AABB& bounds, const Color& color, float duration) {
    Vec3 min = bounds.min;
    Vec3 max = bounds.max;

    // Draw 2D rectangle on XY plane
    drawLineWithCategory(Vec3(min.x, min.y, 0.0f), Vec3(max.x, min.y, 0.0f), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, min.y, 0.0f), Vec3(max.x, max.y, 0.0f), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(max.x, max.y, 0.0f), Vec3(min.x, max.y, 0.0f), color, duration, false, LineCategory::BoundingBox);
    drawLineWithCategory(Vec3(min.x, max.y, 0.0f), Vec3(min.x, min.y, 0.0f), color, duration, false, LineCategory::BoundingBox);
}

void DebugRendererDX12::draw2DOrientedBoundingBox(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration) {
    float c = std::cos(rotation);
    float s = std::sin(rotation);

    Vec2 halfSize = Vec2(size.x * 0.5f, size.y * 0.5f);
    Vec2 corners[4] = {
        Vec2(-halfSize.x, -halfSize.y),
        Vec2(halfSize.x, -halfSize.y),
        Vec2(halfSize.x, halfSize.y),
        Vec2(-halfSize.x, halfSize.y)
    };

    // Rotate and translate corners
    for (int i = 0; i < 4; ++i) {
        float rx = corners[i].x * c - corners[i].y * s;
        float ry = corners[i].x * s + corners[i].y * c;
        corners[i] = Vec2(rx + center.x, ry + center.y);
    }

    // Draw edges
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        drawLineWithCategory(
            Vec3(corners[i].x, corners[i].y, 0.0f),
            Vec3(corners[next].x, corners[next].y, 0.0f),
            color, duration, false, LineCategory::BoundingBox
        );
    }
}

void DebugRendererDX12::draw3DBoundingBox(const AABB& bounds, const Color& color, float duration) {
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

void DebugRendererDX12::draw3DOrientedBoundingBox(const OBB& obb, const Color& color, float duration) {
    // Get OBB corners using GetCorner method
    Vec3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i] = obb.GetCorner(i);
    }

    // Bottom face (corners 0,1,3,2 form the bottom)
    drawLineWithCategory(corners[0], corners[1], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[1], corners[3], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[3], corners[2], color, duration, true, LineCategory::BoundingBox);
    drawLineWithCategory(corners[2], corners[0], color, duration, true, LineCategory::BoundingBox);

    // Top face (corners 4,5,7,6 form the top)
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

// ===== Gizmo Rendering =====

void DebugRendererDX12::draw2DGizmo(const DebugGizmo& gizmo) {
    std::vector<GizmoSegment> segments = GizmoUtils::BuildGizmo2DGeometry(gizmo);
    for (const GizmoSegment& seg : segments) {
        drawLineWithCategory(seg.start, seg.end, seg.color, 0.0f, false, LineCategory::Gizmo);
    }
}

void DebugRendererDX12::draw3DGizmo(const DebugGizmo& gizmo) {
    float arrowLength = gizmo.size;
    Vec3 origin = gizmo.position;

    Color xColor = (gizmo.highlightAxis == GizmoAxis::X || gizmo.highlightAxis == GizmoAxis::XY ||
                    gizmo.highlightAxis == GizmoAxis::XZ || gizmo.highlightAxis == GizmoAxis::XYZ)
                   ? Color(1.0f, 1.0f, 0.0f, 1.0f) : Color(1.0f, 0.0f, 0.0f, 1.0f);
    Color yColor = (gizmo.highlightAxis == GizmoAxis::Y || gizmo.highlightAxis == GizmoAxis::XY ||
                    gizmo.highlightAxis == GizmoAxis::YZ || gizmo.highlightAxis == GizmoAxis::XYZ)
                   ? Color(1.0f, 1.0f, 0.0f, 1.0f) : Color(0.0f, 1.0f, 0.0f, 1.0f);
    Color zColor = (gizmo.highlightAxis == GizmoAxis::Z || gizmo.highlightAxis == GizmoAxis::XZ ||
                    gizmo.highlightAxis == GizmoAxis::YZ || gizmo.highlightAxis == GizmoAxis::XYZ)
                   ? Color(1.0f, 1.0f, 0.0f, 1.0f) : Color(0.0f, 0.0f, 1.0f, 1.0f);

    // X axis (red)
    drawGizmoArrow(origin, Vec3(1.0f, 0.0f, 0.0f), arrowLength, xColor, true);

    // Y axis (green)
    drawGizmoArrow(origin, Vec3(0.0f, 1.0f, 0.0f), arrowLength, yColor, true);

    // Z axis (blue)
    drawGizmoArrow(origin, Vec3(0.0f, 0.0f, 1.0f), arrowLength, zColor, true);
}

// ===== Basic Primitives =====

void DebugRendererDX12::drawLine(const Vec3& start, const Vec3& end, const Color& color,
                                  float duration, bool depthTest) {
    drawLineWithCategory(start, end, color, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX12::drawLineStrip(const std::vector<Vec3>& points, const Color& color,
                                       float duration, bool depthTest) {
    drawLineStripWithCategory(points, color, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX12::drawRay(const Vec3& origin, const Vec3& direction, const Color& color,
                                 float length, float duration, bool depthTest) {
    Vec3 normalizedDir = direction;
    float dirLength = std::sqrt(normalizedDir.x * normalizedDir.x +
                                 normalizedDir.y * normalizedDir.y +
                                 normalizedDir.z * normalizedDir.z);
    if (dirLength > 0.0001f) {
        normalizedDir = Vec3(normalizedDir.x / dirLength, normalizedDir.y / dirLength, normalizedDir.z / dirLength);
    }
    Vec3 end = Vec3(origin.x + normalizedDir.x * length,
                    origin.y + normalizedDir.y * length,
                    origin.z + normalizedDir.z * length);
    drawLine(origin, end, color, duration, depthTest);
}

void DebugRendererDX12::drawBox(const Vec3& center, const Vec3& size, const Color& color,
                                 bool wireframe, float duration, bool depthTest) {
    drawBoxWithCategory(center, size, color, wireframe, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX12::drawSphere(const Vec3& center, float radius, const Color& color,
                                    bool wireframe, int segments, float duration, bool depthTest) {
    (void)wireframe;

    // Draw three circles for each axis
    drawCircleWithCategory(center, Vec3(1.0f, 0.0f, 0.0f), radius, color, segments, duration, depthTest, LineCategory::Other);
    drawCircleWithCategory(center, Vec3(0.0f, 1.0f, 0.0f), radius, color, segments, duration, depthTest, LineCategory::Other);
    drawCircleWithCategory(center, Vec3(0.0f, 0.0f, 1.0f), radius, color, segments, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX12::drawCircle(const Vec3& center, const Vec3& normal, float radius, const Color& color,
                                    int segments, float duration, bool depthTest) {
    drawCircleWithCategory(center, normal, radius, color, segments, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX12::drawArrow(const Vec3& start, const Vec3& end, const Color& color,
                                   float arrowSize, float duration, bool depthTest) {
    drawArrowWithCategory(start, end, color, arrowSize, duration, depthTest, LineCategory::Other);
}

void DebugRendererDX12::drawText(const Vec3& position, const std::string& text, const Color& color,
                                  float size, float duration) {
    debug::EmitDebugText(*this, position, text, color, size, duration);
}

void DebugRendererDX12::drawAxes(const Vec3& position, float size, float duration) {
    drawLine(position, Vec3(position.x + size, position.y, position.z), Color(1.0f, 0.0f, 0.0f, 1.0f), duration, true);
    drawLine(position, Vec3(position.x, position.y + size, position.z), Color(0.0f, 1.0f, 0.0f, 1.0f), duration, true);
    drawLine(position, Vec3(position.x, position.y, position.z + size), Color(0.0f, 0.0f, 1.0f, 1.0f), duration, true);
}

void DebugRendererDX12::drawTransform(const Vec3& position, const Mat4& rotation, float size, float duration) {
    Vec3 xAxis = Vec3(rotation[0][0], rotation[0][1], rotation[0][2]);
    Vec3 yAxis = Vec3(rotation[1][0], rotation[1][1], rotation[1][2]);
    Vec3 zAxis = Vec3(rotation[2][0], rotation[2][1], rotation[2][2]);

    drawLine(position, Vec3(position.x + xAxis.x * size, position.y + xAxis.y * size, position.z + xAxis.z * size), Color(1.0f, 0.0f, 0.0f, 1.0f), duration, true);
    drawLine(position, Vec3(position.x + yAxis.x * size, position.y + yAxis.y * size, position.z + yAxis.z * size), Color(0.0f, 1.0f, 0.0f, 1.0f), duration, true);
    drawLine(position, Vec3(position.x + zAxis.x * size, position.y + zAxis.y * size, position.z + zAxis.z * size), Color(0.0f, 0.0f, 1.0f, 1.0f), duration, true);
}

// ===== Statistics =====

size_t DebugRendererDX12::getLineCount() const {
    size_t count = 0;
    for (int cat = 0; cat < Impl::NUM_CATEGORIES; ++cat) {
        for (int depth = 0; depth < 2; ++depth) {
            count += m_impl->lineIndices[cat][depth].size() / 2;
        }
    }
    return count;
}

size_t DebugRendererDX12::getPrimitiveCount() const {
    return getLineCount();
}

// ===== Internal Methods =====

void DebugRendererDX12::drawLineInternal(const Vec3& start, const Vec3& end, const Color& color,
                                          bool depthTest, LineCategory category) {
    int catIdx = static_cast<int>(category);
    int depthIdx = depthTest ? 1 : 0;

    uint32_t startIndex = static_cast<uint32_t>(m_impl->lineVertices[catIdx][depthIdx].size());

    m_impl->lineVertices[catIdx][depthIdx].push_back({start, color});
    m_impl->lineVertices[catIdx][depthIdx].push_back({end, color});

    m_impl->lineIndices[catIdx][depthIdx].push_back(startIndex);
    m_impl->lineIndices[catIdx][depthIdx].push_back(startIndex + 1);
}

void DebugRendererDX12::drawLineWithCategory(const Vec3& start, const Vec3& end, const Color& color,
                                              float duration, bool depthTest, LineCategory category) {
    if (duration > 0.0f) {
        m_impl->persistentLines.push_back({start, end, color, duration, depthTest, category});
    }
    drawLineInternal(start, end, color, depthTest, category);
}

void DebugRendererDX12::drawLineStripWithCategory(const std::vector<Vec3>& points, const Color& color,
                                                   float duration, bool depthTest, LineCategory category) {
    for (size_t i = 1; i < points.size(); ++i) {
        drawLineWithCategory(points[i-1], points[i], color, duration, depthTest, category);
    }
}

void DebugRendererDX12::drawBoxWithCategory(const Vec3& center, const Vec3& size, const Color& color,
                                             bool /*wireframe*/, float duration, bool depthTest, LineCategory category) {
    Vec3 halfSize = Vec3(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);
    Vec3 min = Vec3(center.x - halfSize.x, center.y - halfSize.y, center.z - halfSize.z);
    Vec3 max = Vec3(center.x + halfSize.x, center.y + halfSize.y, center.z + halfSize.z);

    // Bottom face
    drawLineWithCategory(Vec3(min.x, min.y, min.z), Vec3(max.x, min.y, min.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(max.x, min.y, min.z), Vec3(max.x, min.y, max.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(max.x, min.y, max.z), Vec3(min.x, min.y, max.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(min.x, min.y, max.z), Vec3(min.x, min.y, min.z), color, duration, depthTest, category);

    // Top face
    drawLineWithCategory(Vec3(min.x, max.y, min.z), Vec3(max.x, max.y, min.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(max.x, max.y, min.z), Vec3(max.x, max.y, max.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(max.x, max.y, max.z), Vec3(min.x, max.y, max.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(min.x, max.y, max.z), Vec3(min.x, max.y, min.z), color, duration, depthTest, category);

    // Vertical edges
    drawLineWithCategory(Vec3(min.x, min.y, min.z), Vec3(min.x, max.y, min.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(max.x, min.y, min.z), Vec3(max.x, max.y, min.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(max.x, min.y, max.z), Vec3(max.x, max.y, max.z), color, duration, depthTest, category);
    drawLineWithCategory(Vec3(min.x, min.y, max.z), Vec3(min.x, max.y, max.z), color, duration, depthTest, category);
}

void DebugRendererDX12::drawCircleWithCategory(const Vec3& center, const Vec3& normal, float radius,
                                                const Color& color, int segments, float duration,
                                                bool depthTest, LineCategory category) {
    // Create orthonormal basis
    Vec3 n = normal;
    float nLength = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (nLength > 0.0001f) {
        n = Vec3(n.x / nLength, n.y / nLength, n.z / nLength);
    }

    Vec3 up = (std::abs(n.y) < 0.999f) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);

    Vec3 right = Vec3(
        up.y * n.z - up.z * n.y,
        up.z * n.x - up.x * n.z,
        up.x * n.y - up.y * n.x
    );
    float rightLength = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rightLength > 0.0001f) {
        right = Vec3(right.x / rightLength, right.y / rightLength, right.z / rightLength);
    }

    up = Vec3(
        n.y * right.z - n.z * right.y,
        n.z * right.x - n.x * right.z,
        n.x * right.y - n.y * right.x
    );

    float angleStep = 2.0f * math::PI / static_cast<float>(segments);

    Vec3 prevPoint = Vec3(center.x + right.x * radius, center.y + right.y * radius, center.z + right.z * radius);
    for (int i = 1; i <= segments; ++i) {
        float angle = angleStep * static_cast<float>(i);
        float c = std::cos(angle);
        float s = std::sin(angle);
        Vec3 offset = Vec3(
            (right.x * c + up.x * s) * radius,
            (right.y * c + up.y * s) * radius,
            (right.z * c + up.z * s) * radius
        );
        Vec3 point = Vec3(center.x + offset.x, center.y + offset.y, center.z + offset.z);
        drawLineWithCategory(prevPoint, point, color, duration, depthTest, category);
        prevPoint = point;
    }
}

void DebugRendererDX12::drawArrowWithCategory(const Vec3& start, const Vec3& end, const Color& color,
                                               float arrowSize, float duration, bool depthTest, LineCategory category) {
    // Main line
    drawLineWithCategory(start, end, color, duration, depthTest, category);

    // Arrow head
    Vec3 direction = Vec3(end.x - start.x, end.y - start.y, end.z - start.z);
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (length < 0.0001f) return;

    direction = Vec3(direction.x / length, direction.y / length, direction.z / length);

    // Create perpendicular vectors
    Vec3 up = (std::abs(direction.y) < 0.999f) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
    Vec3 right = Vec3(
        up.y * direction.z - up.z * direction.y,
        up.z * direction.x - up.x * direction.z,
        up.x * direction.y - up.y * direction.x
    );
    float rightLength = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rightLength > 0.0001f) {
        right = Vec3(right.x / rightLength, right.y / rightLength, right.z / rightLength);
    }

    up = Vec3(
        direction.y * right.z - direction.z * right.y,
        direction.z * right.x - direction.x * right.z,
        direction.x * right.y - direction.y * right.x
    );

    Vec3 arrowBase = Vec3(end.x - direction.x * arrowSize, end.y - direction.y * arrowSize, end.z - direction.z * arrowSize);
    float arrowWidth = arrowSize * 0.4f;

    drawLineWithCategory(end, Vec3(arrowBase.x + right.x * arrowWidth, arrowBase.y + right.y * arrowWidth, arrowBase.z + right.z * arrowWidth), color, duration, depthTest, category);
    drawLineWithCategory(end, Vec3(arrowBase.x - right.x * arrowWidth, arrowBase.y - right.y * arrowWidth, arrowBase.z - right.z * arrowWidth), color, duration, depthTest, category);
    drawLineWithCategory(end, Vec3(arrowBase.x + up.x * arrowWidth, arrowBase.y + up.y * arrowWidth, arrowBase.z + up.z * arrowWidth), color, duration, depthTest, category);
    drawLineWithCategory(end, Vec3(arrowBase.x - up.x * arrowWidth, arrowBase.y - up.y * arrowWidth, arrowBase.z - up.z * arrowWidth), color, duration, depthTest, category);
}

void DebugRendererDX12::drawGizmoArrow(const Vec3& origin, const Vec3& direction, float length, const Color& color, bool depthTest) {
    Vec3 end = Vec3(origin.x + direction.x * length, origin.y + direction.y * length, origin.z + direction.z * length);
    drawArrowWithCategory(origin, end, color, length * 0.15f, 0.0f, depthTest, LineCategory::Gizmo);
}

void DebugRendererDX12::drawGizmoCircle(const Vec3& center, const Vec3& normal, float radius, const Color& color, bool depthTest) {
    drawCircleWithCategory(center, normal, radius, color, 32, 0.0f, depthTest, LineCategory::Gizmo);
}

void DebugRendererDX12::drawGizmoBox(const Vec3& center, float size, const Color& color, bool depthTest) {
    drawBoxWithCategory(center, Vec3(size, size, size), color, true, 0.0f, depthTest, LineCategory::Gizmo);
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12
