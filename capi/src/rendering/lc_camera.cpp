
#include "rendering/lc_camera.h"
#include "lc_internal.h"

#include <lupine\core\CameraNodes.hpp>

#include <memory>

namespace {

void SetCameraError(LCResult code, const char* message) {
    ::SetError(code, message);
}

} // anonymous namespace


/* ============================================================================
 * Camera3D Functions
 * ============================================================================ */

LC_API LCResult lc_camera3d_create(const char* name, LCNodeHandle* out_camera) {
    if (!out_camera) {
        SetCameraError(LC_ERROR_NULL_POINTER, "out_camera is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string cameraName = name ? name : "";
        auto camera = std::make_shared<lupine::core::Camera3D>(cameraName);
        *out_camera = CreateHandle(camera);
        return LC_SUCCESS;
    } catch (...) {
        SetCameraError(LC_ERROR_INTERNAL_ERROR, "Failed to create Camera3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_get_projection_type(LCNodeHandle camera, LCCameraProjectionType* out_type) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_type) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        auto projType = camera3d->GetProjectionType();
        *out_type = (projType == lupine::core::Camera3D::ProjectionType::Perspective)
                    ? LC_CAMERA_PROJECTION_PERSPECTIVE
                    : LC_CAMERA_PROJECTION_ORTHOGRAPHIC;
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_set_projection_type(LCNodeHandle camera, LCCameraProjectionType type) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        auto projType = (type == LC_CAMERA_PROJECTION_PERSPECTIVE)
                        ? lupine::core::Camera3D::ProjectionType::Perspective
                        : lupine::core::Camera3D::ProjectionType::Orthographic;
        camera3d->SetProjectionType(projType);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_get_fov(LCNodeHandle camera, float* out_fov) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_fov) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_fov = camera3d->GetFOV();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_set_fov(LCNodeHandle camera, float fov) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        camera3d->SetFOV(fov);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_get_near_plane(LCNodeHandle camera, float* out_near) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_near) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_near = camera3d->GetNearPlane();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_set_near_plane(LCNodeHandle camera, float near_plane) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        camera3d->SetNearPlane(near_plane);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_get_far_plane(LCNodeHandle camera, float* out_far) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_far) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_far = camera3d->GetFarPlane();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_set_far_plane(LCNodeHandle camera, float far_plane) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        camera3d->SetFarPlane(far_plane);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_get_ortho_size(LCNodeHandle camera, float* out_size) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_size) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_size = camera3d->GetOrthoSize();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_set_ortho_size(LCNodeHandle camera, float size) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        camera3d->SetOrthoSize(size);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_is_active(LCNodeHandle camera, bool* out_active) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_active) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_active = camera3d->IsActive();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera3d_set_active(LCNodeHandle camera, bool active) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera3d = std::dynamic_pointer_cast<lupine::core::Camera3D>(nodePtr);
        if (!camera3d) return LC_ERROR_NODE_INVALID_TYPE;

        camera3d->SetActive(active);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Camera2D Functions
 * ============================================================================ */

LC_API LCResult lc_camera2d_create(const char* name, LCNodeHandle* out_camera) {
    if (!out_camera) {
        SetCameraError(LC_ERROR_NULL_POINTER, "out_camera is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string cameraName = name ? name : "";
        auto camera = std::make_shared<lupine::core::Camera2D>(cameraName);
        *out_camera = CreateHandle(camera);
        return LC_SUCCESS;
    } catch (...) {
        SetCameraError(LC_ERROR_INTERNAL_ERROR, "Failed to create Camera2D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera2d_get_zoom(LCNodeHandle camera, float* out_zoom) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_zoom) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera2d = std::dynamic_pointer_cast<lupine::core::Camera2D>(nodePtr);
        if (!camera2d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_zoom = camera2d->GetZoom();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera2d_set_zoom(LCNodeHandle camera, float zoom) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera2d = std::dynamic_pointer_cast<lupine::core::Camera2D>(nodePtr);
        if (!camera2d) return LC_ERROR_NODE_INVALID_TYPE;

        camera2d->SetZoom(zoom);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera2d_get_ortho_size(LCNodeHandle camera, float* out_size) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_size) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera2d = std::dynamic_pointer_cast<lupine::core::Camera2D>(nodePtr);
        if (!camera2d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_size = camera2d->GetOrthoSize();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera2d_set_ortho_size(LCNodeHandle camera, float size) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera2d = std::dynamic_pointer_cast<lupine::core::Camera2D>(nodePtr);
        if (!camera2d) return LC_ERROR_NODE_INVALID_TYPE;

        camera2d->SetOrthoSize(size);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera2d_get_aspect_ratio(LCNodeHandle camera, float* out_aspect) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_aspect) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera2d = std::dynamic_pointer_cast<lupine::core::Camera2D>(nodePtr);
        if (!camera2d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_aspect = camera2d->GetAspectRatio();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera2d_set_aspect_ratio(LCNodeHandle camera, float aspect_ratio) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera2d = std::dynamic_pointer_cast<lupine::core::Camera2D>(nodePtr);
        if (!camera2d) return LC_ERROR_NODE_INVALID_TYPE;

        camera2d->SetAspectRatio(aspect_ratio);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera2d_is_active(LCNodeHandle camera, bool* out_active) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_active) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera2d = std::dynamic_pointer_cast<lupine::core::Camera2D>(nodePtr);
        if (!camera2d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_active = camera2d->IsActive();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera2d_set_active(LCNodeHandle camera, bool active) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto camera2d = std::dynamic_pointer_cast<lupine::core::Camera2D>(nodePtr);
        if (!camera2d) return LC_ERROR_NODE_INVALID_TYPE;

        camera2d->SetActive(active);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * CameraUI Functions
 * ============================================================================ */

LC_API LCResult lc_camera_ui_create(const char* name, LCNodeHandle* out_camera) {
    if (!out_camera) {
        SetCameraError(LC_ERROR_NULL_POINTER, "out_camera is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string cameraName = name ? name : "";
        auto camera = std::make_shared<lupine::core::CameraUI>(cameraName);
        *out_camera = CreateHandle(camera);
        return LC_SUCCESS;
    } catch (...) {
        SetCameraError(LC_ERROR_INTERNAL_ERROR, "Failed to create CameraUI");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_get_canvas_size(LCNodeHandle camera, LCVec2* out_size) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_size) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& size = cameraUI->GetCanvasSize();
        out_size->x = size.x;
        out_size->y = size.y;
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_set_canvas_size(LCNodeHandle camera, LCVec2 size) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        cameraUI->SetCanvasSize(lupine::math::Vec2(size.x, size.y));
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_get_origin(LCNodeHandle camera, LCVec2* out_origin) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_origin) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& origin = cameraUI->GetOrigin();
        out_origin->x = origin.x;
        out_origin->y = origin.y;
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_set_origin(LCNodeHandle camera, LCVec2 origin) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        cameraUI->SetOrigin(lupine::math::Vec2(origin.x, origin.y));
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_get_scale_factor(LCNodeHandle camera, float* out_factor) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_factor) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        *out_factor = cameraUI->GetScaleFactor();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_set_scale_factor(LCNodeHandle camera, float factor) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        cameraUI->SetScaleFactor(factor);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_is_pixel_perfect(LCNodeHandle camera, bool* out_pixel_perfect) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_pixel_perfect) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        *out_pixel_perfect = cameraUI->IsPixelPerfect();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_set_pixel_perfect(LCNodeHandle camera, bool pixel_perfect) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        cameraUI->SetPixelPerfect(pixel_perfect);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_is_active(LCNodeHandle camera, bool* out_active) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_active) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        *out_active = cameraUI->IsActive();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_set_active(LCNodeHandle camera, bool active) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        cameraUI->SetActive(active);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_get_position(LCNodeHandle camera, LCVec2* out_position) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_position) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& pos = cameraUI->GetPosition();
        out_position->x = pos.x;
        out_position->y = pos.y;
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_set_position(LCNodeHandle camera, LCVec2 position) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        cameraUI->SetPosition(lupine::math::Vec2(position.x, position.y));
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

