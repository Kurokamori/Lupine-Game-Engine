#pragma once

#include "MathCommon.hpp"
#include <glm/vec3.hpp>

namespace lupine {
namespace math {

    // ========================================
    // Vec3 - 3D Vector
    // ========================================
    
    class Vec3 {
    public:
        float x, y, z;
        
        // Constructors
        Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
        Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}
        Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
        Vec3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}
        
        // Conversion to GLM
        operator glm::vec3() const { return glm::vec3(x, y, z); }
        glm::vec3 ToGLM() const { return glm::vec3(x, y, z); }
        
        // Static constructors
        static Vec3 Zero() { return Vec3(0.0f, 0.0f, 0.0f); }
        static Vec3 One() { return Vec3(1.0f, 1.0f, 1.0f); }
        static Vec3 UnitX() { return Vec3(1.0f, 0.0f, 0.0f); }
        static Vec3 UnitY() { return Vec3(0.0f, 1.0f, 0.0f); }
        static Vec3 UnitZ() { return Vec3(0.0f, 0.0f, 1.0f); }
        static Vec3 Up() { return Vec3(0.0f, 1.0f, 0.0f); }
        static Vec3 Down() { return Vec3(0.0f, -1.0f, 0.0f); }
        static Vec3 Left() { return Vec3(-1.0f, 0.0f, 0.0f); }
        static Vec3 Right() { return Vec3(1.0f, 0.0f, 0.0f); }
        static Vec3 Forward() { return Vec3(0.0f, 0.0f, -1.0f); }
        static Vec3 Back() { return Vec3(0.0f, 0.0f, 1.0f); }
        
        // Arithmetic operators
        Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
        Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
        Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
        Vec3 operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }
        Vec3 operator*(const Vec3& other) const { return Vec3(x * other.x, y * other.y, z * other.z); }
        Vec3 operator/(const Vec3& other) const { return Vec3(x / other.x, y / other.y, z / other.z); }
        Vec3 operator-() const { return Vec3(-x, -y, -z); }
        
        // Compound assignment operators
        Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
        Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
        Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
        Vec3& operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }
        Vec3& operator*=(const Vec3& other) { x *= other.x; y *= other.y; z *= other.z; return *this; }
        Vec3& operator/=(const Vec3& other) { x /= other.x; y /= other.y; z /= other.z; return *this; }
        
        // Comparison operators
        bool operator==(const Vec3& other) const { 
            return Equals(x, other.x) && Equals(y, other.y) && Equals(z, other.z); 
        }
        bool operator!=(const Vec3& other) const { return !(*this == other); }
        
        // Array access
        float& operator[](int index) { return (&x)[index]; }
        const float& operator[](int index) const { return (&x)[index]; }
        
        // Vector operations
        float Length() const { return std::sqrt(x * x + y * y + z * z); }
        float LengthSquared() const { return x * x + y * y + z * z; }
        
        Vec3 Normalized() const {
            float len = Length();
            return IsZero(len) ? Vec3::Zero() : Vec3(x / len, y / len, z / len);
        }
        
        void Normalize() {
            float len = Length();
            if (!IsZero(len)) {
                x /= len;
                y /= len;
                z /= len;
            }
        }
        
        float Dot(const Vec3& other) const { 
            return x * other.x + y * other.y + z * other.z; 
        }
        
        Vec3 Cross(const Vec3& other) const {
            return Vec3(
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            );
        }
        
        float Distance(const Vec3& other) const { return (*this - other).Length(); }
        float DistanceSquared(const Vec3& other) const { return (*this - other).LengthSquared(); }
        
        Vec3 Lerp(const Vec3& other, float t) const {
            return Vec3(
                lupine::math::Lerp(x, other.x, t),
                lupine::math::Lerp(y, other.y, t),
                lupine::math::Lerp(z, other.z, t)
            );
        }
    };
    
    // Scalar * Vec3
    inline Vec3 operator*(float scalar, const Vec3& vec) {
        return vec * scalar;
    }
    
    // Utility functions
    inline float Dot(const Vec3& a, const Vec3& b) { return a.Dot(b); }
    inline Vec3 Cross(const Vec3& a, const Vec3& b) { return a.Cross(b); }
    inline float Distance(const Vec3& a, const Vec3& b) { return a.Distance(b); }
    inline float Length(const Vec3& v) { return v.Length(); }
    inline Vec3 Normalize(const Vec3& v) { return v.Normalized(); }
    inline Vec3 Lerp(const Vec3& a, const Vec3& b, float t) { return a.Lerp(b, t); }

} // namespace math
} // namespace lupine

