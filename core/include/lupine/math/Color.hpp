#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include "Vec4.hpp"
#include <glm/vec4.hpp>
#include <cstdint>

namespace lupine {
namespace math {

    // ========================================
    // Color - RGBA Color
    // ========================================
    
    class Color {
    public:
        float r, g, b, a;
        
        // Constructors
        Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
        Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
        Color(float gray, float a = 1.0f) : r(gray), g(gray), b(gray), a(a) {}
        Color(const Vec3& rgb, float a = 1.0f) : r(rgb.x), g(rgb.y), b(rgb.z), a(a) {}
        Color(const Vec4& rgba) : r(rgba.x), g(rgba.y), b(rgba.z), a(rgba.w) {}
        Color(const glm::vec4& rgba) : r(rgba.r), g(rgba.g), b(rgba.b), a(rgba.a) {}
        
        // Conversion to GLM
        operator glm::vec4() const { return glm::vec4(r, g, b, a); }
        glm::vec4 ToGLM() const { return glm::vec4(r, g, b, a); }
        
        // Conversion to Vec3/Vec4
        Vec3 ToVec3() const { return Vec3(r, g, b); }
        Vec4 ToVec4() const { return Vec4(r, g, b, a); }
        
        // Array access
        float& operator[](int index) { return (&r)[index]; }
        const float& operator[](int index) const { return (&r)[index]; }
        
        // Arithmetic operators
        Color operator+(const Color& other) const {
            return Color(r + other.r, g + other.g, b + other.b, a + other.a);
        }
        
        Color operator-(const Color& other) const {
            return Color(r - other.r, g - other.g, b - other.b, a - other.a);
        }
        
        Color operator*(float scalar) const {
            return Color(r * scalar, g * scalar, b * scalar, a * scalar);
        }
        
        Color operator*(const Color& other) const {
            return Color(r * other.r, g * other.g, b * other.b, a * other.a);
        }
        
        Color operator/(float scalar) const {
            return Color(r / scalar, g / scalar, b / scalar, a / scalar);
        }
        
        Color& operator+=(const Color& other) {
            r += other.r; g += other.g; b += other.b; a += other.a;
            return *this;
        }
        
        Color& operator-=(const Color& other) {
            r -= other.r; g -= other.g; b -= other.b; a -= other.a;
            return *this;
        }
        
        Color& operator*=(float scalar) {
            r *= scalar; g *= scalar; b *= scalar; a *= scalar;
            return *this;
        }
        
        Color& operator*=(const Color& other) {
            r *= other.r; g *= other.g; b *= other.b; a *= other.a;
            return *this;
        }
        
        Color& operator/=(float scalar) {
            r /= scalar; g /= scalar; b /= scalar; a /= scalar;
            return *this;
        }
        
        // Comparison
        bool operator==(const Color& other) const {
            return Equals(r, other.r) && Equals(g, other.g) &&
                   Equals(b, other.b) && Equals(a, other.a);
        }
        
        bool operator!=(const Color& other) const {
            return !(*this == other);
        }
        
        // Utility functions
        Color Clamped() const {
            return Color(
                Saturate(r),
                Saturate(g),
                Saturate(b),
                Saturate(a)
            );
        }
        
        void Clamp() {
            r = Saturate(r);
            g = Saturate(g);
            b = Saturate(b);
            a = Saturate(a);
        }
        
        Color Lerp(const Color& other, float t) const {
            return Color(
                lupine::math::Lerp(r, other.r, t),
                lupine::math::Lerp(g, other.g, t),
                lupine::math::Lerp(b, other.b, t),
                lupine::math::Lerp(a, other.a, t)
            );
        }
        
        // Convert to 32-bit RGBA (0-255 per channel)
        uint32_t ToRGBA32() const {
            uint8_t ir = static_cast<uint8_t>(Saturate(r) * 255.0f);
            uint8_t ig = static_cast<uint8_t>(Saturate(g) * 255.0f);
            uint8_t ib = static_cast<uint8_t>(Saturate(b) * 255.0f);
            uint8_t ia = static_cast<uint8_t>(Saturate(a) * 255.0f);
            return (ir << 24) | (ig << 16) | (ib << 8) | ia;
        }
        
        // Create from 32-bit RGBA
        static Color FromRGBA32(uint32_t rgba) {
            return Color(
                ((rgba >> 24) & 0xFF) / 255.0f,
                ((rgba >> 16) & 0xFF) / 255.0f,
                ((rgba >> 8) & 0xFF) / 255.0f,
                (rgba & 0xFF) / 255.0f
            );
        }
        
        // Predefined colors
        static Color White()   { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static Color Black()   { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static Color Red()     { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
        static Color Green()   { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
        static Color Blue()    { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
        static Color Yellow()  { return Color(1.0f, 1.0f, 0.0f, 1.0f); }
        static Color Cyan()    { return Color(0.0f, 1.0f, 1.0f, 1.0f); }
        static Color Magenta() { return Color(1.0f, 0.0f, 1.0f, 1.0f); }
        static Color Purple()  { return Color(0.5f, 0.0f, 0.5f, 1.0f); }
        static Color Gray()    { return Color(0.5f, 0.5f, 0.5f, 1.0f); }
        static Color Clear()   { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
    };
    
    // Scalar * Color
    inline Color operator*(float scalar, const Color& color) {
        return color * scalar;
    }
    
    // Utility functions
    inline Color Lerp(const Color& a, const Color& b, float t) {
        return a.Lerp(b, t);
    }

} // namespace math
} // namespace lupine

