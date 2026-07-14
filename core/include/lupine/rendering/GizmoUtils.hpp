#pragma once

#include "lupine/rendering/debug/DebugDrawCommands.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {

/**
 * Gizmo hit testing result
 */
struct GizmoHitResult {
    bool hit = false;
    GizmoAxis hitAxis = GizmoAxis::None;
    GizmoType hitOperation = GizmoType::Translation;  // Which operation (for "All" mode)
    float distance = 0.0f;  // Distance to hit point (for sorting)

    // For 2D scale-rectangle handles: the local-space direction of the grabbed
    // handle, e.g. (+1,+1) for the top-right corner, (-1, 0) for the left edge.
    // A zero vector denotes a non-directional handle (uniform centre scale).
    math::Vec2 handleDir = math::Vec2(0.0f, 0.0f);
};

/**
 * A single coloured line segment emitted by the shared 2D gizmo geometry
 * builder. Backends render these through their own gizmo-category line
 * primitive so the visuals stay identical across graphics APIs.
 */
struct GizmoSegment {
    math::Vec3 start;
    math::Vec3 end;
    math::Color color;
};

/**
 * Gizmo utilities for hit testing and interaction
 */
class GizmoUtils {
public:
    /**
     * Test if a 2D screen position hits a 2D gizmo
     * @param screenPos Screen position (in viewport coordinates)
     * @param gizmoWorldPos Gizmo position in world space
     * @param gizmoSize Size of the gizmo
     * @param gizmoType Type of gizmo being displayed
     * @param viewMatrix View matrix
     * @param projMatrix Projection matrix
     * @param viewportWidth Viewport width
     * @param viewportHeight Viewport height
     * @param rotation2D Rotation angle for 2D gizmo (in radians)
     * @return Hit test result
     */
    static GizmoHitResult TestGizmo2D(
        const math::Vec2& screenPos,
        const math::Vec3& gizmoWorldPos,
        float gizmoSize,
        GizmoType gizmoType,
        const math::Mat4& viewMatrix,
        const math::Mat4& projMatrix,
        float viewportWidth,
        float viewportHeight,
        float rotation2D = 0.0f,
        const math::Vec2& halfExtents = math::Vec2(0.0f, 0.0f)
    );

    /**
     * Build the shared line-segment geometry for a 2D gizmo. Backends iterate the
     * returned segments and submit them through their own gizmo-category line
     * primitive, so the gizmo looks identical on every graphics backend.
     */
    static std::vector<GizmoSegment> BuildGizmo2DGeometry(const DebugGizmo& gizmo);

    /**
     * Recover the oriented half-extents of a rotated rectangle from its
     * axis-aligned world bounding box. Given the world AABB half-size and the
     * rectangle's rotation, this solves for the local half-extents (a, b) such
     * that rotating a rectangle of those extents by `rotation` reproduces the
     * AABB. Falls back to the AABB half-size near degenerate (±45°) angles.
     */
    static math::Vec2 SolveOrientedHalfExtents2D(const math::Vec2& aabbHalfSize, float rotation);

    /**
     * Test if a 3D ray hits a 3D gizmo
     * @param ray Ray in world space
     * @param gizmoWorldPos Gizmo position in world space
     * @param gizmoSize Size of the gizmo
     * @param gizmoType Type of gizmo being displayed
     * @param rotation3D Rotation for 3D gizmo
     * @return Hit test result
     */
    static GizmoHitResult TestGizmo3D(
        const math::Ray& ray,
        const math::Vec3& gizmoWorldPos,
        float gizmoSize,
        GizmoType gizmoType,
        const math::Quat& rotation3D = math::Quat::Identity()
    );

private:
    // Helper functions for hit testing specific gizmo components

    // 2D hit testing helpers
    static bool TestArrow2D(const math::Vec2& screenPos, const math::Vec2& arrowStart,
                           const math::Vec2& arrowEnd, float threshold);
    static bool TestCircle2D(const math::Vec2& screenPos, const math::Vec2& center,
                            float radius, float thickness);
    static bool TestScaleHandle2D(const math::Vec2& screenPos, const math::Vec2& handlePos,
                                 float handleSize);

    // 3D hit testing helpers
    static bool TestArrow3D(const math::Ray& ray, const math::Vec3& arrowStart,
                           const math::Vec3& arrowEnd, float thickness, float& distance);
    static bool TestCircle3D(const math::Ray& ray, const math::Vec3& center,
                            const math::Vec3& normal, float radius, float thickness, float& distance);
    static bool TestScaleHandle3D(const math::Ray& ray, const math::Vec3& handlePos,
                                 float handleSize, float& distance);

    // Utility functions
    static math::Vec2 WorldToScreen(const math::Vec3& worldPos, const math::Mat4& viewProj,
                                    float viewportWidth, float viewportHeight);
    static float DistancePointToLineSegment(const math::Vec2& point, const math::Vec2& lineStart,
                                           const math::Vec2& lineEnd);
};

} // namespace lupine
