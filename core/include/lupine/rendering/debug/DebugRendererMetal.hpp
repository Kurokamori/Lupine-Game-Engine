#pragma once

#include "DebugRenderer.hpp"
#include "../backends/GfxDeviceMetal.hpp"
#include <memory>

namespace lupine {

#ifdef LUPINE_HAS_METAL

/**
 * Metal implementation of the debug renderer.
 * Provides line, grid, gizmo, and bounding box rendering for the editor.
 *
 * Uses Metal Shading Language (MSL) for shaders with:
 * - Constant buffer for view-projection matrix (buffer slot 0)
 * - Line list topology for all debug primitives
 * - Separate pipelines for depth-tested and non-depth-tested rendering
 * - Category-based line width multipliers for visual hierarchy
 */
class DebugRendererMetal : public DebugRenderer {
public:
    DebugRendererMetal();
    ~DebugRendererMetal() override;

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
    void draw3DOrientedBoundingBox(const OBB& bounds, const Color& color, float duration = 0.0f) override;

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

#else // !LUPINE_HAS_METAL

/**
 * Stub Metal debug renderer for non-Apple platforms.
 */
class DebugRendererMetal : public DebugRenderer {
public:
    DebugRendererMetal() = default;
    ~DebugRendererMetal() override = default;

    bool initialize(IGfxDevice*) override { return false; }
    void shutdown() override {}
    void beginFrame() override {}
    void endFrame() override {}
    void render(IGfxCommandList*, const Mat4&, float) override {}
    void clear() override {}

    void draw2DGrid(const DebugGrid&) override {}
    void draw3DGrid(const DebugGrid&) override {}

    void draw2DBoundingBox(const AABB&, const Color&, float) override {}
    void draw2DOrientedBoundingBox(const Vec2&, const Vec2&, float, const Color&, float) override {}
    void draw3DBoundingBox(const AABB&, const Color&, float) override {}
    void draw3DOrientedBoundingBox(const OBB&, const Color&, float) override {}

    void draw2DGizmo(const DebugGizmo&) override {}
    void draw3DGizmo(const DebugGizmo&) override {}

    void drawLine(const Vec3&, const Vec3&, const Color&, float, bool) override {}
    void drawLineStrip(const std::vector<Vec3>&, const Color&, float, bool) override {}
    void drawRay(const Vec3&, const Vec3&, const Color&, float, float, bool) override {}
    void drawBox(const Vec3&, const Vec3&, const Color&, bool, float, bool) override {}
    void drawSphere(const Vec3&, float, const Color&, bool, int, float, bool) override {}
    void drawCircle(const Vec3&, const Vec3&, float, const Color&, int, float, bool) override {}
    void drawArrow(const Vec3&, const Vec3&, const Color&, float, float, bool) override {}
    void drawText(const Vec3&, const std::string&, const Color&, float, float) override {}

    void drawAxes(const Vec3&, float, float) override {}
    void drawTransform(const Vec3&, const Mat4&, float, float) override {}

    size_t getLineCount() const override { return 0; }
    size_t getPrimitiveCount() const override { return 0; }
};

#endif // LUPINE_HAS_METAL

} // namespace lupine
