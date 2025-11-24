#include "lupine/rendering/GizmoUtils.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {

using namespace math;

Vec2 GizmoUtils::WorldToScreen(const Vec3& worldPos, const Mat4& viewProj,
                                float viewportWidth, float viewportHeight) {

    Vec4 clipPos = viewProj * Vec4(worldPos.x, worldPos.y, worldPos.z, 1.0f);

    if (std::abs(clipPos.w) < 0.0001f) {
        return Vec2(0.0f, 0.0f);
    }

    Vec3 ndcPos = Vec3(clipPos.x / clipPos.w, clipPos.y / clipPos.w, clipPos.z / clipPos.w);

    float screenX = (ndcPos.x * 0.5f + 0.5f) * viewportWidth;
    float screenY = (1.0f - (ndcPos.y * 0.5f + 0.5f)) * viewportHeight;

    return Vec2(screenX, screenY);
}

float GizmoUtils::DistancePointToLineSegment(const Vec2& point, const Vec2& lineStart,
                                             const Vec2& lineEnd) {
    Vec2 line = lineEnd - lineStart;
    float lineLength = line.Length();

    if (lineLength < 0.0001f) {
        return (point - lineStart).Length();
    }

    float t = std::max(0.0f, std::min(1.0f, (point - lineStart).Dot(line) / (lineLength * lineLength)));
    Vec2 projection = lineStart + line * t;

    return (point - projection).Length();
}

bool GizmoUtils::TestArrow2D(const Vec2& screenPos, const Vec2& arrowStart,
                             const Vec2& arrowEnd, float threshold) {
    float distance = DistancePointToLineSegment(screenPos, arrowStart, arrowEnd);
    return distance <= threshold;
}

bool GizmoUtils::TestCircle2D(const Vec2& screenPos, const Vec2& center,
                              float radius, float thickness) {
    float distance = (screenPos - center).Length();
    return std::abs(distance - radius) <= thickness;
}

bool GizmoUtils::TestScaleHandle2D(const Vec2& screenPos, const Vec2& handlePos,
                                   float handleSize) {
    return std::abs(screenPos.x - handlePos.x) <= handleSize &&
           std::abs(screenPos.y - handlePos.y) <= handleSize;
}

bool GizmoUtils::TestArrow3D(const Ray& ray, const Vec3& arrowStart,
                             const Vec3& arrowEnd, float thickness, float& distance) {

    Vec3 axisVec = arrowEnd - arrowStart;
    float axisLength = axisVec.Length();

    if (axisLength < 0.0001f) {
        return false;
    }

    Vec3 axisDir = axisVec / axisLength;
    Vec3 rayDir = ray.direction;

    Vec3 w0 = ray.origin - arrowStart;

    float a = rayDir.Dot(rayDir);
    float b = rayDir.Dot(axisDir);
    float c = axisDir.Dot(axisDir);
    float d = rayDir.Dot(w0);
    float e = axisDir.Dot(w0);

    float denom = a * c - b * b;

    float rayParam, axisParam;

    if (std::abs(denom) < 0.0001f) {

        rayParam = 0.0f;
        axisParam = e / c;
    } else {
        rayParam = (b * e - c * d) / denom;
        axisParam = (a * e - b * d) / denom;
    }

    if (rayParam < 0.0f) {
        rayParam = 0.0f;

        axisParam = e;
    }

    axisParam = std::max(0.0f, std::min(axisLength, axisParam));

    Vec3 closestOnRay = ray.origin + rayDir * rayParam;
    Vec3 closestOnAxis = arrowStart + axisDir * axisParam;

    float distBetween = (closestOnAxis - closestOnRay).Length();

    if (distBetween <= thickness * 2.0f) {
        distance = rayParam;
        return true;
    }

    return false;
}

bool GizmoUtils::TestCircle3D(const Ray& ray, const Vec3& center,
                              const Vec3& normal, float radius, float thickness, float& distance) {

    float denom = ray.direction.Dot(normal);

    if (std::abs(denom) < 0.0001f) {
        return false;
    }

    float t = (center - ray.origin).Dot(normal) / denom;

    if (t < 0.0f) {
        return false;
    }

    Vec3 hitPoint = ray.origin + ray.direction * t;
    float distFromCenter = (hitPoint - center).Length();

    if (std::abs(distFromCenter - radius) <= thickness) {
        distance = t;
        return true;
    }

    return false;
}

bool GizmoUtils::TestScaleHandle3D(const Ray& ray, const Vec3& handlePos,
                                   float handleSize, float& distance) {

    Vec3 oc = ray.origin - handlePos;
    float b = oc.Dot(ray.direction);
    float c = oc.Dot(oc) - handleSize * handleSize;
    float discriminant = b * b - c;

    if (discriminant < 0.0f) {
        return false;
    }

    distance = -b - std::sqrt(discriminant);

    if (distance < 0.0f) {
        distance = -b + std::sqrt(discriminant);
    }

    return distance >= 0.0f;
}

GizmoHitResult GizmoUtils::TestGizmo2D(
    const Vec2& screenPos,
    const Vec3& gizmoWorldPos,
    float gizmoSize,
    GizmoType gizmoType,
    const Mat4& viewMatrix,
    const Mat4& projMatrix,
    float viewportWidth,
    float viewportHeight,
    float rotation2D
) {
    GizmoHitResult result;
    Mat4 viewProj = projMatrix * viewMatrix;

    Vec2 gizmoScreenPos = WorldToScreen(gizmoWorldPos, viewProj, viewportWidth, viewportHeight);

    float cosR = std::cos(rotation2D);
    float sinR = std::sin(rotation2D);
    Vec3 xAxis(cosR, sinR, 0.0f);
    Vec3 yAxis(-sinR, cosR, 0.0f);

    float arrowThreshold = 15.0f;
    float circleThickness = 15.0f;
    float handleSize = 15.0f;

    if (gizmoType == GizmoType::All) {
        float closestDist = 1e10f;

        Vec2 xEnd = WorldToScreen(gizmoWorldPos + xAxis * gizmoSize * 0.8f, viewProj, viewportWidth, viewportHeight);
        Vec2 yEnd = WorldToScreen(gizmoWorldPos + yAxis * gizmoSize * 0.8f, viewProj, viewportWidth, viewportHeight);

        if (TestArrow2D(screenPos, gizmoScreenPos, xEnd, arrowThreshold)) {
            float dist = (screenPos - gizmoScreenPos).Length();
            if (dist < closestDist) {
                closestDist = dist;
                result.hit = true;
                result.hitAxis = GizmoAxis::X;
                result.hitOperation = GizmoType::Translation;
            }
        }

        if (TestArrow2D(screenPos, gizmoScreenPos, yEnd, arrowThreshold)) {
            float dist = (screenPos - gizmoScreenPos).Length();
            if (dist < closestDist) {
                closestDist = dist;
                result.hit = true;
                result.hitAxis = GizmoAxis::Y;
                result.hitOperation = GizmoType::Translation;
            }
        }

        float rotationRadius = (gizmoScreenPos - WorldToScreen(
            gizmoWorldPos + xAxis * gizmoSize, viewProj, viewportWidth, viewportHeight)).Length();

        if (TestCircle2D(screenPos, gizmoScreenPos, rotationRadius, circleThickness)) {
            result.hit = true;
            result.hitAxis = GizmoAxis::Z;
            result.hitOperation = GizmoType::Rotation;
            result.distance = closestDist;
        }

        Vec2 xScaleHandle = WorldToScreen(gizmoWorldPos + xAxis * gizmoSize * 0.6f, viewProj, viewportWidth, viewportHeight);
        Vec2 yScaleHandle = WorldToScreen(gizmoWorldPos + yAxis * gizmoSize * 0.6f, viewProj, viewportWidth, viewportHeight);

        if (TestScaleHandle2D(screenPos, xScaleHandle, handleSize)) {
            result.hit = true;
            result.hitAxis = GizmoAxis::X;
            result.hitOperation = GizmoType::Scale;
        }

        if (TestScaleHandle2D(screenPos, yScaleHandle, handleSize)) {
            result.hit = true;
            result.hitAxis = GizmoAxis::Y;
            result.hitOperation = GizmoType::Scale;
        }

    } else {

        switch (gizmoType) {
            case GizmoType::Translation: {
                Vec2 xEnd = WorldToScreen(gizmoWorldPos + xAxis * gizmoSize, viewProj, viewportWidth, viewportHeight);
                Vec2 yEnd = WorldToScreen(gizmoWorldPos + yAxis * gizmoSize, viewProj, viewportWidth, viewportHeight);

                if (TestArrow2D(screenPos, gizmoScreenPos, xEnd, arrowThreshold)) {
                    result.hit = true;
                    result.hitAxis = GizmoAxis::X;
                    result.hitOperation = GizmoType::Translation;
                } else if (TestArrow2D(screenPos, gizmoScreenPos, yEnd, arrowThreshold)) {
                    result.hit = true;
                    result.hitAxis = GizmoAxis::Y;
                    result.hitOperation = GizmoType::Translation;
                }
                break;
            }

            case GizmoType::Rotation: {
                float rotationRadius = (gizmoScreenPos - WorldToScreen(
                    gizmoWorldPos + xAxis * gizmoSize, viewProj, viewportWidth, viewportHeight)).Length();

                if (TestCircle2D(screenPos, gizmoScreenPos, rotationRadius, circleThickness)) {
                    result.hit = true;
                    result.hitAxis = GizmoAxis::Z;
                    result.hitOperation = GizmoType::Rotation;
                }
                break;
            }

            case GizmoType::Scale: {
                Vec2 xScaleHandle = WorldToScreen(gizmoWorldPos + xAxis * gizmoSize, viewProj, viewportWidth, viewportHeight);
                Vec2 yScaleHandle = WorldToScreen(gizmoWorldPos + yAxis * gizmoSize, viewProj, viewportWidth, viewportHeight);

                if (TestScaleHandle2D(screenPos, xScaleHandle, handleSize)) {
                    result.hit = true;
                    result.hitAxis = GizmoAxis::X;
                    result.hitOperation = GizmoType::Scale;
                } else if (TestScaleHandle2D(screenPos, yScaleHandle, handleSize)) {
                    result.hit = true;
                    result.hitAxis = GizmoAxis::Y;
                    result.hitOperation = GizmoType::Scale;
                }
                break;
            }

            default:
                break;
        }
    }

    return result;
}

GizmoHitResult GizmoUtils::TestGizmo3D(
    const Ray& ray,
    const Vec3& gizmoWorldPos,
    float gizmoSize,
    GizmoType gizmoType,
    const Quat& rotation3D
) {
    GizmoHitResult result;
    float closestDist = 1e10f;
    float thickness = gizmoSize * 0.05f;

    Vec3 xAxis = rotation3D * Vec3(1, 0, 0);
    Vec3 yAxis = rotation3D * Vec3(0, 1, 0);
    Vec3 zAxis = rotation3D * Vec3(0, 0, 1);

    if (gizmoType == GizmoType::All) {

        float dist;
        if (TestArrow3D(ray, gizmoWorldPos, gizmoWorldPos + xAxis * gizmoSize * 0.8f, thickness, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                result.hit = true;
                result.hitAxis = GizmoAxis::X;
                result.hitOperation = GizmoType::Translation;
            }
        }
        if (TestArrow3D(ray, gizmoWorldPos, gizmoWorldPos + yAxis * gizmoSize * 0.8f, thickness, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                result.hit = true;
                result.hitAxis = GizmoAxis::Y;
                result.hitOperation = GizmoType::Translation;
            }
        }
        if (TestArrow3D(ray, gizmoWorldPos, gizmoWorldPos + zAxis * gizmoSize * 0.8f, thickness, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                result.hit = true;
                result.hitAxis = GizmoAxis::Z;
                result.hitOperation = GizmoType::Translation;
            }
        }

        if (TestCircle3D(ray, gizmoWorldPos, xAxis, gizmoSize * 1.2f, thickness, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                result.hit = true;
                result.hitAxis = GizmoAxis::X;
                result.hitOperation = GizmoType::Rotation;
            }
        }
        if (TestCircle3D(ray, gizmoWorldPos, yAxis, gizmoSize * 1.2f, thickness, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                result.hit = true;
                result.hitAxis = GizmoAxis::Y;
                result.hitOperation = GizmoType::Rotation;
            }
        }
        if (TestCircle3D(ray, gizmoWorldPos, zAxis, gizmoSize * 1.2f, thickness, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                result.hit = true;
                result.hitAxis = GizmoAxis::Z;
                result.hitOperation = GizmoType::Rotation;
            }
        }

        result.distance = closestDist;

    } else {

        float dist;
        switch (gizmoType) {
            case GizmoType::Translation: {
                if (TestArrow3D(ray, gizmoWorldPos, gizmoWorldPos + xAxis * gizmoSize, thickness, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::X;
                        result.hitOperation = GizmoType::Translation;
                    }
                }
                if (TestArrow3D(ray, gizmoWorldPos, gizmoWorldPos + yAxis * gizmoSize, thickness, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::Y;
                        result.hitOperation = GizmoType::Translation;
                    }
                }
                if (TestArrow3D(ray, gizmoWorldPos, gizmoWorldPos + zAxis * gizmoSize, thickness, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::Z;
                        result.hitOperation = GizmoType::Translation;
                    }
                }
                break;
            }

            case GizmoType::Rotation: {
                if (TestCircle3D(ray, gizmoWorldPos, xAxis, gizmoSize, thickness, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::X;
                        result.hitOperation = GizmoType::Rotation;
                    }
                }
                if (TestCircle3D(ray, gizmoWorldPos, yAxis, gizmoSize, thickness, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::Y;
                        result.hitOperation = GizmoType::Rotation;
                    }
                }
                if (TestCircle3D(ray, gizmoWorldPos, zAxis, gizmoSize, thickness, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::Z;
                        result.hitOperation = GizmoType::Rotation;
                    }
                }
                break;
            }

            case GizmoType::Scale: {
                float handleSize = gizmoSize * 0.1f;
                if (TestScaleHandle3D(ray, gizmoWorldPos + xAxis * gizmoSize, handleSize, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::X;
                        result.hitOperation = GizmoType::Scale;
                    }
                }
                if (TestScaleHandle3D(ray, gizmoWorldPos + yAxis * gizmoSize, handleSize, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::Y;
                        result.hitOperation = GizmoType::Scale;
                    }
                }
                if (TestScaleHandle3D(ray, gizmoWorldPos + zAxis * gizmoSize, handleSize, dist)) {
                    if (dist < closestDist) {
                        closestDist = dist;
                        result.hit = true;
                        result.hitAxis = GizmoAxis::Z;
                        result.hitOperation = GizmoType::Scale;
                    }
                }
                break;
            }

            default:
                break;
        }

        result.distance = closestDist;
    }

    return result;
}

}
