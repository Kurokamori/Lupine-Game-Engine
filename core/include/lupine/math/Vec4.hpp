#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include <glm/vec4.hpp>

namespace lupine {
namespace math {

    // ========================================
    // Vec4 - 4D Vector
    // ========================================
    
    class Vec4 {
    public:
        float x, y, z, w;
        
        // Constructors
        Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
        Vec4(float scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}
        Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
        Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
        Vec4(const glm::vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
        
        // Conversion to GLM
        operator glm::vec4() const { return glm::vec4(x, y, z, w); }
        glm::vec4 ToGLM() const { return glm::vec4(x, y, z, w); }
        
        // Conversion to Vec3
        Vec3 ToVec3() const { return Vec3(x, y, z); }
        Vec3 xyz() const { return Vec3(x, y, z); }
        
        // Static constructors
        static Vec4 Zero() { return Vec4(0.0f, 0.0f, 0.0f, 0.0f); }
        static Vec4 One() { return Vec4(1.0f, 1.0f, 1.0f, 1.0f); }
        static Vec4 UnitX() { return Vec4(1.0f, 0.0f, 0.0f, 0.0f); }
        static Vec4 UnitY() { return Vec4(0.0f, 1.0f, 0.0f, 0.0f); }
        static Vec4 UnitZ() { return Vec4(0.0f, 0.0f, 1.0f, 0.0f); }
        static Vec4 UnitW() { return Vec4(0.0f, 0.0f, 0.0f, 1.0f); }
        
        // Arithmetic operators
        Vec4 operator+(const Vec4& other) const { 
            return Vec4(x + other.x, y + other.y, z + other.z, w + other.w); 
        }
        Vec4 operator-(const Vec4& other) const { 
            return Vec4(x - other.x, y - other.y, z - other.z, w - other.w); 
        }
        Vec4 operator*(float scalar) const { 
            return Vec4(x * scalar, y * scalar, z * scalar, w * scalar); 
        }
        Vec4 operator/(float scalar) const { 
            return Vec4(x / scalar, y / scalar, z / scalar, w / scalar); 
        }
        Vec4 operator*(const Vec4& other) const { 
            return Vec4(x * other.x, y * other.y, z * other.z, w * other.w); 
        }
        Vec4 operator/(const Vec4& other) const { 
            return Vec4(x / other.x, y / other.y, z / other.z, w / other.w); 
        }
        Vec4 operator-() const { return Vec4(-x, -y, -z, -w); }
        
        // Compound assignment operators
        Vec4& operator+=(const Vec4& other) { 
            x += other.x; y += other.y; z += other.z; w += other.w; return *this; 
        }
        Vec4& operator-=(const Vec4& other) { 
            x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this; 
        }
        Vec4& operator*=(float scalar) { 
            x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; 
        }
        Vec4& operator/=(float scalar) { 
            x /= scalar; y /= scalar; z /= scalar; w /= scalar; return *this; 
        }
        Vec4& operator*=(const Vec4& other) { 
            x *= other.x; y *= other.y; z *= other.z; w *= other.w; return *this; 
        }
        Vec4& operator/=(const Vec4& other) { 
            x /= other.x; y /= other.y; z /= other.z; w /= other.w; return *this; 
        }
        
        // Comparison operators
        bool operator==(const Vec4& other) const { 
            return Equals(x, other.x) && Equals(y, other.y) && 
                   Equals(z, other.z) && Equals(w, other.w); 
        }
        bool operator!=(const Vec4& other) const { return !(*this == other); }
        
        // Array access
        float& operator[](int index) { return (&x)[index]; }
        const float& operator[](int index) const { return (&x)[index]; }
        
        // Vector operations
        float Length() const { return std::sqrt(x * x + y * y + z * z + w * w); }
        float LengthSquared() const { return x * x + y * y + z * z + w * w; }
        
        Vec4 Normalized() const {
            float len = Length();
            return IsZero(len) ? Vec4::Zero() : Vec4(x / len, y / len, z / len, w / len);
        }
        
        void Normalize() {
            float len = Length();
            if (!IsZero(len)) {
                x /= len;
                y /= len;
                z /= len;
                w /= len;
            }
        }
        
        float Dot(const Vec4& other) const { 
            return x * other.x + y * other.y + z * other.z + w * other.w; 
        }
        
        Vec4 Lerp(const Vec4& other, float t) const {
            return Vec4(
                lupine::math::Lerp(x, other.x, t),
                lupine::math::Lerp(y, other.y, t),
                lupine::math::Lerp(z, other.z, t),
                lupine::math::Lerp(w, other.w, t)
            );
        }
    };
    
    // Scalar * Vec4
    inline Vec4 operator*(float scalar, const Vec4& vec) {
        return vec * scalar;
    }
    
    // Utility functions
    inline float Dot(const Vec4& a, const Vec4& b) { return a.Dot(b); }
    inline Vec4 Normalize(const Vec4& v) { return v.Normalized(); }
    inline Vec4 Lerp(const Vec4& a, const Vec4& b, float t) { return a.Lerp(b, t); }

} // namespace math
} // namespace lupine

