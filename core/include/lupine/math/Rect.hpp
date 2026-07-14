#pragma once

#include "Vec2.hpp"

namespace lupine {
namespace math {

/**
 * Rect - Simple 2D rectangle structure
 * Used for UI layout and positioning.
 *
 * COORDINATE CONVENTION
 * ---------------------
 * `position` is the MINIMUM corner (minimum x, minimum y) and `size` is always
 * non-negative, so the rectangle spans [position, position + size] on both axes.
 *
 * The 2D/UI canvas this type is used with is Y-UP: +Y points toward the top of the
 * screen. `position.y` is therefore the BOTTOM edge of the rect and
 * `position.y + size.y` is the TOP edge -- the opposite of a Y-down screen-space
 * convention.
 *
 * Do not open-code `position.y + ...` when you mean an edge. Use the named
 * accessors below: GetTopEdge()/GetBottomEdge() and GetTopLeft()/GetBottomLeft()
 * are all expressed in canvas (Y-up) terms, so code reads the way it behaves.
 * GetMinY()/GetMaxY() are the convention-free spellings for math that genuinely
 * does not care which end is "up".
 */
struct Rect {
    Vec2 position;  // Minimum corner (min x, min y). In the Y-up canvas this is the BOTTOM-left.
    Vec2 size;      // Width and height (non-negative)

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

    /**
     * Build a rect from canvas (Y-up) edges, where top >= bottom and right >= left.
     * Degenerate/inverted inputs collapse to zero extent rather than producing a
     * negative size.
     */
    static Rect FromEdges(float left, float top, float right, float bottom) {
        float width = right - left;
        float height = top - bottom;
        if (width < 0.0f) { width = 0.0f; }
        if (height < 0.0f) { height = 0.0f; }
        return Rect(left, bottom, width, height);
    }

    // Get center point of the rectangle
    Vec2 GetCenter() const {
        return Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
    }

    // Convention-free extents.
    float GetMinX() const { return position.x; }
    float GetMaxX() const { return position.x + size.x; }
    float GetMinY() const { return position.y; }
    float GetMaxY() const { return position.y + size.y; }

    // Canvas-oriented (Y-up) edges: the top edge is the MAXIMUM y.
    float GetLeftEdge() const { return GetMinX(); }
    float GetRightEdge() const { return GetMaxX(); }
    float GetTopEdge() const { return GetMaxY(); }
    float GetBottomEdge() const { return GetMinY(); }

    // Canvas-oriented (Y-up) corners.
    Vec2 GetTopLeft() const { return Vec2(GetMinX(), GetMaxY()); }
    Vec2 GetTopRight() const { return Vec2(GetMaxX(), GetMaxY()); }
    Vec2 GetBottomLeft() const { return Vec2(GetMinX(), GetMinY()); }
    Vec2 GetBottomRight() const { return Vec2(GetMaxX(), GetMinY()); }

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

    /**
     * The overlap of two rects, or a zero-extent rect when they do not overlap.
     *
     * Deliberately spelled in convention-free min/max terms: the arithmetic is the same
     * either way up, but the names are not. This used to call the two Y bounds "top" and
     * "bottom" with top = max(minY) -- which is the BOTTOM edge on the Y-up canvas -- and
     * that mislabelling is exactly the confusion that flipped half the container family.
     */
    Rect GetIntersection(const Rect& other) const {
        float minX = std::max(GetMinX(), other.GetMinX());
        float minY = std::max(GetMinY(), other.GetMinY());
        float maxX = std::min(GetMaxX(), other.GetMaxX());
        float maxY = std::min(GetMaxY(), other.GetMaxY());

        if (maxX > minX && maxY > minY) {
            return Rect(minX, minY, maxX - minX, maxY - minY);
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
