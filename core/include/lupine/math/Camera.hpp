#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include "Vec4.hpp"
#include "Mat4.hpp"
#include "Ray.hpp"
#include "Transform.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace lupine {
namespace math {

    // ========================================
    // Camera Helper Functions
    // ========================================
    
    namespace Camera {
        
        // ========================================
        // Projection Matrices (OpenGL: Z range [-1, 1])
        // ========================================

        // Create perspective projection matrix (OpenGL convention: Z in [-1, 1])
        inline Mat4 Perspective(float fovY, float aspectRatio, float nearPlane, float farPlane) {
            return Mat4(glm::perspective(fovY, aspectRatio, nearPlane, farPlane));
        }

        // Create perspective projection matrix with FOV in degrees
        inline Mat4 PerspectiveDeg(float fovYDegrees, float aspectRatio, float nearPlane, float farPlane) {
            return Perspective(Radians(fovYDegrees), aspectRatio, nearPlane, farPlane);
        }

        // Create orthographic projection matrix (OpenGL convention: Z in [-1, 1])
        inline Mat4 Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
            return Mat4(glm::ortho(left, right, bottom, top, nearPlane, farPlane));
        }

        // Create orthographic projection matrix from width/height
        inline Mat4 OrthographicCentered(float width, float height, float nearPlane, float farPlane) {
            float halfWidth = width * 0.5f;
            float halfHeight = height * 0.5f;
            return Orthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
        }

        // Create infinite perspective projection (far plane at infinity)
        inline Mat4 InfinitePerspective(float fovY, float aspectRatio, float nearPlane) {
            return Mat4(glm::infinitePerspective(fovY, aspectRatio, nearPlane));
        }

        // ========================================
        // Projection Matrices (DirectX/Vulkan: Z range [0, 1])
        // ========================================

        // Create perspective projection matrix (DirectX/Vulkan convention: Z in [0, 1])
        inline Mat4 PerspectiveZO(float fovY, float aspectRatio, float nearPlane, float farPlane) {
            return Mat4(glm::perspectiveRH_ZO(fovY, aspectRatio, nearPlane, farPlane));
        }

        // Create perspective projection matrix with FOV in degrees (DirectX/Vulkan)
        inline Mat4 PerspectiveDegZO(float fovYDegrees, float aspectRatio, float nearPlane, float farPlane) {
            return PerspectiveZO(Radians(fovYDegrees), aspectRatio, nearPlane, farPlane);
        }

        // Create orthographic projection matrix (DirectX/Vulkan convention: Z in [0, 1])
        inline Mat4 OrthographicZO(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
            return Mat4(glm::orthoRH_ZO(left, right, bottom, top, nearPlane, farPlane));
        }

        // Create orthographic projection matrix from width/height (DirectX/Vulkan)
        inline Mat4 OrthographicCenteredZO(float width, float height, float nearPlane, float farPlane) {
            float halfWidth = width * 0.5f;
            float halfHeight = height * 0.5f;
            return OrthographicZO(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
        }
        
        // ========================================
        // View Matrices
        // ========================================
        
        // Create view matrix from position and target
        inline Mat4 LookAt(const Vec3& position, const Vec3& target, const Vec3& up = Vec3::Up()) {
            return Mat4(glm::lookAt(position.ToGLM(), target.ToGLM(), up.ToGLM()));
        }
        
        // Create view matrix from transform
        inline Mat4 ViewFromTransform(const Transform& transform) {
            Vec3 position = transform.position;
            Vec3 forward = transform.GetForward();
            Vec3 target = position + forward;
            Vec3 up = transform.GetUp();
            return LookAt(position, target, up);
        }
        
        // Create view matrix from position and rotation
        inline Mat4 ViewFromPositionRotation(const Vec3& position, const Quat& rotation) {
            Transform transform;
            transform.position = position;
            transform.rotation = rotation;
            return ViewFromTransform(transform);
        }
        
        // Create view matrix from position and Euler angles (in radians)
        inline Mat4 ViewFromPositionEuler(const Vec3& position, const Vec3& euler) {
            Transform transform;
            transform.position = position;
            transform.SetEulerAngles(euler);
            return ViewFromTransform(transform);
        }
        
        // Create view matrix from position and Euler angles (in degrees)
        inline Mat4 ViewFromPositionEulerDeg(const Vec3& position, const Vec3& eulerDegrees) {
            Vec3 eulerRadians(
                Radians(eulerDegrees.x),
                Radians(eulerDegrees.y),
                Radians(eulerDegrees.z)
            );
            return ViewFromPositionEuler(position, eulerRadians);
        }
        
        // ========================================
        // Combined View-Projection
        // ========================================
        
        // Create view-projection matrix
        inline Mat4 ViewProjection(const Mat4& view, const Mat4& projection) {
            return projection * view;
        }
        
        // ========================================
        // Camera Utilities
        // ========================================
        
        // Extract frustum planes from view-projection matrix
        // Returns 6 planes: left, right, bottom, top, near, far
        // Note: This would require Plane class, implemented separately
        
        // Calculate FOV from projection matrix
        inline float ExtractFOV(const Mat4& projection) {
            return 2.0f * std::atan(1.0f / projection.GetGLM()[1][1]);
        }
        
        // Calculate aspect ratio from projection matrix
        inline float ExtractAspectRatio(const Mat4& projection) {
            return projection.GetGLM()[1][1] / projection.GetGLM()[0][0];
        }
        
        // Screen to world ray (for picking)
        inline Ray ScreenPointToRay(const Vec3& screenPoint, const Mat4& viewProjection, 
                                   float screenWidth, float screenHeight) {
            // Normalize screen coordinates to [-1, 1]
            float x = (2.0f * screenPoint.x) / screenWidth - 1.0f;
            float y = 1.0f - (2.0f * screenPoint.y) / screenHeight;
            
            // Create ray in clip space
            Vec4 rayClip(x, y, -1.0f, 1.0f);
            
            // Transform to world space
            Mat4 invViewProj = viewProjection.Inverse();
            Vec4 rayWorld = invViewProj * rayClip;
            
            // Perspective divide
            if (!IsZero(rayWorld.w)) {
                rayWorld = rayWorld / rayWorld.w;
            }
            
            Vec3 rayDir = rayWorld.ToVec3().Normalized();
            
            // Extract camera position from inverse view-projection
            Vec4 camPos = invViewProj * Vec4(0.0f, 0.0f, 0.0f, 1.0f);
            Vec3 origin = camPos.ToVec3();
            
            return Ray(origin, rayDir);
        }
        
        // World to screen point
        inline Vec3 WorldToScreenPoint(const Vec3& worldPoint, const Mat4& viewProjection,
                                      float screenWidth, float screenHeight) {
            Vec4 clipSpace = viewProjection * Vec4(worldPoint, 1.0f);
            
            if (IsZero(clipSpace.w)) {
                return Vec3::Zero();
            }
            
            Vec3 ndc = clipSpace.ToVec3() / clipSpace.w;
            
            return Vec3(
                (ndc.x + 1.0f) * 0.5f * screenWidth,
                (1.0f - ndc.y) * 0.5f * screenHeight,
                ndc.z
            );
        }
        
    } // namespace Camera

} // namespace math
} // namespace lupine

