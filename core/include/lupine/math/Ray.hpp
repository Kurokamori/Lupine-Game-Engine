#pragma once

#include "MathCommon.hpp"
#include "Vec3.hpp"

namespace lupine {
namespace math {

    // Forward declarations
    class AABB;
    class Plane;
    
    // ========================================
    // Ray - 3D Ray
    // ========================================
    
    class Ray {
    public:
        Vec3 origin;
        Vec3 direction;
        
        // Constructors
        Ray() : origin(Vec3::Zero()), direction(Vec3::Forward()) {}
        Ray(const Vec3& origin, const Vec3& direction)
            : origin(origin), direction(direction.Normalized()) {}
        
        // Get point at distance along ray
        Vec3 GetPoint(float distance) const {
            return origin + direction * distance;
        }
        
        // Get closest point on ray to a point
        Vec3 ClosestPoint(const Vec3& point) const {
            float t = (point - origin).Dot(direction);
            t = Max(t, 0.0f); // Clamp to ray (not line)
            return origin + direction * t;
        }
        
        // Distance from ray to point
        float Distance(const Vec3& point) const {
            Vec3 closest = ClosestPoint(point);
            return closest.Distance(point);
        }
        
        // Intersect with AABB
        bool IntersectAABB(const AABB& aabb, float& distance) const;
        
        // Intersect with plane
        bool IntersectPlane(const Plane& plane, float& distance) const;
        
        // Intersect with sphere
        bool IntersectSphere(const Vec3& center, float radius, float& distance) const {
            Vec3 oc = origin - center;
            float a = direction.Dot(direction);
            float b = 2.0f * oc.Dot(direction);
            float c = oc.Dot(oc) - radius * radius;
            float discriminant = b * b - 4.0f * a * c;

            if (discriminant < 0.0f) {
                return false;
            }

            float sqrtD = std::sqrt(discriminant);
            float t1 = (-b - sqrtD) / (2.0f * a);
            float t2 = (-b + sqrtD) / (2.0f * a);

            if (t1 >= 0.0f) {
                distance = t1;
                return true;
            } else if (t2 >= 0.0f) {
                distance = t2;
                return true;
            }

            return false;
        }

        // Intersect with triangle using Möller-Trumbore algorithm
        bool IntersectTriangle(const Vec3& v0, const Vec3& v1, const Vec3& v2, float& distance) const {
            const float EPSILON = 0.0000001f;

            Vec3 edge1 = v1 - v0;
            Vec3 edge2 = v2 - v0;
            Vec3 h = direction.Cross(edge2);
            float a = edge1.Dot(h);

            // Ray is parallel to triangle
            if (a > -EPSILON && a < EPSILON) {
                return false;
            }

            float f = 1.0f / a;
            Vec3 s = origin - v0;
            float u = f * s.Dot(h);

            // Intersection point is outside triangle
            if (u < 0.0f || u > 1.0f) {
                return false;
            }

            Vec3 q = s.Cross(edge1);
            float v = f * direction.Dot(q);

            // Intersection point is outside triangle
            if (v < 0.0f || u + v > 1.0f) {
                return false;
            }

            // Calculate distance along ray
            float t = f * edge2.Dot(q);

            // Intersection is behind ray origin
            if (t < EPSILON) {
                return false;
            }

            distance = t;
            return true;
        }

        // Comparison
        bool operator==(const Ray& other) const {
            return origin == other.origin && direction == other.direction;
        }
        
        bool operator!=(const Ray& other) const {
            return !(*this == other);
        }
    };

} // namespace math
} // namespace lupine

