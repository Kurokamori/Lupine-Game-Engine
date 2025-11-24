#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include "Mat3.hpp"
#include "Mat4.hpp"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace lupine {
namespace math {

    // Forward declaration
    class Mat4;
    
    // ========================================
    // Quat - Quaternion for rotations
    // ========================================
    
    class Quat {
    private:
        glm::quat m_Quat;
        
    public:
        // Constructors
        Quat() : m_Quat(1.0f, 0.0f, 0.0f, 0.0f) {}
        Quat(float w, float x, float y, float z) : m_Quat(w, x, y, z) {}
        Quat(const glm::quat& q) : m_Quat(q) {}
        
        // Conversion to GLM
        operator glm::quat() const { return m_Quat; }
        glm::quat ToGLM() const { return m_Quat; }
        const glm::quat& GetGLM() const { return m_Quat; }
        glm::quat& GetGLM() { return m_Quat; }
        
        // Component access
        float w() const { return m_Quat.w; }
        float x() const { return m_Quat.x; }
        float y() const { return m_Quat.y; }
        float z() const { return m_Quat.z; }
        
        float& w() { return m_Quat.w; }
        float& x() { return m_Quat.x; }
        float& y() { return m_Quat.y; }
        float& z() { return m_Quat.z; }
        
        // Static constructors
        static Quat Identity() { return Quat(1.0f, 0.0f, 0.0f, 0.0f); }
        
        // Create from axis-angle
        static Quat FromAxisAngle(const Vec3& axis, float angle) {
            return Quat(glm::angleAxis(angle, axis.ToGLM()));
        }
        
        // Create from Euler angles (in radians)
        static Quat FromEuler(float pitch, float yaw, float roll) {
            return Quat(glm::quat(glm::vec3(pitch, yaw, roll)));
        }
        
        static Quat FromEuler(const Vec3& euler) {
            return FromEuler(euler.x, euler.y, euler.z);
        }
        
        // Create from rotation matrix
        static Quat FromMatrix(const Mat3& mat) {
            return Quat(glm::quat_cast(mat.ToGLM()));
        }
        
        static Quat FromMatrix(const Mat4& mat) {
            return Quat(glm::quat_cast(mat.ToGLM()));
        }
        
        // Create rotation from one vector to another
        static Quat FromToRotation(const Vec3& from, const Vec3& to) {
            return Quat(glm::rotation(from.ToGLM(), to.ToGLM()));
        }
        
        // Look rotation
        static Quat LookRotation(const Vec3& forward, const Vec3& up = Vec3::Up()) {
            return Quat(glm::quatLookAt(forward.ToGLM(), up.ToGLM()));
        }
        
        // Quaternion operations
        Quat operator*(const Quat& other) const {
            return Quat(m_Quat * other.m_Quat);
        }
        
        Quat operator*(float scalar) const {
            return Quat(m_Quat * scalar);
        }
        
        Quat& operator*=(const Quat& other) {
            m_Quat *= other.m_Quat;
            return *this;
        }
        
        Quat& operator*=(float scalar) {
            m_Quat *= scalar;
            return *this;
        }
        
        // Rotate vector
        Vec3 operator*(const Vec3& vec) const {
            glm::vec3 result = m_Quat * vec.ToGLM();
            return Vec3(result);
        }
        
        // Comparison
        bool operator==(const Quat& other) const {
            return Equals(m_Quat.w, other.m_Quat.w) &&
                   Equals(m_Quat.x, other.m_Quat.x) &&
                   Equals(m_Quat.y, other.m_Quat.y) &&
                   Equals(m_Quat.z, other.m_Quat.z);
        }
        bool operator!=(const Quat& other) const { return !(*this == other); }
        
        // Quaternion properties
        float Length() const { return glm::length(m_Quat); }
        float LengthSquared() const { return glm::dot(m_Quat, m_Quat); }
        
        Quat Normalized() const { return Quat(glm::normalize(m_Quat)); }
        void Normalize() { m_Quat = glm::normalize(m_Quat); }
        
        Quat Conjugate() const { return Quat(glm::conjugate(m_Quat)); }
        Quat Inverse() const { return Quat(glm::inverse(m_Quat)); }
        
        float Dot(const Quat& other) const { return glm::dot(m_Quat, other.m_Quat); }
        
        // Conversions
        Vec3 ToEuler() const {
            glm::vec3 euler = glm::eulerAngles(m_Quat);
            return Vec3(euler);
        }
        
        Mat3 ToMat3() const {
            return Mat3(glm::mat3_cast(m_Quat));
        }
        
        Mat4 ToMat4() const {
            return Mat4(glm::mat4_cast(m_Quat));
        }
        
        // Get axis and angle
        void ToAxisAngle(Vec3& axis, float& angle) const {
            angle = glm::angle(m_Quat);
            axis = Vec3(glm::axis(m_Quat));
        }

        // Interpolation
        Quat Slerp(const Quat& other, float t) const {
            return Quat(glm::slerp(m_Quat, other.m_Quat, t));
        }

        // Get forward/up/right vectors
        Vec3 GetForward() const { return *this * Vec3::Forward(); }
        Vec3 GetUp() const { return *this * Vec3::Up(); }
        Vec3 GetRight() const { return *this * Vec3::Right(); }
    };

    // Scalar * Quat
    inline Quat operator*(float scalar, const Quat& quat) {
        return quat * scalar;
    }

    // Utility functions
    inline Quat Normalize(const Quat& q) { return q.Normalized(); }
    inline float Dot(const Quat& a, const Quat& b) { return a.Dot(b); }
    inline Quat Slerp(const Quat& a, const Quat& b, float t) { return a.Slerp(b, t); }

} // namespace math
} // namespace lupine

