#pragma once

#include "lupine/math/Vec3.hpp"
#include "lupine/math/Color.hpp"
#include <string>
#include <vector>

namespace lupine {

class DebugRenderer;

namespace core {

/**
 * DebugDrawQueue - user/script immediate-mode debug primitives.
 *
 * Gameplay code and scripts push debug primitives here (via ScriptAPI debug_draw_*).
 * Unlike the editor-only DebugDraw overlay (grids/gizmos/selection), this queue is
 * drained by the renderer for every active view regardless of the editor debug flag,
 * so debug_draw_* works in shipped runtime builds and never draws editor chrome.
 *
 * Lifetime: a primitive with duration <= 0 is "one-shot" — drawn for the frame it
 * was submitted in, then dropped. A positive duration keeps it alive that many
 * seconds. Tick() ages the queue once per frame (from SceneManager::Update) and is
 * what expires primitives; Submit() (re)draws every live primitive into a renderer.
 */
class DebugDrawQueue {
public:
    static DebugDrawQueue& Get();

    void Line(const math::Vec3& start, const math::Vec3& end, const math::Color& color, float duration = 0.0f);
    void Ray(const math::Vec3& origin, const math::Vec3& direction, const math::Color& color, float duration = 0.0f);
    void Arrow(const math::Vec3& start, const math::Vec3& end, const math::Color& color, float duration = 0.0f);
    void Box(const math::Vec3& center, const math::Vec3& size, const math::Color& color, float duration = 0.0f);
    void Sphere(const math::Vec3& center, float radius, const math::Color& color, float duration = 0.0f);
    void Circle(const math::Vec3& center, const math::Vec3& normal, float radius, const math::Color& color, float duration = 0.0f);
    void Text(const math::Vec3& position, const std::string& text, const math::Color& color, float duration = 0.0f);

    bool Empty() const { return m_Primitives.empty(); }
    size_t Size() const { return m_Primitives.size(); }
    void Clear() { m_Primitives.clear(); }

    // Draw every live primitive into the renderer (as one-frame draws; this queue
    // owns lifetime, not the renderer). Safe to call once per view per frame.
    void Submit(DebugRenderer* renderer) const;

    // Age the queue by deltaTime: drop one-shot primitives drawn last frame and
    // expire timed ones. Call once per frame, before primitives are pushed.
    void Tick(float deltaTime);

private:
    DebugDrawQueue() = default;

    enum class Kind { Line, Ray, Arrow, Box, Sphere, Circle, Text };

    struct Primitive {
        Kind kind;
        math::Vec3 p0;            // start / origin / center / position
        math::Vec3 p1;            // end / direction / size / normal
        float radius = 0.0f;      // sphere / circle
        math::Color color;
        std::string text;         // text only
        float remaining = 0.0f;   // seconds left for timed primitives
        bool oneShot = true;      // duration <= 0
    };

    std::vector<Primitive> m_Primitives;
};

} // namespace core
} // namespace lupine
