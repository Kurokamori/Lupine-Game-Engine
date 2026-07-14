

#include "math/lc_math.h"
#include <lupine/math/Math.hpp>
#include <lupine/math/Camera.hpp>
#include <lupine/math/Gradient.hpp>
#include <lupine/math/Curve.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <algorithm>
#include <random>
#include <string>
#include <cstring>

namespace {

inline lupine::math::Vec2 ToVec2(LCVec2 v) {
    return lupine::math::Vec2(v.x, v.y);
}

inline lupine::math::Vec3 ToVec3(LCVec3 v) {
    return lupine::math::Vec3(v.x, v.y, v.z);
}

inline lupine::math::Vec4 ToVec4(LCVec4 v) {
    return lupine::math::Vec4(v.x, v.y, v.z, v.w);
}

inline lupine::math::Quat ToQuat(LCQuat q) {
    return lupine::math::Quat(q.w, q.x, q.y, q.z);
}

inline lupine::math::Mat4 ToMat4(const LCMat4& m) {
    glm::mat4 glmMat;
    float* ptr = glm::value_ptr(glmMat);
    for (int i = 0; i < 16; ++i) {
        ptr[i] = m.m[i];
    }
    return lupine::math::Mat4(glmMat);
}

inline lupine::math::Color ToColor(LCColor c) {
    return lupine::math::Color(c.r, c.g, c.b, c.a);
}

inline lupine::math::Transform ToTransform(const LCTransform& t) {
    lupine::math::Transform result;
    result.position = ToVec3(t.position);
    result.rotation = ToQuat(t.rotation);
    result.scale = ToVec3(t.scale);
    return result;
}

inline LCVec2 FromVec2(const lupine::math::Vec2& v) {
    return LCVec2{v.x, v.y};
}

inline LCVec3 FromVec3(const lupine::math::Vec3& v) {
    return LCVec3{v.x, v.y, v.z};
}

inline LCVec4 FromVec4(const lupine::math::Vec4& v) {
    return LCVec4{v.x, v.y, v.z, v.w};
}

inline LCQuat FromQuat(const lupine::math::Quat& q) {
    return LCQuat{q.x(), q.y(), q.z(), q.w()};
}

inline LCMat4 FromMat4(const lupine::math::Mat4& m) {
    LCMat4 result;
    const float* ptr = glm::value_ptr(m.ToGLM());
    for (int i = 0; i < 16; ++i) {
        result.m[i] = ptr[i];
    }
    return result;
}

inline LCColor FromColor(const lupine::math::Color& c) {
    return LCColor{c.r, c.g, c.b, c.a};
}

inline LCTransform FromTransform(const lupine::math::Transform& t) {
    LCTransform result;
    result.position = FromVec3(t.position);
    result.rotation = FromQuat(t.rotation);
    result.scale = FromVec3(t.scale);
    return result;
}

}


LC_API LCVec2 lc_vec2(float x, float y) {
    return LCVec2{x, y};
}

LC_API LCVec2 lc_vec2_zero(void) {
    return FromVec2(lupine::math::Vec2::Zero());
}

LC_API LCVec2 lc_vec2_one(void) {
    return FromVec2(lupine::math::Vec2::One());
}

LC_API LCVec2 lc_vec2_unit_x(void) {
    return FromVec2(lupine::math::Vec2::UnitX());
}

LC_API LCVec2 lc_vec2_unit_y(void) {
    return FromVec2(lupine::math::Vec2::UnitY());
}

LC_API LCVec2 lc_vec2_up(void) {
    return LCVec2{0.0f, 1.0f};
}

LC_API LCVec2 lc_vec2_down(void) {
    return LCVec2{0.0f, -1.0f};
}

LC_API LCVec2 lc_vec2_left(void) {
    return LCVec2{-1.0f, 0.0f};
}

LC_API LCVec2 lc_vec2_right(void) {
    return LCVec2{1.0f, 0.0f};
}

LC_API LCVec2 lc_vec2_add(LCVec2 a, LCVec2 b) {
    return FromVec2(ToVec2(a) + ToVec2(b));
}

LC_API LCVec2 lc_vec2_sub(LCVec2 a, LCVec2 b) {
    return FromVec2(ToVec2(a) - ToVec2(b));
}

LC_API LCVec2 lc_vec2_mul(LCVec2 v, float s) {
    return FromVec2(ToVec2(v) * s);
}

LC_API LCVec2 lc_vec2_div(LCVec2 v, float s) {
    return FromVec2(ToVec2(v) / s);
}

LC_API LCVec2 lc_vec2_mul_vec(LCVec2 a, LCVec2 b) {
    return FromVec2(ToVec2(a) * ToVec2(b));
}

LC_API LCVec2 lc_vec2_div_vec(LCVec2 a, LCVec2 b) {
    return FromVec2(ToVec2(a) / ToVec2(b));
}

LC_API LCVec2 lc_vec2_negate(LCVec2 v) {
    return FromVec2(-ToVec2(v));
}

LC_API float lc_vec2_dot(LCVec2 a, LCVec2 b) {
    return ToVec2(a).Dot(ToVec2(b));
}

LC_API float lc_vec2_length(LCVec2 v) {
    return ToVec2(v).Length();
}

LC_API float lc_vec2_length_squared(LCVec2 v) {
    return ToVec2(v).LengthSquared();
}

LC_API float lc_vec2_distance(LCVec2 a, LCVec2 b) {
    return ToVec2(a).Distance(ToVec2(b));
}

LC_API float lc_vec2_distance_squared(LCVec2 a, LCVec2 b) {
    return ToVec2(a).DistanceSquared(ToVec2(b));
}

LC_API LCVec2 lc_vec2_normalize(LCVec2 v) {
    return FromVec2(ToVec2(v).Normalized());
}

LC_API LCVec2 lc_vec2_lerp(LCVec2 a, LCVec2 b, float t) {
    return FromVec2(ToVec2(a).Lerp(ToVec2(b), t));
}

LC_API LCVec2 lc_vec2_clamp_length(LCVec2 v, float max_length) {
    auto vec = ToVec2(v);
    float len = vec.Length();
    if (len > max_length && len > 0.0f) {
        return FromVec2(vec * (max_length / len));
    }
    return v;
}

LC_API LCVec2 lc_vec2_reflect(LCVec2 v, LCVec2 normal) {
    auto vec = ToVec2(v);
    auto norm = ToVec2(normal);

    float dot_vn = vec.Dot(norm);
    return FromVec2(vec - norm * (2.0f * dot_vn));
}

LC_API LCVec2 lc_vec2_project(LCVec2 v, LCVec2 onto) {
    auto vec = ToVec2(v);
    auto ontoVec = ToVec2(onto);

    float dot_v_onto = vec.Dot(ontoVec);
    float dot_onto_onto = ontoVec.Dot(ontoVec);
    if (dot_onto_onto > 0.0f) {
        return FromVec2(ontoVec * (dot_v_onto / dot_onto_onto));
    }
    return LCVec2{0.0f, 0.0f};
}

LC_API LCVec2 lc_vec2_rotate(LCVec2 v, float angle) {
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    return LCVec2{
        v.x * cos_a - v.y * sin_a,
        v.x * sin_a + v.y * cos_a
    };
}

LC_API float lc_vec2_angle(LCVec2 a, LCVec2 b) {
    return ToVec2(a).AngleTo(ToVec2(b));
}

LC_API bool lc_vec2_equals(LCVec2 a, LCVec2 b) {
    return ToVec2(a) == ToVec2(b);
}

LC_API LCVec3 lc_vec3(float x, float y, float z) {
    return LCVec3{x, y, z};
}

LC_API LCVec3 lc_vec3_scalar(float s) {
    return FromVec3(lupine::math::Vec3(s));
}

LC_API LCVec3 lc_vec3_zero(void) {
    return FromVec3(lupine::math::Vec3::Zero());
}

LC_API LCVec3 lc_vec3_one(void) {
    return FromVec3(lupine::math::Vec3::One());
}

LC_API LCVec3 lc_vec3_unit_x(void) {
    return FromVec3(lupine::math::Vec3::UnitX());
}

LC_API LCVec3 lc_vec3_unit_y(void) {
    return FromVec3(lupine::math::Vec3::UnitY());
}

LC_API LCVec3 lc_vec3_unit_z(void) {
    return FromVec3(lupine::math::Vec3::UnitZ());
}

LC_API LCVec3 lc_vec3_up(void) {
    return FromVec3(lupine::math::Vec3::Up());
}

LC_API LCVec3 lc_vec3_down(void) {
    return FromVec3(lupine::math::Vec3::Down());
}

LC_API LCVec3 lc_vec3_left(void) {
    return FromVec3(lupine::math::Vec3::Left());
}

LC_API LCVec3 lc_vec3_right(void) {
    return FromVec3(lupine::math::Vec3::Right());
}

LC_API LCVec3 lc_vec3_forward(void) {
    return FromVec3(lupine::math::Vec3::Forward());
}

LC_API LCVec3 lc_vec3_back(void) {
    return FromVec3(lupine::math::Vec3::Back());
}

LC_API LCVec3 lc_vec3_add(LCVec3 a, LCVec3 b) {
    return FromVec3(ToVec3(a) + ToVec3(b));
}

LC_API LCVec3 lc_vec3_sub(LCVec3 a, LCVec3 b) {
    return FromVec3(ToVec3(a) - ToVec3(b));
}

LC_API LCVec3 lc_vec3_mul(LCVec3 v, float s) {
    return FromVec3(ToVec3(v) * s);
}

LC_API LCVec3 lc_vec3_div(LCVec3 v, float s) {
    return FromVec3(ToVec3(v) / s);
}

LC_API LCVec3 lc_vec3_mul_vec(LCVec3 a, LCVec3 b) {
    return FromVec3(ToVec3(a) * ToVec3(b));
}

LC_API LCVec3 lc_vec3_div_vec(LCVec3 a, LCVec3 b) {
    return FromVec3(ToVec3(a) / ToVec3(b));
}

LC_API LCVec3 lc_vec3_negate(LCVec3 v) {
    return FromVec3(-ToVec3(v));
}

LC_API float lc_vec3_dot(LCVec3 a, LCVec3 b) {
    return ToVec3(a).Dot(ToVec3(b));
}

LC_API LCVec3 lc_vec3_cross(LCVec3 a, LCVec3 b) {
    return FromVec3(ToVec3(a).Cross(ToVec3(b)));
}

LC_API float lc_vec3_length(LCVec3 v) {
    return ToVec3(v).Length();
}

LC_API float lc_vec3_length_squared(LCVec3 v) {
    return ToVec3(v).LengthSquared();
}

LC_API float lc_vec3_distance(LCVec3 a, LCVec3 b) {
    return ToVec3(a).Distance(ToVec3(b));
}

LC_API float lc_vec3_distance_squared(LCVec3 a, LCVec3 b) {
    return ToVec3(a).DistanceSquared(ToVec3(b));
}

LC_API LCVec3 lc_vec3_normalize(LCVec3 v) {
    return FromVec3(ToVec3(v).Normalized());
}

LC_API LCVec3 lc_vec3_lerp(LCVec3 a, LCVec3 b, float t) {
    return FromVec3(ToVec3(a).Lerp(ToVec3(b), t));
}

LC_API LCVec3 lc_vec3_slerp(LCVec3 a, LCVec3 b, float t) {

    auto va = ToVec3(a).Normalized();
    auto vb = ToVec3(b).Normalized();
    float dot = va.Dot(vb);

    dot = std::max(-1.0f, std::min(1.0f, dot));

    float theta = std::acos(dot) * t;
    auto relative = (vb - va * dot).Normalized();
    return FromVec3(va * std::cos(theta) + relative * std::sin(theta));
}

LC_API LCVec3 lc_vec3_clamp_length(LCVec3 v, float max_length) {
    auto vec = ToVec3(v);
    float len = vec.Length();
    if (len > max_length && len > 0.0f) {
        return FromVec3(vec * (max_length / len));
    }
    return v;
}

LC_API LCVec3 lc_vec3_reflect(LCVec3 v, LCVec3 normal) {
    auto vec = ToVec3(v);
    auto norm = ToVec3(normal);

    float dot_vn = vec.Dot(norm);
    return FromVec3(vec - norm * (2.0f * dot_vn));
}

LC_API LCVec3 lc_vec3_project(LCVec3 v, LCVec3 onto) {
    auto vec = ToVec3(v);
    auto ontoVec = ToVec3(onto);

    float dot_v_onto = vec.Dot(ontoVec);
    float dot_onto_onto = ontoVec.Dot(ontoVec);
    if (dot_onto_onto > 0.0f) {
        return FromVec3(ontoVec * (dot_v_onto / dot_onto_onto));
    }
    return LCVec3{0.0f, 0.0f, 0.0f};
}

LC_API float lc_vec3_angle(LCVec3 a, LCVec3 b) {
    auto va = ToVec3(a);
    auto vb = ToVec3(b);
    float dot = va.Dot(vb);
    float len_product = va.Length() * vb.Length();
    if (len_product > 0.0f) {
        return std::acos(std::max(-1.0f, std::min(1.0f, dot / len_product)));
    }
    return 0.0f;
}

LC_API bool lc_vec3_equals(LCVec3 a, LCVec3 b) {
    return ToVec3(a) == ToVec3(b);
}

LC_API LCVec4 lc_vec4(float x, float y, float z, float w) {
    return LCVec4{x, y, z, w};
}

LC_API LCVec4 lc_vec4_scalar(float s) {
    return FromVec4(lupine::math::Vec4(s));
}

LC_API LCVec4 lc_vec4_zero(void) {
    return FromVec4(lupine::math::Vec4::Zero());
}

LC_API LCVec4 lc_vec4_one(void) {
    return FromVec4(lupine::math::Vec4::One());
}

LC_API LCVec4 lc_vec4_add(LCVec4 a, LCVec4 b) {
    return FromVec4(ToVec4(a) + ToVec4(b));
}

LC_API LCVec4 lc_vec4_sub(LCVec4 a, LCVec4 b) {
    return FromVec4(ToVec4(a) - ToVec4(b));
}

LC_API LCVec4 lc_vec4_mul(LCVec4 v, float s) {
    return FromVec4(ToVec4(v) * s);
}

LC_API LCVec4 lc_vec4_div(LCVec4 v, float s) {
    return FromVec4(ToVec4(v) / s);
}

LC_API LCVec4 lc_vec4_negate(LCVec4 v) {
    return FromVec4(-ToVec4(v));
}

LC_API float lc_vec4_dot(LCVec4 a, LCVec4 b) {
    return ToVec4(a).Dot(ToVec4(b));
}

LC_API float lc_vec4_length(LCVec4 v) {
    return ToVec4(v).Length();
}

LC_API float lc_vec4_length_squared(LCVec4 v) {
    return ToVec4(v).LengthSquared();
}

LC_API LCVec4 lc_vec4_normalize(LCVec4 v) {
    return FromVec4(ToVec4(v).Normalized());
}

LC_API LCVec4 lc_vec4_lerp(LCVec4 a, LCVec4 b, float t) {
    return FromVec4(ToVec4(a).Lerp(ToVec4(b), t));
}

LC_API bool lc_vec4_equals(LCVec4 a, LCVec4 b) {
    return ToVec4(a) == ToVec4(b);
}

LC_API LCQuat lc_quat(float x, float y, float z, float w) {
    return LCQuat{x, y, z, w};
}

LC_API LCQuat lc_quat_identity(void) {
    return FromQuat(lupine::math::Quat::Identity());
}

LC_API LCQuat lc_quat_from_axis_angle(LCVec3 axis, float angle) {
    return FromQuat(lupine::math::Quat::FromAxisAngle(ToVec3(axis), angle));
}

LC_API LCQuat lc_quat_from_euler(float pitch, float yaw, float roll) {
    return FromQuat(lupine::math::Quat::FromEuler(pitch, yaw, roll));
}

LC_API LCQuat lc_quat_from_euler_vec(LCVec3 euler) {
    return FromQuat(lupine::math::Quat::FromEuler(ToVec3(euler)));
}

LC_API LCQuat lc_quat_look_rotation(LCVec3 forward, LCVec3 up) {
    return FromQuat(lupine::math::Quat::LookRotation(ToVec3(forward), ToVec3(up)));
}

LC_API LCQuat lc_quat_mul(LCQuat a, LCQuat b) {
    return FromQuat(ToQuat(a) * ToQuat(b));
}

LC_API LCVec3 lc_quat_mul_vec3(LCQuat q, LCVec3 v) {
    return FromVec3(ToQuat(q) * ToVec3(v));
}

LC_API LCQuat lc_quat_conjugate(LCQuat q) {
    return FromQuat(ToQuat(q).Conjugate());
}

LC_API LCQuat lc_quat_inverse(LCQuat q) {
    return FromQuat(ToQuat(q).Inverse());
}

LC_API LCQuat lc_quat_normalize(LCQuat q) {
    return FromQuat(ToQuat(q).Normalized());
}

LC_API float lc_quat_length(LCQuat q) {
    return ToQuat(q).Length();
}

LC_API float lc_quat_dot(LCQuat a, LCQuat b) {
    return ToQuat(a).Dot(ToQuat(b));
}

LC_API LCQuat lc_quat_slerp(LCQuat a, LCQuat b, float t) {
    return FromQuat(ToQuat(a).Slerp(ToQuat(b), t));
}

LC_API LCQuat lc_quat_lerp(LCQuat a, LCQuat b, float t) {
    // Linear interpolation (simpler than slerp, faster but not constant velocity)
    auto qa = ToQuat(a);
    auto qb = ToQuat(b);
    float x = qa.x() * (1.0f - t) + qb.x() * t;
    float y = qa.y() * (1.0f - t) + qb.y() * t;
    float z = qa.z() * (1.0f - t) + qb.z() * t;
    float w = qa.w() * (1.0f - t) + qb.w() * t;
    return FromQuat(lupine::math::Quat(w, x, y, z).Normalized());
}

LC_API LCVec3 lc_quat_to_euler(LCQuat q) {
    return FromVec3(ToQuat(q).ToEuler());
}

LC_API void lc_quat_to_axis_angle(LCQuat q, LCVec3* out_axis, float* out_angle) {
    lupine::math::Vec3 axis;
    float angle;
    ToQuat(q).ToAxisAngle(axis, angle);
    if (out_axis) *out_axis = FromVec3(axis);
    if (out_angle) *out_angle = angle;
}

LC_API bool lc_quat_equals(LCQuat a, LCQuat b) {
    return ToQuat(a) == ToQuat(b);
}

LC_API LCMat4 lc_mat4_identity(void) {
    return FromMat4(lupine::math::Mat4::Identity());
}

LC_API LCMat4 lc_mat4_zero(void) {
    return FromMat4(lupine::math::Mat4::Zero());
}

LC_API LCMat4 lc_mat4_from_array(const float* values) {
    LCMat4 result;
    if (values) {
        for (int i = 0; i < 16; ++i) {
            result.m[i] = values[i];
        }
    }
    return result;
}

LC_API LCMat4 lc_mat4_translate(LCVec3 translation) {
    return FromMat4(lupine::math::Mat4::Translate(ToVec3(translation)));
}

LC_API LCMat4 lc_mat4_rotate(LCQuat rotation) {
    return FromMat4(ToQuat(rotation).ToMat4());
}

LC_API LCMat4 lc_mat4_rotate_axis_angle(LCVec3 axis, float angle) {
    return FromMat4(lupine::math::Mat4::Rotate(angle, ToVec3(axis)));
}

LC_API LCMat4 lc_mat4_rotate_euler(float pitch, float yaw, float roll) {
    auto quat = lupine::math::Quat::FromEuler(pitch, yaw, roll);
    return FromMat4(quat.ToMat4());
}

LC_API LCMat4 lc_mat4_scale(LCVec3 scale) {
    return FromMat4(lupine::math::Mat4::Scale(ToVec3(scale)));
}

LC_API LCMat4 lc_mat4_trs(LCVec3 translation, LCQuat rotation, LCVec3 scale) {
    auto euler = ToQuat(rotation).ToEuler();
    return FromMat4(lupine::math::Mat4::TRS(ToVec3(translation), euler, ToVec3(scale)));
}

LC_API LCMat4 lc_mat4_perspective(float fov, float aspect, float near, float far) {
    return FromMat4(lupine::math::Camera::Perspective(fov, aspect, near, far));
}

LC_API LCMat4 lc_mat4_orthographic(float left, float right, float bottom, float top, float near, float far) {
    return FromMat4(lupine::math::Camera::Orthographic(left, right, bottom, top, near, far));
}

LC_API LCMat4 lc_mat4_look_at(LCVec3 eye, LCVec3 center, LCVec3 up) {
    return FromMat4(lupine::math::Camera::LookAt(ToVec3(eye), ToVec3(center), ToVec3(up)));
}

LC_API LCMat4 lc_mat4_mul(const LCMat4* a, const LCMat4* b) {
    if (!a || !b) return lc_mat4_identity();
    return FromMat4(ToMat4(*a) * ToMat4(*b));
}

LC_API LCVec4 lc_mat4_mul_vec4(const LCMat4* m, LCVec4 v) {
    if (!m) return v;
    return FromVec4(ToMat4(*m) * ToVec4(v));
}

LC_API LCVec3 lc_mat4_mul_vec3(const LCMat4* m, LCVec3 v, float w) {
    if (!m) return v;
    lupine::math::Vec4 v4(v.x, v.y, v.z, w);
    lupine::math::Vec4 result = ToMat4(*m) * v4;
    return LCVec3{result.x, result.y, result.z};
}

LC_API LCMat4 lc_mat4_transpose(const LCMat4* m) {
    if (!m) return lc_mat4_identity();
    return FromMat4(ToMat4(*m).Transposed());
}

LC_API LCMat4 lc_mat4_inverse(const LCMat4* m) {
    if (!m) return lc_mat4_identity();
    return FromMat4(ToMat4(*m).Inverse());
}

LC_API float lc_mat4_determinant(const LCMat4* m) {
    if (!m) return 0.0f;
    return ToMat4(*m).Determinant();
}

LC_API bool lc_mat4_equals(const LCMat4* a, const LCMat4* b) {
    if (!a || !b) return false;
    return ToMat4(*a) == ToMat4(*b);
}

LC_API LCColor lc_color(float r, float g, float b, float a) {
    return LCColor{r, g, b, a};
}

LC_API LCColor lc_color_rgb(float r, float g, float b) {
    return LCColor{r, g, b, 1.0f};
}

LC_API LCColor lc_color_from_bytes(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return LCColor{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
}

LC_API LCColor lc_color_from_hex(unsigned int hex) {
    // Assume RGBA format: 0xRRGGBBAA
    return FromColor(lupine::math::Color::FromRGBA32(hex));
}

LC_API LCColor lc_color_from_hsv(float h, float s, float v) {
    // HSV to RGB conversion
    float c = v * s;
    float h_prime = h / 60.0f;
    float x = c * (1.0f - std::abs(std::fmod(h_prime, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h_prime < 1.0f) {
        r = c; g = x; b = 0.0f;
    } else if (h_prime < 2.0f) {
        r = x; g = c; b = 0.0f;
    } else if (h_prime < 3.0f) {
        r = 0.0f; g = c; b = x;
    } else if (h_prime < 4.0f) {
        r = 0.0f; g = x; b = c;
    } else if (h_prime < 5.0f) {
        r = x; g = 0.0f; b = c;
    } else {
        r = c; g = 0.0f; b = x;
    }

    return LCColor{r + m, g + m, b + m, 1.0f};
}

LC_API LCColor lc_color_white(void) {
    return FromColor(lupine::math::Color::White());
}

LC_API LCColor lc_color_black(void) {
    return FromColor(lupine::math::Color::Black());
}

LC_API LCColor lc_color_red(void) {
    return FromColor(lupine::math::Color::Red());
}

LC_API LCColor lc_color_green(void) {
    return FromColor(lupine::math::Color::Green());
}

LC_API LCColor lc_color_blue(void) {
    return FromColor(lupine::math::Color::Blue());
}

LC_API LCColor lc_color_yellow(void) {
    return FromColor(lupine::math::Color::Yellow());
}

LC_API LCColor lc_color_cyan(void) {
    return FromColor(lupine::math::Color::Cyan());
}

LC_API LCColor lc_color_magenta(void) {
    return FromColor(lupine::math::Color::Magenta());
}

LC_API LCColor lc_color_gray(void) {
    return FromColor(lupine::math::Color::Gray());
}

LC_API LCColor lc_color_transparent(void) {
    return LCColor{0.0f, 0.0f, 0.0f, 0.0f};
}

LC_API LCColor lc_color_lerp(LCColor a, LCColor b, float t) {
    return FromColor(ToColor(a).Lerp(ToColor(b), t));
}

LC_API LCColor lc_color_mul(LCColor a, LCColor b) {
    return FromColor(ToColor(a) * ToColor(b));
}

LC_API LCColor lc_color_add(LCColor a, LCColor b) {
    return FromColor(ToColor(a) + ToColor(b));
}

LC_API unsigned int lc_color_to_hex(LCColor c) {
    return ToColor(c).ToRGBA32();
}

LC_API void lc_color_to_bytes(LCColor c, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a) {
    if (r) *r = static_cast<unsigned char>(lupine::math::Saturate(c.r) * 255.0f);
    if (g) *g = static_cast<unsigned char>(lupine::math::Saturate(c.g) * 255.0f);
    if (b) *b = static_cast<unsigned char>(lupine::math::Saturate(c.b) * 255.0f);
    if (a) *a = static_cast<unsigned char>(lupine::math::Saturate(c.a) * 255.0f);
}

LC_API LCVec3 lc_color_to_hsv(LCColor c) {
    // RGB to HSV conversion
    float r = c.r, g = c.g, b = c.b;
    float max_val = std::max({r, g, b});
    float min_val = std::min({r, g, b});
    float delta = max_val - min_val;

    float h = 0.0f, s = 0.0f, v = max_val;

    if (delta > 0.0f) {
        s = delta / max_val;

        if (max_val == r) {
            h = 60.0f * std::fmod((g - b) / delta, 6.0f);
        } else if (max_val == g) {
            h = 60.0f * ((b - r) / delta + 2.0f);
        } else {
            h = 60.0f * ((r - g) / delta + 4.0f);
        }

        if (h < 0.0f) h += 360.0f;
    }

    return LCVec3{h, s, v};
}

namespace {

int HexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // anonymous namespace

LC_API LCColor lc_color_from_hex_string(const char* hex) {
    if (!hex) return LCColor{0.0f, 0.0f, 0.0f, 1.0f};

    std::string s = hex;
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    }

    auto pair = [&](size_t i) -> float {
        int hi = HexNibble(s[i]);
        int lo = HexNibble(s[i + 1]);
        if (hi < 0 || lo < 0) return 0.0f;
        return static_cast<float>(hi * 16 + lo) / 255.0f;
    };
    auto single = [&](size_t i) -> float {
        int v = HexNibble(s[i]);
        if (v < 0) return 0.0f;
        return static_cast<float>(v * 16 + v) / 255.0f;
    };

    if (s.size() == 3) {
        return LCColor{single(0), single(1), single(2), 1.0f};
    }
    if (s.size() == 4) {
        return LCColor{single(0), single(1), single(2), single(3)};
    }
    if (s.size() == 6) {
        return LCColor{pair(0), pair(2), pair(4), 1.0f};
    }
    if (s.size() == 8) {
        return LCColor{pair(0), pair(2), pair(4), pair(6)};
    }
    return LCColor{0.0f, 0.0f, 0.0f, 1.0f};
}

LC_API LCResult lc_color_to_hex_string(LCColor color, char* out_buffer, size_t buffer_size) {
    if (!out_buffer) return LC_ERROR_NULL_POINTER;
    if (buffer_size < 10) return LC_ERROR_INVALID_PARAMETER;

    auto clampByte = [](float v) -> int {
        int b = static_cast<int>(v * 255.0f + 0.5f);
        if (b < 0) return 0;
        if (b > 255) return 255;
        return b;
    };
    static const char* digits = "0123456789abcdef";
    int comps[4] = { clampByte(color.r), clampByte(color.g), clampByte(color.b), clampByte(color.a) };

    out_buffer[0] = '#';
    for (int i = 0; i < 4; ++i) {
        out_buffer[1 + i * 2] = digits[(comps[i] >> 4) & 0xF];
        out_buffer[2 + i * 2] = digits[comps[i] & 0xF];
    }
    out_buffer[9] = '\0';
    return LC_SUCCESS;
}

LC_API LCColor lc_color_from_hsv01(float h, float s, float v, float a) {
    h = h - std::floor(h);
    s = std::min(std::max(s, 0.0f), 1.0f);
    v = std::min(std::max(v, 0.0f), 1.0f);

    float r = v, g = v, b = v;
    if (s > 0.0f) {
        float hSector = h * 6.0f;
        int i = static_cast<int>(std::floor(hSector)) % 6;
        if (i < 0) i += 6;
        float f = hSector - std::floor(hSector);
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));
        switch (i) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    }
    return LCColor{r, g, b, a};
}

LC_API LCColor lc_sample_gradient(const char* gradient_json, float t) {
    if (!gradient_json) return FromColor(lupine::math::Color::White());
    lupine::math::Gradient g = lupine::math::Gradient::FromJsonString(std::string(gradient_json));
    return FromColor(g.Sample(t));
}

LC_API float lc_sample_curve(const char* curve_json, float t, float empty_default) {
    if (!curve_json) return empty_default;
    lupine::math::Curve c = lupine::math::Curve::FromJsonString(std::string(curve_json));
    return c.Sample(t, empty_default);
}

LC_API LCTransform lc_transform_identity(void) {
    return FromTransform(lupine::math::Transform::Identity());
}

LC_API LCTransform lc_transform_create(LCVec3 position, LCQuat rotation, LCVec3 scale) {
    LCTransform result;
    result.position = position;
    result.rotation = rotation;
    result.scale = scale;
    return result;
}

LC_API LCMat4 lc_transform_to_matrix(const LCTransform* transform) {
    if (!transform) return lc_mat4_identity();
    return FromMat4(ToTransform(*transform).ToMatrix());
}

LC_API LCTransform lc_transform_from_matrix(const LCMat4* matrix) {
    if (!matrix) return lc_transform_identity();
    // Extract position, rotation, and scale from matrix
    // This is a simplified extraction - for production use, consider using a decomposition library
    auto mat = ToMat4(*matrix);
    lupine::math::Transform result;

    // Extract translation (last column)
    auto glm_mat = mat.ToGLM();
    result.position = lupine::math::Vec3(glm_mat[3][0], glm_mat[3][1], glm_mat[3][2]);

    // Extract scale
    lupine::math::Vec3 col0(glm_mat[0][0], glm_mat[0][1], glm_mat[0][2]);
    lupine::math::Vec3 col1(glm_mat[1][0], glm_mat[1][1], glm_mat[1][2]);
    lupine::math::Vec3 col2(glm_mat[2][0], glm_mat[2][1], glm_mat[2][2]);
    result.scale = lupine::math::Vec3(col0.Length(), col1.Length(), col2.Length());

    // Extract rotation (normalize the columns to remove scale)
    if (result.scale.x > 0.0f) col0 = col0 / result.scale.x;
    if (result.scale.y > 0.0f) col1 = col1 / result.scale.y;
    if (result.scale.z > 0.0f) col2 = col2 / result.scale.z;

    // Build rotation matrix and convert to quaternion
    glm::mat3 rot_mat;
    rot_mat[0] = col0.ToGLM();
    rot_mat[1] = col1.ToGLM();
    rot_mat[2] = col2.ToGLM();
    result.rotation = lupine::math::Quat::FromMatrix(lupine::math::Mat3(rot_mat));

    return FromTransform(result);
}

LC_API LCVec3 lc_transform_apply_to_point(const LCTransform* transform, LCVec3 point) {
    if (!transform) return point;
    return FromVec3(ToTransform(*transform).TransformPoint(ToVec3(point)));
}

LC_API LCVec3 lc_transform_apply_to_direction(const LCTransform* transform, LCVec3 direction) {
    if (!transform) return direction;
    return FromVec3(ToTransform(*transform).TransformDirection(ToVec3(direction)));
}

LC_API LCTransform lc_transform_mul(const LCTransform* a, const LCTransform* b) {
    if (!a || !b) return lc_transform_identity();
    return FromTransform(ToTransform(*a) * ToTransform(*b));
}

LC_API LCTransform lc_transform_inverse(const LCTransform* transform) {
    if (!transform) return lc_transform_identity();
    return FromTransform(ToTransform(*transform).Inverse());
}

LC_API LCTransform lc_transform_lerp(const LCTransform* a, const LCTransform* b, float t) {
    if (!a || !b) return lc_transform_identity();
    return FromTransform(ToTransform(*a).Lerp(ToTransform(*b), t));
}

LC_API float lc_min(float a, float b) {
    return std::min(a, b);
}

LC_API float lc_max(float a, float b) {
    return std::max(a, b);
}

LC_API float lc_clamp(float value, float min, float max) {
    return lupine::math::Clamp(value, min, max);
}

LC_API float lc_lerp(float a, float b, float t) {
    return lupine::math::Lerp(a, b, t);
}

LC_API float lc_saturate(float value) {
    return lupine::math::Saturate(value);
}

LC_API float lc_sign(float value) {
    return lupine::math::Sign(value);
}

LC_API float lc_abs(float value) {
    return std::abs(value);
}

LC_API float lc_sqrt(float value) {
    return std::sqrt(value);
}

LC_API float lc_pow(float base, float exponent) {
    return std::pow(base, exponent);
}

LC_API float lc_deg_to_rad(float degrees) {
    return degrees * LC_DEG_TO_RAD;
}

LC_API float lc_rad_to_deg(float radians) {
    return radians * LC_RAD_TO_DEG;
}

LC_API float lc_angle_normalize(float angle) {
    float result = std::fmod(angle, LC_TWO_PI);
    if (result < 0.0f) result += LC_TWO_PI;
    return result;
}

LC_API float lc_angle_difference(float a, float b) {
    return lupine::math::AngleDifference(a, b);
}

LC_API float lc_lerp_angle(float a, float b, float t) {
    return lupine::math::LerpAngle(a, b, t);
}

LC_API bool lc_equals(float a, float b) {
    return lupine::math::Equals(a, b);
}

LC_API bool lc_is_zero(float value) {
    return lupine::math::IsZero(value);
}

namespace {

// Shared C-API RNG so lc_random_seed() makes every lc_random_* helper deterministic,
// mirroring the ScriptRng pattern in ScriptAPI.
std::mt19937& CapiRng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

} // anonymous namespace

LC_API float lc_random_float(void) {
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    return dis(CapiRng());
}

LC_API bool lc_random_bool(void) {
    std::uniform_int_distribution<int> dis(0, 1);
    return dis(CapiRng()) != 0;
}

LC_API int lc_random_sign(void) {
    std::uniform_int_distribution<int> dis(0, 1);
    return dis(CapiRng()) == 0 ? -1 : 1;
}

LC_API void lc_random_seed(int seed) {
    CapiRng().seed(static_cast<std::mt19937::result_type>(seed));
}

LC_API float lc_random_range(float min, float max) {
    std::uniform_real_distribution<float> dis(min, max);
    return dis(CapiRng());
}

LC_API int lc_random_range_int(int min, int max) {
    std::uniform_int_distribution<int> dis(min, max);
    return dis(CapiRng());
}

LC_API float lc_wrap(float value, float min, float max) {
    float range = max - min;
    if (std::abs(range) < 0.00001f) {
        return min;
    }
    float result = std::fmod(value - min, range);
    if (result < 0.0f) {
        result += range;
    }
    return result + min;
}

LC_API int lc_wrap_int(int value, int min, int max) {
    int range = max - min;
    if (range == 0) {
        return min;
    }
    int result = (value - min) % range;
    if (result < 0) {
        result += range;
    }
    return result + min;
}

LC_API float lc_ping_pong(float value, float length) {
    if (length <= 0.0f) {
        return 0.0f;
    }
    float t = std::fmod(std::abs(value), length * 2.0f);
    return length - std::abs(t - length);
}

LC_API float lc_snapped(float value, float step) {
    if (std::abs(step) < 0.00001f) {
        return value;
    }
    return std::round(value / step) * step;
}

LC_API bool lc_is_equal_approx(float a, float b) {
    float tolerance = 0.00001f * std::max(1.0f, std::max(std::abs(a), std::abs(b)));
    return std::abs(a - b) <= tolerance;
}

LC_API float lc_ease(float t, float curve) {
    t = std::min(std::max(t, 0.0f), 1.0f);
    if (curve > 0.0f) {
        if (curve < 1.0f) {
            return 1.0f - std::pow(1.0f - t, 1.0f / curve);
        }
        return std::pow(t, curve);
    }
    if (curve < 0.0f) {
        if (t < 0.5f) {
            return std::pow(t * 2.0f, -curve) * 0.5f;
        }
        return (1.0f - std::pow(1.0f - (t - 0.5f) * 2.0f, -curve)) * 0.5f + 0.5f;
    }
    return 0.0f;
}

LC_API float lc_pos_mod(float a, float b) {
    if (std::abs(b) < 0.00001f) {
        return 0.0f;
    }
    float result = std::fmod(a, b);
    if ((result < 0.0f && b > 0.0f) || (result > 0.0f && b < 0.0f)) {
        result += b;
    }
    return result;
}

LC_API int lc_pos_mod_int(int a, int b) {
    if (b == 0) {
        return 0;
    }
    int result = a % b;
    if ((result < 0 && b > 0) || (result > 0 && b < 0)) {
        result += b;
    }
    return result;
}

LC_API float lc_move_toward(float from, float to, float delta) {
    if (std::abs(to - from) <= delta) {
        return to;
    }
    return from + (to > from ? delta : -delta);
}

LC_API float lc_smoothstep(float from, float to, float t) {
    if (std::abs(to - from) < 0.0001f) {
        return t < from ? 0.0f : 1.0f;
    }
    t = lc_clamp((t - from) / (to - from), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

LC_API float lc_inverse_lerp(float from, float to, float value) {
    if (std::abs(to - from) < 0.0001f) {
        return 0.0f;
    }
    return (value - from) / (to - from);
}

LC_API float lc_remap(float value, float from_min, float from_max, float to_min, float to_max) {
    float t = lc_inverse_lerp(from_min, from_max, value);
    return lc_lerp(to_min, to_max, t);
}
