#include "rendering/lc_debug_draw.h"

#include <lupine/rendering/debug/DebugDrawQueue.hpp>
#include <lupine/math/Vec2.hpp>
#include <lupine/math/Vec3.hpp>
#include <lupine/math/Color.hpp>

#include <string>

namespace {

inline lupine::math::Vec3 ToVec3(LCVec3 v) {
    return lupine::math::Vec3(v.x, v.y, v.z);
}

inline lupine::math::Color ToColor(LCColor c) {
    return lupine::math::Color(c.r, c.g, c.b, c.a);
}

} // anonymous namespace

LC_API void lc_debug_draw_line(LCVec3 start, LCVec3 end, LCColor color, float duration) {
    try {
        lupine::core::DebugDrawQueue::Get().Line(ToVec3(start), ToVec3(end), ToColor(color), duration);
    } catch (...) {
    }
}

LC_API void lc_debug_draw_line_2d(LCVec2 start, LCVec2 end, LCColor color, float duration) {
    try {
        lupine::core::DebugDrawQueue::Get().Line(
            lupine::math::Vec3(start.x, start.y, 0.0f),
            lupine::math::Vec3(end.x, end.y, 0.0f),
            ToColor(color), duration);
    } catch (...) {
    }
}

LC_API void lc_debug_draw_ray(LCVec3 origin, LCVec3 direction, LCColor color, float duration) {
    try {
        lupine::core::DebugDrawQueue::Get().Ray(ToVec3(origin), ToVec3(direction), ToColor(color), duration);
    } catch (...) {
    }
}

LC_API void lc_debug_draw_box(LCVec3 center, LCVec3 size, LCColor color, float duration) {
    try {
        lupine::core::DebugDrawQueue::Get().Box(ToVec3(center), ToVec3(size), ToColor(color), duration);
    } catch (...) {
    }
}

LC_API void lc_debug_draw_sphere(LCVec3 center, float radius, LCColor color, float duration) {
    try {
        lupine::core::DebugDrawQueue::Get().Sphere(ToVec3(center), radius, ToColor(color), duration);
    } catch (...) {
    }
}

LC_API void lc_debug_draw_circle(LCVec3 center, LCVec3 normal, float radius, LCColor color, float duration) {
    try {
        lupine::core::DebugDrawQueue::Get().Circle(ToVec3(center), ToVec3(normal), radius, ToColor(color), duration);
    } catch (...) {
    }
}

LC_API void lc_debug_draw_text(LCVec3 position, const char* text, LCColor color, float duration) {
    try {
        lupine::core::DebugDrawQueue::Get().Text(
            ToVec3(position), text ? std::string(text) : std::string(), ToColor(color), duration);
    } catch (...) {
    }
}

LC_API void lc_debug_draw_text_2d(LCVec2 position, const char* text, LCColor color, float duration) {
    try {
        lupine::core::DebugDrawQueue::Get().Text(
            lupine::math::Vec3(position.x, position.y, 0.0f),
            text ? std::string(text) : std::string(), ToColor(color), duration);
    } catch (...) {
    }
}
