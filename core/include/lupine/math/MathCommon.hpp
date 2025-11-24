#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cmath>
#include <limits>

namespace lupine {
namespace math {

    // ========================================
    // Constants
    // ========================================
    
    constexpr float PI = glm::pi<float>();
    constexpr float TWO_PI = glm::two_pi<float>();
    constexpr float HALF_PI = glm::half_pi<float>();
    constexpr float EPSILON = 1e-6f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;
    
    constexpr double PI_D = glm::pi<double>();
    constexpr double TWO_PI_D = glm::two_pi<double>();
    constexpr double HALF_PI_D = glm::half_pi<double>();
    constexpr double EPSILON_D = 1e-12;
    constexpr double DEG_TO_RAD_D = PI_D / 180.0;
    constexpr double RAD_TO_DEG_D = 180.0 / PI_D;

    // ========================================
    // Conversion Functions
    // ========================================
    
    inline float Radians(float degrees) {
        return degrees * DEG_TO_RAD;
    }
    
    inline float Degrees(float radians) {
        return radians * RAD_TO_DEG;
    }
    
    inline double Radians(double degrees) {
        return degrees * DEG_TO_RAD_D;
    }
    
    inline double Degrees(double radians) {
        return radians * RAD_TO_DEG_D;
    }

    // ========================================
    // Epsilon Comparison Functions
    // ========================================
    
    inline bool Equals(float a, float b, float epsilon = EPSILON) {
        return std::abs(a - b) <= epsilon;
    }
    
    inline bool Equals(double a, double b, double epsilon = EPSILON_D) {
        return std::abs(a - b) <= epsilon;
    }
    
    inline bool IsZero(float value, float epsilon = EPSILON) {
        return std::abs(value) <= epsilon;
    }
    
    inline bool IsZero(double value, double epsilon = EPSILON_D) {
        return std::abs(value) <= epsilon;
    }

    // ========================================
    // Utility Functions
    // ========================================
    
    template<typename T>
    inline T Clamp(T value, T min, T max) {
        return value < min ? min : (value > max ? max : value);
    }
    
    template<typename T>
    inline T Lerp(T a, T b, float t) {
        return a + (b - a) * t;
    }
    
    template<typename T>
    inline T Min(T a, T b) {
        return a < b ? a : b;
    }
    
    template<typename T>
    inline T Max(T a, T b) {
        return a > b ? a : b;
    }
    
    inline float Saturate(float value) {
        return Clamp(value, 0.0f, 1.0f);
    }
    
    inline double Saturate(double value) {
        return Clamp(value, 0.0, 1.0);
    }
    
    inline float Sign(float value) {
        return value < 0.0f ? -1.0f : (value > 0.0f ? 1.0f : 0.0f);
    }
    
    inline double Sign(double value) {
        return value < 0.0 ? -1.0 : (value > 0.0 ? 1.0 : 0.0);
    }

    inline float Pow(float base, float exponent) {
        return std::pow(base, exponent);
    }

    inline double Pow(double base, double exponent) {
        return std::pow(base, exponent);
    }

    inline float Floor(float value) {
        return std::floor(value);
    }

    inline double Floor(double value) {
        return std::floor(value);
    }

    // ========================================
    // Angle Normalization
    // ========================================
    
    inline float NormalizeAngle(float angle) {
        angle = std::fmod(angle, TWO_PI);
        if (angle < 0.0f) angle += TWO_PI;
        return angle;
    }
    
    inline double NormalizeAngle(double angle) {
        angle = std::fmod(angle, TWO_PI_D);
        if (angle < 0.0) angle += TWO_PI_D;
        return angle;
    }

} // namespace math
} // namespace lupine

