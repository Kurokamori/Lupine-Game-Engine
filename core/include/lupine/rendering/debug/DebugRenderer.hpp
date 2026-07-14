#pragma once

#include "DebugDrawCommands.hpp"
#include "../gfx/IGfxDevice.hpp"
#include "../ResourceHandles.hpp"
#include "lupine/math/Mat4.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/OBB.hpp"
#include <memory>
#include <vector>

namespace lupine {

// Import math types
using math::Mat4;
using math::Vec2;
using math::Vec3;
using math::Color;
using math::AABB;
using math::OBB;

/**
 * Debug renderer - handles visualization of debug primitives, grids, gizmos, and bounding boxes.
 *
 * This is a graphics-abstracted system that works with any backend (OpenGL, Vulkan, etc.)
 * and provides immediate-mode style debug drawing for the editor.
 *
 * Features:
 * - 2D orthographic grid rendering
 * - 3D perspective grid rendering (on ZX plane)
 * - Bounding box visualization (2D and 3D)
 * - Gizmo rendering (translation, rotation, scale)
 * - Basic primitives (lines, boxes, spheres, circles, arrows)
 * - Depth testing control
 * - Persistent vs. one-frame drawing
 */
class DebugRenderer {
public:
    virtual ~DebugRenderer() = default;

    /**
     * Initialize the debug renderer with a graphics device.
     */
    virtual bool initialize(IGfxDevice* device) = 0;

    /**
     * Shutdown and release resources.
     */
    virtual void shutdown() = 0;

    // ===== Frame Management =====

    /**
     * Begin a new debug frame.
     * Call this at the start of each frame.
     */
    virtual void beginFrame() = 0;

    /**
     * End the debug frame and prepare for rendering.
     */
    virtual void endFrame() = 0;

    /**
     * Render all debug primitives for a view.
     * @param cmd Command list to record rendering commands into
     * @param viewProjection Combined view-projection matrix
     * @param lineWidth Width of debug lines (default 1.0f for grids, use larger for gizmos/boxes)
     */
    virtual void render(IGfxCommandList* cmd, const Mat4& viewProjection, float lineWidth = 1.0f) = 0;

    /**
     * Clear all debug draw commands.
     */
    virtual void clear() = 0;

    /**
     * Set the screen size for thick line rendering.
     * This is needed for geometry shader-based line expansion.
     * @param width Screen width in pixels
     * @param height Screen height in pixels
     */
    virtual void setScreenSize(float width, float height) { (void)width; (void)height; }

    /**
     * Set the line width for thick line rendering.
     * @param width Line width in pixels
     */
    virtual void setLineWidth(float width) { (void)width; }

    // ===== 2D Grid (Orthographic) =====

    /**
     * Draw a 2D orthographic grid (XY plane).
     * Typically used in 2D editor viewports.
     */
    virtual void draw2DGrid(const DebugGrid& grid) = 0;

    // ===== 3D Grid (Perspective) =====

    /**
     * Draw a 3D perspective grid on the ZX plane (ground plane).
     * Typically used in 3D editor viewports.
     */
    virtual void draw3DGrid(const DebugGrid& grid) = 0;

    // ===== Bounding Boxes =====

    /**
     * Draw a 2D bounding box (AABB in XY plane).
     */
    virtual void draw2DBoundingBox(const AABB& bounds, const Color& color, float duration = 0.0f) = 0;

    /**
     * Draw a 2D oriented bounding box (rotated rectangle in XY plane).
     * @param center Center position of the box
     * @param size Width and height of the box
     * @param rotation Rotation angle in radians
     * @param color Color of the box outline
     * @param duration How long to display (0 = one frame)
     */
    virtual void draw2DOrientedBoundingBox(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration = 0.0f) = 0;

    /**
     * Draw a 3D bounding box (AABB).
     */
    virtual void draw3DBoundingBox(const AABB& bounds, const Color& color, float duration = 0.0f) = 0;

    /**
     * Draw an oriented bounding box (rotated box).
     */
    virtual void draw3DOrientedBoundingBox(const OBB& bounds, const Color& color, float duration = 0.0f) = 0;

    // ===== Gizmos =====

    /**
     * Draw a 2D transformation gizmo.
     * Used for manipulating 2D objects in the editor.
     */
    virtual void draw2DGizmo(const DebugGizmo& gizmo) = 0;

    /**
     * Draw a 3D transformation gizmo.
     * Used for manipulating 3D objects in the editor.
     */
    virtual void draw3DGizmo(const DebugGizmo& gizmo) = 0;

    // ===== Basic Primitives =====

    /**
     * Draw a line from start to end.
     */
    virtual void drawLine(const Vec3& start, const Vec3& end, const Color& color,
                         float duration = 0.0f, bool depthTest = true) = 0;

    /**
     * Draw multiple connected lines.
     */
    virtual void drawLineStrip(const std::vector<Vec3>& points, const Color& color,
                              float duration = 0.0f, bool depthTest = true) = 0;

    /**
     * Draw a ray (origin + direction).
     */
    virtual void drawRay(const Vec3& origin, const Vec3& direction, const Color& color,
                        float length = 1000.0f, float duration = 0.0f, bool depthTest = true) = 0;

    /**
     * Draw a wireframe or filled box.
     */
    virtual void drawBox(const Vec3& center, const Vec3& size, const Color& color,
                        bool wireframe = true, float duration = 0.0f, bool depthTest = true) = 0;

    /**
     * Draw a wireframe or filled sphere.
     */
    virtual void drawSphere(const Vec3& center, float radius, const Color& color,
                           bool wireframe = true, int segments = 16, float duration = 0.0f, bool depthTest = true) = 0;

    /**
     * Draw a circle.
     */
    virtual void drawCircle(const Vec3& center, const Vec3& normal, float radius, const Color& color,
                           int segments = 32, float duration = 0.0f, bool depthTest = true) = 0;

    /**
     * Draw an arrow (directional indicator).
     */
    virtual void drawArrow(const Vec3& start, const Vec3& end, const Color& color,
                          float arrowSize = 0.2f, float duration = 0.0f, bool depthTest = true) = 0;

    /**
     * Draw 3D world-space text.
     */
    virtual void drawText(const Vec3& position, const std::string& text, const Color& color,
                         float size = 1.0f, float duration = 0.0f) = 0;

    // ===== Coordinate Axes =====

    /**
     * Draw XYZ axes at a position.
     * Red = X, Green = Y, Blue = Z
     */
    virtual void drawAxes(const Vec3& position, float size = 1.0f, float duration = 0.0f) = 0;

    /**
     * Draw a transform gizmo showing position and orientation.
     */
    virtual void drawTransform(const Vec3& position, const Mat4& rotation, float size = 1.0f, float duration = 0.0f) = 0;

    // ===== Statistics =====

    /**
     * Get the number of lines queued for rendering.
     */
    virtual size_t getLineCount() const = 0;

    /**
     * Get the number of primitives queued for rendering.
     */
    virtual size_t getPrimitiveCount() const = 0;
};

/**
 * Factory function to create a debug renderer for a specific backend.
 */
std::unique_ptr<DebugRenderer> createDebugRenderer(GraphicsBackend backend);

} // namespace lupine
