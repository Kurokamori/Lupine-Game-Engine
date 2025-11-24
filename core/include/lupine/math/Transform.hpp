#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include "Quat.hpp"
#include "Mat4.hpp"

namespace lupine {
namespace math {

    // ========================================
    // Transform - Position, Rotation, Scale
    // ========================================
    
    class Transform {
    public:
        Vec3 position;
        Quat rotation;
        Vec3 scale;
        
        // Constructors
        Transform()
            : position(Vec3::Zero())
            , rotation(Quat::Identity())
            , scale(Vec3::One())
        {}
        
        Transform(const Vec3& pos, const Quat& rot, const Vec3& scl)
            : position(pos)
            , rotation(rot)
            , scale(scl)
        {}
        
        Transform(const Vec3& pos)
            : position(pos)
            , rotation(Quat::Identity())
            , scale(Vec3::One())
        {}
        
        // Static constructors
        static Transform Identity() {
            return Transform(Vec3::Zero(), Quat::Identity(), Vec3::One());
        }
        
        // Convert to matrix
        Mat4 ToMatrix() const {
            Mat4 translation = Mat4::Translate(position);
            Mat4 rot = rotation.ToMat4();
            Mat4 scl = Mat4::Scale(scale);
            return translation * rot * scl;
        }
        
        // Transform operations
        Vec3 TransformPoint(const Vec3& point) const {
            return position + rotation * (scale * point);
        }
        
        Vec3 TransformDirection(const Vec3& direction) const {
            return rotation * direction;
        }
        
        Vec3 InverseTransformPoint(const Vec3& point) const {
            Vec3 invScale(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
            return invScale * (rotation.Inverse() * (point - position));
        }
        
        Vec3 InverseTransformDirection(const Vec3& direction) const {
            return rotation.Inverse() * direction;
        }
        
        // Combine transforms: parent * local = world
        Transform operator*(const Transform& local) const {
            Transform result;
            result.position = TransformPoint(local.position);
            result.rotation = rotation * local.rotation;
            result.scale = scale * local.scale;
            return result;
        }
        
        Transform& operator*=(const Transform& local) {
            *this = *this * local;
            return *this;
        }
        
        // Comparison
        bool operator==(const Transform& other) const {
            return position == other.position &&
                   rotation == other.rotation &&
                   scale == other.scale;
        }
        
        bool operator!=(const Transform& other) const {
            return !(*this == other);
        }
        
        // Get inverse transform
        Transform Inverse() const {
            Transform result;
            result.rotation = rotation.Inverse();
            result.scale = Vec3(1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z);
            result.position = result.rotation * (result.scale * -position);
            return result;
        }
        
        // Interpolation
        Transform Lerp(const Transform& other, float t) const {
            Transform result;
            result.position = position.Lerp(other.position, t);
            result.rotation = rotation.Slerp(other.rotation, t);
            result.scale = scale.Lerp(other.scale, t);
            return result;
        }
        
        // Get forward/up/right vectors
        Vec3 GetForward() const { return rotation.GetForward(); }
        Vec3 GetUp() const { return rotation.GetUp(); }
        Vec3 GetRight() const { return rotation.GetRight(); }
        
        // Set rotation from Euler angles (in radians)
        void SetEulerAngles(const Vec3& euler) {
            rotation = Quat::FromEuler(euler);
        }
        
        void SetEulerAngles(float pitch, float yaw, float roll) {
            rotation = Quat::FromEuler(pitch, yaw, roll);
        }
        
        // Get Euler angles (in radians)
        Vec3 GetEulerAngles() const {
            return rotation.ToEuler();
        }
        
        // Look at target
        void LookAt(const Vec3& target, const Vec3& up = Vec3::Up()) {
            Vec3 forward = (target - position).Normalized();
            rotation = Quat::LookRotation(forward, up);
        }
        
        // Translate
        void Translate(const Vec3& translation) {
            position += translation;
        }
        
        // Rotate
        void Rotate(const Quat& rot) {
            rotation = rot * rotation;
        }
        
        void Rotate(const Vec3& axis, float angle) {
            rotation = Quat::FromAxisAngle(axis, angle) * rotation;
        }
    };
    
    // Utility functions
    inline Transform Lerp(const Transform& a, const Transform& b, float t) {
        return a.Lerp(b, t);
    }

} // namespace math
} // namespace lupine

