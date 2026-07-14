#include "lupine/components/Curve3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Curve3D::Curve3D()
    : Component("Curve3D")
{
    m_CacheDirty = true;
}

Curve3D::Curve3D(const std::string& name)
    : Component(name)
{
    m_CacheDirty = true;
}

Curve3D::~Curve3D() {
}

void Curve3D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(pointsData, String, std::string("[]"), "Points"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(closedLoop, Bool, false, "Curve"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(bezierSegments, 32, 4, 64, 1, "Curve"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(debugDraw, Bool, true, "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, Color(0.2f, 0.8f, 0.2f, 1.0f), "Debug"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(debugLineWidth, 2.0f, 0.5f, 10.0f, 0.5f, "Debug"));
}

void Curve3D::OnAwake() {
    SyncPointsFromProperty();
}

void Curve3D::OnReady() {
}

void Curve3D::OnRender() {
}

// ===== Point Management =====

void Curve3D::SetCurvePoints(const std::vector<Curve3DPoint>& points) {
    m_CurvePoints = points;
    InvalidateCache();
    SyncPointsToProperty();
}

Curve3DPoint Curve3D::GetCurvePoint(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return Curve3DPoint();
    }
    return m_CurvePoints[index];
}

void Curve3D::SetCurvePoint(size_t index, const Curve3DPoint& point) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints[index] = point;
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve3D::AddCurvePoint(const Curve3DPoint& point) {
    m_CurvePoints.push_back(point);
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve3D::AddPoint(const Vec3& point) {
    m_CurvePoints.push_back(Curve3DPoint(point));
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve3D::InsertCurvePoint(size_t index, const Curve3DPoint& point) {
    if (index > m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints.insert(m_CurvePoints.begin() + index, point);
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve3D::RemovePoint(size_t index) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints.erase(m_CurvePoints.begin() + index);
    InvalidateCache();
    SyncPointsToProperty();
}

void Curve3D::ClearPoints() {
    m_CurvePoints.clear();
    InvalidateCache();
    SyncPointsToProperty();
}

Vec3 Curve3D::GetPointPosition(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    return m_CurvePoints[index].position;
}

void Curve3D::SetPointPosition(size_t index, const Vec3& position) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints[index].position = position;
    InvalidateCache();
    SyncPointsToProperty();
}

// ===== Bezier Control =====

void Curve3D::SetPointControlHandles(size_t index, const Vec3& controlIn, const Vec3& controlOut, bool symmetric) {
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

void Curve3D::SetPointUseBezier(size_t index, bool useBezier) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints[index].useBezier = useBezier;
    InvalidateCache();
    SyncPointsToProperty();
}

bool Curve3D::GetPointUseBezier(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return false;
    }
    return m_CurvePoints[index].useBezier;
}

Vec3 Curve3D::GetPointControlIn(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    return m_CurvePoints[index].controlIn;
}

Vec3 Curve3D::GetPointControlOut(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    return m_CurvePoints[index].controlOut;
}

float Curve3D::GetPointTilt(size_t index) const {
    if (index >= m_CurvePoints.size()) {
        return 0.0f;
    }
    return m_CurvePoints[index].tilt;
}

void Curve3D::SetPointTilt(size_t index, float tilt) {
    if (index >= m_CurvePoints.size()) {
        return;
    }
    m_CurvePoints[index].tilt = tilt;
    InvalidateCache();
    SyncPointsToProperty();
}

// ===== Curve Properties =====

bool Curve3D::GetClosedLoop() const {
    return GetPropertyValue<bool>("closedLoop");
}

void Curve3D::SetClosedLoop(bool closed) {
    SetPropertyValue<bool>("closedLoop", closed);
    InvalidateCache();
}

bool Curve3D::GetDebugDraw() const {
    return GetPropertyValue<bool>("debugDraw");
}

void Curve3D::SetDebugDraw(bool enabled) {
    SetPropertyValue<bool>("debugDraw", enabled);
}

Color Curve3D::GetDebugColor() const {
    return GetPropertyValue<Color>("debugColor");
}

void Curve3D::SetDebugColor(const Color& color) {
    SetPropertyValue<Color>("debugColor", color);
}

int Curve3D::GetBezierSegments() const {
    return GetPropertyValue<int>("bezierSegments");
}

void Curve3D::SetBezierSegments(int segments) {
    SetPropertyValue<int>("bezierSegments", segments);
    InvalidateCache();
}

// ===== Curve Sampling =====

Vec3 Curve3D::EvaluateCubicBezier(const Vec3& p0, const Vec3& p1,
                                  const Vec3& p2, const Vec3& p3, float t) const {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    Vec3 result = p0 * uuu;
    result = result + p1 * (3.0f * uu * t);
    result = result + p2 * (3.0f * u * tt);
    result = result + p3 * ttt;

    return result;
}

void Curve3D::UpdateCache() const {
    if (!m_CacheDirty) return;

    m_TessellatedCache.clear();
    m_TessellatedTilt.clear();
    m_CachedLength = 0.0f;

    if (m_CurvePoints.size() < 2) {
        if (m_CurvePoints.size() == 1) {
            m_TessellatedCache.push_back(m_CurvePoints[0].position);
            m_TessellatedTilt.push_back(m_CurvePoints[0].tilt);
        }
        m_CacheDirty = false;
        return;
    }

    int bezierSegments = GetBezierSegments();
    if (bezierSegments < 4) bezierSegments = 32;

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_CurvePoints.size() : m_CurvePoints.size() - 1;

    auto pushSample = [this](const Vec3& pt, float tilt) {
        if (!m_TessellatedCache.empty()) {
            const Vec3& b = m_TessellatedCache.back();
            if (pt.x == b.x && pt.y == b.y && pt.z == b.z) {
                return;
            }
        }
        m_TessellatedCache.push_back(pt);
        m_TessellatedTilt.push_back(tilt);
    };

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_CurvePoints.size();
        const Curve3DPoint& p0 = m_CurvePoints[i];
        const Curve3DPoint& p1 = m_CurvePoints[nextIdx];

        bool useBezier = p0.useBezier || p1.useBezier;
        bool hasControlPoints = (p0.controlOut.x != 0.0f || p0.controlOut.y != 0.0f || p0.controlOut.z != 0.0f ||
                                 p1.controlIn.x != 0.0f || p1.controlIn.y != 0.0f || p1.controlIn.z != 0.0f);

        if (useBezier && hasControlPoints) {
            Vec3 start = p0.position;
            Vec3 ctrl1 = p0.position + p0.controlOut;
            Vec3 ctrl2 = p1.position + p1.controlIn;
            Vec3 end = p1.position;

            for (int j = 0; j <= bezierSegments; ++j) {
                float t = static_cast<float>(j) / static_cast<float>(bezierSegments);
                Vec3 pt = EvaluateCubicBezier(start, ctrl1, ctrl2, end, t);
                float tilt = p0.tilt + (p1.tilt - p0.tilt) * t;
                pushSample(pt, tilt);
            }
        } else {
            pushSample(p0.position, p0.tilt);
            pushSample(p1.position, p1.tilt);
        }
    }

    for (size_t i = 1; i < m_TessellatedCache.size(); ++i) {
        m_CachedLength += (m_TessellatedCache[i] - m_TessellatedCache[i - 1]).Length();
    }

    if (closedLoop && m_TessellatedCache.size() > 1) {
        m_CachedLength += (m_TessellatedCache[0] - m_TessellatedCache.back()).Length();
    }

    m_CacheDirty = false;
}

std::vector<Vec3> Curve3D::GetTessellatedPoints() const {
    UpdateCache();
    return m_TessellatedCache;
}

float Curve3D::GetCurveLength() const {
    UpdateCache();
    return m_CachedLength;
}

Vec3 Curve3D::SampleCurve(float t) const {
    UpdateCache();

    if (m_TessellatedCache.empty()) {
        return Vec3(0.0f, 0.0f, 0.0f);
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
        Vec3 segStart = m_TessellatedCache[i];
        Vec3 segEnd = m_TessellatedCache[nextIdx];
        Vec3 diff = segEnd - segStart;
        float segLen = diff.Length();

        if (accumulatedDist + segLen >= targetDist) {
            float segT = (segLen > 0.001f) ? (targetDist - accumulatedDist) / segLen : 0.0f;
            return segStart + diff * segT;
        }

        accumulatedDist += segLen;
    }

    return closedLoop ? m_TessellatedCache[0] : m_TessellatedCache.back();
}

Vec3 Curve3D::SampleTangent(float t) const {
    UpdateCache();

    if (m_TessellatedCache.size() < 2) {
        return Vec3(1.0f, 0.0f, 0.0f);
    }

    t = std::max(0.0f, std::min(1.0f, t));

    float targetDist = t * m_CachedLength;
    float accumulatedDist = 0.0f;

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec3 segStart = m_TessellatedCache[i];
        Vec3 segEnd = m_TessellatedCache[nextIdx];
        Vec3 diff = segEnd - segStart;
        float segLen = diff.Length();

        if (accumulatedDist + segLen >= targetDist || i == numSegments - 1) {
            if (segLen > 0.001f) {
                return diff * (1.0f / segLen);
            }
            return Vec3(1.0f, 0.0f, 0.0f);
        }

        accumulatedDist += segLen;
    }

    return Vec3(1.0f, 0.0f, 0.0f);
}

float Curve3D::SampleTilt(float t) const {
    UpdateCache();

    if (m_TessellatedTilt.empty()) {
        return 0.0f;
    }
    if (m_TessellatedTilt.size() == 1) {
        return m_TessellatedTilt[0];
    }

    t = std::max(0.0f, std::min(1.0f, t));

    float targetDist = t * m_CachedLength;
    float accumulatedDist = 0.0f;

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec3 diff = m_TessellatedCache[nextIdx] - m_TessellatedCache[i];
        float segLen = diff.Length();

        if (accumulatedDist + segLen >= targetDist) {
            float segT = (segLen > 0.001f) ? (targetDist - accumulatedDist) / segLen : 0.0f;
            return m_TessellatedTilt[i] + (m_TessellatedTilt[nextIdx] - m_TessellatedTilt[i]) * segT;
        }

        accumulatedDist += segLen;
    }

    return m_TessellatedTilt.back();
}

Vec3 Curve3D::SampleUpVector(float t) const {
    Vec3 tangent = SampleTangent(t);
    if (tangent.LengthSquared() < 0.0001f) {
        return Vec3::Up();
    }
    tangent = tangent.Normalized();

    // Build a stable frame around the tangent using a world-up reference,
    // falling back to +Z when the tangent is nearly vertical.
    Vec3 refUp = (std::abs(tangent.y) > 0.99f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3::Up();
    Vec3 right = tangent.Cross(refUp).Normalized();
    Vec3 up = right.Cross(tangent).Normalized();

    float tilt = SampleTilt(t);
    if (std::abs(tilt) > 0.00001f) {
        up = Quat::FromAxisAngle(tangent, tilt) * up;
    }
    return up;
}

Quat Curve3D::SampleOrientation(float t) const {
    Vec3 tangent = SampleTangent(t);
    if (tangent.LengthSquared() < 0.0001f) {
        return Quat::Identity();
    }
    Vec3 up = SampleUpVector(t);
    return Quat::LookRotation(tangent.Normalized(), up);
}

// ===== World-space helpers =====

Mat4 Curve3D::GetOwnerWorldMatrix() const {
    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (node3D) {
        return node3D->GetGlobalTransformMatrix();
    }
    return Mat4();
}

Vec3 Curve3D::LocalToWorld(const Vec3& localPoint) const {
    return GetOwnerWorldMatrix().TransformPoint(localPoint);
}

Vec3 Curve3D::LocalDirectionToWorld(const Vec3& localDir) const {
    return GetOwnerWorldMatrix().TransformDirection(localDir).Normalized();
}

Vec3 Curve3D::SampleCurveWorld(float t) const {
    return LocalToWorld(SampleCurve(t));
}

Vec3 Curve3D::SampleTangentWorld(float t) const {
    return LocalDirectionToWorld(SampleTangent(t));
}

Vec3 Curve3D::SampleUpVectorWorld(float t) const {
    return GetOwnerWorldMatrix().TransformDirection(SampleUpVector(t)).Normalized();
}

Quat Curve3D::SampleOrientationWorld(float t) const {
    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    Quat ownerRot = node3D ? node3D->GetGlobalRotation() : Quat::Identity();
    return ownerRot * SampleOrientation(t);
}

// ===== Serialization =====

void Curve3D::SyncPointsToProperty() {
    nlohmann::json pointsArray = nlohmann::json::array();

    for (const auto& pt : m_CurvePoints) {
        nlohmann::json pointObj;
        pointObj["x"] = pt.position.x;
        pointObj["y"] = pt.position.y;
        pointObj["z"] = pt.position.z;
        pointObj["ctrlInX"] = pt.controlIn.x;
        pointObj["ctrlInY"] = pt.controlIn.y;
        pointObj["ctrlInZ"] = pt.controlIn.z;
        pointObj["ctrlOutX"] = pt.controlOut.x;
        pointObj["ctrlOutY"] = pt.controlOut.y;
        pointObj["ctrlOutZ"] = pt.controlOut.z;
        pointObj["useBezier"] = pt.useBezier;
        pointObj["symmetric"] = pt.symmetricHandles;
        pointObj["tilt"] = pt.tilt;
        pointsArray.push_back(pointObj);
    }

    std::string jsonStr = pointsArray.dump();
    SetPropertyValue<std::string>("pointsData", jsonStr);
}

void Curve3D::SyncPointsFromProperty() {
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
            Curve3DPoint pt;

            if (pointObj.contains("x")) pt.position.x = pointObj["x"].get<float>();
            if (pointObj.contains("y")) pt.position.y = pointObj["y"].get<float>();
            if (pointObj.contains("z")) pt.position.z = pointObj["z"].get<float>();
            if (pointObj.contains("ctrlInX")) pt.controlIn.x = pointObj["ctrlInX"].get<float>();
            if (pointObj.contains("ctrlInY")) pt.controlIn.y = pointObj["ctrlInY"].get<float>();
            if (pointObj.contains("ctrlInZ")) pt.controlIn.z = pointObj["ctrlInZ"].get<float>();
            if (pointObj.contains("ctrlOutX")) pt.controlOut.x = pointObj["ctrlOutX"].get<float>();
            if (pointObj.contains("ctrlOutY")) pt.controlOut.y = pointObj["ctrlOutY"].get<float>();
            if (pointObj.contains("ctrlOutZ")) pt.controlOut.z = pointObj["ctrlOutZ"].get<float>();
            if (pointObj.contains("useBezier")) pt.useBezier = pointObj["useBezier"].get<bool>();
            if (pointObj.contains("symmetric")) pt.symmetricHandles = pointObj["symmetric"].get<bool>();
            if (pointObj.contains("tilt")) pt.tilt = pointObj["tilt"].get<float>();

            m_CurvePoints.push_back(pt);
        }

        InvalidateCache();
    } catch (const std::exception&) {
    }
}

// ===== Scripting bridge =====

nlohmann::json Curve3D::CallMethod(const std::string& method, const nlohmann::json& args) {
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
    auto vec3Json = [](const Vec3& v) -> nlohmann::json {
        nlohmann::json o;
        o["x"] = v.x; o["y"] = v.y; o["z"] = v.z;
        return o;
    };

    if (method == "add_point") {
        AddPoint(Vec3(argF(0, 0.0f), argF(1, 0.0f), argF(2, 0.0f)));
    } else if (method == "set_point_position") {
        SetPointPosition(static_cast<size_t>(argI(0, 0)), Vec3(argF(1, 0.0f), argF(2, 0.0f), argF(3, 0.0f)));
    } else if (method == "remove_point") {
        RemovePoint(static_cast<size_t>(argI(0, 0)));
    } else if (method == "clear_points") {
        ClearPoints();
    } else if (method == "get_point_count") {
        return static_cast<int>(GetPointCount());
    } else if (method == "get_point_position") {
        return vec3Json(GetPointPosition(static_cast<size_t>(argI(0, 0))));
    } else if (method == "set_point_use_bezier") {
        SetPointUseBezier(static_cast<size_t>(argI(0, 0)), argB(1, true));
    } else if (method == "set_point_control_handles") {
        SetPointControlHandles(static_cast<size_t>(argI(0, 0)),
                               Vec3(argF(1, 0.0f), argF(2, 0.0f), argF(3, 0.0f)),
                               Vec3(argF(4, 0.0f), argF(5, 0.0f), argF(6, 0.0f)),
                               argB(7, true));
    } else if (method == "set_closed_loop") {
        SetClosedLoop(argB(0, false));
    } else if (method == "get_closed_loop") {
        return GetClosedLoop();
    } else if (method == "get_curve_length") {
        return GetCurveLength();
    } else if (method == "sample_curve") {
        return vec3Json(SampleCurve(argF(0, 0.0f)));
    } else if (method == "sample_tangent") {
        return vec3Json(SampleTangent(argF(0, 0.0f)));
    } else if (method == "sample_curve_world") {
        return vec3Json(SampleCurveWorld(argF(0, 0.0f)));
    } else if (method == "sample_tangent_world") {
        return vec3Json(SampleTangentWorld(argF(0, 0.0f)));
    } else if (method == "set_point_tilt") {
        SetPointTilt(static_cast<size_t>(argI(0, 0)), argF(1, 0.0f));
    } else if (method == "get_point_tilt") {
        return GetPointTilt(static_cast<size_t>(argI(0, 0)));
    } else if (method == "sample_tilt") {
        return SampleTilt(argF(0, 0.0f));
    } else if (method == "sample_up_vector") {
        return vec3Json(SampleUpVector(argF(0, 0.0f)));
    } else if (method == "sample_up_vector_world") {
        return vec3Json(SampleUpVectorWorld(argF(0, 0.0f)));
    } else if (method == "sample_orientation") {
        Quat q = SampleOrientation(argF(0, 0.0f));
        nlohmann::json o;
        o["w"] = q.w(); o["x"] = q.x(); o["y"] = q.y(); o["z"] = q.z();
        return o;
    } else if (method == "sample_orientation_world") {
        Quat q = SampleOrientationWorld(argF(0, 0.0f));
        nlohmann::json o;
        o["w"] = q.w(); o["x"] = q.x(); o["y"] = q.y(); o["z"] = q.z();
        return o;
    }
    return nlohmann::json();
}

// ===== Rendering =====

void Curve3D::RenderDebugCurve(RenderContext& ctx) {
    UpdateCache();

    if (m_TessellatedCache.size() < 2) {
        return;
    }

    Mat4 world = GetOwnerWorldMatrix();
    Color debugColor = GetDebugColor();
    float lineWidth = GetPropertyValue<float>("debugLineWidth");
    bool closedLoop = GetClosedLoop();

    size_t numSegments = closedLoop ? m_TessellatedCache.size() : m_TessellatedCache.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_TessellatedCache.size();
        Vec3 start = world.TransformPoint(m_TessellatedCache[i]);
        Vec3 end = world.TransformPoint(m_TessellatedCache[nextIdx]);
        ctx.drawLine(start, end, debugColor, lineWidth);
    }
}

void Curve3D::RenderEditorHandles(RenderContext& ctx) {
    if (m_CurvePoints.empty()) {
        return;
    }

    Mat4 world = GetOwnerWorldMatrix();
    Color anchorColor(0.2f, 0.6f, 1.0f, 1.0f);
    Color selectedColor(1.0f, 0.85f, 0.2f, 1.0f);
    Color handleLineColor(0.4f, 0.6f, 1.0f, 0.8f);
    Color handlePointColor(1.0f, 1.0f, 1.0f, 1.0f);

    float anchorSize = 0.12f;
    float handleSize = 0.09f;

    for (size_t i = 0; i < m_CurvePoints.size(); ++i) {
        Vec3 anchorWorld = world.TransformPoint(m_CurvePoints[i].position);
        bool isSelected = (static_cast<int>(i) == m_SelectedPointIndex);
        ctx.drawBox(anchorWorld, Vec3(anchorSize, anchorSize, anchorSize),
                    isSelected ? selectedColor : anchorColor, !isSelected);
    }

    if (m_ShowBezierHandles && m_SelectedPointIndex >= 0 &&
        m_SelectedPointIndex < static_cast<int>(m_CurvePoints.size())) {
        const Curve3DPoint& pt = m_CurvePoints[m_SelectedPointIndex];
        Vec3 anchorWorld = world.TransformPoint(pt.position);

        Vec3 inWorld = world.TransformPoint(pt.position + pt.controlIn);
        ctx.drawLine(anchorWorld, inWorld, handleLineColor, 1.0f);
        ctx.drawBox(inWorld, Vec3(handleSize, handleSize, handleSize), handlePointColor, false);

        Vec3 outWorld = world.TransformPoint(pt.position + pt.controlOut);
        ctx.drawLine(anchorWorld, outWorld, handleLineColor, 1.0f);
        ctx.drawBox(outWorld, Vec3(handleSize, handleSize, handleSize), handlePointColor, false);
    }
}

void Curve3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    SyncPointsFromProperty();

    if (GetDebugDraw()) {
        RenderDebugCurve(ctx);
    }

    Scene* scene = m_Owner->GetScene();
    if (scene && scene->IsInEditor()) {
        RenderEditorHandles(ctx);
    }
}

AABB Curve3D::getWorldBounds() const {
    Mat4 world = GetOwnerWorldMatrix();

    UpdateCache();

    if (m_TessellatedCache.empty()) {
        Vec3 c = world.TransformPoint(Vec3::Zero());
        return AABB(c - Vec3(1.0f), c + Vec3(1.0f));
    }

    Vec3 minPt = world.TransformPoint(m_TessellatedCache[0]);
    Vec3 maxPt = minPt;

    for (const auto& pt : m_TessellatedCache) {
        Vec3 worldPt = world.TransformPoint(pt);
        minPt.x = std::min(minPt.x, worldPt.x);
        minPt.y = std::min(minPt.y, worldPt.y);
        minPt.z = std::min(minPt.z, worldPt.z);
        maxPt.x = std::max(maxPt.x, worldPt.x);
        maxPt.y = std::max(maxPt.y, worldPt.y);
        maxPt.z = std::max(maxPt.z, worldPt.z);
    }

    float padding = 0.25f;
    return AABB(minPt - Vec3(padding), maxPt + Vec3(padding));
}

RenderLayer Curve3D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType Curve3D::getSpatialType() const {
    return SpatialType::World3D;
}

} // namespace components
} // namespace lupine
