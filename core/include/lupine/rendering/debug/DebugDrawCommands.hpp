#pragma once

#include "../ResourceHandles.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/AABB.hpp"
#include "lupine/math/Mat4.hpp"
#include "lupine/math/Quat.hpp"
#include <string>
#include <vector>

namespace lupine {

// Import math types
using math::Vec2;
using math::Vec3;
using math::Color;
using math::AABB;
using math::Mat4;
using math::Quat;

/**
 * Debug line - rendered as a 1-pixel line
 */
struct DebugLine {
    Vec3 start;
    Vec3 end;
    Color color;
    float thickness = 1.0f;
    float duration = 0.0f; // 0 = one frame, > 0 = persist for N seconds
    bool depthTest = true;
};

/**
 * Debug ray - infinite line in one direction
 */
struct DebugRay {
    Vec3 origin;
    Vec3 direction;
    Color color;
    float length = 1000.0f;
    float duration = 0.0f;
    bool depthTest = true;
};

/**
 * Debug box - wireframe or filled
 */
struct DebugBox {
    Vec3 center;
    Vec3 size;
    Color color;
    bool wireframe = true;
    float duration = 0.0f;
    bool depthTest = true;
};

/**
 * Debug sphere - wireframe or filled
 */
struct DebugSphere {
    Vec3 center;
    float radius;
    Color color;
    bool wireframe = true;
    int segments = 16;
    float duration = 0.0f;
    bool depthTest = true;
};

/**
 * Debug circle - 2D or 3D circle
 */
struct DebugCircle {
    Vec3 center;
    Vec3 normal; // Circle normal (for 3D orientation)
    float radius;
    Color color;
    int segments = 32;
    float duration = 0.0f;
    bool depthTest = true;
};

/**
 * Debug arrow - direction indicator
 */
struct DebugArrow {
    Vec3 start;
    Vec3 end;
    Color color;
    float arrowSize = 0.2f;
    float duration = 0.0f;
    bool depthTest = true;
};

/**
 * Debug text - 3D world-space text
 */
struct DebugText {
    Vec3 position;
    std::string text;
    Color color;
    float size = 1.0f;
    float duration = 0.0f;
};

/**
 * Gizmo type
 */
enum class GizmoType {
    Translation,  // Move gizmo (3 arrows)
    Rotation,     // Rotate gizmo (3 circles)
    Scale,        // Scale gizmo (3 boxes)
    All           // Combined gizmo
};

/**
 * Gizmo axis
 */
enum class GizmoAxis {
    None,
    X,
    Y,
    Z,
    XY,
    XZ,
    YZ,
    XYZ
};

/**
 * Transform space for gizmo operations
 */
enum class TransformSpace {
    Local,   // Transform relative to object's local axes
    Parent,  // Transform relative to parent's axes
    World    // Transform relative to world axes
};

/**
 * Debug gizmo - transformation handles
 */
struct DebugGizmo {
    Vec3 position;
    GizmoType type;
    float size = 1.0f;
    GizmoAxis highlightAxis = GizmoAxis::None; // Which axis is hovered/selected
    float rotation2D = 0.0f;  // Rotation angle for 2D gizmos (in radians)
    Quat rotation3D = Quat::Identity();  // Rotation for 3D gizmos

    // Oriented half-extents (in world units) of the selection's local bounding
    // rectangle, used by the 2D scale gizmo to draw the manipulation rectangle.
    // When zero, the scale gizmo falls back to a square derived from `size`.
    Vec2 halfExtents = Vec2(0.0f, 0.0f);
};

/**
 * Grid configuration
 */
struct DebugGrid {
    Vec3 center = Vec3(0, 0, 0);
    float cellSize = 1.0f;
    int cellCount = 20;           // Number of cells in each direction
    Color gridColor = Color(0.8f, 0.8f, 0.95f, 0.3f);
    Color axisColor = Color(0.7f, 0.7f, 0.7f, 0.6f);
    bool is3D = true;             // If false, draws 2D grid
    bool drawAxes = true;         // Draw major axes (X/Z for 3D, X/Y for 2D)
};

/**
 * Bounding box debug draw
 */
struct DebugBoundingBox {
    AABB bounds;
    Color color;
    bool is3D = true;
    float duration = 0.0f;
};

} // namespace lupine
