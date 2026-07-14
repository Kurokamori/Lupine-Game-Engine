#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/rendering/gfx/GfxTypes.hpp"
#include "lupine/math/Camera.hpp"
#include "lupine/math/MathCommon.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {

Mat4 Camera2D::getViewMatrix() const {

    // Rotate by +rotation (not -rotation) so a positive camera rotation turns the
    // visible scene in the same, expected direction.
    Mat4 translation = Mat4::Translate(Vec3(-position.x, -position.y, 0.0f));
    Mat4 rotationMat = Mat4::Rotate(rotation, Vec3(0.0f, 0.0f, 1.0f));
    Mat4 zoomMat = Mat4::Scale(Vec3(zoom, zoom, 1.0f));

    return zoomMat * rotationMat * translation;
}

Mat4 Camera2D::getProjectionMatrix(float aspectRatio) const {

    float halfHeight = orthoSize * 0.5f;
    float halfWidth = halfHeight * aspectRatio;

    // Y-flip is handled by the Vulkan viewport (negative height)
    // Use zero-to-one depth range for DirectX/Vulkan/Metal, negative-one-to-one for OpenGL
    if (IsZeroToOneDepth(backend)) {
        return math::Camera::OrthographicZO(
            -halfWidth, halfWidth,
            -halfHeight, halfHeight,
            -1.0f, 1.0f
        );
    } else {
        return math::Camera::Orthographic(
            -halfWidth, halfWidth,
            -halfHeight, halfHeight,
            -1.0f, 1.0f
        );
    }
}

Mat4 Camera3D::getViewMatrix() const {
    return math::Camera::LookAt(position, target, up);
}

Mat4 Camera3D::getProjectionMatrix(float aspectRatio) const {
    // Y-flip is handled by the Vulkan viewport (negative height)
    // Use zero-to-one depth range for DirectX/Vulkan/Metal, negative-one-to-one for OpenGL
    bool useZO = IsZeroToOneDepth(backend);

    // Debug: Log the backend being used
    static bool loggedOnce = false;
    if (!loggedOnce) {
        
        loggedOnce = true;
    }

    if (projectionType == ProjectionType::Perspective) {
        if (useZO) {
            return math::Camera::PerspectiveDegZO(fov, aspectRatio, nearPlane, farPlane);
        } else {
            return math::Camera::PerspectiveDeg(fov, aspectRatio, nearPlane, farPlane);
        }
    } else {
        float halfHeight = orthoSize * 0.5f;
        float halfWidth = halfHeight * aspectRatio;
        if (useZO) {
            return math::Camera::OrthographicZO(
                -halfWidth, halfWidth,
                -halfHeight, halfHeight,
                nearPlane, farPlane
            );
        } else {
            return math::Camera::Orthographic(
                -halfWidth, halfWidth,
                -halfHeight, halfHeight,
                nearPlane, farPlane
            );
        }
    }
}

Mat4 CameraCanvas::getViewMatrix() const {
    // Transform the whole canvas by the camera position (screen shake, UI camera
    // movement, slide transitions), rotation and scale, all about the canvas origin.
    // Defaults (zero position/rotation, unit scale) keep this an identity view so
    // internal full-screen blits and unconfigured scenes are unaffected.
    bool noTranslate = (position.x == 0.0f && position.y == 0.0f);
    bool noRotate = (rotation == 0.0f);
    bool noZoom = (zoom == 1.0f);
    if (noTranslate && noRotate && noZoom) {
        return Mat4::Identity();
    }

    // Rotate by +rotation (matching Camera2D) so a positive value turns the UI in the
    // expected direction; zoom magnifies uniformly about the canvas origin.
    Mat4 translation = Mat4::Translate(Vec3(-position.x, -position.y, 0.0f));
    Mat4 rotationMat = Mat4::Rotate(rotation, Vec3(0.0f, 0.0f, 1.0f));
    Mat4 zoomMat = Mat4::Scale(Vec3(zoom, zoom, 1.0f));

    return zoomMat * rotationMat * translation;
}

Mat4 CameraCanvas::getProjectionMatrix(float) const {

    float width = canvasSize.x;
    float height = canvasSize.y;

    float left = -origin.x * width;
    float right = left + width;
    float top = -origin.y * height;
    float bottom = top + height;

    if (scaleFactor != 1.0f) {
        left /= scaleFactor;
        right /= scaleFactor;
        top /= scaleFactor;
        bottom /= scaleFactor;
    }

    if (pixelPerfect) {
        left = math::Floor(left);
        right = math::Floor(right);
        top = math::Floor(top);
        bottom = math::Floor(bottom);
    }

    bool useZO = IsZeroToOneDepth(backend);

    // A Y-up canvas (the user-facing UI canvas) matches Camera2D and the centered
    // UIControl layout: +Y is up on every backend. glm::ortho maps its 'bottom'
    // argument to NDC -1 and 'top' to +1, and both GL and DX viewports place NDC +1
    // at the visual top. Passing (top, bottom) therefore sends the smaller-Y 'top'
    // edge to -1 and the larger-Y 'bottom' edge to +1, so world +Y renders upward —
    // no per-backend swap (identical mapping to Camera2D).
    if (yUp) {
        if (useZO) {
            return math::Camera::OrthographicZO(left, right, top, bottom, -1.0f, 1.0f);
        } else {
            return math::Camera::Orthographic(left, right, top, bottom, -1.0f, 1.0f);
        }
    }

    // Legacy top-left screen convention (Y=0 at top, +Y down), used by internal
    // full-screen blits (renderTexturedQuad). glm::ortho expects (left, right,
    // bottom_y, top_y, near, far).
    // - OpenGL/WebGL: bottom-left framebuffer origin, swap to get Y=0 at top.
    // - Vulkan/DX11/DX12/Metal: top-left origin, no swap needed.
    bool needsSwap = (backend == GraphicsBackend::OpenGL || backend == GraphicsBackend::WebGL);

    if (needsSwap) {
        if (useZO) {
            return math::Camera::OrthographicZO(left, right, bottom, top, -1.0f, 1.0f);
        } else {
            return math::Camera::Orthographic(left, right, bottom, top, -1.0f, 1.0f);
        }
    } else {
        if (useZO) {
            return math::Camera::OrthographicZO(left, right, top, bottom, -1.0f, 1.0f);
        } else {
            return math::Camera::Orthographic(left, right, top, bottom, -1.0f, 1.0f);
        }
    }
}

}

