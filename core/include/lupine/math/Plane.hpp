#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"

namespace lupine {
namespace math {

    // Forward declarations
    class Ray;
    
    // ========================================
    // Plane - 3D Plane
    // ========================================
    
    class Plane {
    public:
        Vec3 normal;
        float distance; // Distance from origin along normal
        
        // Constructors
        Plane() : normal(Vec3::Up()), distance(0.0f) {}
        Plane(const Vec3& normal, float distance)
            : normal(normal.Normalized()), distance(distance) {}
        Plane(const Vec3& normal, const Vec3& point)
            : normal(normal.Normalized()), distance(normal.Normalized().Dot(point)) {}
        
        // Create from three points
        static Plane FromPoints(const Vec3& p0, const Vec3& p1, const Vec3& p2) {
            Vec3 v1 = p1 - p0;
            Vec3 v2 = p2 - p0;
            Vec3 normal = v1.Cross(v2).Normalized();
            return Plane(normal, p0);
        }
        
        // Get a point on the plane
        Vec3 GetPoint() const {
            return normal * distance;
        }
        
        // Distance from point to plane (signed)
        float DistanceToPoint(const Vec3& point) const {
            return normal.Dot(point) - distance;
        }
        
        // Get closest point on plane to a point
        Vec3 ClosestPoint(const Vec3& point) const {
            float dist = DistanceToPoint(point);
            return point - normal * dist;
        }
        
        // Check which side of plane a point is on
        // Returns: > 0 if on normal side, < 0 if on opposite side, 0 if on plane
        float GetSide(const Vec3& point) const {
            return DistanceToPoint(point);
        }
        
        bool IsFrontFacing(const Vec3& point) const {
            return GetSide(point) > 0.0f;
        }
        
        // Flip the plane
        Plane Flipped() const {
            return Plane(-normal, -distance);
        }
        
        void Flip() {
            normal = -normal;
            distance = -distance;
        }
        
        // Intersect with ray
        bool IntersectRay(const Ray& ray, float& dist) const;
        
        // Intersect three planes to get a point
        static bool IntersectPlanes(const Plane& p1, const Plane& p2, const Plane& p3, Vec3& point) {
            Vec3 n1 = p1.normal;
            Vec3 n2 = p2.normal;
            Vec3 n3 = p3.normal;
            
            float denom = n1.Dot(n2.Cross(n3));
            
            if (IsZero(denom)) {
                return false; // Planes don't intersect at a point
            }
            
            point = (p1.distance * n2.Cross(n3) +
                    p2.distance * n3.Cross(n1) +
                    p3.distance * n1.Cross(n2)) / denom;
            
            return true;
        }
        
        // Comparison
        bool operator==(const Plane& other) const {
            return normal == other.normal && Equals(distance, other.distance);
        }
        
        bool operator!=(const Plane& other) const {
            return !(*this == other);
        }
    };

} // namespace math
} // namespace lupine

