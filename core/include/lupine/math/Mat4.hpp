#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include "Vec4.hpp"
#include "Mat3.hpp"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lupine {
namespace math {

    // ========================================
    // Mat4 - 4x4 Matrix
    // ========================================
    
    class Mat4 {
    private:
        glm::mat4 m_Matrix;
        
    public:
        // Constructors
        Mat4() : m_Matrix(1.0f) {}
        Mat4(float diagonal) : m_Matrix(diagonal) {}
        Mat4(const glm::mat4& mat) : m_Matrix(mat) {}
        Mat4(const Mat3& mat3) : m_Matrix(mat3.ToGLM()) {}
        
        // Conversion to GLM
        operator glm::mat4() const { return m_Matrix; }
        glm::mat4 ToGLM() const { return m_Matrix; }
        const glm::mat4& GetGLM() const { return m_Matrix; }
        glm::mat4& GetGLM() { return m_Matrix; }
        
        // Static constructors
        static Mat4 Identity() { return Mat4(1.0f); }
        static Mat4 Zero() { return Mat4(0.0f); }
        
        // Matrix operations
        Mat4 operator+(const Mat4& other) const { return Mat4(m_Matrix + other.m_Matrix); }
        Mat4 operator-(const Mat4& other) const { return Mat4(m_Matrix - other.m_Matrix); }
        Mat4 operator*(const Mat4& other) const { return Mat4(m_Matrix * other.m_Matrix); }
        Mat4 operator*(float scalar) const { return Mat4(m_Matrix * scalar); }
        Mat4 operator/(float scalar) const { return Mat4(m_Matrix / scalar); }
        
        Mat4& operator+=(const Mat4& other) { m_Matrix += other.m_Matrix; return *this; }
        Mat4& operator-=(const Mat4& other) { m_Matrix -= other.m_Matrix; return *this; }
        Mat4& operator*=(const Mat4& other) { m_Matrix *= other.m_Matrix; return *this; }
        Mat4& operator*=(float scalar) { m_Matrix *= scalar; return *this; }
        Mat4& operator/=(float scalar) { m_Matrix /= scalar; return *this; }
        
        // Transform vector (as direction)
        Vec3 TransformDirection(const Vec3& vec) const {
            glm::vec4 result = m_Matrix * glm::vec4(vec.ToGLM(), 0.0f);
            return Vec3(result.x, result.y, result.z);
        }
        
        // Transform point (as position)
        Vec3 TransformPoint(const Vec3& vec) const {
            glm::vec4 result = m_Matrix * glm::vec4(vec.ToGLM(), 1.0f);
            return Vec3(result.x, result.y, result.z);
        }
        
        // Transform Vec4
        Vec4 operator*(const Vec4& vec) const {
            glm::vec4 result = m_Matrix * vec.ToGLM();
            return Vec4(result);
        }
        
        // Comparison
        bool operator==(const Mat4& other) const {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    if (!Equals(m_Matrix[i][j], other.m_Matrix[i][j])) {
                        return false;
                    }
                }
            }
            return true;
        }
        bool operator!=(const Mat4& other) const { return !(*this == other); }
        
        // Access
        glm::vec4& operator[](int index) { return m_Matrix[index]; }
        const glm::vec4& operator[](int index) const { return m_Matrix[index]; }
        
        // Matrix properties
        Mat4 Transposed() const { return Mat4(glm::transpose(m_Matrix)); }
        void Transpose() { m_Matrix = glm::transpose(m_Matrix); }
        
        Mat4 Inverse() const { return Mat4(glm::inverse(m_Matrix)); }
        void Invert() { m_Matrix = glm::inverse(m_Matrix); }
        
        float Determinant() const { return glm::determinant(m_Matrix); }
        
        // Get column/row
        Vec4 GetColumn(int index) const {
            return Vec4(m_Matrix[index][0], m_Matrix[index][1], 
                       m_Matrix[index][2], m_Matrix[index][3]);
        }
        
        Vec4 GetRow(int index) const {
            return Vec4(m_Matrix[0][index], m_Matrix[1][index], 
                       m_Matrix[2][index], m_Matrix[3][index]);
        }
        
        // Extract 3x3 rotation matrix
        Mat3 ToMat3() const {
            return Mat3(glm::mat3(m_Matrix));
        }
        
        // Static factory methods
        static Mat4 Translate(const Vec3& translation) {
            return Mat4(glm::translate(glm::mat4(1.0f), translation.ToGLM()));
        }
        
        static Mat4 Scale(const Vec3& scale) {
            return Mat4(glm::scale(glm::mat4(1.0f), scale.ToGLM()));
        }
        
        static Mat4 Rotate(float angle, const Vec3& axis) {
            return Mat4(glm::rotate(glm::mat4(1.0f), angle, axis.ToGLM()));
        }
        
        static Mat4 TRS(const Vec3& translation, const Vec3& rotation, const Vec3& scale);
    };
    
    // Scalar * Mat4
    inline Mat4 operator*(float scalar, const Mat4& mat) {
        return mat * scalar;
    }
    
    // Utility functions
    inline Mat4 Transpose(const Mat4& mat) { return mat.Transposed(); }
    inline Mat4 Inverse(const Mat4& mat) { return mat.Inverse(); }
    inline float Determinant(const Mat4& mat) { return mat.Determinant(); }

} // namespace math
} // namespace lupine

