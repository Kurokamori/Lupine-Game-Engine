#pragma once

// Lupine Engine Math Library
// Comprehensive math types and utilities for game development

// Core math utilities and constants
#include "MathCommon.hpp"

// Vector types
#include "Vec2.hpp"
#include "Vec3.hpp"
#include "Vec4.hpp"

// Matrix types
#include "Mat3.hpp"
#include "Mat4.hpp"

// Quaternion
#include "Quat.hpp"

// Transform
#include "Transform.hpp"

// Color
#include "Color.hpp"

// Geometry helpers
#include "AABB.hpp"
#include "OBB.hpp"
#include "Ray.hpp"
#include "Plane.hpp"
#include "Rect.hpp"

// Camera utilities
#include "Camera.hpp"

namespace lupine {
namespace math {

    // ========================================
    // Additional Utility Functions
    // ========================================
    
    // Smooth interpolation functions
    inline float SmoothStep(float edge0, float edge1, float x) {
        float t = Saturate((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }
    
    inline float SmootherStep(float edge0, float edge1, float x) {
        float t = Saturate((x - edge0) / (edge1 - edge0));
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }
    
    // Inverse lerp
    inline float InverseLerp(float a, float b, float value) {
        if (Equals(a, b)) return 0.0f;
        return Saturate((value - a) / (b - a));
    }
    
    // Remap value from one range to another
    inline float Remap(float value, float fromMin, float fromMax, float toMin, float toMax) {
        float t = InverseLerp(fromMin, fromMax, value);
        return Lerp(toMin, toMax, t);
    }
    
    // Angle utilities
    inline float AngleDifference(float a, float b) {
        float diff = std::fmod(b - a + PI, TWO_PI) - PI;
        return diff < -PI ? diff + TWO_PI : diff;
    }
    
    inline float LerpAngle(float a, float b, float t) {
        float diff = AngleDifference(a, b);
        return a + diff * t;
    }
    
    // Move towards
    inline float MoveTowards(float current, float target, float maxDelta) {
        if (std::abs(target - current) <= maxDelta) {
            return target;
        }
        return current + Sign(target - current) * maxDelta;
    }
    
    inline Vec3 MoveTowards(const Vec3& current, const Vec3& target, float maxDistanceDelta) {
        Vec3 diff = target - current;
        float distance = diff.Length();
        
        if (distance <= maxDistanceDelta || IsZero(distance)) {
            return target;
        }
        
        return current + diff / distance * maxDistanceDelta;
    }
    
    // Approximately equal (using epsilon)
    inline bool Approximately(float a, float b) {
        return Equals(a, b);
    }
    
    // Repeat value in range [0, length)
    inline float Repeat(float value, float length) {
        return Clamp(value - std::floor(value / length) * length, 0.0f, length);
    }
    
    // Ping-pong value between 0 and length
    inline float PingPong(float value, float length) {
        value = Repeat(value, length * 2.0f);
        return length - std::abs(value - length);
    }
    
    // Delta angle (shortest difference between two angles)
    inline float DeltaAngle(float current, float target) {
        return AngleDifference(current, target);
    }

} // namespace math

// Bring math namespace into lupine namespace for convenience
using namespace math;

} // namespace lupine

