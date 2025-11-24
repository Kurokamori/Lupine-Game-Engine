#pragma once

#include "MathCommon.hpp"
#include <glm/vec2.hpp>

namespace lupine {
namespace math {

    // ========================================
    // Vec2 - 2D Vector
    // ========================================
    
    class Vec2 {
    public:
        float x, y;
        
        // Constructors
        Vec2() : x(0.0f), y(0.0f) {}
        Vec2(float scalar) : x(scalar), y(scalar) {}
        Vec2(float x, float y) : x(x), y(y) {}
        Vec2(const glm::vec2& v) : x(v.x), y(v.y) {}
        
        // Conversion to GLM
        operator glm::vec2() const { return glm::vec2(x, y); }
        glm::vec2 ToGLM() const { return glm::vec2(x, y); }
        
        // Static constructors
        static Vec2 Zero() { return Vec2(0.0f, 0.0f); }
        static Vec2 One() { return Vec2(1.0f, 1.0f); }
        static Vec2 UnitX() { return Vec2(1.0f, 0.0f); }
        static Vec2 UnitY() { return Vec2(0.0f, 1.0f); }
        
        // Arithmetic operators
        Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
        Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
        Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
        Vec2 operator/(float scalar) const { return Vec2(x / scalar, y / scalar); }
        Vec2 operator*(const Vec2& other) const { return Vec2(x * other.x, y * other.y); }
        Vec2 operator/(const Vec2& other) const { return Vec2(x / other.x, y / other.y); }
        Vec2 operator-() const { return Vec2(-x, -y); }
        
        // Compound assignment operators
        Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
        Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
        Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
        Vec2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }
        Vec2& operator*=(const Vec2& other) { x *= other.x; y *= other.y; return *this; }
        Vec2& operator/=(const Vec2& other) { x /= other.x; y /= other.y; return *this; }
        
        // Comparison operators
        bool operator==(const Vec2& other) const { return Equals(x, other.x) && Equals(y, other.y); }
        bool operator!=(const Vec2& other) const { return !(*this == other); }
        
        // Array access
        float& operator[](int index) { return (&x)[index]; }
        const float& operator[](int index) const { return (&x)[index]; }
        
        // Vector operations
        float Length() const { return std::sqrt(x * x + y * y); }
        float LengthSquared() const { return x * x + y * y; }
        
        Vec2 Normalized() const {
            float len = Length();
            return IsZero(len) ? Vec2::Zero() : Vec2(x / len, y / len);
        }
        
        void Normalize() {
            float len = Length();
            if (!IsZero(len)) {
                x /= len;
                y /= len;
            }
        }
        
        float Dot(const Vec2& other) const { return x * other.x + y * other.y; }
        
        // 2D cross product returns scalar (z-component of 3D cross)
        float Cross(const Vec2& other) const { return x * other.y - y * other.x; }
        
        float Distance(const Vec2& other) const { return (*this - other).Length(); }
        float DistanceSquared(const Vec2& other) const { return (*this - other).LengthSquared(); }
        
        Vec2 Lerp(const Vec2& other, float t) const {
            return Vec2(
                lupine::math::Lerp(x, other.x, t),
                lupine::math::Lerp(y, other.y, t)
            );
        }
        
        Vec2 Perpendicular() const { return Vec2(-y, x); }
        
        float Angle() const { return std::atan2(y, x); }
        float AngleTo(const Vec2& other) const {
            return std::atan2(Cross(other), Dot(other));
        }
    };
    
    // Scalar * Vec2
    inline Vec2 operator*(float scalar, const Vec2& vec) {
        return vec * scalar;
    }
    
    // Utility functions
    inline float Dot(const Vec2& a, const Vec2& b) { return a.Dot(b); }
    inline float Cross(const Vec2& a, const Vec2& b) { return a.Cross(b); }
    inline float Distance(const Vec2& a, const Vec2& b) { return a.Distance(b); }
    inline Vec2 Normalize(const Vec2& v) { return v.Normalized(); }
    inline Vec2 Lerp(const Vec2& a, const Vec2& b, float t) { return a.Lerp(b, t); }

} // namespace math
} // namespace lupine

