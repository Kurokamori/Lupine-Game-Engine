#include "lupine/math/Math.hpp"

namespace lupine {
namespace math {

    Mat4 Mat4::TRS(const Vec3& translation, const Vec3& rotation, const Vec3& scale) {
        Mat4 t = Translate(translation);

        Mat4 r = Rotate(rotation.z, Vec3::UnitZ()) *
                 Rotate(rotation.y, Vec3::UnitY()) *
                 Rotate(rotation.x, Vec3::UnitX());
        Mat4 s = Scale(scale);
        return t * r * s;
    }

    bool AABB::IntersectRay(const Ray& ray, float& distance) const {
        Vec3 invDir(1.0f / ray.direction.x, 1.0f / ray.direction.y, 1.0f / ray.direction.z);

        float t1 = (min.x - ray.origin.x) * invDir.x;
        float t2 = (max.x - ray.origin.x) * invDir.x;
        float t3 = (min.y - ray.origin.y) * invDir.y;
        float t4 = (max.y - ray.origin.y) * invDir.y;
        float t5 = (min.z - ray.origin.z) * invDir.z;
        float t6 = (max.z - ray.origin.z) * invDir.z;

        float tmin = Max(Max(Min(t1, t2), Min(t3, t4)), Min(t5, t6));
        float tmax = Min(Min(Max(t1, t2), Max(t3, t4)), Max(t5, t6));

        if (tmax < 0.0f) {
            distance = tmax;
            return false;
        }

        if (tmin > tmax) {
            distance = tmax;
            return false;
        }

        distance = tmin;
        return true;
    }

    bool Ray::IntersectAABB(const AABB& aabb, float& distance) const {
        return aabb.IntersectRay(*this, distance);
    }

    bool Ray::IntersectPlane(const Plane& plane, float& distance) const {
        float denom = direction.Dot(plane.normal);

        if (IsZero(denom)) {
            return false;
        }

        float t = (plane.distance - origin.Dot(plane.normal)) / denom;

        if (t < 0.0f) {
            return false;
        }

        distance = t;
        return true;
    }

    bool Plane::IntersectRay(const Ray& ray, float& dist) const {
        return ray.IntersectPlane(*this, dist);
    }

}
}

