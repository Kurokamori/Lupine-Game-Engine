#include "lupine/components/Line2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Line2D::Line2D()
    : Component("Line2D")
{

    m_Points.push_back(Vec2(0.0f, 0.0f));
    m_Points.push_back(Vec2(100.0f, 0.0f));
}

Line2D::Line2D(const std::string& name)
    : Component(name)
{

    m_Points.push_back(Vec2(0.0f, 0.0f));
    m_Points.push_back(Vec2(100.0f, 0.0f));
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

void Line2D::SetPoints(const std::vector<Vec2>& points) {
    m_Points = points;
    SyncPointsToProperty();
}

Vec2 Line2D::GetPoint(size_t index) const {
    if (index >= m_Points.size()) {

        return Vec2(0.0f, 0.0f);
    }
    return m_Points[index];
}

void Line2D::SetPoint(size_t index, const Vec2& point) {
    if (index >= m_Points.size()) {

        return;
    }
    m_Points[index] = point;
    SyncPointsToProperty();
}

void Line2D::AddPoint(const Vec2& point) {
    m_Points.push_back(point);
    SyncPointsToProperty();
}

void Line2D::InsertPoint(size_t index, const Vec2& point) {
    if (index > m_Points.size()) {

        return;
    }
    m_Points.insert(m_Points.begin() + index, point);
    SyncPointsToProperty();
}

void Line2D::RemovePoint(size_t index) {
    if (index >= m_Points.size()) {

        return;
    }
    m_Points.erase(m_Points.begin() + index);
    SyncPointsToProperty();
}

void Line2D::ClearPoints() {
    m_Points.clear();
    SyncPointsToProperty();
}

void Line2D::SetBeginning(const Vec2& point) {
    if (m_Points.empty()) {
        m_Points.push_back(point);
    } else {
        m_Points[0] = point;
    }
    SyncPointsToProperty();
}

Vec2 Line2D::GetBeginning() const {
    if (m_Points.empty()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_Points[0];
}

void Line2D::SetEnd(const Vec2& point) {
    if (m_Points.empty()) {
        m_Points.push_back(point);
    } else {
        m_Points[m_Points.size() - 1] = point;
    }
    SyncPointsToProperty();
}

Vec2 Line2D::GetEnd() const {
    if (m_Points.empty()) {
        return Vec2(0.0f, 0.0f);
    }
    return m_Points[m_Points.size() - 1];
}

const Color& Line2D::GetStrokeColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("strokeColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    static Color defaultColor = Color::White();
    return defaultColor;
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

    for (const auto& point : m_Points) {
        j.push_back({point.x, point.y});
    }

    SetPropertyValue<std::string>("pointsData", j.dump());
}

void Line2D::SyncPointsFromProperty() {

    std::string pointsData = GetPropertyValue<std::string>("pointsData");

    if (pointsData.empty() || pointsData == "[]") {

        m_Points.clear();
        m_Points.push_back(Vec2(0.0f, 0.0f));
        m_Points.push_back(Vec2(100.0f, 0.0f));
        return;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(pointsData);
        m_Points.clear();

        if (j.is_array()) {
            for (const auto& pointJson : j) {
                if (pointJson.is_array() && pointJson.size() >= 2) {
                    float x = pointJson[0].get<float>();
                    float y = pointJson[1].get<float>();
                    m_Points.push_back(Vec2(x, y));
                }
            }
        }

        if (m_Points.size() < 2) {
            m_Points.clear();
            m_Points.push_back(Vec2(0.0f, 0.0f));
            m_Points.push_back(Vec2(100.0f, 0.0f));
        }
    } catch (const std::exception& e) {

        m_Points.clear();
        m_Points.push_back(Vec2(0.0f, 0.0f));
        m_Points.push_back(Vec2(100.0f, 0.0f));
    }
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
    Vec2 topLeft(midpoint.x - length * 0.5f, midpoint.y - strokeWidth * 0.5f);

    ctx.drawRoundedRect(topLeft, size, Vec4(0.0f, 0.0f, 0.0f, 0.0f), strokeColor, 0);
}

void Line2D::RenderPolyline(RenderContext& ctx) {
    if (m_Points.size() < 2) {
        return;
    }

    Color strokeColor = GetStrokeColor();
    float strokeWidth = GetStrokeWidth();
    bool closedLoop = GetClosedLoop();

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);

    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    for (size_t i = 0; i < m_Points.size() - 1; ++i) {
        Vec2 start = m_Points[i] + offset;
        Vec2 end = m_Points[i + 1] + offset;

        Vec2 dir = end - start;
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

        if (length < 0.001f) {
            continue;
        }

        Vec2 midpoint = (start + end) * 0.5f;
        Vec2 size(length, strokeWidth);
        Vec2 topLeft(midpoint.x - length * 0.5f, midpoint.y - strokeWidth * 0.5f);

        ctx.drawRoundedRect(topLeft, size, Vec4(strokeWidth * 0.5f, strokeWidth * 0.5f, strokeWidth * 0.5f, strokeWidth * 0.5f), strokeColor, 0);
    }

    if (closedLoop && m_Points.size() > 2) {
        Vec2 start = m_Points[m_Points.size() - 1] + offset;
        Vec2 end = m_Points[0] + offset;

        Vec2 dir = end - start;
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

        if (length >= 0.001f) {
            Vec2 midpoint = (start + end) * 0.5f;
            Vec2 size(length, strokeWidth);
            Vec2 topLeft(midpoint.x - length * 0.5f, midpoint.y - strokeWidth * 0.5f);

            ctx.drawRoundedRect(topLeft, size, Vec4(strokeWidth * 0.5f, strokeWidth * 0.5f, strokeWidth * 0.5f, strokeWidth * 0.5f), strokeColor, 0);
        }
    }
}

void Line2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    SyncPointsFromProperty();

    RenderPolyline(ctx);
}

AABB Line2D::getWorldBounds() const {
    if (!m_Owner || m_Points.empty()) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 offset(0.0f, 0.0f);

    if (node2D) {
        offset = node2D->GetGlobalPosition();
    }

    Vec2 min = m_Points[0] + offset;
    Vec2 max = min;

    for (const auto& point : m_Points) {
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
