#pragma once

#ifdef LUPINE_HAS_DIRECTX12

#include "DebugRenderer.hpp"
#include "../backends/GfxDeviceDX12.hpp"
#include <memory>

namespace lupine {

/**
 * DirectX 12 implementation of the debug renderer.
 * Provides line, grid, gizmo, and bounding box rendering for the editor.
 */
class DebugRendererDX12 : public DebugRenderer {
public:
    DebugRendererDX12();
    ~DebugRendererDX12() override;

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
    void draw2DBoundingBox(const AABB& bounds, const Color& color, float duration = 0.0f) override;
    void draw2DOrientedBoundingBox(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration = 0.0f) override;
    void draw3DBoundingBox(const AABB& bounds, const Color& color, float duration = 0.0f) override;
    void draw3DOrientedBoundingBox(const OBB& obb, const Color& color, float duration = 0.0f) override;

    // Gizmos
    void draw2DGizmo(const DebugGizmo& gizmo) override;
    void draw3DGizmo(const DebugGizmo& gizmo) override;

    // Basic primitives
    void drawLine(const Vec3& start, const Vec3& end, const Color& color,
                  float duration = 0.0f, bool depthTest = true) override;
    void drawLineStrip(const std::vector<Vec3>& points, const Color& color,
                       float duration = 0.0f, bool depthTest = true) override;
    void drawRay(const Vec3& origin, const Vec3& direction, const Color& color,
                 float length = 1000.0f, float duration = 0.0f, bool depthTest = true) override;
    void drawBox(const Vec3& center, const Vec3& size, const Color& color,
                 bool wireframe = true, float duration = 0.0f, bool depthTest = true) override;
    void drawSphere(const Vec3& center, float radius, const Color& color,
                    bool wireframe = true, int segments = 16, float duration = 0.0f, bool depthTest = true) override;
    void drawCircle(const Vec3& center, const Vec3& normal, float radius, const Color& color,
                    int segments = 32, float duration = 0.0f, bool depthTest = true) override;
    void drawArrow(const Vec3& start, const Vec3& end, const Color& color,
                   float arrowSize = 0.2f, float duration = 0.0f, bool depthTest = true) override;
    void drawText(const Vec3& position, const std::string& text, const Color& color,
                  float size = 1.0f, float duration = 0.0f) override;

    // Axes
    void drawAxes(const Vec3& position, float size = 1.0f, float duration = 0.0f) override;
    void drawTransform(const Vec3& position, const Mat4& rotation, float size = 1.0f, float duration = 0.0f) override;

    // Statistics
    size_t getLineCount() const override;
    size_t getPrimitiveCount() const override;

    // DX12-specific methods
    void setScreenSize(float width, float height) override;
    void setLineWidth(float width) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Line category for different line widths
    enum class LineCategory {
        Grid,
        BoundingBox,
        Gizmo,
        Other
    };

    // Internal drawing methods with category support
    void drawLineInternal(const Vec3& start, const Vec3& end, const Color& color,
                          bool depthTest, LineCategory category);
    void drawLineWithCategory(const Vec3& start, const Vec3& end, const Color& color,
                              float duration, bool depthTest, LineCategory category);
    void drawLineStripWithCategory(const std::vector<Vec3>& points, const Color& color,
                                   float duration, bool depthTest, LineCategory category);
    void drawBoxWithCategory(const Vec3& center, const Vec3& size, const Color& color,
                             bool wireframe, float duration, bool depthTest, LineCategory category);
    void drawCircleWithCategory(const Vec3& center, const Vec3& normal, float radius,
                                const Color& color, int segments, float duration,
                                bool depthTest, LineCategory category);
    void drawArrowWithCategory(const Vec3& start, const Vec3& end, const Color& color,
                               float arrowSize, float duration, bool depthTest, LineCategory category);

    // Helper methods for grid rendering
    void renderGrid(IGfxCommandList* cmd, const DebugGrid& grid, const Mat4& viewProjection);

    // Helper methods for gizmo rendering
    void drawGizmoArrow(const Vec3& origin, const Vec3& direction, float length, const Color& color, bool depthTest);
    void drawGizmoCircle(const Vec3& center, const Vec3& normal, float radius, const Color& color, bool depthTest);
    void drawGizmoBox(const Vec3& center, float size, const Color& color, bool depthTest);
};

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12
