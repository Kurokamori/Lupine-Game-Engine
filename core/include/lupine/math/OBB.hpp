#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include "Quat.hpp"
#include "Mat4.hpp"
#include "AABB.hpp"

namespace lupine {
namespace math {

    // ========================================
    // OBB - Oriented Bounding Box
    // ========================================
    
    class OBB {
    public:
        Vec3 center;
        Vec3 extents;  // Half-sizes along each axis
        Quat rotation;
        
        // Constructors
        OBB() : center(Vec3::Zero()), extents(Vec3::One()), rotation(Quat::Identity()) {}
        OBB(const Vec3& center, const Vec3& extents, const Quat& rotation)
            : center(center), extents(extents), rotation(rotation) {}
        
        // Create from AABB
        static OBB FromAABB(const AABB& aabb) {
            return OBB(aabb.GetCenter(), aabb.GetExtents(), Quat::Identity());
        }
        
        // Get size
        Vec3 GetSize() const {
            return extents * 2.0f;
        }
        
        // Get axes
        Vec3 GetAxisX() const { return rotation * Vec3::UnitX(); }
        Vec3 GetAxisY() const { return rotation * Vec3::UnitY(); }
        Vec3 GetAxisZ() const { return rotation * Vec3::UnitZ(); }
        
        // Get corner point (index 0-7)
        Vec3 GetCorner(int index) const {
            Vec3 localCorner(
                (index & 1) ? extents.x : -extents.x,
                (index & 2) ? extents.y : -extents.y,
                (index & 4) ? extents.z : -extents.z
            );
            return center + rotation * localCorner;
        }
        
        // Contains point
        bool Contains(const Vec3& point) const {
            // Transform point to OBB local space
            Vec3 localPoint = rotation.Inverse() * (point - center);
            
            return std::abs(localPoint.x) <= extents.x &&
                   std::abs(localPoint.y) <= extents.y &&
                   std::abs(localPoint.z) <= extents.z;
        }
        
        // Get closest point on OBB to a point
        Vec3 ClosestPoint(const Vec3& point) const {
            // Transform to local space
            Vec3 localPoint = rotation.Inverse() * (point - center);
            
            // Clamp to extents
            localPoint.x = Clamp(localPoint.x, -extents.x, extents.x);
            localPoint.y = Clamp(localPoint.y, -extents.y, extents.y);
            localPoint.z = Clamp(localPoint.z, -extents.z, extents.z);
            
            // Transform back to world space
            return center + rotation * localPoint;
        }
        
        // Distance from point to OBB
        float Distance(const Vec3& point) const {
            Vec3 closest = ClosestPoint(point);
            return closest.Distance(point);
        }
        
        // Convert to AABB (axis-aligned bounding box that contains this OBB)
        AABB ToAABB() const {
            // Get all 8 corners and find min/max
            AABB result;
            result.min = GetCorner(0);
            result.max = result.min;
            
            for (int i = 1; i < 8; ++i) {
                result.Encapsulate(GetCorner(i));
            }
            
            return result;
        }
        
        // Intersects with another OBB (Separating Axis Theorem)
        bool Intersects(const OBB& other) const {
            // Get axes to test
            Vec3 axes[15];
            axes[0] = GetAxisX();
            axes[1] = GetAxisY();
            axes[2] = GetAxisZ();
            axes[3] = other.GetAxisX();
            axes[4] = other.GetAxisY();
            axes[5] = other.GetAxisZ();
            
            // Cross products of axes
            int idx = 6;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    axes[idx++] = axes[i].Cross(axes[3 + j]);
                }
            }
            
            // Test each axis
            for (int i = 0; i < 15; ++i) {
                if (axes[i].LengthSquared() < EPSILON) continue;
                
                if (!TestAxis(axes[i], other)) {
                    return false;
                }
            }
            
            return true;
        }
        
        // Comparison
        bool operator==(const OBB& other) const {
            return center == other.center &&
                   extents == other.extents &&
                   rotation == other.rotation;
        }
        
        bool operator!=(const OBB& other) const {
            return !(*this == other);
        }
        
    private:
        // Helper for SAT intersection test
        bool TestAxis(const Vec3& axis, const OBB& other) const {
            Vec3 normAxis = axis.Normalized();
            
            // Project this OBB onto axis
            float r1 = ProjectOntoAxis(normAxis);
            
            // Project other OBB onto axis
            float r2 = other.ProjectOntoAxis(normAxis);
            
            // Distance between centers projected onto axis
            float distance = std::abs((other.center - center).Dot(normAxis));
            
            return distance <= (r1 + r2);
        }
        
        float ProjectOntoAxis(const Vec3& axis) const {
            return extents.x * std::abs(GetAxisX().Dot(axis)) +
                   extents.y * std::abs(GetAxisY().Dot(axis)) +
                   extents.z * std::abs(GetAxisZ().Dot(axis));
        }
    };

} // namespace math
} // namespace lupine

