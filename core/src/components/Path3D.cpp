#include "lupine/components/Path3D.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Path3D::Path3D()
    : Curve3D("Path3D")
{
}

Path3D::Path3D(const std::string& name)
    : Curve3D(name)
{
}

Path3D::~Path3D() {
}

void Path3D::DefineProperties() {
    Curve3D::DefineProperties();

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(speed, 5.0f, 0.0f, 1000.0f, 0.1f, "Path"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, false, "Path"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pingPong, Bool, false, "Path"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoStart, Bool, false, "Path"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(showStartEnd, Bool, true, "Path Markers"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(showDirection, Bool, true, "Path Markers"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(startColor, Color, Color(0.2f, 1.0f, 0.2f, 1.0f), "Path Markers"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(endColor, Color, Color(1.0f, 0.2f, 0.2f, 1.0f), "Path Markers"));
}

void Path3D::OnAwake() {
    Curve3D::OnAwake();

    if (GetAutoStart()) {
        StartFollowing();
    }
}

void Path3D::OnUpdate(float deltaTime) {
    if (!m_IsFollowing) {
        return;
    }

    float length = GetCurveLength();
    if (length <= 0.001f) {
        return;
    }

    float speed = GetSpeed();
    float progressDelta = (speed * deltaTime) / length;

    if (GetPingPong()) {
        m_Progress += progressDelta * m_Direction;

        if (m_Progress >= 1.0f) {
            m_Progress = 1.0f;
            m_Direction = -1;
            if (!GetLoop()) {
                if (m_IsReversed) {
                    StopFollowing();
                    m_IsReversed = false;
                } else {
                    m_IsReversed = true;
                }
            }
        } else if (m_Progress <= 0.0f) {
            m_Progress = 0.0f;
            m_Direction = 1;
            if (!GetLoop()) {
                if (m_IsReversed) {
                    StopFollowing();
                    m_IsReversed = false;
                } else {
                    m_IsReversed = true;
                }
            }
        }
    } else {
        m_Progress += progressDelta;

        if (m_Progress >= 1.0f) {
            if (GetLoop()) {
                m_Progress = m_Progress - 1.0f;
            } else {
                m_Progress = 1.0f;
                StopFollowing();
            }
        }
    }
}

// ===== Path Following =====

void Path3D::StartFollowing() {
    m_IsFollowing = true;
}

void Path3D::StopFollowing() {
    m_IsFollowing = false;
}

void Path3D::Reset() {
    m_Progress = 0.0f;
    m_Direction = 1;
    m_IsReversed = false;
}

void Path3D::SetProgress(float progress) {
    m_Progress = std::max(0.0f, std::min(1.0f, progress));
}

float Path3D::GetSpeed() const {
    return GetPropertyValue<float>("speed");
}

void Path3D::SetSpeed(float speed) {
    SetPropertyValue<float>("speed", speed);
}

bool Path3D::GetLoop() const {
    return GetPropertyValue<bool>("loop");
}

void Path3D::SetLoop(bool loop) {
    SetPropertyValue<bool>("loop", loop);
}

bool Path3D::GetPingPong() const {
    return GetPropertyValue<bool>("pingPong");
}

void Path3D::SetPingPong(bool pingPong) {
    SetPropertyValue<bool>("pingPong", pingPong);
}

bool Path3D::GetAutoStart() const {
    return GetPropertyValue<bool>("autoStart");
}

void Path3D::SetAutoStart(bool autoStart) {
    SetPropertyValue<bool>("autoStart", autoStart);
}

// ===== Path Queries (local space) =====

Vec3 Path3D::GetCurrentPosition() const {
    return SampleCurve(m_Progress);
}

Vec3 Path3D::GetCurrentDirection() const {
    return SampleTangent(m_Progress);
}

Vec3 Path3D::GetPositionAtProgress(float t) const {
    return SampleCurve(t);
}

Vec3 Path3D::GetDirectionAtProgress(float t) const {
    return SampleTangent(t);
}

Vec3 Path3D::GetPositionAtDistance(float distance) const {
    float length = GetCurveLength();
    if (length <= 0.001f) {
        return GetStartPosition();
    }
    return SampleCurve(distance / length);
}

Vec3 Path3D::GetDirectionAtDistance(float distance) const {
    float length = GetCurveLength();
    if (length <= 0.001f) {
        return Vec3(1.0f, 0.0f, 0.0f);
    }
    return SampleTangent(distance / length);
}

Vec3 Path3D::GetStartPosition() const {
    if (m_CurvePoints.empty()) {
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    return m_CurvePoints[0].position;
}

Vec3 Path3D::GetEndPosition() const {
    if (m_CurvePoints.empty()) {
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    if (GetClosedLoop()) {
        return m_CurvePoints[0].position;
    }
    return m_CurvePoints.back().position;
}

// ===== Path Queries (world space) =====

Vec3 Path3D::GetCurrentPositionWorld() const {
    return SampleCurveWorld(m_Progress);
}

Vec3 Path3D::GetCurrentDirectionWorld() const {
    return SampleTangentWorld(m_Progress);
}

// ===== Closest-point queries =====

Vec3 Path3D::GetClosestPoint(const Vec3& position) const {
    UpdateCache();

    if (m_TessellatedCache.empty()) {
        return position;
    }
    if (m_TessellatedCache.size() == 1) {
        return m_TessellatedCache[0];
    }

    Vec3 closestPoint = m_TessellatedCache[0];
    float minDistSq = std::numeric_limits<float>::max();

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec3 a = m_TessellatedCache[i];
        Vec3 b = m_TessellatedCache[nextIdx];

        Vec3 ab = b - a;
        Vec3 ap = position - a;

        float abLenSq = ab.LengthSquared();
        float t = 0.0f;
        if (abLenSq > 0.0001f) {
            t = std::max(0.0f, std::min(1.0f, ap.Dot(ab) / abLenSq));
        }

        Vec3 closest = a + ab * t;
        float distSq = (position - closest).LengthSquared();

        if (distSq < minDistSq) {
            minDistSq = distSq;
            closestPoint = closest;
        }
    }

    return closestPoint;
}

float Path3D::GetClosestProgress(const Vec3& position) const {
    UpdateCache();

    if (m_TessellatedCache.empty() || m_CachedLength <= 0.001f) {
        return 0.0f;
    }

    float closestDist = 0.0f;
    float minDistSq = std::numeric_limits<float>::max();
    float accumulatedDist = 0.0f;

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec3 a = m_TessellatedCache[i];
        Vec3 b = m_TessellatedCache[nextIdx];

        Vec3 ab = b - a;
        Vec3 ap = position - a;

        float abLenSq = ab.LengthSquared();
        float segLen = std::sqrt(abLenSq);
        float t = 0.0f;
        if (abLenSq > 0.0001f) {
            t = std::max(0.0f, std::min(1.0f, ap.Dot(ab) / abLenSq));
        }

        Vec3 closest = a + ab * t;
        float distSq = (position - closest).LengthSquared();

        if (distSq < minDistSq) {
            minDistSq = distSq;
            closestDist = accumulatedDist + t * segLen;
        }

        accumulatedDist += segLen;
    }

    return closestDist / m_CachedLength;
}

float Path3D::GetClosestDistance(const Vec3& position) const {
    return GetClosestProgress(position) * GetCurveLength();
}

float Path3D::GetRemainingDistance() const {
    return (1.0f - m_Progress) * GetCurveLength();
}

float Path3D::GetTraveledDistance() const {
    return m_Progress * GetCurveLength();
}

// ===== Path Modification =====

void Path3D::ReversePath() {
    if (m_CurvePoints.size() < 2) {
        return;
    }

    std::reverse(m_CurvePoints.begin(), m_CurvePoints.end());

    for (auto& pt : m_CurvePoints) {
        std::swap(pt.controlIn, pt.controlOut);
    }

    m_IsReversed = !m_IsReversed;
    InvalidateCache();
    SyncPointsToProperty();
}

// ===== Scripting bridge =====

nlohmann::json Path3D::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argF = [&](size_t i, float fallback) -> float {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<float>();
        }
        return fallback;
    };
    auto vec3Json = [](const Vec3& v) -> nlohmann::json {
        nlohmann::json o;
        o["x"] = v.x; o["y"] = v.y; o["z"] = v.z;
        return o;
    };

    if (method == "start_following") {
        StartFollowing();
    } else if (method == "stop_following") {
        StopFollowing();
    } else if (method == "reset") {
        Reset();
    } else if (method == "is_following") {
        return IsFollowing();
    } else if (method == "get_progress") {
        return GetProgress();
    } else if (method == "set_progress") {
        SetProgress(argF(0, 0.0f));
    } else if (method == "set_speed") {
        SetSpeed(argF(0, 0.0f));
    } else if (method == "get_speed") {
        return GetSpeed();
    } else if (method == "set_loop") {
        SetLoop(args.is_array() && !args.empty() && args[0].is_boolean() ? args[0].get<bool>() : false);
    } else if (method == "set_ping_pong") {
        SetPingPong(args.is_array() && !args.empty() && args[0].is_boolean() ? args[0].get<bool>() : false);
    } else if (method == "get_current_position") {
        return vec3Json(GetCurrentPosition());
    } else if (method == "get_current_direction") {
        return vec3Json(GetCurrentDirection());
    } else if (method == "get_current_position_world") {
        return vec3Json(GetCurrentPositionWorld());
    } else if (method == "get_current_direction_world") {
        return vec3Json(GetCurrentDirectionWorld());
    } else if (method == "get_position_at_progress") {
        return vec3Json(GetPositionAtProgress(argF(0, 0.0f)));
    } else if (method == "get_position_at_distance") {
        return vec3Json(GetPositionAtDistance(argF(0, 0.0f)));
    } else if (method == "get_closest_progress") {
        return GetClosestProgress(Vec3(argF(0, 0.0f), argF(1, 0.0f), argF(2, 0.0f)));
    } else if (method == "get_closest_point") {
        return vec3Json(GetClosestPoint(Vec3(argF(0, 0.0f), argF(1, 0.0f), argF(2, 0.0f))));
    } else if (method == "get_remaining_distance") {
        return GetRemainingDistance();
    } else if (method == "get_traveled_distance") {
        return GetTraveledDistance();
    } else if (method == "reverse_path") {
        ReversePath();
    }

    return Curve3D::CallMethod(method, args);
}

// ===== Rendering =====

void Path3D::RenderPathMarkers(RenderContext& ctx) {
    if (m_CurvePoints.empty()) {
        return;
    }

    Mat4 world = GetOwnerWorldMatrix();

    bool showStartEnd = GetPropertyValue<bool>("showStartEnd");
    bool showDirection = GetPropertyValue<bool>("showDirection");
    Color startColor = GetPropertyValue<Color>("startColor");
    Color endColor = GetPropertyValue<Color>("endColor");

    if (showStartEnd) {
        Vec3 startPos = world.TransformPoint(GetStartPosition());
        ctx.drawBox(startPos, Vec3(0.25f, 0.25f, 0.25f), startColor, false);

        if (!GetClosedLoop()) {
            Vec3 endPos = world.TransformPoint(GetEndPosition());
            ctx.drawBox(endPos, Vec3(0.3f, 0.3f, 0.3f), endColor, true);
        }
    }

    if (showDirection) {
        float length = GetCurveLength();
        if (length > 0.5f) {
            int numArrows = std::max(1, static_cast<int>(length / 2.0f));
            Color arrowColor = GetDebugColor();
            arrowColor.a = 0.7f;

            float arrowSize = std::max(0.1f, std::min(0.4f, length * 0.05f));

            for (int i = 1; i <= numArrows; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(numArrows + 1);
                Vec3 pos = world.TransformPoint(SampleCurve(t));
                Vec3 dir = LocalDirectionToWorld(SampleTangent(t));

                if (dir.LengthSquared() < 0.0001f) {
                    continue;
                }

                // Use the curve's tilt-aware up frame so banking is visible.
                Vec3 up = SampleUpVectorWorld(t);
                Vec3 side = dir.Cross(up).Normalized();

                Vec3 tip = pos + dir * arrowSize;
                Vec3 left = pos - dir * arrowSize * 0.4f + side * arrowSize * 0.5f;
                Vec3 right = pos - dir * arrowSize * 0.4f - side * arrowSize * 0.5f;

                ctx.drawLine(left, tip, arrowColor, 1.5f);
                ctx.drawLine(right, tip, arrowColor, 1.5f);

                // Up-vector tick to visualize tilt/banking.
                Color upColor(0.4f, 0.6f, 1.0f, 0.6f);
                ctx.drawLine(pos, pos + up * arrowSize, upColor, 1.0f);
            }
        }
    }
}

void Path3D::buildDrawCommands(RenderContext& ctx) {
    Curve3D::buildDrawCommands(ctx);

    if (GetDebugDraw()) {
        RenderPathMarkers(ctx);
    }
}

} // namespace components
} // namespace lupine
