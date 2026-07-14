#include "lupine/components/Line2D.hpp"
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

Line2D::Line2D()
    : Component("Line2D")
{
    // Start with empty points - user will add via editor
    m_PointsCacheDirty = true;
}

Line2D::Line2D(const std::string& name)
    : Component(name)
{
    // Start with empty points - user will add via editor
    m_PointsCacheDirty = true;
}

Line2D::~Line2D() {

}

void Line2D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(pointsData, String, std::string("[]"), "Points"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(strokeColor, Color, Color::White(), "Stroke"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(strokeWidth, 2.0f, 0.1f, 50.0f, 0.5f, "Stroke"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(closedLoop, Bool, false, "Stroke"));

    DefineProperty(PROPERTY_ENUM_GROUP(capStyle, 0, "Style", Butt, Round, Square));
    DefineProperty(PROPERTY_ENUM_GROUP(joinStyle, 0, "Style", Miter, Round, Bevel));

    DefineProperty(PROPERTY_DEFAULT_GROUP(antiAliasing, Bool, true, "Quality"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(smoothness, 8, 3, 32, 1, "Quality"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(bezierSegments, 16, 4, 64, 1, "Quality"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uiSpace, Bool, true, "Rendering"));
}

void Line2D::OnAwake() {
    SyncPointsFromProperty();
}

void Line2D::OnReady() {
}

void Line2D::OnRender() {
}

// ===== Legacy Point Management (positions only) =====

void Line2D::UpdatePointsCache() const {
    if (!m_PointsCacheDirty) return;

    m_PointPositionsCache.clear();
    m_PointPositionsCache.reserve(m_Line2DPoints.size());
    for (const auto& pt : m_Line2DPoints) {
        m_PointPositionsCache.push_back(pt.position);
    }
    m_PointsCacheDirty = false;
}

const std::vector<Vec2>& Line2D::GetPoints() const {
    UpdatePointsCache();
    return m_PointPositionsCache;
}

std::vector<Vec2> Line2D::GetPointPositions() const {
    std::vector<Vec2> positions;
    positions.reserve(m_Line2DPoints.size());
    for (const auto& pt : m_Line2DPoints) {
        positions.push_back(pt.position);
    }
    return positions;
}

void Line2D::SetPoints(const std::vector<Vec2>& points) {
    m_Line2DPoints.clear();
    for (const auto& pos : points) {
        m_Line2DPoints.push_back(Line2DPoint(pos));
    }
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

Vec2 Line2D::GetPoint(size_t index) const {
    if (index >= m_Line2DPoints.size()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_Line2DPoints[index].position;
}

void Line2D::SetPoint(size_t index, const Vec2& point) {
    if (index >= m_Line2DPoints.size()) {
        return;
    }
    m_Line2DPoints[index].position = point;
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::AddPoint(const Vec2& point) {
    m_Line2DPoints.push_back(Line2DPoint(point));
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::InsertPoint(size_t index, const Vec2& point) {
    if (index > m_Line2DPoints.size()) {
        return;
    }
    m_Line2DPoints.insert(m_Line2DPoints.begin() + index, Line2DPoint(point));
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::RemovePoint(size_t index) {
    if (index >= m_Line2DPoints.size()) {
        return;
    }
    m_Line2DPoints.erase(m_Line2DPoints.begin() + index);
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::ClearPoints() {
    m_Line2DPoints.clear();
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::SetBeginning(const Vec2& point) {
    if (m_Line2DPoints.empty()) {
        m_Line2DPoints.push_back(Line2DPoint(point));
    } else {
        m_Line2DPoints[0].position = point;
    }
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

Vec2 Line2D::GetBeginning() const {
    if (m_Line2DPoints.empty()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_Line2DPoints[0].position;
}

void Line2D::SetEnd(const Vec2& point) {
    if (m_Line2DPoints.empty()) {
        m_Line2DPoints.push_back(Line2DPoint(point));
    } else {
        m_Line2DPoints.back().position = point;
    }
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

Vec2 Line2D::GetEnd() const {
    if (m_Line2DPoints.empty()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_Line2DPoints.back().position;
}

// ===== Bezier Point Management =====

void Line2D::SetLine2DPoints(const std::vector<Line2DPoint>& points) {
    m_Line2DPoints = points;
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

Line2DPoint Line2D::GetLine2DPoint(size_t index) const {
    if (index >= m_Line2DPoints.size()) {
        return Line2DPoint();
    }
    return m_Line2DPoints[index];
}

void Line2D::SetLine2DPoint(size_t index, const Line2DPoint& point) {
    if (index >= m_Line2DPoints.size()) {
        return;
    }
    m_Line2DPoints[index] = point;
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::AddLine2DPoint(const Line2DPoint& point) {
    m_Line2DPoints.push_back(point);
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::InsertLine2DPoint(size_t index, const Line2DPoint& point) {
    if (index > m_Line2DPoints.size()) {
        return;
    }
    m_Line2DPoints.insert(m_Line2DPoints.begin() + index, point);
    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::SetPointControlHandles(size_t index, const Vec2& controlIn, const Vec2& controlOut, bool symmetric) {
    if (index >= m_Line2DPoints.size()) {
        return;
    }
    m_Line2DPoints[index].controlIn = controlIn;
    m_Line2DPoints[index].controlOut = controlOut;
    m_Line2DPoints[index].symmetricHandles = symmetric;
    m_Line2DPoints[index].useBezier = true;
    SyncPointsToProperty();
}

void Line2D::SetPointUseBezier(size_t index, bool useBezier) {
    if (index >= m_Line2DPoints.size()) {
        return;
    }
    m_Line2DPoints[index].useBezier = useBezier;
    SyncPointsToProperty();
}

bool Line2D::GetPointUseBezier(size_t index) const {
    if (index >= m_Line2DPoints.size()) {
        return false;
    }
    return m_Line2DPoints[index].useBezier;
}

Vec2 Line2D::GetPointControlIn(size_t index) const {
    if (index >= m_Line2DPoints.size()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_Line2DPoints[index].controlIn;
}

Vec2 Line2D::GetPointControlOut(size_t index) const {
    if (index >= m_Line2DPoints.size()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_Line2DPoints[index].controlOut;
}

Color Line2D::GetStrokeColor() const {
    return GetPropertyValue<Color>("strokeColor");
}

void Line2D::SetStrokeColor(const Color& color) {
    SetPropertyValue<Color>("strokeColor", color);
}

float Line2D::GetStrokeWidth() const {
    return GetPropertyValue<float>("strokeWidth");
}

void Line2D::SetStrokeWidth(float width) {
    SetPropertyValue<float>("strokeWidth", width);
}

bool Line2D::GetClosedLoop() const {
    return GetPropertyValue<bool>("closedLoop");
}

void Line2D::SetClosedLoop(bool closed) {
    SetPropertyValue<bool>("closedLoop", closed);
}

Line2D::CapStyle Line2D::GetCapStyle() const {
    int style = GetPropertyValue<int>("capStyle");
    return IntToCapStyle(style);
}

void Line2D::SetCapStyle(CapStyle style) {
    SetPropertyValue<int>("capStyle", CapStyleToInt(style));
}

Line2D::JoinStyle Line2D::GetJoinStyle() const {
    int style = GetPropertyValue<int>("joinStyle");
    return IntToJoinStyle(style);
}

void Line2D::SetJoinStyle(JoinStyle style) {
    SetPropertyValue<int>("joinStyle", JoinStyleToInt(style));
}

bool Line2D::GetAntiAliasing() const {
    return GetPropertyValue<bool>("antiAliasing");
}

void Line2D::SetAntiAliasing(bool aa) {
    SetPropertyValue<bool>("antiAliasing", aa);
}

int Line2D::GetSmoothness() const {
    return GetPropertyValue<int>("smoothness");
}

void Line2D::SetSmoothness(int smoothness) {
    SetPropertyValue<int>("smoothness", smoothness);
}

int Line2D::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Line2D::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int Line2D::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Line2D::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

bool Line2D::GetUISpace() const {
    return GetPropertyValue<bool>("uiSpace");
}

void Line2D::SetUISpace(bool uiSpace) {
    SetPropertyValue<bool>("uiSpace", uiSpace);
}

int Line2D::CapStyleToInt(CapStyle style) const {
    return static_cast<int>(style);
}

Line2D::CapStyle Line2D::IntToCapStyle(int value) const {
    if (value < 0 || value > 2) return CapStyle::Butt;
    return static_cast<CapStyle>(value);
}

int Line2D::JoinStyleToInt(JoinStyle style) const {
    return static_cast<int>(style);
}

Line2D::JoinStyle Line2D::IntToJoinStyle(int value) const {
    if (value < 0 || value > 2) return JoinStyle::Miter;
    return static_cast<JoinStyle>(value);
}

void Line2D::SyncPointsToProperty() {
    nlohmann::json j = nlohmann::json::array();

    for (const auto& pt : m_Line2DPoints) {
        nlohmann::json pointObj;
        pointObj["x"] = pt.position.x;
        pointObj["y"] = pt.position.y;
        pointObj["ctrlInX"] = pt.controlIn.x;
        pointObj["ctrlInY"] = pt.controlIn.y;
        pointObj["ctrlOutX"] = pt.controlOut.x;
        pointObj["ctrlOutY"] = pt.controlOut.y;
        pointObj["useBezier"] = pt.useBezier;
        pointObj["symmetric"] = pt.symmetricHandles;
        j.push_back(pointObj);
    }

    SetPropertyValue<std::string>("pointsData", j.dump());
}

void Line2D::SyncPointsFromProperty() {
    std::string pointsData = GetPropertyValue<std::string>("pointsData");

    // If empty or "[]", just clear points - don't add defaults
    if (pointsData.empty() || pointsData == "[]") {
        m_Line2DPoints.clear();
        m_PointsCacheDirty = true;
        return;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(pointsData);
        m_Line2DPoints.clear();

        if (j.is_array()) {
            for (const auto& pointJson : j) {
                Line2DPoint pt;

                // Handle new format with Bezier data
                if (pointJson.is_object()) {
                    pt.position.x = pointJson.value("x", 0.0f);
                    pt.position.y = pointJson.value("y", 0.0f);
                    pt.controlIn.x = pointJson.value("ctrlInX", 0.0f);
                    pt.controlIn.y = pointJson.value("ctrlInY", 0.0f);
                    pt.controlOut.x = pointJson.value("ctrlOutX", 0.0f);
                    pt.controlOut.y = pointJson.value("ctrlOutY", 0.0f);
                    pt.useBezier = pointJson.value("useBezier", false);
                    pt.symmetricHandles = pointJson.value("symmetric", true);
                }
                // Handle legacy format [x, y]
                else if (pointJson.is_array() && pointJson.size() >= 2) {
                    pt.position.x = pointJson[0].get<float>();
                    pt.position.y = pointJson[1].get<float>();
                    pt.useBezier = false;
                }

                m_Line2DPoints.push_back(pt);
            }
        }

        // Don't add default points - allow empty or single-point lines
        m_PointsCacheDirty = true;
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::ECS, "Line2D: failed to parse points property, clearing points: {}", e.what());
        // On parse error, just clear the points
        m_Line2DPoints.clear();
        m_PointsCacheDirty = true;
    }
}

// Cubic Bezier curve evaluation: B(t) = (1-t)^3*P0 + 3*(1-t)^2*t*P1 + 3*(1-t)*t^2*P2 + t^3*P3
Vec2 Line2D::EvaluateCubicBezier(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float t) const {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    Vec2 result = p0 * uuu;                 // (1-t)^3 * P0
    result = result + p1 * (3.0f * uu * t); // 3 * (1-t)^2 * t * P1
    result = result + p2 * (3.0f * u * tt); // 3 * (1-t) * t^2 * P2
    result = result + p3 * ttt;             // t^3 * P3

    return result;
}

std::vector<Vec2> Line2D::TessellateCurve() const {
    std::vector<Vec2> tessellatedPoints;

    if (m_Line2DPoints.size() < 2) {
        return tessellatedPoints;
    }

    int bezierSegments = GetPropertyValue<int>("bezierSegments");
    if (bezierSegments < 4) bezierSegments = 32;  // Higher default for smoother curves

    bool closedLoop = GetClosedLoop();
    size_t numSegments = closedLoop ? m_Line2DPoints.size() : m_Line2DPoints.size() - 1;

    for (size_t i = 0; i < numSegments; ++i) {
        size_t nextIdx = (i + 1) % m_Line2DPoints.size();

        const Line2DPoint& pt0 = m_Line2DPoints[i];
        const Line2DPoint& pt1 = m_Line2DPoints[nextIdx];

        // Check if either point uses Bezier curves AND has non-zero control handles
        bool useBezier = (pt0.useBezier && (pt0.controlOut.x != 0.0f || pt0.controlOut.y != 0.0f)) ||
                         (pt1.useBezier && (pt1.controlIn.x != 0.0f || pt1.controlIn.y != 0.0f));

        if (useBezier) {
            // Generate Bezier curve points
            Vec2 p0 = pt0.position;
            Vec2 p1 = pt0.useBezier ? (pt0.position + pt0.controlOut) : pt0.position;  // Control point 1 (from start)
            Vec2 p2 = pt1.useBezier ? (pt1.position + pt1.controlIn) : pt1.position;   // Control point 2 (to end)
            Vec2 p3 = pt1.position;

            // Add starting point only for the first segment
            if (i == 0) {
                tessellatedPoints.push_back(p0);
            }

            // Add intermediate points (skip t=0 to avoid duplicates)
            for (int j = 1; j <= bezierSegments; ++j) {
                float t = static_cast<float>(j) / static_cast<float>(bezierSegments);
                tessellatedPoints.push_back(EvaluateCubicBezier(p0, p1, p2, p3, t));
            }
        } else {
            // Straight line segment - add start point only for first segment
            if (i == 0) {
                tessellatedPoints.push_back(pt0.position);
            }
            tessellatedPoints.push_back(pt1.position);
        }
    }

    return tessellatedPoints;
}

void Line2D::RenderLine(RenderContext& ctx, const Vec2& start, const Vec2& end) {
    Color strokeColor = GetStrokeColor();
    float strokeWidth = GetStrokeWidth();

    Vec2 dir = end - start;
    float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

    if (length < 0.001f) {
        return;
    }

    float angle = std::atan2(dir.y, dir.x);

    Vec2 midpoint = (start + end) * 0.5f;
    Vec2 size(length, strokeWidth);

    // Use rotated drawRoundedRect for proper line rendering
    ctx.drawRoundedRect(midpoint, size, Vec4(strokeWidth * 0.5f, strokeWidth * 0.5f, strokeWidth * 0.5f, strokeWidth * 0.5f), strokeColor, angle, 0);
}

void Line2D::RenderPolyline(RenderContext& ctx) {
    if (m_Line2DPoints.size() < 2) {
        return;
    }

    Color strokeColor = GetStrokeColor();
    CapStyle capStyle = GetCapStyle();
    JoinStyle joinStyle = GetJoinStyle();
    bool closedLoop = GetClosedLoop();

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    Vec2 scale(1.0f, 1.0f);

    if (node2D) {
        offset = node2D->GetGlobalPosition();
        scale = node2D->GetGlobalScale();
    }

    // Stroke is a single width; scale it by the average axis scale.
    float strokeWidth = GetStrokeWidth() * (std::abs(scale.x) + std::abs(scale.y)) * 0.5f;
    float halfStroke = strokeWidth * 0.5f;

    // Get tessellated curve points
    std::vector<Vec2> points = TessellateCurve();

    if (points.size() < 2) {
        return;
    }

    // Scale local point coordinates so the polyline grows with the node's scale.
    for (Vec2& point : points) {
        point.x *= scale.x;
        point.y *= scale.y;
    }

    // Render line segments between tessellated points
    for (size_t i = 0; i < points.size() - 1; ++i) {
        Vec2 start = points[i] + offset;
        Vec2 end = points[i + 1] + offset;

        Vec2 dir = end - start;
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

        if (length < 0.001f) {
            continue;
        }

        float angle = std::atan2(dir.y, dir.x);
        Vec2 midpoint = (start + end) * 0.5f;

        // Determine corner radius based on cap style (for segment ends) and join style (for middle)
        bool isFirstSegment = (i == 0);
        bool isLastSegment = (i == points.size() - 2);

        // For segments, we use corner radius based on cap style at ends
        float cornerRadius = 0.0f;
        if (capStyle == CapStyle::Round) {
            cornerRadius = halfStroke;
        }

        Vec2 size(length, strokeWidth);
        Vec4 corners(cornerRadius, cornerRadius, cornerRadius, cornerRadius);

        ctx.drawRoundedRect(midpoint, size, corners, strokeColor, angle, 0);

        // Handle caps at line endpoints (only for non-closed loops)
        if (!closedLoop) {
            if (isFirstSegment && capStyle == CapStyle::Round) {
                ctx.drawCircle(Vec3(start.x, start.y, 0.0f), halfStroke, strokeColor, 0);
            } else if (isFirstSegment && capStyle == CapStyle::Square) {
                // Square cap extends past the endpoint
                Vec2 capDir = dir * (-halfStroke / length);
                Vec2 capCenter = start + capDir * 0.5f;
                ctx.drawRoundedRect(capCenter, Vec2(halfStroke, strokeWidth), Vec4(0,0,0,0), strokeColor, angle, 0);
            }

            if (isLastSegment && capStyle == CapStyle::Round) {
                ctx.drawCircle(Vec3(end.x, end.y, 0.0f), halfStroke, strokeColor, 0);
            } else if (isLastSegment && capStyle == CapStyle::Square) {
                Vec2 capDir = dir * (halfStroke / length);
                Vec2 capCenter = end + capDir * 0.5f;
                ctx.drawRoundedRect(capCenter, Vec2(halfStroke, strokeWidth), Vec4(0,0,0,0), strokeColor, angle, 0);
            }
        }

        // Handle joins at internal points (where two segments meet)
        bool needsJoin = !isLastSegment || closedLoop;
        if (needsJoin && i + 1 < points.size()) {
            size_t nextSegEnd = (closedLoop && isLastSegment) ? 1 : i + 2;
            if (nextSegEnd < points.size() || (closedLoop && isLastSegment)) {
                Vec2 nextEnd = (closedLoop && isLastSegment) ? (points[1] + offset) : (points[i + 2] + offset);
                Vec2 nextDir = nextEnd - end;
                float nextLength = std::sqrt(nextDir.x * nextDir.x + nextDir.y * nextDir.y);

                if (nextLength > 0.001f) {
                    // Calculate angle between segments
                    Vec2 normDir = dir * (1.0f / length);
                    Vec2 normNextDir = nextDir * (1.0f / nextLength);
                    float dotProduct = normDir.x * normNextDir.x + normDir.y * normNextDir.y;

                    if (joinStyle == JoinStyle::Round) {
                        // Round join - draw circle at joint
                        ctx.drawCircle(Vec3(end.x, end.y, 0.0f), halfStroke, strokeColor, 0);
                    } else if (joinStyle == JoinStyle::Miter) {
                        // Miter join - extend lines to meet at a point
                        // Only apply miter if angle isn't too sharp (miter limit)
                        if (dotProduct > -0.9f) {
                            // Draw a small square at the joint for miter effect
                            float jointAngle = (angle + std::atan2(nextDir.y, nextDir.x)) * 0.5f;
                            ctx.drawRoundedRect(end, Vec2(strokeWidth * 1.2f, strokeWidth * 1.2f),
                                              Vec4(0,0,0,0), strokeColor, jointAngle, 0);
                        } else {
                            // Fall back to bevel for very sharp angles
                            ctx.drawCircle(Vec3(end.x, end.y, 0.0f), halfStroke * 0.5f, strokeColor, 0);
                        }
                    } else if (joinStyle == JoinStyle::Bevel) {
                        // Bevel join - flat corner (just small fill for gap)
                        ctx.drawCircle(Vec3(end.x, end.y, 0.0f), halfStroke * 0.4f, strokeColor, 0);
                    }
                } else {
                    // Degenerate case - just fill gap
                    ctx.drawCircle(Vec3(end.x, end.y, 0.0f), halfStroke * 0.5f, strokeColor, 0);
                }
            }
        }
    }

    // Handle join at closing point for closed loops
    if (closedLoop) {
        Vec2 firstPoint = points[0] + offset;
        if (joinStyle == JoinStyle::Round) {
            ctx.drawCircle(Vec3(firstPoint.x, firstPoint.y, 0.0f), halfStroke, strokeColor, 0);
        } else if (joinStyle == JoinStyle::Miter) {
            // Calculate angle for closing miter
            Vec2 lastDir = points[points.size() - 1] - points[points.size() - 2];
            Vec2 firstDir = points[1] - points[0];
            float lastAngle = std::atan2(lastDir.y, lastDir.x);
            float firstAngle = std::atan2(firstDir.y, firstDir.x);
            float jointAngle = (lastAngle + firstAngle) * 0.5f;
            ctx.drawRoundedRect(firstPoint, Vec2(strokeWidth * 1.2f, strokeWidth * 1.2f),
                              Vec4(0,0,0,0), strokeColor, jointAngle, 0);
        } else if (joinStyle == JoinStyle::Bevel) {
            ctx.drawCircle(Vec3(firstPoint.x, firstPoint.y, 0.0f), halfStroke * 0.4f, strokeColor, 0);
        }
    }
}

void Line2D::RenderBezierHandles(RenderContext& ctx) {
    if (m_Line2DPoints.empty() || m_SelectedPointIndex < 0) {
        return;
    }

    if (m_SelectedPointIndex >= static_cast<int>(m_Line2DPoints.size())) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    // Handle colors (like Adobe Illustrator style)
    Color handleLineColor(0.4f, 0.6f, 1.0f, 0.8f);   // Blue lines
    Color handlePointColor(1.0f, 1.0f, 1.0f, 1.0f);   // White control points
    Color anchorPointColor(0.2f, 0.6f, 1.0f, 1.0f);   // Blue anchor points

    float handleRadius = 3.0f;    // Small circles at control handle ends
    float anchorRadius = 2.5f;    // Small square for anchor point
    float handleLineWidth = 1.0f;

    // Only draw handles for the selected point
    const Line2DPoint& pt = m_Line2DPoints[m_SelectedPointIndex];
    Vec2 anchorPos = pt.position + offset;

    // Draw anchor point (small filled blue square)
    ctx.drawRoundedRect(anchorPos, Vec2(anchorRadius * 2, anchorRadius * 2),
                       Vec4(0.0f, 0.0f, 0.0f, 0.0f), anchorPointColor, 0.0f, 0);

    // Always draw control handles when a point is selected (so user can drag them to create curves)
    // Control In handle (direction towards previous point)
    Vec2 ctrlInPos = anchorPos + pt.controlIn;

    // Draw line from anchor to control in
    Vec2 dir = ctrlInPos - anchorPos;
    float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length > 0.001f) {
        float angle = std::atan2(dir.y, dir.x);
        Vec2 midpoint = (anchorPos + ctrlInPos) * 0.5f;
        ctx.drawRoundedRect(midpoint, Vec2(length, handleLineWidth),
                           Vec4(0.0f, 0.0f, 0.0f, 0.0f), handleLineColor, angle, 0);
    }

    // Draw control in point circle (small handle at end)
    ctx.drawCircle(Vec3(ctrlInPos.x, ctrlInPos.y, 0.0f), handleRadius, handlePointColor, 0);

    // Control Out handle (direction towards next point)
    Vec2 ctrlOutPos = anchorPos + pt.controlOut;

    // Draw line from anchor to control out
    dir = ctrlOutPos - anchorPos;
    length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length > 0.001f) {
        float angle = std::atan2(dir.y, dir.x);
        Vec2 midpoint = (anchorPos + ctrlOutPos) * 0.5f;
        ctx.drawRoundedRect(midpoint, Vec2(length, handleLineWidth),
                           Vec4(0.0f, 0.0f, 0.0f, 0.0f), handleLineColor, angle, 0);
    }

    // Draw control out point circle (small handle at end)
    ctx.drawCircle(Vec3(ctrlOutPos.x, ctrlOutPos.y, 0.0f), handleRadius, handlePointColor, 0);
}

int Line2D::HitTestControlPoint(const Vec2& worldPos, float radius) const {
    if (m_Line2DPoints.empty() || m_SelectedPointIndex < 0) {
        return -1;
    }

    if (m_SelectedPointIndex >= static_cast<int>(m_Line2DPoints.size())) {
        return -1;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    int i = m_SelectedPointIndex;
    const Line2DPoint& pt = m_Line2DPoints[i];
    Vec2 anchorPos = pt.position + offset;

    // Test control out first (on top) - always test since handles are always visible
    Vec2 ctrlOutPos = anchorPos + pt.controlOut;
    float distOut = std::sqrt((worldPos.x - ctrlOutPos.x) * (worldPos.x - ctrlOutPos.x) +
                              (worldPos.y - ctrlOutPos.y) * (worldPos.y - ctrlOutPos.y));
    if (distOut <= radius) {
        return i * 10 + 2;  // Control out
    }

    Vec2 ctrlInPos = anchorPos + pt.controlIn;
    float distIn = std::sqrt((worldPos.x - ctrlInPos.x) * (worldPos.x - ctrlInPos.x) +
                             (worldPos.y - ctrlInPos.y) * (worldPos.y - ctrlInPos.y));
    if (distIn <= radius) {
        return i * 10 + 1;  // Control in
    }

    // Test anchor point last (underneath handles)
    float distAnchor = std::sqrt((worldPos.x - anchorPos.x) * (worldPos.x - anchorPos.x) +
                                 (worldPos.y - anchorPos.y) * (worldPos.y - anchorPos.y));
    if (distAnchor <= radius) {
        return i * 10 + 0;  // Anchor
    }

    return -1;
}

void Line2D::MoveControlPoint(int controlPointId, const Vec2& worldPos) {
    if (controlPointId < 0 || m_Line2DPoints.empty()) {
        return;
    }

    int pointIndex = controlPointId / 10;
    int handleType = controlPointId % 10;  // 0=anchor, 1=ctrlIn, 2=ctrlOut

    if (pointIndex < 0 || pointIndex >= static_cast<int>(m_Line2DPoints.size())) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);
    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    Line2DPoint& pt = m_Line2DPoints[pointIndex];
    Vec2 localPos = worldPos - offset;

    if (handleType == 0) {
        // Moving anchor point
        pt.position = localPos;
    } else if (handleType == 1) {
        // Moving control in
        pt.controlIn = localPos - pt.position - offset;
        // Automatically enable bezier when control handle is moved from origin
        if (pt.controlIn.x != 0.0f || pt.controlIn.y != 0.0f) {
            pt.useBezier = true;
        }
        if (pt.symmetricHandles) {
            pt.controlOut = Vec2(-pt.controlIn.x, -pt.controlIn.y);
        }
    } else if (handleType == 2) {
        // Moving control out
        pt.controlOut = localPos - pt.position - offset;
        // Automatically enable bezier when control handle is moved from origin
        if (pt.controlOut.x != 0.0f || pt.controlOut.y != 0.0f) {
            pt.useBezier = true;
        }
        if (pt.symmetricHandles) {
            pt.controlIn = Vec2(-pt.controlOut.x, -pt.controlOut.y);
        }
    }

    m_PointsCacheDirty = true;
    SyncPointsToProperty();
}

void Line2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    SyncPointsFromProperty();
    UpdatePointsCache();

    RenderPolyline(ctx);

    // Render Bezier handles in editor mode
    if (m_ShowBezierHandles) {
        RenderBezierHandles(ctx);
    }
}

AABB Line2D::getWorldBounds() const {
    if (!m_Owner || m_Line2DPoints.empty()) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);

    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    // Use tessellated points for accurate bounds with Bezier curves
    std::vector<Vec2> points = TessellateCurve();
    if (points.empty()) {
        return AABB();
    }

    Vec2 min = points[0] + offset;
    Vec2 max = min;

    for (const auto& point : points) {
        Vec2 worldPoint = point + offset;
        min.x = std::min(min.x, worldPoint.x);
        min.y = std::min(min.y, worldPoint.y);
        max.x = std::max(max.x, worldPoint.x);
        max.y = std::max(max.y, worldPoint.y);
    }

    float halfStroke = GetStrokeWidth() * 0.5f;
    min -= Vec2(halfStroke, halfStroke);
    max += Vec2(halfStroke, halfStroke);

    return AABB(
        Vec3(min.x, min.y, -0.1f),
        Vec3(max.x, max.y, 0.1f)
    );
}

RenderLayer Line2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType Line2D::getSpatialType() const {
    return SpatialType::World2D;
}

}
}
