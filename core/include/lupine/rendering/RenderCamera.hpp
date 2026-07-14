#pragma once

#include "ResourceHandles.hpp"
#include "gfx/GfxTypes.hpp"
#include "lupine/math/Mat4.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Transform.hpp"
#include "lupine/math/Color.hpp"
#include <optional>

namespace lupine {

// Import math types into lupine namespace for rendering
using math::Mat4;
using math::Vec2;
using math::Vec3;
using math::Color;
using math::Transform;

/**
 * Camera type enumeration
 */
enum class CameraType {
    Camera2D,      // Orthographic, for 2D world (Node2D)
    Camera3D,      // Perspective or orthographic, for 3D world (Node3D)
    CameraCanvas   // Screen-space, for UI/Canvas rendering
};

/**
 * Projection type for 3D cameras
 */
enum class ProjectionType {
    Perspective,
    Orthographic
};

/**
 * Clear flags - what to clear before rendering
 */
enum class CameraClearFlags : uint32_t {
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
    All = Color | Depth | Stencil
};

inline CameraClearFlags operator|(CameraClearFlags a, CameraClearFlags b) {
    return static_cast<CameraClearFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline CameraClearFlags operator&(CameraClearFlags a, CameraClearFlags b) {
    return static_cast<CameraClearFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * Skybox data (optional)
 */
struct Skybox {
    TextureHandle cubemap;
    float intensity = 1.0f;
    float rotation = 0.0f;
};

/**
 * Base camera class.
 * All camera types inherit from this.
 */
class RenderCamera {
public:
    virtual ~RenderCamera() = default;

    virtual CameraType getType() const = 0;
    virtual Mat4 getViewMatrix() const = 0;
    virtual Mat4 getProjectionMatrix(float aspectRatio) const = 0;

    // Graphics backend (set by RenderView/RenderWorld to handle coordinate system differences)
    GraphicsBackend backend = GraphicsBackend::OpenGL;

    // Editor camera flag (set by editor to allow different rendering behavior)
    bool isEditorCamera = false;

    // Clear settings
    CameraClearFlags clearFlags = CameraClearFlags::All;
    Color clearColor = Color(0.2f, 0.2f, 0.2f, 1.0f);
    float clearDepth = 1.0f;
    uint8_t clearStencil = 0;

    // Skybox (optional)
    std::optional<Skybox> skybox;

    // Render layer mask (which layers this camera renders)
    uint32_t renderLayerMask = 0xFFFFFFFF; // All layers by default
};

/**
 * 2D Camera.
 * Orthographic projection for 2D world rendering (Node2D).
 */
class Camera2D : public RenderCamera {
public:
    Camera2D() = default;

    CameraType getType() const override { return CameraType::Camera2D; }

    Mat4 getViewMatrix() const override;
    Mat4 getProjectionMatrix(float aspectRatio) const override;

    // Position in 2D world
    Vec2 position = Vec2(0.0f, 0.0f);
    float rotation = 0.0f; // Rotation in radians

    // Zoom level (1.0 = normal, 2.0 = 2x zoom in, 0.5 = 2x zoom out)
    float zoom = 1.0f;

    // Orthographic bounds (in world units)
    float orthoSize = 10.0f; // Height of the view in world units
};

/**
 * 3D Camera.
 * Perspective or orthographic projection for 3D world rendering (Node3D).
 */
class Camera3D : public RenderCamera {
public:
    Camera3D() = default;

    CameraType getType() const override { return CameraType::Camera3D; }

    Mat4 getViewMatrix() const override;
    Mat4 getProjectionMatrix(float aspectRatio) const override;

    // Transform in 3D world
    Vec3 position = Vec3(0.0f, 0.0f, 5.0f);
    Vec3 target = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 up = Vec3(0.0f, 1.0f, 0.0f);

    // Projection settings
    ProjectionType projectionType = ProjectionType::Perspective;

    // Perspective settings
    float fov = 60.0f; // Field of view in degrees
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    // Orthographic settings (used when projectionType == Orthographic)
    float orthoSize = 10.0f; // Height of the view in world units
};

/**
 * Canvas/UI Camera.
 * Screen-space rendering for UI elements.
 * Pixel-perfect or logical coordinate rendering.
 */
class CameraCanvas : public RenderCamera {
public:
    CameraCanvas() = default;

    CameraType getType() const override { return CameraType::CameraCanvas; }

    Mat4 getViewMatrix() const override;
    Mat4 getProjectionMatrix(float aspectRatio) const override;

    // Canvas size (in logical pixels)
    Vec2 canvasSize = Vec2(1920.0f, 1080.0f);

    // Screen-space offset applied to the whole canvas (canvas pixels). Drives the
    // CameraUI node's position, shake and smoothing. Defaults to zero so internal
    // full-screen blits and existing scenes are unaffected.
    Vec2 position = Vec2(0.0f, 0.0f);

    // Origin position within the canvas (0,0 = top-left, 0.5,0.5 = centered, 1,1 = bottom-right).
    // Kept at top-left here so internal full-screen texture blits (renderTexturedQuad) are
    // unaffected; the user-facing CameraUI node defaults to a centered origin and propagates it.
    Vec2 origin = Vec2(0.0f, 0.0f);

    // Scale factor for HiDPI displays
    float scaleFactor = 1.0f;

    // Rotation of the whole canvas about its origin, in radians. Drives the
    // CameraUI node's rotation property. Zero by default so internal full-screen
    // blits and existing scenes keep an identity view.
    float rotation = 0.0f;

    // Uniform zoom of the whole canvas about its origin (1 = normal, 2 = 2x in,
    // 0.5 = 2x out). Drives the CameraUI node's zoom property. Defaults to 1 so
    // internal full-screen blits keep an identity view.
    float zoom = 1.0f;

    // Whether to use pixel-perfect rendering
    bool pixelPerfect = false;

    // When true the canvas uses a Y-up convention (+Y is up on every backend),
    // matching Camera2D and the centered UIControl layout, so UI renders right-side
    // up and coincides with the editor view. When false (default) it keeps the
    // legacy top-left screen convention (Y=0 at top, +Y down) used by internal
    // full-screen blits (renderTexturedQuad). The user-facing CameraUI sets this true.
    bool yUp = false;
};

} // namespace lupine
