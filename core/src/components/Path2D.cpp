#include "lupine/components/Path2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Path2D::Path2D()
    : Curve2D("Path2D")
{
}

Path2D::Path2D(const std::string& name)
    : Curve2D(name)
{
}

Path2D::~Path2D() {
}

void Path2D::DefineProperties() {
    // Call parent to define curve properties
    Curve2D::DefineProperties();

    // Path-specific properties
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(speed, 100.0f, 0.0f, 10000.0f, 10.0f, "Path"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, false, "Path"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pingPong, Bool, false, "Path"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoStart, Bool, false, "Path"));

    // Visual
    DefineProperty(PROPERTY_DEFAULT_GROUP(showStartEnd, Bool, true, "Path Markers"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(showDirection, Bool, true, "Path Markers"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(startColor, Color, Color(0.2f, 1.0f, 0.2f, 1.0f), "Path Markers"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(endColor, Color, Color(1.0f, 0.2f, 0.2f, 1.0f), "Path Markers"));
}

void Path2D::OnAwake() {
    Curve2D::OnAwake();
    
    if (GetAutoStart()) {
        StartFollowing();
    }
}

void Path2D::OnUpdate(float deltaTime) {
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
                // Check if we've completed a full cycle
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

void Path2D::StartFollowing() {
    m_IsFollowing = true;
}

void Path2D::StopFollowing() {
    m_IsFollowing = false;
}

void Path2D::Reset() {
    m_Progress = 0.0f;
    m_Direction = 1;
    m_IsReversed = false;
}

void Path2D::SetProgress(float progress) {
    m_Progress = std::max(0.0f, std::min(1.0f, progress));
}

float Path2D::GetSpeed() const {
    return GetPropertyValue<float>("speed");
}

void Path2D::SetSpeed(float speed) {
    SetPropertyValue<float>("speed", speed);
}

bool Path2D::GetLoop() const {
    return GetPropertyValue<bool>("loop");
}

void Path2D::SetLoop(bool loop) {
    SetPropertyValue<bool>("loop", loop);
}

bool Path2D::GetPingPong() const {
    return GetPropertyValue<bool>("pingPong");
}

void Path2D::SetPingPong(bool pingPong) {
    SetPropertyValue<bool>("pingPong", pingPong);
}

bool Path2D::GetAutoStart() const {
    return GetPropertyValue<bool>("autoStart");
}

void Path2D::SetAutoStart(bool autoStart) {
    SetPropertyValue<bool>("autoStart", autoStart);
}

// ===== Path Queries =====

Vec2 Path2D::GetCurrentPosition() const {
    return SampleCurve(m_Progress);
}

Vec2 Path2D::GetCurrentDirection() const {
    return SampleTangent(m_Progress);
}

Vec2 Path2D::GetPositionAtProgress(float t) const {
    return SampleCurve(t);
}

Vec2 Path2D::GetDirectionAtProgress(float t) const {
    return SampleTangent(t);
}

Vec2 Path2D::GetPositionAtDistance(float distance) const {
    float length = GetCurveLength();
    if (length <= 0.001f) {
        return GetStartPosition();
    }
    float t = distance / length;
    return SampleCurve(t);
}

Vec2 Path2D::GetDirectionAtDistance(float distance) const {
    float length = GetCurveLength();
    if (length <= 0.001f) {
        return Vec2(1.0f, 0.0f);
    }
    float t = distance / length;
    return SampleTangent(t);
}

Vec2 Path2D::GetStartPosition() const {
    if (m_CurvePoints.empty()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_CurvePoints[0].position;
}

Vec2 Path2D::GetEndPosition() const {
    if (m_CurvePoints.empty()) {
        return Vec2(0.0f, 0.0f);
    }
    if (GetClosedLoop()) {
        return m_CurvePoints[0].position;
    }
    return m_CurvePoints.back().position;
}

Vec2 Path2D::GetClosestPoint(const math::Vec2& position) const {
    UpdateCache();

    if (m_TessellatedCache.empty()) {
        return position;
    }

    if (m_TessellatedCache.size() == 1) {
        return m_TessellatedCache[0];
    }

    Vec2 closestPoint = m_TessellatedCache[0];
    float minDistSq = std::numeric_limits<float>::max();

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec2 a = m_TessellatedCache[i];
        Vec2 b = m_TessellatedCache[nextIdx];

        Vec2 ab = b - a;
        Vec2 ap = position - a;

        float abLenSq = ab.x * ab.x + ab.y * ab.y;
        float t = 0.0f;
        if (abLenSq > 0.0001f) {
            t = (ap.x * ab.x + ap.y * ab.y) / abLenSq;
            t = std::max(0.0f, std::min(1.0f, t));
        }

        Vec2 closest = a + ab * t;
        Vec2 diff = position - closest;
        float distSq = diff.x * diff.x + diff.y * diff.y;

        if (distSq < minDistSq) {
            minDistSq = distSq;
            closestPoint = closest;
        }
    }

    return closestPoint;
}

float Path2D::GetClosestProgress(const math::Vec2& position) const {
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
        Vec2 a = m_TessellatedCache[i];
        Vec2 b = m_TessellatedCache[nextIdx];

        Vec2 ab = b - a;
        Vec2 ap = position - a;

        float abLenSq = ab.x * ab.x + ab.y * ab.y;
        float segLen = std::sqrt(abLenSq);
        float t = 0.0f;
        if (abLenSq > 0.0001f) {
            t = (ap.x * ab.x + ap.y * ab.y) / abLenSq;
            t = std::max(0.0f, std::min(1.0f, t));
        }

        Vec2 closest = a + ab * t;
        Vec2 diff = position - closest;
        float distSq = diff.x * diff.x + diff.y * diff.y;

        if (distSq < minDistSq) {
            minDistSq = distSq;
            closestDist = accumulatedDist + t * segLen;
        }

        accumulatedDist += segLen;
    }

    return closestDist / m_CachedLength;
}

float Path2D::GetClosestDistance(const math::Vec2& position) const {
    return GetClosestProgress(position) * GetCurveLength();
}

float Path2D::GetRemainingDistance() const {
    return (1.0f - m_Progress) * GetCurveLength();
}

float Path2D::GetTraveledDistance() const {
    return m_Progress * GetCurveLength();
}

// ===== Path Modification =====

void Path2D::ReversePath() {
    if (m_CurvePoints.size() < 2) {
        return;
    }

    std::reverse(m_CurvePoints.begin(), m_CurvePoints.end());

    // Swap control in/out for each point
    for (auto& pt : m_CurvePoints) {
        std::swap(pt.controlIn, pt.controlOut);
    }

    m_IsReversed = !m_IsReversed;
    InvalidateCache();
    SyncPointsToProperty();
}

// ===== Scripting bridge =====

nlohmann::json Path2D::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argF = [&](size_t i, float fallback) -> float {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<float>();
        }
        return fallback;
    };
    auto vec2Json = [](const Vec2& v) -> nlohmann::json {
        nlohmann::json o;
        o["x"] = v.x; o["y"] = v.y;
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
        return vec2Json(GetCurrentPosition());
    } else if (method == "get_current_direction") {
        return vec2Json(GetCurrentDirection());
    } else if (method == "get_position_at_progress") {
        return vec2Json(GetPositionAtProgress(argF(0, 0.0f)));
    } else if (method == "get_position_at_distance") {
        return vec2Json(GetPositionAtDistance(argF(0, 0.0f)));
    } else if (method == "get_closest_progress") {
        return GetClosestProgress(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
    } else if (method == "get_closest_point") {
        return vec2Json(GetClosestPoint(Vec2(argF(0, 0.0f), argF(1, 0.0f))));
    } else if (method == "get_remaining_distance") {
        return GetRemainingDistance();
    } else if (method == "get_traveled_distance") {
        return GetTraveledDistance();
    } else if (method == "reverse_path") {
        ReversePath();
    }

    return Curve2D::CallMethod(method, args);
}

// ===== Rendering =====

void Path2D::RenderPathMarkers(RenderContext& ctx) {
    if (m_CurvePoints.empty()) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    bool showStartEnd = GetPropertyValue<bool>("showStartEnd");
    bool showDirection = GetPropertyValue<bool>("showDirection");
    Color startColor = GetPropertyValue<Color>("startColor");
    Color endColor = GetPropertyValue<Color>("endColor");

    if (showStartEnd) {
        // Draw start marker (larger circle)
        Vec2 startPos = GetStartPosition() + offset;
        ctx.drawCircle(Vec3(startPos.x, startPos.y, 0.0f), 8.0f, startColor, 0);

        // Draw end marker (square)
        Vec2 endPos = GetEndPosition() + offset;
        if (!GetClosedLoop()) {
            ctx.drawRoundedRect(endPos, Vec2(12.0f, 12.0f), Vec4(0,0,0,0), endColor, 0.0f, 0);
        }
    }

    // Draw direction arrows along the path
    if (showDirection && m_TessellatedCache.size() >= 2) {
        float length = GetCurveLength();
        if (length > 20.0f) {
            int numArrows = std::max(1, static_cast<int>(length / 50.0f));
            Color arrowColor = GetDebugColor();
            arrowColor.a = 0.6f;

            for (int i = 1; i <= numArrows; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(numArrows + 1);
                Vec2 pos = SampleCurve(t) + offset;
                Vec2 dir = SampleTangent(t);

                // Draw arrow head
                float arrowSize = 6.0f;

                // Arrow head triangle (as three small rects)
                Vec2 tip = pos + dir * arrowSize;
                Vec2 perpDir(-dir.y, dir.x);
                Vec2 left = pos - dir * arrowSize * 0.3f + perpDir * arrowSize * 0.5f;
                Vec2 right = pos - dir * arrowSize * 0.3f - perpDir * arrowSize * 0.5f;

                // Draw lines from corners to tip
                auto drawLine = [&](const Vec2& from, const Vec2& to) {
                    Vec2 lineDir = to - from;
                    float lineLen = std::sqrt(lineDir.x * lineDir.x + lineDir.y * lineDir.y);
                    if (lineLen > 0.001f) {
                        float lineAngle = std::atan2(lineDir.y, lineDir.x);
                        Vec2 mid = (from + to) * 0.5f;
                        ctx.drawRoundedRect(mid, Vec2(lineLen, 2.0f), Vec4(0,0,0,0), arrowColor, lineAngle, 0);
                    }
                };

                drawLine(left, tip);
                drawLine(right, tip);
                drawLine(left, right);
            }
        }
    }
}

void Path2D::buildDrawCommands(RenderContext& ctx) {
    // Call parent rendering (debug curve + bezier handles)
    Curve2D::buildDrawCommands(ctx);

    // Add path-specific markers
    if (GetDebugDraw()) {
        RenderPathMarkers(ctx);
    }
}

} // namespace components
} // namespace lupine

