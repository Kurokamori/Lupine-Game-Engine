#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/rendering/gfx/GfxTypes.hpp"
#include "lupine/math/Camera.hpp"
#include "lupine/math/MathCommon.hpp"

namespace lupine {

Mat4 Camera2D::getViewMatrix() const {

    Mat4 translation = Mat4::Translate(Vec3(-position.x, -position.y, 0.0f));
    Mat4 rotationMat = Mat4::Rotate(-rotation, Vec3(0.0f, 0.0f, 1.0f));
    Mat4 zoomMat = Mat4::Scale(Vec3(zoom, zoom, 1.0f));

    return zoomMat * rotationMat * translation;
}

Mat4 Camera2D::getProjectionMatrix(float aspectRatio) const {

    float halfHeight = orthoSize * 0.5f;
    float halfWidth = halfHeight * aspectRatio;

    return math::Camera::Orthographic(
        -halfWidth, halfWidth,
        -halfHeight, halfHeight,
        -1.0f, 1.0f
    );
}

Mat4 Camera3D::getViewMatrix() const {
    return math::Camera::LookAt(position, target, up);
}

Mat4 Camera3D::getProjectionMatrix(float aspectRatio) const {
    if (projectionType == ProjectionType::Perspective) {

        return math::Camera::PerspectiveDeg(fov, aspectRatio, nearPlane, farPlane);
    } else {

        float halfHeight = orthoSize * 0.5f;
        float halfWidth = halfHeight * aspectRatio;

        return math::Camera::Orthographic(
            -halfWidth, halfWidth,
            -halfHeight, halfHeight,
            nearPlane, farPlane
        );
    }
}

Mat4 CameraCanvas::getViewMatrix() const {

    return Mat4::Identity();
}

Mat4 CameraCanvas::getProjectionMatrix(float aspectRatio) const {

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

    if (IsBottomLeftOrigin(backend)) {

        return math::Camera::Orthographic(left, right, top, bottom, -1.0f, 1.0f);
    } else {

        return math::Camera::Orthographic(left, right, bottom, top, -1.0f, 1.0f);
    }
}

}

