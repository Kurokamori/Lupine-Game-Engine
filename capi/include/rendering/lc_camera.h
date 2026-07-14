/**
 * @file lc_camera.h
 * @brief Lupine Engine C API - Camera management
 *
 * This header provides camera functionality for 2D, 3D, and UI rendering.
 */

#ifndef LUPINE_CAPI_CAMERA_H
#define LUPINE_CAPI_CAMERA_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"


/* ============================================================================
 * Camera Types
 * ============================================================================ */

/**
 * @brief Camera projection types
 */
typedef enum {
    LC_CAMERA_PROJECTION_PERSPECTIVE,
    LC_CAMERA_PROJECTION_ORTHOGRAPHIC
} LCCameraProjectionType;

/* ============================================================================
 * Camera3D Functions
 * ============================================================================ */

/**
 * @brief Create a Camera3D node
 * @param name Optional name for the camera (can be NULL)
 * @param out_camera Output parameter for the created camera node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_create(const char* name, LCNodeHandle* out_camera);

/**
 * @brief Get the projection type of a Camera3D
 * @param camera Camera3D node handle
 * @param out_type Output parameter for projection type
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_get_projection_type(LCNodeHandle camera, LCCameraProjectionType* out_type);

/**
 * @brief Set the projection type of a Camera3D
 * @param camera Camera3D node handle
 * @param type Projection type to set
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_set_projection_type(LCNodeHandle camera, LCCameraProjectionType type);

/**
 * @brief Get the field of view of a Camera3D (in degrees)
 * @param camera Camera3D node handle
 * @param out_fov Output parameter for FOV
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_get_fov(LCNodeHandle camera, float* out_fov);

/**
 * @brief Set the field of view of a Camera3D (in degrees)
 * @param camera Camera3D node handle
 * @param fov Field of view in degrees
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_set_fov(LCNodeHandle camera, float fov);

/**
 * @brief Get the near plane distance of a Camera3D
 * @param camera Camera3D node handle
 * @param out_near Output parameter for near plane
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_get_near_plane(LCNodeHandle camera, float* out_near);

/**
 * @brief Set the near plane distance of a Camera3D
 * @param camera Camera3D node handle
 * @param near_plane Near plane distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_set_near_plane(LCNodeHandle camera, float near_plane);

/**
 * @brief Get the far plane distance of a Camera3D
 * @param camera Camera3D node handle
 * @param out_far Output parameter for far plane
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_get_far_plane(LCNodeHandle camera, float* out_far);

/**
 * @brief Set the far plane distance of a Camera3D
 * @param camera Camera3D node handle
 * @param far_plane Far plane distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_set_far_plane(LCNodeHandle camera, float far_plane);

/**
 * @brief Get the orthographic size of a Camera3D (height in world units)
 * @param camera Camera3D node handle
 * @param out_size Output parameter for ortho size
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_get_ortho_size(LCNodeHandle camera, float* out_size);

/**
 * @brief Set the orthographic size of a Camera3D (height in world units)
 * @param camera Camera3D node handle
 * @param size Orthographic size
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_set_ortho_size(LCNodeHandle camera, float size);

/**
 * @brief Check if a Camera3D is active
 * @param camera Camera3D node handle
 * @param out_active Output parameter for active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_is_active(LCNodeHandle camera, bool* out_active);

/**
 * @brief Set whether a Camera3D is active
 * @param camera Camera3D node handle
 * @param active Active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera3d_set_active(LCNodeHandle camera, bool active);

/* ============================================================================
 * Camera2D Functions
 * ============================================================================ */

/**
 * @brief Create a Camera2D node
 * @param name Optional name for the camera (can be NULL)
 * @param out_camera Output parameter for the created camera node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_create(const char* name, LCNodeHandle* out_camera);

/**
 * @brief Get the zoom level of a Camera2D
 * @param camera Camera2D node handle
 * @param out_zoom Output parameter for zoom level
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_get_zoom(LCNodeHandle camera, float* out_zoom);

/**
 * @brief Set the zoom level of a Camera2D
 * @param camera Camera2D node handle
 * @param zoom Zoom level (1.0 = normal, 2.0 = 2x zoom in, 0.5 = 2x zoom out)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_set_zoom(LCNodeHandle camera, float zoom);

/**
 * @brief Get the orthographic size of a Camera2D
 * @param camera Camera2D node handle
 * @param out_size Output parameter for ortho size
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_get_ortho_size(LCNodeHandle camera, float* out_size);

/**
 * @brief Set the orthographic size of a Camera2D
 * @param camera Camera2D node handle
 * @param size Orthographic size (height in world units)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_set_ortho_size(LCNodeHandle camera, float size);

/**
 * @brief Get the aspect ratio of a Camera2D
 * @param camera Camera2D node handle
 * @param out_aspect Output parameter for aspect ratio
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_get_aspect_ratio(LCNodeHandle camera, float* out_aspect);

/**
 * @brief Set the aspect ratio of a Camera2D
 * @param camera Camera2D node handle
 * @param aspect_ratio Aspect ratio (width / height)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_set_aspect_ratio(LCNodeHandle camera, float aspect_ratio);

/**
 * @brief Check if a Camera2D is active
 * @param camera Camera2D node handle
 * @param out_active Output parameter for active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_is_active(LCNodeHandle camera, bool* out_active);

/**
 * @brief Set whether a Camera2D is active
 * @param camera Camera2D node handle
 * @param active Active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera2d_set_active(LCNodeHandle camera, bool active);

/* ============================================================================
 * CameraUI Functions
 * ============================================================================ */

/**
 * @brief Create a CameraUI node
 * @param name Optional name for the camera (can be NULL)
 * @param out_camera Output parameter for the created camera node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_create(const char* name, LCNodeHandle* out_camera);

/**
 * @brief Get the canvas size of a CameraUI
 * @param camera CameraUI node handle
 * @param out_size Output parameter for canvas size
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_get_canvas_size(LCNodeHandle camera, LCVec2* out_size);

/**
 * @brief Set the canvas size of a CameraUI
 * @param camera CameraUI node handle
 * @param size Canvas size in logical pixels
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_set_canvas_size(LCNodeHandle camera, LCVec2 size);

/**
 * @brief Get the origin position of a CameraUI
 * @param camera CameraUI node handle
 * @param out_origin Output parameter for origin (0,0 = top-left, 1,1 = bottom-right)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_get_origin(LCNodeHandle camera, LCVec2* out_origin);

/**
 * @brief Set the origin position of a CameraUI
 * @param camera CameraUI node handle
 * @param origin Origin position (0,0 = top-left, 1,1 = bottom-right)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_set_origin(LCNodeHandle camera, LCVec2 origin);

/**
 * @brief Get the scale factor of a CameraUI (for HiDPI)
 * @param camera CameraUI node handle
 * @param out_factor Output parameter for scale factor
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_get_scale_factor(LCNodeHandle camera, float* out_factor);

/**
 * @brief Set the scale factor of a CameraUI (for HiDPI)
 * @param camera CameraUI node handle
 * @param factor Scale factor
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_set_scale_factor(LCNodeHandle camera, float factor);

/**
 * @brief Check if a CameraUI has pixel-perfect rendering
 * @param camera CameraUI node handle
 * @param out_pixel_perfect Output parameter for pixel-perfect state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_is_pixel_perfect(LCNodeHandle camera, bool* out_pixel_perfect);

/**
 * @brief Set whether a CameraUI has pixel-perfect rendering
 * @param camera CameraUI node handle
 * @param pixel_perfect Pixel-perfect state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_set_pixel_perfect(LCNodeHandle camera, bool pixel_perfect);

/**
 * @brief Check if a CameraUI is active
 * @param camera CameraUI node handle
 * @param out_active Output parameter for active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_is_active(LCNodeHandle camera, bool* out_active);

/**
 * @brief Set whether a CameraUI is active
 * @param camera CameraUI node handle
 * @param active Active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_set_active(LCNodeHandle camera, bool active);

/**
 * @brief Get the screen-space position of a CameraUI
 * @param camera CameraUI node handle
 * @param out_position Output parameter for position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_get_position(LCNodeHandle camera, LCVec2* out_position);

/**
 * @brief Set the screen-space position of a CameraUI
 * @param camera CameraUI node handle
 * @param position Screen-space position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_set_position(LCNodeHandle camera, LCVec2 position);

/**
 * @brief Get the canvas rotation of a CameraUI (radians, about the origin)
 * @param camera CameraUI node handle
 * @param out_rotation Output parameter for rotation in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_get_rotation(LCNodeHandle camera, float* out_rotation);

/**
 * @brief Set the canvas rotation of a CameraUI (radians, about the origin)
 * @param camera CameraUI node handle
 * @param rotation Rotation in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_set_rotation(LCNodeHandle camera, float rotation);

/**
 * @brief Get the canvas zoom of a CameraUI (uniform, about the origin)
 * @param camera CameraUI node handle
 * @param out_zoom Output parameter for the zoom (1 = normal, 2 = 2x in)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_get_zoom(LCNodeHandle camera, float* out_zoom);

/**
 * @brief Set the canvas zoom of a CameraUI (uniform, about the origin)
 * @param camera CameraUI node handle
 * @param zoom Zoom factor; 1 = normal, 2 = 2x in, 0.5 = 2x out
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_camera_ui_set_zoom(LCNodeHandle camera, float zoom);

/* ============================================================================
 * Screen <-> World Conversion
 * ============================================================================
 *
 * These resolve against the active Camera2D / Camera3D in the current scene and
 * the window size. Screen coordinates are window pixels with the origin at the
 * top-left (matching lc_input_get_mouse_position). They return LC_ERROR_NOT_FOUND
 * when there is no current scene; when a scene exists but has no active camera or
 * known window size the conversion succeeds and returns the input unchanged (2D)
 * or zero (3D), matching the scripting-language behaviour.
 */

/**
 * @brief Convert a screen-space point to 2D world space
 * @param screen_pos Screen-space point in window pixels (top-left origin)
 * @param out_world Output parameter for the world-space point
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_screen_to_world_2d(LCVec2 screen_pos, LCVec2* out_world);

/**
 * @brief Convert a 2D world-space point to screen space
 * @param world_pos World-space point
 * @param out_screen Output parameter for the screen-space point in window pixels
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_world_to_screen_2d(LCVec2 world_pos, LCVec2* out_screen);

/**
 * @brief Convert a screen-space point to a 3D world-space point at a distance
 * @param screen_pos Screen-space point in window pixels (top-left origin)
 * @param distance Distance along the view ray from the camera
 * @param out_world Output parameter for the world-space point
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_screen_to_world_3d(LCVec2 screen_pos, float distance, LCVec3* out_world);

/**
 * @brief Convert a 3D world-space point to screen space
 * @param world_pos World-space point
 * @param out_screen Output parameter (xy = window pixels, z = NDC depth)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_world_to_screen_3d(LCVec3 world_pos, LCVec3* out_screen);

/**
 * @brief Build a world-space picking ray from a screen-space point
 * @param screen_pos Screen-space point in window pixels (top-left origin)
 * @param out_origin Output parameter for the ray origin (world space)
 * @param out_direction Output parameter for the normalized ray direction
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_screen_to_world_ray_3d(LCVec2 screen_pos, LCVec3* out_origin, LCVec3* out_direction);


#endif /* LUPINE_CAPI_CAMERA_H */
