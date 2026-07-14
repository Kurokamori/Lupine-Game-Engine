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

Vec2 GizmoUtils::SolveOrientedHalfExtents2D(const Vec2& aabbHalfSize, float rotation) {
    float c = std::abs(std::cos(rotation));
    float s = std::abs(std::sin(rotation));

    float denom = c * c - s * s;

    if (std::abs(denom) < 0.02f) {
        // Near a ±45° multiple the system is degenerate; the AABB no longer
        // uniquely determines the oriented rectangle, so use the AABB extents.
        return aabbHalfSize;
    }

    float a = (c * aabbHalfSize.x - s * aabbHalfSize.y) / denom;
    float b = (c * aabbHalfSize.y - s * aabbHalfSize.x) / denom;

    if (a <= 0.0f || b <= 0.0f) {
        return aabbHalfSize;
    }

    return Vec2(a, b);
}

namespace {

// Local helper used by both the geometry builder and hit testing to map a
// point expressed in the gizmo's local (rotated) frame back to world space.
inline Vec3 GizmoLocalToWorld(const Vec3& center, float cosR, float sinR, float lx, float ly) {
    return Vec3(center.x + lx * cosR - ly * sinR,
                center.y + lx * sinR + ly * cosR,
                center.z);
}

// Resolve the oriented half-extents used for the scale rectangle, applying a
// sensible square fallback when the selection has no measurable bounds.
//
// The emptiness test must use an absolute world epsilon. `gizmo.size` is 50/zoom —
// a SCREEN-constant, deliberately not a world size — so testing world half-extents
// against a fraction of it gives a threshold that grows without bound as you zoom
// out. A one-line Label is only ~10 world units tall but ~50 wide, so past a
// certain zoom such a threshold overtakes the height while leaving the width alone,
// substituting a zoom-dependent value on the Y axis only: the rectangle stretches
// and skews as you zoom, and never matches the text.
inline Vec2 ResolveScaleExtents(const DebugGizmo& gizmo) {
    constexpr float kEmptyExtent = 1e-3f;
    const float fallback = gizmo.size * 0.7f;

    float ex = gizmo.halfExtents.x;
    float ey = gizmo.halfExtents.y;

    // Nothing measurable at all (an empty Node2D, or a component reporting zero
    // extents): fall back to a handle-sized square so there is still something to
    // grab. This is the only case where a screen-relative size is correct, because
    // there is no world size to be relative to.
    if (ex < kEmptyExtent && ey < kEmptyExtent) {
        return Vec2(fallback, fallback);
    }

    // One flat axis (a horizontal Line2D, a 1px sprite) still needs a grabbable
    // thickness, but it is derived from the other axis so it stays a world size and
    // the rectangle cannot skew with zoom. The corner handles keep their own
    // screen-space hit radius, so the thin rect remains clickable.
    if (ex < kEmptyExtent) ex = std::max(ey * 0.05f, kEmptyExtent);
    if (ey < kEmptyExtent) ey = std::max(ex * 0.05f, kEmptyExtent);

    return Vec2(ex, ey);
}

} // namespace

std::vector<GizmoSegment> GizmoUtils::BuildGizmo2DGeometry(const DebugGizmo& gizmo) {
    std::vector<GizmoSegment> segments;

    const Vec3 center = gizmo.position;
    const float G = gizmo.size;
    const float cosR = std::cos(gizmo.rotation2D);
    const float sinR = std::sin(gizmo.rotation2D);

    const Color xBase(0.90f, 0.27f, 0.27f, 1.0f);
    const Color yBase(0.32f, 0.85f, 0.35f, 1.0f);
    const Color rotBase(0.32f, 0.58f, 0.95f, 1.0f);
    const Color scaleBase(1.0f, 0.62f, 0.16f, 1.0f);
    const Color moveBase(0.92f, 0.92f, 0.96f, 1.0f);
    const Color hi(1.0f, 0.93f, 0.20f, 1.0f);

    auto isHi = [&](GizmoAxis a) -> bool { return gizmo.highlightAxis == a; };
    auto L2W = [&](float lx, float ly) -> Vec3 { return GizmoLocalToWorld(center, cosR, sinR, lx, ly); };
    auto addSeg = [&](const Vec3& a, const Vec3& b, const Color& col) {
        segments.push_back(GizmoSegment{a, b, col});
    };
    auto addSquare = [&](float cx, float cy, float hs, const Color& col) {
        Vec3 p0 = L2W(cx - hs, cy - hs);
        Vec3 p1 = L2W(cx + hs, cy - hs);
        Vec3 p2 = L2W(cx + hs, cy + hs);
        Vec3 p3 = L2W(cx - hs, cy + hs);
        addSeg(p0, p1, col);
        addSeg(p1, p2, col);
        addSeg(p2, p3, col);
        addSeg(p3, p0, col);
    };

    auto emitTranslation = [&]() {
        float len = G;
        float head = G * 0.22f;
        float headW = head * 0.55f;
        Color cx = (isHi(GizmoAxis::X) || isHi(GizmoAxis::XY)) ? hi : xBase;
        Color cy = (isHi(GizmoAxis::Y) || isHi(GizmoAxis::XY)) ? hi : yBase;

        Vec3 xTip = L2W(len, 0.0f);
        addSeg(center, xTip, cx);
        addSeg(xTip, L2W(len - head, headW), cx);
        addSeg(xTip, L2W(len - head, -headW), cx);

        Vec3 yTip = L2W(0.0f, len);
        addSeg(center, yTip, cy);
        addSeg(yTip, L2W(headW, len - head), cy);
        addSeg(yTip, L2W(-headW, len - head), cy);

        Color cc = isHi(GizmoAxis::XY) ? hi : moveBase;
        addSquare(0.0f, 0.0f, G * 0.13f, cc);
    };

    auto emitRotation = [&](float radius) {
        Color c = isHi(GizmoAxis::Z) ? hi : rotBase;
        const int N = 64;
        Vec3 prev(center.x + radius, center.y, center.z);
        for (int i = 1; i <= N; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(N) * 2.0f * math::PI;
            Vec3 cur(center.x + std::cos(t) * radius, center.y + std::sin(t) * radius, center.z);
            addSeg(prev, cur, c);
            prev = cur;
        }
    };

    auto emitScaleRect = [&](bool includeUniformCenter) {
        Vec2 e = ResolveScaleExtents(gizmo);
        float ex = e.x, ey = e.y;
        float hs = G * 0.10f;

        addSeg(L2W(-ex, -ey), L2W(ex, -ey), scaleBase);
        addSeg(L2W(ex, -ey), L2W(ex, ey), scaleBase);
        addSeg(L2W(ex, ey), L2W(-ex, ey), scaleBase);
        addSeg(L2W(-ex, ey), L2W(-ex, -ey), scaleBase);

        Color cCorner = isHi(GizmoAxis::XY) ? hi : scaleBase;
        addSquare(-ex, -ey, hs, cCorner);
        addSquare(ex, -ey, hs, cCorner);
        addSquare(ex, ey, hs, cCorner);
        addSquare(-ex, ey, hs, cCorner);

        Color cX = isHi(GizmoAxis::X) ? hi : scaleBase;
        Color cY = isHi(GizmoAxis::Y) ? hi : scaleBase;
        addSquare(ex, 0.0f, hs, cX);
        addSquare(-ex, 0.0f, hs, cX);
        addSquare(0.0f, ey, hs, cY);
        addSquare(0.0f, -ey, hs, cY);

        if (includeUniformCenter) {
            Color cU = isHi(GizmoAxis::XYZ) ? hi : scaleBase;
            addSquare(0.0f, 0.0f, G * 0.12f, cU);
        }
    };

    switch (gizmo.type) {
        case GizmoType::Translation:
            emitTranslation();
            break;
        case GizmoType::Rotation:
            emitRotation(G);
            break;
        case GizmoType::Scale:
            emitScaleRect(true);
            break;
        case GizmoType::All:
            emitScaleRect(false);
            emitRotation(G * 1.25f);
            emitTranslation();
            break;
    }

    return segments;
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
    float rotation2D,
    const Vec2& halfExtents
) {
    GizmoHitResult result;
    Mat4 viewProj = projMatrix * viewMatrix;

    float cosR = std::cos(rotation2D);
    float sinR = std::sin(rotation2D);

    Vec2 centerScreen = WorldToScreen(gizmoWorldPos, viewProj, viewportWidth, viewportHeight);

    auto L2S = [&](float lx, float ly) -> Vec2 {
        Vec3 w = GizmoLocalToWorld(gizmoWorldPos, cosR, sinR, lx, ly);
        return WorldToScreen(w, viewProj, viewportWidth, viewportHeight);
    };

    const float handleHit = 12.0f;
    const float lineHit = 8.0f;
    const float ringHit = 9.0f;

    DebugGizmo probe;
    probe.size = gizmoSize;
    probe.halfExtents = halfExtents;
    Vec2 e = ResolveScaleExtents(probe);
    float ex = e.x, ey = e.y;

    // Candidates are grouped into priority tiers so that a smaller, more specific
    // target (e.g. a corner handle) always wins over a larger one it overlaps
    // (e.g. the rectangle edge line that passes through that corner). A lower
    // tier always beats a higher tier; ties within a tier are broken by distance.
    int bestTier = 1000;
    float bestDist = 1e10f;

    auto consider = [&](float d, float threshold, int tier, GizmoType op, GizmoAxis axis, const Vec2& dir) {
        if (d > threshold) {
            return;
        }
        if (tier < bestTier || (tier == bestTier && d < bestDist)) {
            bestTier = tier;
            bestDist = d;
            result.hit = true;
            result.hitOperation = op;
            result.hitAxis = axis;
            result.handleDir = dir;
        }
    };

    auto considerHandle = [&](float lx, float ly, int tier, GizmoType op, GizmoAxis axis, const Vec2& dir) {
        Vec2 p = L2S(lx, ly);
        consider((screenPos - p).Length(), handleHit, tier, op, axis, dir);
    };

    auto considerLine = [&](const Vec2& a, const Vec2& b, int tier, GizmoType op, GizmoAxis axis,
                            const Vec2& dir, float threshold) {
        consider(DistancePointToLineSegment(screenPos, a, b), threshold, tier, op, axis, dir);
    };

    bool doScale = (gizmoType == GizmoType::Scale || gizmoType == GizmoType::All);
    bool doRotation = (gizmoType == GizmoType::Rotation || gizmoType == GizmoType::All);
    bool doTranslation = (gizmoType == GizmoType::Translation || gizmoType == GizmoType::All);

    // Tier 0: corner handles (most specific). Tier 1: edge / centre handles.
    // Tier 2: line fallbacks (arrows, rectangle edges, rotation ring).
    if (doScale) {
        considerHandle(-ex, -ey, 0, GizmoType::Scale, GizmoAxis::XY, Vec2(-1.0f, -1.0f));
        considerHandle(ex, -ey, 0, GizmoType::Scale, GizmoAxis::XY, Vec2(1.0f, -1.0f));
        considerHandle(ex, ey, 0, GizmoType::Scale, GizmoAxis::XY, Vec2(1.0f, 1.0f));
        considerHandle(-ex, ey, 0, GizmoType::Scale, GizmoAxis::XY, Vec2(-1.0f, 1.0f));

        considerHandle(ex, 0.0f, 1, GizmoType::Scale, GizmoAxis::X, Vec2(1.0f, 0.0f));
        considerHandle(-ex, 0.0f, 1, GizmoType::Scale, GizmoAxis::X, Vec2(-1.0f, 0.0f));
        considerHandle(0.0f, ey, 1, GizmoType::Scale, GizmoAxis::Y, Vec2(0.0f, 1.0f));
        considerHandle(0.0f, -ey, 1, GizmoType::Scale, GizmoAxis::Y, Vec2(0.0f, -1.0f));

        if (gizmoType == GizmoType::Scale) {
            considerHandle(0.0f, 0.0f, 1, GizmoType::Scale, GizmoAxis::XYZ, Vec2(0.0f, 0.0f));
        }

        considerLine(L2S(-ex, ey), L2S(ex, ey), 2, GizmoType::Scale, GizmoAxis::Y, Vec2(0.0f, 1.0f), lineHit);
        considerLine(L2S(-ex, -ey), L2S(ex, -ey), 2, GizmoType::Scale, GizmoAxis::Y, Vec2(0.0f, -1.0f), lineHit);
        considerLine(L2S(ex, -ey), L2S(ex, ey), 2, GizmoType::Scale, GizmoAxis::X, Vec2(1.0f, 0.0f), lineHit);
        considerLine(L2S(-ex, -ey), L2S(-ex, ey), 2, GizmoType::Scale, GizmoAxis::X, Vec2(-1.0f, 0.0f), lineHit);
    }

    if (doTranslation) {
        considerHandle(0.0f, 0.0f, 1, GizmoType::Translation, GizmoAxis::XY, Vec2(0.0f, 0.0f));
        considerLine(centerScreen, L2S(gizmoSize, 0.0f), 2, GizmoType::Translation, GizmoAxis::X, Vec2(0.0f, 0.0f), lineHit);
        considerLine(centerScreen, L2S(0.0f, gizmoSize), 2, GizmoType::Translation, GizmoAxis::Y, Vec2(0.0f, 0.0f), lineHit);
    }

    if (doRotation) {
        float ringRadiusLocal = (gizmoType == GizmoType::All) ? gizmoSize * 1.25f : gizmoSize;
        Vec2 ringEdge = WorldToScreen(Vec3(gizmoWorldPos.x + ringRadiusLocal, gizmoWorldPos.y, gizmoWorldPos.z),
                                      viewProj, viewportWidth, viewportHeight);
        float ringRadiusPx = (ringEdge - centerScreen).Length();
        float distToRing = std::abs((screenPos - centerScreen).Length() - ringRadiusPx);
        consider(distToRing, ringHit, 2, GizmoType::Rotation, GizmoAxis::Z, Vec2(0.0f, 0.0f));
    }

    result.distance = bestDist;
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
