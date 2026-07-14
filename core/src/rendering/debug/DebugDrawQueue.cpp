#include "lupine/rendering/debug/DebugDrawQueue.hpp"
#include "lupine/rendering/debug/DebugRenderer.hpp"
#include <algorithm>

namespace lupine {
namespace core {

DebugDrawQueue& DebugDrawQueue::Get() {
    static DebugDrawQueue instance;
    return instance;
}

void DebugDrawQueue::Line(const math::Vec3& start, const math::Vec3& end, const math::Color& color, float duration) {
    Primitive p;
    p.kind = Kind::Line;
    p.p0 = start;
    p.p1 = end;
    p.color = color;
    p.remaining = duration;
    p.oneShot = duration <= 0.0f;
    m_Primitives.push_back(p);
}

void DebugDrawQueue::Ray(const math::Vec3& origin, const math::Vec3& direction, const math::Color& color, float duration) {
    Primitive p;
    p.kind = Kind::Ray;
    p.p0 = origin;
    p.p1 = direction;
    p.color = color;
    p.remaining = duration;
    p.oneShot = duration <= 0.0f;
    m_Primitives.push_back(p);
}

void DebugDrawQueue::Arrow(const math::Vec3& start, const math::Vec3& end, const math::Color& color, float duration) {
    Primitive p;
    p.kind = Kind::Arrow;
    p.p0 = start;
    p.p1 = end;
    p.color = color;
    p.remaining = duration;
    p.oneShot = duration <= 0.0f;
    m_Primitives.push_back(p);
}

void DebugDrawQueue::Box(const math::Vec3& center, const math::Vec3& size, const math::Color& color, float duration) {
    Primitive p;
    p.kind = Kind::Box;
    p.p0 = center;
    p.p1 = size;
    p.color = color;
    p.remaining = duration;
    p.oneShot = duration <= 0.0f;
    m_Primitives.push_back(p);
}

void DebugDrawQueue::Sphere(const math::Vec3& center, float radius, const math::Color& color, float duration) {
    Primitive p;
    p.kind = Kind::Sphere;
    p.p0 = center;
    p.radius = radius;
    p.color = color;
    p.remaining = duration;
    p.oneShot = duration <= 0.0f;
    m_Primitives.push_back(p);
}

void DebugDrawQueue::Circle(const math::Vec3& center, const math::Vec3& normal, float radius, const math::Color& color, float duration) {
    Primitive p;
    p.kind = Kind::Circle;
    p.p0 = center;
    p.p1 = normal;
    p.radius = radius;
    p.color = color;
    p.remaining = duration;
    p.oneShot = duration <= 0.0f;
    m_Primitives.push_back(p);
}

void DebugDrawQueue::Text(const math::Vec3& position, const std::string& text, const math::Color& color, float duration) {
    Primitive p;
    p.kind = Kind::Text;
    p.p0 = position;
    p.text = text;
    p.color = color;
    p.remaining = duration;
    p.oneShot = duration <= 0.0f;
    m_Primitives.push_back(p);
}

void DebugDrawQueue::Submit(DebugRenderer* renderer) const {
    if (!renderer) {
        return;
    }
    for (const Primitive& p : m_Primitives) {
        switch (p.kind) {
            case Kind::Line:
                renderer->drawLine(p.p0, p.p1, p.color, 0.0f);
                break;
            case Kind::Ray:
                // p1 carries the full direction (with magnitude); length 1 so the
                // ray ends at origin + direction.
                renderer->drawRay(p.p0, p.p1, p.color, 1.0f, 0.0f);
                break;
            case Kind::Arrow:
                renderer->drawArrow(p.p0, p.p1, p.color, 0.2f, 0.0f);
                break;
            case Kind::Box:
                renderer->drawBox(p.p0, p.p1, p.color, true, 0.0f);
                break;
            case Kind::Sphere:
                renderer->drawSphere(p.p0, p.radius, p.color, true, 16, 0.0f);
                break;
            case Kind::Circle:
                renderer->drawCircle(p.p0, p.p1, p.radius, p.color, 32, 0.0f);
                break;
            case Kind::Text:
                renderer->drawText(p.p0, p.text, p.color, 1.0f, 0.0f);
                break;
        }
    }
}

void DebugDrawQueue::Tick(float deltaTime) {
    for (Primitive& p : m_Primitives) {
        if (!p.oneShot) {
            p.remaining -= deltaTime;
        }
    }
    m_Primitives.erase(
        std::remove_if(m_Primitives.begin(), m_Primitives.end(),
            [](const Primitive& p) {
                return p.oneShot || p.remaining <= 0.0f;
            }),
        m_Primitives.end());
}

} // namespace core
} // namespace lupine
