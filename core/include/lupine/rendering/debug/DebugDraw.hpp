#pragma once

#include "lupine/rendering/debug/DebugRenderer.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/AABB.hpp"
#include "lupine/math/OBB.hpp"
#include "lupine/math/Mat4.hpp"
#include <memory>

namespace lupine {

// Import math types
using math::Vec2;
using math::Vec3;
using math::Color;
using math::AABB;
using math::Mat4;

/**
 * DebugDraw - Global debug drawing API for editor visualization
 * 
 * This provides a simple, globally accessible API for nodes and components
 * to draw debug visualization in the editor (cameras, lights, collision shapes, etc.)
 * 
 * Usage:
 *   DebugDraw::Line(start, end, Color::Red());
 *   DebugDraw::Box(center, size, Color::Green(), true);
 *   DebugDraw::Sphere(position, radius, Color::Blue());
 * 
 * Note: This only works in editor mode. In runtime builds, these calls are no-ops.
 */
class DebugDraw {
public:
    // ===== Initialization (called by RenderWorld) =====
    
    /**
     * Set the active debug renderer.
     * Called by RenderWorld at the start of debug overlay rendering.
     */
    static void SetRenderer(DebugRenderer* renderer);
    
    /**
     * Clear the active debug renderer.
     * Called by RenderWorld at the end of debug overlay rendering.
     */
    static void ClearRenderer();
    
    /**
     * Check if debug drawing is currently available.
     */
    static bool IsAvailable();

    /**
     * Set the current camera type for context-aware debug rendering.
     * Called by RenderWorld before rendering scene debug overlays.
     */
    static void SetCurrentCameraType(CameraType cameraType);

    /**
     * Get the current camera type.
     */
    static CameraType GetCurrentCameraType();

    /**
     * Set the current render view for context-aware debug rendering.
     * Called by RenderWorld before rendering scene debug overlays.
     */
    static void SetCurrentRenderView(class RenderView* view);

    /**
     * Check if collision shapes rendering is enabled in the current view.
     */
    static bool IsCollisionShapesEnabled();

    // ===== Drawing Functions =====
    
    /**
     * Draw a line from start to end.
     */
    static void Line(const Vec3& start, const Vec3& end, const Color& color,
                     float duration = 0.0f, bool depthTest = true);
    
    /**
     * Draw multiple connected lines.
     */
    static void LineStrip(const std::vector<Vec3>& points, const Color& color,
                          float duration = 0.0f, bool depthTest = true);
    
    /**
     * Draw a ray (origin + direction).
     */
    static void Ray(const Vec3& origin, const Vec3& direction, const Color& color,
                    float length = 1000.0f, float duration = 0.0f, bool depthTest = true);
    
    /**
     * Draw a wireframe or filled box.
     */
    static void Box(const Vec3& center, const Vec3& size, const Color& color,
                    bool wireframe = true, float duration = 0.0f, bool depthTest = true);
    
    /**
     * Draw a wireframe or filled sphere.
     */
    static void Sphere(const Vec3& center, float radius, const Color& color,
                       bool wireframe = true, int segments = 16, float duration = 0.0f, bool depthTest = true);
    
    /**
     * Draw a circle.
     */
    static void Circle(const Vec3& center, const Vec3& normal, float radius, const Color& color,
                       int segments = 32, float duration = 0.0f, bool depthTest = true);
    
    /**
     * Draw an arrow (directional indicator).
     */
    static void Arrow(const Vec3& start, const Vec3& end, const Color& color,
                      float arrowSize = 0.2f, float duration = 0.0f, bool depthTest = true);
    
    /**
     * Draw 3D world-space text.
     */
    static void Text(const Vec3& position, const std::string& text, const Color& color,
                     float size = 1.0f, float duration = 0.0f);
    
    /**
     * Draw XYZ axes at a position.
     * Red = X, Green = Y, Blue = Z
     */
    static void Axes(const Vec3& position, float size = 1.0f, float duration = 0.0f);
    
    /**
     * Draw a transform gizmo showing position and orientation.
     */
    static void Transform(const Vec3& position, const Mat4& rotation, float size = 1.0f, float duration = 0.0f);
    
    /**
     * Draw a 2D bounding box (AABB in XY plane).
     */
    static void BoundingBox2D(const AABB& bounds, const Color& color, float duration = 0.0f);

    /**
     * Draw a 2D oriented bounding box (rotated rectangle in XY plane).
     */
    static void OrientedBox2D(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration = 0.0f);

    /**
     * Draw a 3D bounding box (AABB).
     */
    static void BoundingBox3D(const AABB& bounds, const Color& color, float duration = 0.0f);

    /**
     * Draw a 3D oriented bounding box (OBB).
     */
    static void OrientedBox3D(const class OBB& obb, const Color& color, float duration = 0.0f);

private:
    static DebugRenderer* s_ActiveRenderer;
    static CameraType s_CurrentCameraType;
    static class RenderView* s_CurrentRenderView;
};

} // namespace lupine

