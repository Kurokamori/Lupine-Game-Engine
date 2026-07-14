
#include "rendering/lc_camera.h"
#include "../core/lc_internal.h"

#include <lupine/core/CameraNodes.hpp>
#include <lupine/core/SceneManager.hpp>
#include <lupine/core/Scene.hpp>
#include <lupine/core/Node.hpp>
#include <lupine/scripting/ScriptAPI.hpp>

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

/* ============================================================================
 * Camera2D Functions
 * ============================================================================ */

}
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

/* ============================================================================
 * CameraUI Functions
 * ============================================================================ */

}
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

LC_API LCResult lc_camera_ui_get_rotation(LCNodeHandle camera, float* out_rotation) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_rotation) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        *out_rotation = cameraUI->GetRotation();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_set_rotation(LCNodeHandle camera, float rotation) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        cameraUI->SetRotation(rotation);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_get_zoom(LCNodeHandle camera, float* out_zoom) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;
    if (!out_zoom) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        *out_zoom = cameraUI->GetZoom();
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_camera_ui_set_zoom(LCNodeHandle camera, float zoom) {
    if (!IsValidHandle(camera)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(camera);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto cameraUI = std::dynamic_pointer_cast<lupine::core::CameraUI>(nodePtr);
        if (!cameraUI) return LC_ERROR_NODE_INVALID_TYPE;

        cameraUI->SetZoom(zoom);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Screen <-> World Conversion
 * ============================================================================ */

namespace {

// Build a ScriptAPI bound to the current scene's root node so the screen<->world
// conversions resolve against the same active camera and viewport the runtime
// uses. Returns false when there is no current scene to resolve against.
bool MakeSceneScriptAPI(lupine::scripting::ScriptAPI& api) {
    lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
    if (!sceneManager) {
        return false;
    }
    lupine::core::Scene* scene = sceneManager->GetCurrentScene();
    if (!scene) {
        return false;
    }
    std::shared_ptr<lupine::core::Node> root = scene->GetRoot();
    if (!root) {
        return false;
    }
    api.SetSceneManager(sceneManager);
    api.SetOwner(root.get());
    return true;
}

} // anonymous namespace

LC_API LCResult lc_screen_to_world_2d(LCVec2 screen_pos, LCVec2* out_world) {
    if (!out_world) {
        SetCameraError(LC_ERROR_NULL_POINTER, "out_world is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::scripting::ScriptAPI api;
        if (!MakeSceneScriptAPI(api)) {
            SetCameraError(LC_ERROR_NOT_FOUND, "no current scene to resolve against");
            return LC_ERROR_NOT_FOUND;
        }
        lupine::math::Vec2 result = api.ScreenToWorld2D(lupine::math::Vec2(screen_pos.x, screen_pos.y));
        out_world->x = result.x;
        out_world->y = result.y;
        return LC_SUCCESS;
    } catch (...) {
        SetCameraError(LC_ERROR_INTERNAL_ERROR, "lc_screen_to_world_2d failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_world_to_screen_2d(LCVec2 world_pos, LCVec2* out_screen) {
    if (!out_screen) {
        SetCameraError(LC_ERROR_NULL_POINTER, "out_screen is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::scripting::ScriptAPI api;
        if (!MakeSceneScriptAPI(api)) {
            SetCameraError(LC_ERROR_NOT_FOUND, "no current scene to resolve against");
            return LC_ERROR_NOT_FOUND;
        }
        lupine::math::Vec2 result = api.WorldToScreen2D(lupine::math::Vec2(world_pos.x, world_pos.y));
        out_screen->x = result.x;
        out_screen->y = result.y;
        return LC_SUCCESS;
    } catch (...) {
        SetCameraError(LC_ERROR_INTERNAL_ERROR, "lc_world_to_screen_2d failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_screen_to_world_3d(LCVec2 screen_pos, float distance, LCVec3* out_world) {
    if (!out_world) {
        SetCameraError(LC_ERROR_NULL_POINTER, "out_world is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::scripting::ScriptAPI api;
        if (!MakeSceneScriptAPI(api)) {
            SetCameraError(LC_ERROR_NOT_FOUND, "no current scene to resolve against");
            return LC_ERROR_NOT_FOUND;
        }
        lupine::math::Vec3 result = api.ScreenToWorld3D(lupine::math::Vec2(screen_pos.x, screen_pos.y), distance);
        out_world->x = result.x;
        out_world->y = result.y;
        out_world->z = result.z;
        return LC_SUCCESS;
    } catch (...) {
        SetCameraError(LC_ERROR_INTERNAL_ERROR, "lc_screen_to_world_3d failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_world_to_screen_3d(LCVec3 world_pos, LCVec3* out_screen) {
    if (!out_screen) {
        SetCameraError(LC_ERROR_NULL_POINTER, "out_screen is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::scripting::ScriptAPI api;
        if (!MakeSceneScriptAPI(api)) {
            SetCameraError(LC_ERROR_NOT_FOUND, "no current scene to resolve against");
            return LC_ERROR_NOT_FOUND;
        }
        lupine::math::Vec3 result = api.WorldToScreen3D(lupine::math::Vec3(world_pos.x, world_pos.y, world_pos.z));
        out_screen->x = result.x;
        out_screen->y = result.y;
        out_screen->z = result.z;
        return LC_SUCCESS;
    } catch (...) {
        SetCameraError(LC_ERROR_INTERNAL_ERROR, "lc_world_to_screen_3d failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_screen_to_world_ray_3d(LCVec2 screen_pos, LCVec3* out_origin, LCVec3* out_direction) {
    if (!out_origin || !out_direction) {
        SetCameraError(LC_ERROR_NULL_POINTER, "out_origin or out_direction is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::scripting::ScriptAPI api;
        if (!MakeSceneScriptAPI(api)) {
            SetCameraError(LC_ERROR_NOT_FOUND, "no current scene to resolve against");
            return LC_ERROR_NOT_FOUND;
        }
        lupine::scripting::ScriptAPI::ScreenRay ray =
            api.ScreenToWorldRay3D(lupine::math::Vec2(screen_pos.x, screen_pos.y));
        out_origin->x = ray.origin.x;
        out_origin->y = ray.origin.y;
        out_origin->z = ray.origin.z;
        out_direction->x = ray.direction.x;
        out_direction->y = ray.direction.y;
        out_direction->z = ray.direction.z;
        return LC_SUCCESS;
    } catch (...) {
        SetCameraError(LC_ERROR_INTERNAL_ERROR, "lc_screen_to_world_ray_3d failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}