#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"
#include "Vec4.hpp"
#include "Mat4.hpp"
#include <algorithm>

namespace lupine {
namespace math {

    // Forward declarations
    class Ray;
    class Plane;
    
    // ========================================
    // AABB - Axis-Aligned Bounding Box
    // ========================================
    
    class AABB {
    public:
        Vec3 min;
        Vec3 max;
        
        // Constructors
        AABB() : min(Vec3::Zero()), max(Vec3::Zero()) {}
        AABB(const Vec3& min, const Vec3& max) : min(min), max(max) {}
        
        // Create from center and size
        static AABB FromCenterSize(const Vec3& center, const Vec3& size) {
            Vec3 halfSize = size * 0.5f;
            return AABB(center - halfSize, center + halfSize);
        }
        
        // Properties
        Vec3 GetCenter() const {
            return (min + max) * 0.5f;
        }
        
        Vec3 GetSize() const {
            return max - min;
        }
        
        Vec3 GetExtents() const {
            return (max - min) * 0.5f;
        }
        
        float GetVolume() const {
            Vec3 size = GetSize();
            return size.x * size.y * size.z;
        }
        
        float GetSurfaceArea() const {
            Vec3 size = GetSize();
            return 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
        }
        
        // Expand to include point
        void Encapsulate(const Vec3& point) {
            min.x = Min(min.x, point.x);
            min.y = Min(min.y, point.y);
            min.z = Min(min.z, point.z);
            max.x = Max(max.x, point.x);
            max.y = Max(max.y, point.y);
            max.z = Max(max.z, point.z);
        }
        
        // Expand to include another AABB
        void Encapsulate(const AABB& other) {
            Encapsulate(other.min);
            Encapsulate(other.max);
        }
        
        // Expand by amount
        void Expand(float amount) {
            Vec3 expansion(amount, amount, amount);
            min -= expansion;
            max += expansion;
        }
        
        void Expand(const Vec3& amount) {
            min -= amount;
            max += amount;
        }
        
        // Contains point
        bool Contains(const Vec3& point) const {
            return point.x >= min.x && point.x <= max.x &&
                   point.y >= min.y && point.y <= max.y &&
                   point.z >= min.z && point.z <= max.z;
        }
        
        // Contains AABB
        bool Contains(const AABB& other) const {
            return Contains(other.min) && Contains(other.max);
        }
        
        // Intersects with another AABB
        bool Intersects(const AABB& other) const {
            return (min.x <= other.max.x && max.x >= other.min.x) &&
                   (min.y <= other.max.y && max.y >= other.min.y) &&
                   (min.z <= other.max.z && max.z >= other.min.z);
        }
        
        // Get closest point on AABB to a point
        Vec3 ClosestPoint(const Vec3& point) const {
            return Vec3(
                Clamp(point.x, min.x, max.x),
                Clamp(point.y, min.y, max.y),
                Clamp(point.z, min.z, max.z)
            );
        }
        
        // Distance from point to AABB
        float Distance(const Vec3& point) const {
            Vec3 closest = ClosestPoint(point);
            return closest.Distance(point);
        }
        
        // Get corner point (index 0-7)
        Vec3 GetCorner(int index) const {
            return Vec3(
                (index & 1) ? max.x : min.x,
                (index & 2) ? max.y : min.y,
                (index & 4) ? max.z : min.z
            );
        }
        
        // Intersection with ray (returns true if hit, sets distance)
        bool IntersectRay(const Ray& ray, float& distance) const;

        // Transform AABB by matrix (rotates corners and creates new AABB)
        AABB Transform(const Mat4& transform) const {
            // Transform all 8 corners and create a new AABB
            AABB result;
            result.min = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
            result.max = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (int i = 0; i < 8; ++i) {
                Vec3 corner = GetCorner(i);
                Vec4 transformed = transform * Vec4(corner.x, corner.y, corner.z, 1.0f);
                Vec3 transformedCorner(transformed.x, transformed.y, transformed.z);
                result.Encapsulate(transformedCorner);
            }

            return result;
        }

        // Comparison
        bool operator==(const AABB& other) const {
            return min == other.min && max == other.max;
        }
        
        bool operator!=(const AABB& other) const {
            return !(*this == other);
        }
        
        // Merge two AABBs
        static AABB Merge(const AABB& a, const AABB& b) {
            return AABB(
                Vec3(Min(a.min.x, b.min.x), Min(a.min.y, b.min.y), Min(a.min.z, b.min.z)),
                Vec3(Max(a.max.x, b.max.x), Max(a.max.y, b.max.y), Max(a.max.z, b.max.z))
            );
        }
        
        // Get intersection of two AABBs
        static AABB Intersection(const AABB& a, const AABB& b) {
            return AABB(
                Vec3(Max(a.min.x, b.min.x), Max(a.min.y, b.min.y), Max(a.min.z, b.min.z)),
                Vec3(Min(a.max.x, b.max.x), Min(a.max.y, b.max.y), Min(a.max.z, b.max.z))
            );
        }
    };

} // namespace math
} // namespace lupine

