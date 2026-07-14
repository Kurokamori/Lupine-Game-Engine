#include "lupine/components/Curve2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Curve2D::Curve2D()
    : Component("Curve2D")
{
    m_CacheDirty = true;
}

Curve2D::Curve2D(const std::string& name)
    : Component(name)
{
    m_CacheDirty = true;
}

Curve2D::~Curve2D() {
}

void Curve2D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(pointsData, String, std::string("[]"), "Points"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(closedLoop, Bool, false, "Curve"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(bezierSegments, 32, 4, 64, 1, "Curve"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(debugDraw, Bool, true, "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, Color(0.2f, 0.8f, 0.2f, 1.0f), "Debug"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(debugLineWidth, 2.0f, 0.5f, 10.0f, 0.5f, "Debug"));
}

void Curve2D::OnAwake() {
    SyncPointsFromProperty();
}

void Curve2D::OnReady() {
}

void Curve2D::OnRender() {
}

// ===== Point Management =====

void Curve2D::SetCurvePoints(const std::vector<Curve2DPoint>& points) {
    m_CurvePoints = points;
    InvalidateCache();
    SyncPointsToProperty();
}

Curve2DPoint Curve2D::GetCurvePoint(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return Curve2DPoint();
    }
    return m_CurvePoints[index];
}

void Curve2D::SetCurvePoint(size_t index, const Curve2DPoint& point) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints[index] = point;
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve2D::AddCurvePoint(const Curve2DPoint& point) {
    m_CurvePoints.push_back(point);
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve2D::AddPoint(const Vec2& point) {
    m_CurvePoints.push_back(Curve2DPoint(point));
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve2D::InsertCurvePoint(size_t index, const Curve2DPoint& point) {
    if (index > m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints.insert(m_CurvePoints.begin() + index, point);
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve2D::RemovePoint(size_t index) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints.erase(m_CurvePoints.begin() + index);
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve2D::ClearPoints() {
    m_CurvePoints.clear();
    InvalidateCache();
    SyncPointsToProperty();
}

Vec2 Curve2D::GetPointPosition(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_CurvePoints[index].position;
}

void Curve2D::SetPointPosition(size_t index, const Vec2& position) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints[index].position = position;
    InvalidateCache();
    SyncPointsToProperty();
}

// ===== Bezier Control =====

void Curve2D::SetPointControlHandles(size_t index, const Vec2& controlIn, const Vec2& controlOut, bool symmetric) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints[index].controlIn = controlIn;
    m_CurvePoints[index].controlOut = controlOut;
    m_CurvePoints[index].symmetricHandles = symmetric;
    m_CurvePoints[index].useBezier = true;
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve2D::SetPointUseBezier(size_t index, bool useBezier) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints[index].useBezier = useBezier;
    InvalidateCache();
    SyncPointsToProperty();
}

bool Curve2D::GetPointUseBezier(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return false;
    }
    return m_CurvePoints[index].useBezier;
}

Vec2 Curve2D::GetPointControlIn(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_CurvePoints[index].controlIn;
}

Vec2 Curve2D::GetPointControlOut(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_CurvePoints[index].controlOut;
}

// ===== Curve Properties =====

bool Curve2D::GetClosedLoop() const {
    return GetPropertyValue<bool>("closedLoop");
}

void Curve2D::SetClosedLoop(bool closed) {
    SetPropertyValue<bool>("closedLoop", closed);
    InvalidateCache();
}

bool Curve2D::GetDebugDraw() const {
    return GetPropertyValue<bool>("debugDraw");
}

void Curve2D::SetDebugDraw(bool enabled) {
    SetPropertyValue<bool>("debugDraw", enabled);
}

Color Curve2D::GetDebugColor() const {
    return GetPropertyValue<Color>("debugColor");
}

void Curve2D::SetDebugColor(const Color& color) {
    SetPropertyValue<Color>("debugColor", color);
}

int Curve2D::GetBezierSegments() const {
    return GetPropertyValue<int>("bezierSegments");
}

void Curve2D::SetBezierSegments(int segments) {
    SetPropertyValue<int>("bezierSegments", segments);
    InvalidateCache();
}

// ===== Curve Sampling =====

Vec2 Curve2D::EvaluateCubicBezier(const Vec2& p0, const Vec2& p1,
                                   const Vec2& p2, const Vec2& p3, float t) const {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    Vec2 result = p0 * uuu;
    result = result + p1 * (3.0f * uu * t);
    result = result + p2 * (3.0f * u * tt);
    result = result + p3 * ttt;

    return result;
}

void Curve2D::UpdateCache() const {
    if (!m_CacheDirty) return;

    m_TessellatedCache.clear();
    m_CachedLength = 0.0f;

    if (m_CurvePoints.size() < 2) {
        if (m_CurvePoints.size() == 1) {
            m_TessellatedCache.push_back(m_CurvePoints[0].position);
        }
        m_CacheDirty = false;
        return;
    }

    int bezierSegments = GetBezierSegments();
    if (bezierSegments < 4) bezierSegments = 32;

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_CurvePoints.size() : m_CurvePoints.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_CurvePoints.size();
        const Curve2DPoint& p0 = m_CurvePoints[i];
        const Curve2DPoint& p1 = m_CurvePoints[nextIdx];

        bool useBezier = p0.useBezier || p1.useBezier;
        bool hasControlPoints = (p0.controlOut.x != 0.0f || p0.controlOut.y != 0.0f ||
                                 p1.controlIn.x != 0.0f || p1.controlIn.y != 0.0f);

        if (useBezier && hasControlPoints) {
            Vec2 start = p0.position;
            Vec2 ctrl1 = p0.position + p0.controlOut;
            Vec2 ctrl2 = p1.position + p1.controlIn;
            Vec2 end = p1.position;

            for (int j = 0; j <= bezierSegments; ++j) {
                float t = static_cast<float>(j) / static_cast<float>(bezierSegments);
                Vec2 pt = EvaluateCubicBezier(start, ctrl1, ctrl2, end, t);

                if (m_TessellatedCache.empty() ||
                    (pt.x != m_TessellatedCache.back().x || pt.y != m_TessellatedCache.back().y)) {
                    m_TessellatedCache.push_back(pt);
                }
            }
        } else {
            if (m_TessellatedCache.empty() ||
                (p0.position.x != m_TessellatedCache.back().x || p0.position.y != m_TessellatedCache.back().y)) {
                m_TessellatedCache.push_back(p0.position);
            }
            if (p1.position.x != m_TessellatedCache.back().x || p1.position.y != m_TessellatedCache.back().y) {
                m_TessellatedCache.push_back(p1.position);
            }
        }
    }

    // Calculate length
    for (size_t i = 1; i < m_TessellatedCache.size(); ++i) {
        Vec2 diff = m_TessellatedCache[i] - m_TessellatedCache[i - 1];
        m_CachedLength += std::sqrt(diff.x * diff.x + diff.y * diff.y);
    }

    if (closedLoop && m_TessellatedCache.size() > 1) {
        Vec2 diff = m_TessellatedCache[0] - m_TessellatedCache.back();
        m_CachedLength += std::sqrt(diff.x * diff.x + diff.y * diff.y);
    }

    m_CacheDirty = false;
}

std::vector<Vec2> Curve2D::GetTessellatedPoints() const {
    UpdateCache();
    return m_TessellatedCache;
}

float Curve2D::GetCurveLength() const {
    UpdateCache();
    return m_CachedLength;
}

Vec2 Curve2D::SampleCurve(float t) const {
    UpdateCache();

    if (m_TessellatedCache.empty()) {
        return Vec2(0.0f, 0.0f);
    }

    if (m_TessellatedCache.size() == 1) {
        return m_TessellatedCache[0];
    }

    t = std::max(0.0f, std::min(1.0f, t));

    float targetDist = t * m_CachedLength;
    float accumulatedDist = 0.0f;

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec2 segStart = m_TessellatedCache[i];
        Vec2 segEnd = m_TessellatedCache[nextIdx];
        Vec2 diff = segEnd - segStart;
        float segLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);

        if (accumulatedDist + segLen >= targetDist) {
            float segT = (segLen > 0.001f) ? (targetDist - accumulatedDist) / segLen : 0.0f;
            return segStart + diff * segT;
        }

        accumulatedDist += segLen;
    }

    return closedLoop ? m_TessellatedCache[0] : m_TessellatedCache.back();
}

Vec2 Curve2D::SampleTangent(float t) const {
    UpdateCache();

    if (m_TessellatedCache.size() < 2) {
        return Vec2(1.0f, 0.0f);
    }

    t = std::max(0.0f, std::min(1.0f, t));

    float targetDist = t * m_CachedLength;
    float accumulatedDist = 0.0f;

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec2 segStart = m_TessellatedCache[i];
        Vec2 segEnd = m_TessellatedCache[nextIdx];
        Vec2 diff = segEnd - segStart;
        float segLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);

        if (accumulatedDist + segLen >= targetDist || i == numSegments - 1) {
            if (segLen > 0.001f) {
                return diff * (1.0f / segLen);
            }
            return Vec2(1.0f, 0.0f);
        }

        accumulatedDist += segLen;
    }

    return Vec2(1.0f, 0.0f);
}

// ===== Serialization =====

void Curve2D::SyncPointsToProperty() {
    nlohmann::json pointsArray = nlohmann::json::array();

    for (const auto& pt : m_CurvePoints) {
        nlohmann::json pointObj;
        pointObj["x"] = pt.position.x;
        pointObj["y"] = pt.position.y;
        pointObj["ctrlInX"] = pt.controlIn.x;
        pointObj["ctrlInY"] = pt.controlIn.y;
        pointObj["ctrlOutX"] = pt.controlOut.x;
        pointObj["ctrlOutY"] = pt.controlOut.y;
        pointObj["useBezier"] = pt.useBezier;
        pointObj["symmetric"] = pt.symmetricHandles;
        pointsArray.push_back(pointObj);
    }

    std::string jsonStr = pointsArray.dump();
    SetPropertyValue<std::string>("pointsData", jsonStr);
}

void Curve2D::SyncPointsFromProperty() {
    std::string jsonStr = GetPropertyValue<std::string>("pointsData");

    if (jsonStr.empty() || jsonStr == "[]") {
        m_CurvePoints.clear();
        InvalidateCache();
        return;
    }

    try {
        nlohmann::json pointsArray = nlohmann::json::parse(jsonStr);

        if (!pointsArray.is_array()) {
            return;
        }

        m_CurvePoints.clear();
        for (const auto& pointObj : pointsArray) {
            Curve2DPoint pt;

            if (pointObj.contains("x") && pointObj.contains("y")) {
                pt.position.x = pointObj["x"].get<float>();
                pt.position.y = pointObj["y"].get<float>();
            }
            if (pointObj.contains("ctrlInX") && pointObj.contains("ctrlInY")) {
                pt.controlIn.x = pointObj["ctrlInX"].get<float>();
                pt.controlIn.y = pointObj["ctrlInY"].get<float>();
            }
            if (pointObj.contains("ctrlOutX") && pointObj.contains("ctrlOutY")) {
                pt.controlOut.x = pointObj["ctrlOutX"].get<float>();
                pt.controlOut.y = pointObj["ctrlOutY"].get<float>();
            }
            if (pointObj.contains("useBezier")) {
                pt.useBezier = pointObj["useBezier"].get<bool>();
            }
            if (pointObj.contains("symmetric")) {
                pt.symmetricHandles = pointObj["symmetric"].get<bool>();
            }

            m_CurvePoints.push_back(pt);
        }

        InvalidateCache();
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::ECS, "Curve2D: failed to parse points property: {}", e.what());
    }
}

// ===== Scripting bridge =====

nlohmann::json Curve2D::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argF = [&](size_t i, float fallback) -> float {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<float>();
        }
        return fallback;
    };
    auto argI = [&](size_t i, int fallback) -> int {
        if (args.is_array() && i < args.size() && args[i].is_number_integer()) {
            return args[i].get<int>();
        }
        return fallback;
    };
    auto argB = [&](size_t i, bool fallback) -> bool {
        if (args.is_array() && i < args.size() && args[i].is_boolean()) {
            return args[i].get<bool>();
        }
        return fallback;
    };
    auto vec2Json = [](const Vec2& v) -> nlohmann::json {
        nlohmann::json o;
        o["x"] = v.x; o["y"] = v.y;
        return o;
    };

    if (method == "add_point") {
        AddPoint(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
    } else if (method == "set_point_position") {
        SetPointPosition(static_cast<size_t>(argI(0, 0)), Vec2(argF(1, 0.0f), argF(2, 0.0f)));
    } else if (method == "remove_point") {
        RemovePoint(static_cast<size_t>(argI(0, 0)));
    } else if (method == "clear_points") {
        ClearPoints();
    } else if (method == "get_point_count") {
        return static_cast<int>(GetPointCount());
    } else if (method == "get_point_position") {
        return vec2Json(GetPointPosition(static_cast<size_t>(argI(0, 0))));
    } else if (method == "set_point_use_bezier") {
        SetPointUseBezier(static_cast<size_t>(argI(0, 0)), argB(1, true));
    } else if (method == "set_point_control_handles") {
        SetPointControlHandles(static_cast<size_t>(argI(0, 0)),
                               Vec2(argF(1, 0.0f), argF(2, 0.0f)),
                               Vec2(argF(3, 0.0f), argF(4, 0.0f)),
                               argB(5, true));
    } else if (method == "set_closed_loop") {
        SetClosedLoop(argB(0, false));
    } else if (method == "get_closed_loop") {
        return GetClosedLoop();
    } else if (method == "get_curve_length") {
        return GetCurveLength();
    } else if (method == "sample_curve") {
        return vec2Json(SampleCurve(argF(0, 0.0f)));
    } else if (method == "sample_tangent") {
        return vec2Json(SampleTangent(argF(0, 0.0f)));
    }
    return nlohmann::json();
}

// ===== Hit Testing & Control Point Movement =====

int Curve2D::HitTestControlPoint(const Vec2& worldPos, float radius) const {
    if (m_CurvePoints.empty() || m_SelectedPointIndex < 0) {
        return -1;
    }

    if (m_SelectedPointIndex >= static_cast<int>(m_CurvePoints.size())) {
        return -1;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    int i = m_SelectedPointIndex;
    const Curve2DPoint& pt = m_CurvePoints[i];
    Vec2 anchorPos = pt.position + offset;

    // Test control out first
    Vec2 ctrlOutPos = anchorPos + pt.controlOut;
    float distOut = std::sqrt((worldPos.x - ctrlOutPos.x) * (worldPos.x - ctrlOutPos.x) +
                              (worldPos.y - ctrlOutPos.y) * (worldPos.y - ctrlOutPos.y));
    if (distOut <= radius) {
        return i * 10 + 2;
    }

    Vec2 ctrlInPos = anchorPos + pt.controlIn;
    float distIn = std::sqrt((worldPos.x - ctrlInPos.x) * (worldPos.x - ctrlInPos.x) +
                             (worldPos.y - ctrlInPos.y) * (worldPos.y - ctrlInPos.y));
    if (distIn <= radius) {
        return i * 10 + 1;
    }

    // Test anchor
    float distAnchor = std::sqrt((worldPos.x - anchorPos.x) * (worldPos.x - anchorPos.x) +
                                 (worldPos.y - anchorPos.y) * (worldPos.y - anchorPos.y));
    if (distAnchor <= radius) {
        return i * 10 + 0;
    }

    return -1;
}

void Curve2D::MoveControlPoint(int controlPointId, const Vec2& worldPos) {
    if (controlPointId < 0 || m_CurvePoints.empty()) {
        return;
    }

    int pointIndex = controlPointId / 10;
    int handleType = controlPointId % 10;

    if (pointIndex < 0 || pointIndex >= static_cast<int>(m_CurvePoints.size())) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    Curve2DPoint& pt = m_CurvePoints[pointIndex];
    Vec2 localPos = worldPos - offset;

    if (handleType == 0) {
        pt.position = localPos;
    } else if (handleType == 1) {
        pt.controlIn = localPos - pt.position - offset;
        if (pt.controlIn.x != 0.0f || pt.controlIn.y != 0.0f) {
            pt.useBezier = true;
        }
        if (pt.symmetricHandles) {
            pt.controlOut = Vec2(-pt.controlIn.x, -pt.controlIn.y);
        }
    } else if (handleType == 2) {
        pt.controlOut = localPos - pt.position - offset;
        if (pt.controlOut.x != 0.0f || pt.controlOut.y != 0.0f) {
            pt.useBezier = true;
        }
        if (pt.symmetricHandles) {
            pt.controlIn = Vec2(-pt.controlOut.x, -pt.controlOut.y);
        }
    }

    InvalidateCache();
    SyncPointsToProperty();
}

// ===== Rendering =====

void Curve2D::RenderDebugCurve(RenderContext& ctx) {
    UpdateCache();

    if (m_TessellatedCache.size() < 2) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    Color debugColor = GetDebugColor();
    float lineWidth = GetPropertyValue<float>("debugLineWidth");
    float halfWidth = lineWidth * 0.5f;
    bool closedLoop = GetClosedLoop();

    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec2 start = m_TessellatedCache[i] + offset;
        Vec2 end = m_TessellatedCache[nextIdx] + offset;

        Vec2 dir = end - start;
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

        if (length < 0.001f) continue;

        float angle = std::atan2(dir.y, dir.x);
        Vec2 midpoint = (start + end) * 0.5f;

        ctx.drawRoundedRect(midpoint, Vec2(length, lineWidth), Vec4(0,0,0,0), debugColor, angle, 0);

        // Draw small circles at joints
        ctx.drawCircle(Vec3(start.x, start.y, 0.0f), halfWidth, debugColor, 0);
    }

    // Draw final point
    if (!closedLoop && !m_TessellatedCache.empty()) {
        Vec2 lastPt = m_TessellatedCache.back() + offset;
        ctx.drawCircle(Vec3(lastPt.x, lastPt.y, 0.0f), halfWidth, debugColor, 0);
    }
}

void Curve2D::RenderBezierHandles(RenderContext& ctx) {
    if (m_CurvePoints.empty() || m_SelectedPointIndex < 0) {
        return;
    }

    if (m_SelectedPointIndex >= static_cast<int>(m_CurvePoints.size())) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    Color handleLineColor(0.4f, 0.6f, 1.0f, 0.8f);
    Color handlePointColor(1.0f, 1.0f, 1.0f, 1.0f);
    Color anchorPointColor(0.2f, 0.6f, 1.0f, 1.0f);

    float handleRadius = 3.0f;
    float anchorRadius = 2.5f;
    float handleLineWidth = 1.0f;

    const Curve2DPoint& pt = m_CurvePoints[m_SelectedPointIndex];
    Vec2 anchorPos = pt.position + offset;

    // Draw anchor point
    ctx.drawRoundedRect(anchorPos, Vec2(anchorRadius * 2, anchorRadius * 2),
                       Vec4(0.0f, 0.0f, 0.0f, 0.0f), anchorPointColor, 0.0f, 0);

    // Control In
    Vec2 ctrlInPos = anchorPos + pt.controlIn;
    Vec2 dir = ctrlInPos - anchorPos;
    float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length > 0.001f) {
        float angle = std::atan2(dir.y, dir.x);
        Vec2 midpoint = (anchorPos + ctrlInPos) * 0.5f;
        ctx.drawRoundedRect(midpoint, Vec2(length, handleLineWidth),
                           Vec4(0.0f, 0.0f, 0.0f, 0.0f), handleLineColor, angle, 0);
    }
    ctx.drawCircle(Vec3(ctrlInPos.x, ctrlInPos.y, 0.0f), handleRadius, handlePointColor, 0);

    // Control Out
    Vec2 ctrlOutPos = anchorPos + pt.controlOut;
    dir = ctrlOutPos - anchorPos;
    length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length > 0.001f) {
        float angle = std::atan2(dir.y, dir.x);
        Vec2 midpoint = (anchorPos + ctrlOutPos) * 0.5f;
        ctx.drawRoundedRect(midpoint, Vec2(length, handleLineWidth),
                           Vec4(0.0f, 0.0f, 0.0f, 0.0f), handleLineColor, angle, 0);
    }
    ctx.drawCircle(Vec3(ctrlOutPos.x, ctrlOutPos.y, 0.0f), handleRadius, handlePointColor, 0);
}

void Curve2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    SyncPointsFromProperty();

    // Only render if debug draw is enabled
    if (GetDebugDraw()) {
        RenderDebugCurve(ctx);
    }

    // Render handles when editing
    if (m_ShowBezierHandles) {
        RenderBezierHandles(ctx);
    }
}

AABB Curve2D::getWorldBounds() const {
    if (m_CurvePoints.empty()) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    UpdateCache();

    if (m_TessellatedCache.empty()) {
        return AABB();
    }

    Vec2 minPt = m_TessellatedCache[0] + offset;
    Vec2 maxPt = minPt;

    for (const auto& pt : m_TessellatedCache) {
        Vec2 worldPt = pt + offset;
        minPt.x = std::min(minPt.x, worldPt.x);
        minPt.y = std::min(minPt.y, worldPt.y);
        maxPt.x = std::max(maxPt.x, worldPt.x);
        maxPt.y = std::max(maxPt.y, worldPt.y);
    }

    float padding = 5.0f;
    return AABB(
        Vec3(minPt.x - padding, minPt.y - padding, -0.1f),
        Vec3(maxPt.x + padding, maxPt.y + padding, 0.1f)
    );
}

RenderLayer Curve2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType Curve2D::getSpatialType() const {
    return SpatialType::World2D;
}

} // namespace components
} // namespace lupine

