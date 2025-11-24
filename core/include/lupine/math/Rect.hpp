#pragma once

#include "Vec2.hpp"

namespace lupine {
namespace math {

/**
 * Rect - Simple 2D rectangle structure
 * Used for UI layout and positioning
 */
struct Rect {
    Vec2 position;  // Top-left position
    Vec2 size;      // Width and height

    Rect()
        : position(0.0f, 0.0f)
        , size(0.0f, 0.0f)
    {}

    Rect(const Vec2& pos, const Vec2& sz)
        : position(pos)
        , size(sz)
    {}

    Rect(float x, float y, float width, float height)
        : position(x, y)
        , size(width, height)
    {}

    // Get center point of the rectangle
    Vec2 GetCenter() const {
        return Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
    }

    // Get corners
    Vec2 GetTopLeft() const { return position; }
    Vec2 GetTopRight() const { return Vec2(position.x + size.x, position.y); }
    Vec2 GetBottomLeft() const { return Vec2(position.x, position.y + size.y); }
    Vec2 GetBottomRight() const { return Vec2(position.x + size.x, position.y + size.y); }

    // Check if a point is inside the rectangle
    bool Contains(const Vec2& point) const {
        return point.x >= position.x && point.x <= position.x + size.x &&
               point.y >= position.y && point.y <= position.y + size.y;
    }

    // Check if this rectangle intersects another
    bool Intersects(const Rect& other) const {
        return position.x < other.position.x + other.size.x &&
               position.x + size.x > other.position.x &&
               position.y < other.position.y + other.size.y &&
               position.y + size.y > other.position.y;
    }

    // Get the area of the rectangle
    float GetArea() const {
        return size.x * size.y;
    }

    // Expand the rectangle by a margin on all sides
    Rect Expanded(float margin) const {
        return Rect(
            position.x - margin,
            position.y - margin,
            size.x + margin * 2.0f,
            size.y + margin * 2.0f
        );
    }

    // Contract the rectangle by a margin on all sides
    Rect Contracted(float margin) const {
        return Expanded(-margin);
    }

    // Get the intersection of two rectangles
    Rect GetIntersection(const Rect& other) const {
        float left = std::max(position.x, other.position.x);
        float top = std::max(position.y, other.position.y);
        float right = std::min(position.x + size.x, other.position.x + other.size.x);
        float bottom = std::min(position.y + size.y, other.position.y + other.size.y);

        if (right > left && bottom > top) {
            return Rect(left, top, right - left, bottom - top);
        }

        return Rect(); // Empty rectangle
    }

    // Equality operators
    bool operator==(const Rect& other) const {
        return position == other.position && size == other.size;
    }

    bool operator!=(const Rect& other) const {
        return !(*this == other);
    }
};

} // namespace math
} // namespace lupine
