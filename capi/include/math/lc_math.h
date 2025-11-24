/**
 * @file lc_math.h
 * @brief Lupine Engine C API - Math types and operations
 *
 * This header provides math types and utilities for the Lupine Engine C API.
 * All types are plain-old-data structures that can be passed by value.
 */

#ifndef LUPINE_CAPI_MATH_H
#define LUPINE_CAPI_MATH_H

#include "core/lc_core.h"
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Math Constants
 * ============================================================================ */

#define LC_PI           3.14159265358979323846f
#define LC_TWO_PI       6.28318530717958647692f
#define LC_HALF_PI      1.57079632679489661923f
#define LC_E            2.71828182845904523536f
#define LC_EPSILON      1.192092896e-07f
#define LC_DEG_TO_RAD   0.01745329251994329576f
#define LC_RAD_TO_DEG   57.2957795130823208768f

/* ============================================================================
 * Vec2 - 2D Vector
 * ============================================================================ */

/**
 * @brief 2D vector (POD type, pass by value)
 */
typedef struct LCVec2 {
    float x, y;
} LCVec2;

/* Vec2 Constructors */
LC_API LCVec2 lc_vec2(float x, float y);
LC_API LCVec2 lc_vec2_zero(void);
LC_API LCVec2 lc_vec2_one(void);
LC_API LCVec2 lc_vec2_unit_x(void);
LC_API LCVec2 lc_vec2_unit_y(void);
LC_API LCVec2 lc_vec2_up(void);
LC_API LCVec2 lc_vec2_down(void);
LC_API LCVec2 lc_vec2_left(void);
LC_API LCVec2 lc_vec2_right(void);

/* Vec2 Operations */
LC_API LCVec2 lc_vec2_add(LCVec2 a, LCVec2 b);
LC_API LCVec2 lc_vec2_sub(LCVec2 a, LCVec2 b);
LC_API LCVec2 lc_vec2_mul(LCVec2 v, float s);
LC_API LCVec2 lc_vec2_div(LCVec2 v, float s);
LC_API LCVec2 lc_vec2_mul_vec(LCVec2 a, LCVec2 b);
LC_API LCVec2 lc_vec2_div_vec(LCVec2 a, LCVec2 b);
LC_API LCVec2 lc_vec2_negate(LCVec2 v);

LC_API float lc_vec2_dot(LCVec2 a, LCVec2 b);
LC_API float lc_vec2_length(LCVec2 v);
LC_API float lc_vec2_length_squared(LCVec2 v);
LC_API float lc_vec2_distance(LCVec2 a, LCVec2 b);
LC_API float lc_vec2_distance_squared(LCVec2 a, LCVec2 b);

LC_API LCVec2 lc_vec2_normalize(LCVec2 v);
LC_API LCVec2 lc_vec2_lerp(LCVec2 a, LCVec2 b, float t);
LC_API LCVec2 lc_vec2_clamp_length(LCVec2 v, float max_length);
LC_API LCVec2 lc_vec2_reflect(LCVec2 v, LCVec2 normal);
LC_API LCVec2 lc_vec2_project(LCVec2 v, LCVec2 onto);
LC_API LCVec2 lc_vec2_rotate(LCVec2 v, float angle);

LC_API float lc_vec2_angle(LCVec2 a, LCVec2 b);
LC_API bool lc_vec2_equals(LCVec2 a, LCVec2 b);

/* ============================================================================
 * Vec3 - 3D Vector
 * ============================================================================ */

/**
 * @brief 3D vector (POD type, pass by value)
 */
typedef struct LCVec3 {
    float x, y, z;
} LCVec3;

/* Vec3 Constructors */
LC_API LCVec3 lc_vec3(float x, float y, float z);
LC_API LCVec3 lc_vec3_scalar(float s);
LC_API LCVec3 lc_vec3_zero(void);
LC_API LCVec3 lc_vec3_one(void);
LC_API LCVec3 lc_vec3_unit_x(void);
LC_API LCVec3 lc_vec3_unit_y(void);
LC_API LCVec3 lc_vec3_unit_z(void);
LC_API LCVec3 lc_vec3_up(void);
LC_API LCVec3 lc_vec3_down(void);
LC_API LCVec3 lc_vec3_left(void);
LC_API LCVec3 lc_vec3_right(void);
LC_API LCVec3 lc_vec3_forward(void);
LC_API LCVec3 lc_vec3_back(void);

/* Vec3 Operations */
LC_API LCVec3 lc_vec3_add(LCVec3 a, LCVec3 b);
LC_API LCVec3 lc_vec3_sub(LCVec3 a, LCVec3 b);
LC_API LCVec3 lc_vec3_mul(LCVec3 v, float s);
LC_API LCVec3 lc_vec3_div(LCVec3 v, float s);
LC_API LCVec3 lc_vec3_mul_vec(LCVec3 a, LCVec3 b);
LC_API LCVec3 lc_vec3_div_vec(LCVec3 a, LCVec3 b);
LC_API LCVec3 lc_vec3_negate(LCVec3 v);

LC_API float lc_vec3_dot(LCVec3 a, LCVec3 b);
LC_API LCVec3 lc_vec3_cross(LCVec3 a, LCVec3 b);
LC_API float lc_vec3_length(LCVec3 v);
LC_API float lc_vec3_length_squared(LCVec3 v);
LC_API float lc_vec3_distance(LCVec3 a, LCVec3 b);
LC_API float lc_vec3_distance_squared(LCVec3 a, LCVec3 b);

LC_API LCVec3 lc_vec3_normalize(LCVec3 v);
LC_API LCVec3 lc_vec3_lerp(LCVec3 a, LCVec3 b, float t);
LC_API LCVec3 lc_vec3_slerp(LCVec3 a, LCVec3 b, float t);
LC_API LCVec3 lc_vec3_clamp_length(LCVec3 v, float max_length);
LC_API LCVec3 lc_vec3_reflect(LCVec3 v, LCVec3 normal);
LC_API LCVec3 lc_vec3_project(LCVec3 v, LCVec3 onto);

LC_API float lc_vec3_angle(LCVec3 a, LCVec3 b);
LC_API bool lc_vec3_equals(LCVec3 a, LCVec3 b);

/* ============================================================================
 * Vec4 - 4D Vector
 * ============================================================================ */

/**
 * @brief 4D vector (POD type, pass by value)
 */
typedef struct LCVec4 {
    float x, y, z, w;
} LCVec4;

/* Vec4 Constructors */
LC_API LCVec4 lc_vec4(float x, float y, float z, float w);
LC_API LCVec4 lc_vec4_scalar(float s);
LC_API LCVec4 lc_vec4_zero(void);
LC_API LCVec4 lc_vec4_one(void);

/* Vec4 Operations */
LC_API LCVec4 lc_vec4_add(LCVec4 a, LCVec4 b);
LC_API LCVec4 lc_vec4_sub(LCVec4 a, LCVec4 b);
LC_API LCVec4 lc_vec4_mul(LCVec4 v, float s);
LC_API LCVec4 lc_vec4_div(LCVec4 v, float s);
LC_API LCVec4 lc_vec4_negate(LCVec4 v);

LC_API float lc_vec4_dot(LCVec4 a, LCVec4 b);
LC_API float lc_vec4_length(LCVec4 v);
LC_API float lc_vec4_length_squared(LCVec4 v);

LC_API LCVec4 lc_vec4_normalize(LCVec4 v);
LC_API LCVec4 lc_vec4_lerp(LCVec4 a, LCVec4 b, float t);
LC_API bool lc_vec4_equals(LCVec4 a, LCVec4 b);

/* ============================================================================
 * Quat - Quaternion
 * ============================================================================ */

/**
 * @brief Quaternion for rotations (POD type, pass by value)
 * Components: (x, y, z, w) where w is the scalar part
 */
typedef struct LCQuat {
    float x, y, z, w;
} LCQuat;

/* Quat Constructors */
LC_API LCQuat lc_quat(float x, float y, float z, float w);
LC_API LCQuat lc_quat_identity(void);
LC_API LCQuat lc_quat_from_axis_angle(LCVec3 axis, float angle);
LC_API LCQuat lc_quat_from_euler(float pitch, float yaw, float roll);
LC_API LCQuat lc_quat_from_euler_vec(LCVec3 euler);
LC_API LCQuat lc_quat_look_rotation(LCVec3 forward, LCVec3 up);

/* Quat Operations */
LC_API LCQuat lc_quat_mul(LCQuat a, LCQuat b);
LC_API LCVec3 lc_quat_mul_vec3(LCQuat q, LCVec3 v);
LC_API LCQuat lc_quat_conjugate(LCQuat q);
LC_API LCQuat lc_quat_inverse(LCQuat q);
LC_API LCQuat lc_quat_normalize(LCQuat q);

LC_API float lc_quat_length(LCQuat q);
LC_API float lc_quat_dot(LCQuat a, LCQuat b);
LC_API LCQuat lc_quat_slerp(LCQuat a, LCQuat b, float t);
LC_API LCQuat lc_quat_lerp(LCQuat a, LCQuat b, float t);

LC_API LCVec3 lc_quat_to_euler(LCQuat q);
LC_API void lc_quat_to_axis_angle(LCQuat q, LCVec3* out_axis, float* out_angle);
LC_API bool lc_quat_equals(LCQuat a, LCQuat b);

/* ============================================================================
 * Mat4 - 4x4 Matrix
 * ============================================================================ */

/**
 * @brief 4x4 matrix in column-major order (POD type, pass by pointer for efficiency)
 * m[col][row] - column-major like OpenGL/GLM
 */
typedef struct LCMat4 {
    float m[16];  // Column-major: [m0,m1,m2,m3, m4,m5,m6,m7, m8,m9,m10,m11, m12,m13,m14,m15]
} LCMat4;

/* Mat4 Constructors */
LC_API LCMat4 lc_mat4_identity(void);
LC_API LCMat4 lc_mat4_zero(void);
LC_API LCMat4 lc_mat4_from_array(const float* values);

/* Mat4 Transform Constructors */
LC_API LCMat4 lc_mat4_translate(LCVec3 translation);
LC_API LCMat4 lc_mat4_rotate(LCQuat rotation);
LC_API LCMat4 lc_mat4_rotate_axis_angle(LCVec3 axis, float angle);
LC_API LCMat4 lc_mat4_rotate_euler(float pitch, float yaw, float roll);
LC_API LCMat4 lc_mat4_scale(LCVec3 scale);
LC_API LCMat4 lc_mat4_trs(LCVec3 translation, LCQuat rotation, LCVec3 scale);

/* Mat4 Projection Constructors */
LC_API LCMat4 lc_mat4_perspective(float fov, float aspect, float near, float far);
LC_API LCMat4 lc_mat4_orthographic(float left, float right, float bottom, float top, float near, float far);
LC_API LCMat4 lc_mat4_look_at(LCVec3 eye, LCVec3 center, LCVec3 up);

/* Mat4 Operations */
LC_API LCMat4 lc_mat4_mul(const LCMat4* a, const LCMat4* b);
LC_API LCVec4 lc_mat4_mul_vec4(const LCMat4* m, LCVec4 v);
LC_API LCVec3 lc_mat4_mul_vec3(const LCMat4* m, LCVec3 v, float w);
LC_API LCMat4 lc_mat4_transpose(const LCMat4* m);
LC_API LCMat4 lc_mat4_inverse(const LCMat4* m);

LC_API float lc_mat4_determinant(const LCMat4* m);
LC_API bool lc_mat4_equals(const LCMat4* a, const LCMat4* b);

/* ============================================================================
 * Color
 * ============================================================================ */

/**
 * @brief RGBA color (POD type, pass by value)
 * Components are in range [0, 1]
 */
typedef struct LCColor {
    float r, g, b, a;
} LCColor;

/* Color Constructors */
LC_API LCColor lc_color(float r, float g, float b, float a);
LC_API LCColor lc_color_rgb(float r, float g, float b);
LC_API LCColor lc_color_from_bytes(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
LC_API LCColor lc_color_from_hex(unsigned int hex);
LC_API LCColor lc_color_from_hsv(float h, float s, float v);

/* Predefined colors */
LC_API LCColor lc_color_white(void);
LC_API LCColor lc_color_black(void);
LC_API LCColor lc_color_red(void);
LC_API LCColor lc_color_green(void);
LC_API LCColor lc_color_blue(void);
LC_API LCColor lc_color_yellow(void);
LC_API LCColor lc_color_cyan(void);
LC_API LCColor lc_color_magenta(void);
LC_API LCColor lc_color_gray(void);
LC_API LCColor lc_color_transparent(void);

/* Color Operations */
LC_API LCColor lc_color_lerp(LCColor a, LCColor b, float t);
LC_API LCColor lc_color_mul(LCColor a, LCColor b);
LC_API LCColor lc_color_add(LCColor a, LCColor b);
LC_API unsigned int lc_color_to_hex(LCColor c);
LC_API void lc_color_to_bytes(LCColor c, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);
LC_API LCVec3 lc_color_to_hsv(LCColor c);

/* ============================================================================
 * Transform
 * ============================================================================ */

/**
 * @brief 3D transform (position, rotation, scale)
 */
typedef struct LCTransform {
    LCVec3 position;
    LCQuat rotation;
    LCVec3 scale;
} LCTransform;

/* Transform Constructors */
LC_API LCTransform lc_transform_identity(void);
LC_API LCTransform lc_transform_create(LCVec3 position, LCQuat rotation, LCVec3 scale);

/* Transform Operations */
LC_API LCMat4 lc_transform_to_matrix(const LCTransform* transform);
LC_API LCTransform lc_transform_from_matrix(const LCMat4* matrix);
LC_API LCVec3 lc_transform_apply_to_point(const LCTransform* transform, LCVec3 point);
LC_API LCVec3 lc_transform_apply_to_direction(const LCTransform* transform, LCVec3 direction);
LC_API LCTransform lc_transform_mul(const LCTransform* a, const LCTransform* b);
LC_API LCTransform lc_transform_inverse(const LCTransform* transform);
LC_API LCTransform lc_transform_lerp(const LCTransform* a, const LCTransform* b, float t);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Scalar math */
LC_API float lc_min(float a, float b);
LC_API float lc_max(float a, float b);
LC_API float lc_clamp(float value, float min, float max);
LC_API float lc_lerp(float a, float b, float t);
LC_API float lc_saturate(float value);
LC_API float lc_sign(float value);
LC_API float lc_abs(float value);
LC_API float lc_sqrt(float value);
LC_API float lc_pow(float base, float exponent);

/* Angle utilities */
LC_API float lc_deg_to_rad(float degrees);
LC_API float lc_rad_to_deg(float radians);
LC_API float lc_angle_normalize(float angle);
LC_API float lc_angle_difference(float a, float b);
LC_API float lc_lerp_angle(float a, float b, float t);

/* Comparison */
LC_API bool lc_equals(float a, float b);
LC_API bool lc_is_zero(float value);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_MATH_H */
