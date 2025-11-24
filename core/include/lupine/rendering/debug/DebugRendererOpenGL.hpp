#pragma once

#include "DebugRenderer.hpp"
#include "../backends/GfxDeviceOpenGL.hpp"
#include <vector>

namespace lupine {

/**
 * OpenGL implementation of the debug renderer.
 */
class DebugRendererOpenGL : public DebugRenderer {
public:
    DebugRendererOpenGL();
    ~DebugRendererOpenGL() override;

    // DebugRenderer interface
    bool initialize(IGfxDevice* device) override;
    void shutdown() override;

    void beginFrame() override;
    void endFrame() override;
    void render(IGfxCommandList* cmd, const Mat4& viewProjection, float lineWidth = 1.0f) override;
    void clear() override;

    // Grid rendering
    void draw2DGrid(const DebugGrid& grid) override;
    void draw3DGrid(const DebugGrid& grid) override;

    // Bounding boxes
    void draw2DBoundingBox(const AABB& bounds, const Color& color, float duration) override;
    void draw2DOrientedBoundingBox(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration) override;
    void draw3DBoundingBox(const AABB& bounds, const Color& color, float duration) override;
    void draw3DOrientedBoundingBox(const OBB& bounds, const Color& color, float duration) override;

    // Gizmos
    void draw2DGizmo(const DebugGizmo& gizmo) override;
    void draw3DGizmo(const DebugGizmo& gizmo) override;

    // Primitives
    void drawLine(const Vec3& start, const Vec3& end, const Color& color,
                 float duration, bool depthTest) override;
    void drawLineStrip(const std::vector<Vec3>& points, const Color& color,
                      float duration, bool depthTest) override;
    void drawRay(const Vec3& origin, const Vec3& direction, const Color& color,
                float length, float duration, bool depthTest) override;
    void drawBox(const Vec3& center, const Vec3& size, const Color& color,
                bool wireframe, float duration, bool depthTest) override;
    void drawSphere(const Vec3& center, float radius, const Color& color,
                   bool wireframe, int segments, float duration, bool depthTest) override;
    void drawCircle(const Vec3& center, const Vec3& normal, float radius, const Color& color,
                   int segments, float duration, bool depthTest) override;
    void drawArrow(const Vec3& start, const Vec3& end, const Color& color,
                  float arrowSize, float duration, bool depthTest) override;
    void drawText(const Vec3& position, const std::string& text, const Color& color,
                 float size, float duration) override;

    // Helpers
    void drawAxes(const Vec3& position, float size, float duration) override;
    void drawTransform(const Vec3& position, const Mat4& rotation, float size, float duration) override;

    // Statistics
    size_t getLineCount() const override;
    size_t getPrimitiveCount() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Helper methods
    void createGridMesh(bool is3D);
    void createGizmaMeshes();
    void createPrimitiveMeshes();
    void updateLineBuffer();
    void renderLines(IGfxCommandList* cmd, const Mat4& viewProjection, bool depthTest, float lineWidth);
    void renderGrid(const DebugGrid& grid, const Mat4& viewProjection);
    void renderGizmo(const DebugGizmo& gizmo, const Mat4& viewProjection, bool is3D);
};

} // namespace lupine
