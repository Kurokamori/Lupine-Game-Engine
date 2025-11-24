#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include <glm/mat3x3.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lupine {
namespace math {

    // ========================================
    // Mat3 - 3x3 Matrix
    // ========================================
    
    class Mat3 {
    private:
        glm::mat3 m_Matrix;
        
    public:
        // Constructors
        Mat3() : m_Matrix(1.0f) {}
        Mat3(float diagonal) : m_Matrix(diagonal) {}
        Mat3(const glm::mat3& mat) : m_Matrix(mat) {}
        
        Mat3(float m00, float m01, float m02,
             float m10, float m11, float m12,
             float m20, float m21, float m22) {
            m_Matrix[0][0] = m00; m_Matrix[1][0] = m01; m_Matrix[2][0] = m02;
            m_Matrix[0][1] = m10; m_Matrix[1][1] = m11; m_Matrix[2][1] = m12;
            m_Matrix[0][2] = m20; m_Matrix[1][2] = m21; m_Matrix[2][2] = m22;
        }
        
        // Conversion to GLM
        operator glm::mat3() const { return m_Matrix; }
        glm::mat3 ToGLM() const { return m_Matrix; }
        const glm::mat3& GetGLM() const { return m_Matrix; }
        glm::mat3& GetGLM() { return m_Matrix; }
        
        // Static constructors
        static Mat3 Identity() { return Mat3(1.0f); }
        static Mat3 Zero() { return Mat3(0.0f); }
        
        // Matrix operations
        Mat3 operator+(const Mat3& other) const { return Mat3(m_Matrix + other.m_Matrix); }
        Mat3 operator-(const Mat3& other) const { return Mat3(m_Matrix - other.m_Matrix); }
        Mat3 operator*(const Mat3& other) const { return Mat3(m_Matrix * other.m_Matrix); }
        Mat3 operator*(float scalar) const { return Mat3(m_Matrix * scalar); }
        Mat3 operator/(float scalar) const { return Mat3(m_Matrix / scalar); }
        
        Mat3& operator+=(const Mat3& other) { m_Matrix += other.m_Matrix; return *this; }
        Mat3& operator-=(const Mat3& other) { m_Matrix -= other.m_Matrix; return *this; }
        Mat3& operator*=(const Mat3& other) { m_Matrix *= other.m_Matrix; return *this; }
        Mat3& operator*=(float scalar) { m_Matrix *= scalar; return *this; }
        Mat3& operator/=(float scalar) { m_Matrix /= scalar; return *this; }
        
        // Transform vector
        Vec3 operator*(const Vec3& vec) const {
            glm::vec3 result = m_Matrix * vec.ToGLM();
            return Vec3(result);
        }
        
        // Comparison
        bool operator==(const Mat3& other) const {
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    if (!Equals(m_Matrix[i][j], other.m_Matrix[i][j])) {
                        return false;
                    }
                }
            }
            return true;
        }
        bool operator!=(const Mat3& other) const { return !(*this == other); }
        
        // Access
        glm::vec3& operator[](int index) { return m_Matrix[index]; }
        const glm::vec3& operator[](int index) const { return m_Matrix[index]; }
        
        // Matrix properties
        Mat3 Transposed() const { return Mat3(glm::transpose(m_Matrix)); }
        void Transpose() { m_Matrix = glm::transpose(m_Matrix); }
        
        Mat3 Inverse() const { return Mat3(glm::inverse(m_Matrix)); }
        void Invert() { m_Matrix = glm::inverse(m_Matrix); }
        
        float Determinant() const { return glm::determinant(m_Matrix); }
        
        // Get column/row
        Vec3 GetColumn(int index) const {
            return Vec3(m_Matrix[index][0], m_Matrix[index][1], m_Matrix[index][2]);
        }
        
        Vec3 GetRow(int index) const {
            return Vec3(m_Matrix[0][index], m_Matrix[1][index], m_Matrix[2][index]);
        }
        
        void SetColumn(int index, const Vec3& col) {
            m_Matrix[index][0] = col.x;
            m_Matrix[index][1] = col.y;
            m_Matrix[index][2] = col.z;
        }
        
        void SetRow(int index, const Vec3& row) {
            m_Matrix[0][index] = row.x;
            m_Matrix[1][index] = row.y;
            m_Matrix[2][index] = row.z;
        }
        
        // Static factory methods
        static Mat3 Scale(const Vec3& scale) {
            return Mat3(glm::scale(glm::mat4(1.0f), scale.ToGLM()));
        }
        
        static Mat3 Rotate(float angle, const Vec3& axis) {
            return Mat3(glm::rotate(glm::mat4(1.0f), angle, axis.ToGLM()));
        }
    };
    
    // Scalar * Mat3
    inline Mat3 operator*(float scalar, const Mat3& mat) {
        return mat * scalar;
    }
    
    // Utility functions
    inline Mat3 Transpose(const Mat3& mat) { return mat.Transposed(); }
    inline Mat3 Inverse(const Mat3& mat) { return mat.Inverse(); }
    inline float Determinant(const Mat3& mat) { return mat.Determinant(); }

} // namespace math
} // namespace lupine

