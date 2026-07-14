#include "lupine/components/Shape2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Shape2D::Shape2D()
    : Component("Shape2D")
{
}

Shape2D::Shape2D(const std::string& name)
    : Component(name)
{
}

Shape2D::~Shape2D() {

}

void Shape2D::DefineProperties() {

    DefineProperty(PROPERTY_ENUM_GROUP(shapeType, 0, "Shape", Circle, Square, Triangle, Pentagon, Hexagon));

    DefineProperty(PROPERTY_DEFAULT_GROUP(filled, Bool, true, "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Appearance"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 50.0f, 0.0f, 1000.0f, 1.0f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 100.0f, 0.0f, 1000.0f, 1.0f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 100.0f, 0.0f, 1000.0f, 1.0f, "Size"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, false, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color::Black(), "Border"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 2.0f, 0.0f, 20.0f, 0.5f, "Border"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(circleSegments, 32, 3, 128, 1, "Detail"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uiSpace, Bool, true, "Rendering"));

    DefineProperty(PROPERTY_FILE_GROUP(materialOverride, std::string(""), "*.lsh,*.mat,*.material", "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shaderParameters, String, std::string(""), "Appearance"));
}

void Shape2D::OnAwake() {

}

void Shape2D::OnReady() {

}

void Shape2D::OnRender() {

}

Shape2D::ShapeType Shape2D::GetShapeType() const {
    int type = GetPropertyValue<int>("shapeType");
    return IntToShapeType(type);
}

void Shape2D::SetShapeType(ShapeType type) {
    SetPropertyValue<int>("shapeType", ShapeTypeToInt(type));
}

Color Shape2D::GetColor() const {
    return GetPropertyValue<Color>("color");
}

void Shape2D::SetColor(const Color& color) {
    SetPropertyValue<Color>("color", color);
}

bool Shape2D::GetFilled() const {
    return GetPropertyValue<bool>("filled");
}

void Shape2D::SetFilled(bool filled) {
    SetPropertyValue<bool>("filled", filled);
}

float Shape2D::GetSize() const {
    return GetRadius();
}

void Shape2D::SetSize(float size) {
    SetRadius(size);
}

float Shape2D::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void Shape2D::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
}

float Shape2D::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void Shape2D::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
}

float Shape2D::GetRadius() const {
    return GetPropertyValue<float>("radius");
}

void Shape2D::SetRadius(float radius) {
    SetPropertyValue<float>("radius", radius);
}

bool Shape2D::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void Shape2D::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
}

Color Shape2D::GetBorderColor() const {
    return GetPropertyValue<Color>("borderColor");
}

void Shape2D::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
}

float Shape2D::GetBorderWidth() const {
    return GetPropertyValue<float>("borderWidth");
}

void Shape2D::SetBorderWidth(float width) {
    SetPropertyValue<float>("borderWidth", width);
}

int Shape2D::GetCircleSegments() const {
    return GetPropertyValue<int>("circleSegments");
}

void Shape2D::SetCircleSegments(int segments) {
    SetPropertyValue<int>("circleSegments", segments);
}

int Shape2D::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Shape2D::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int Shape2D::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Shape2D::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

bool Shape2D::GetUISpace() const {
    return GetPropertyValue<bool>("uiSpace");
}

void Shape2D::SetUISpace(bool uiSpace) {
    SetPropertyValue<bool>("uiSpace", uiSpace);
}

const std::string& Shape2D::GetShader() const {
    // Custom .lsh shaders live in the material override slot.
    static std::string cachedPath;
    std::string materialPath = GetPropertyValue<std::string>("materialOverride");
    if (materialPath.size() >= 4 && materialPath.compare(materialPath.size() - 4, 4, ".lsh") == 0) {
        cachedPath = materialPath;
    } else {
        cachedPath = std::string();
    }
    return cachedPath;
}

void Shape2D::SetShader(const std::string& shaderPath) {
    std::string resPath = shaderPath;
    if (!shaderPath.empty() && !(shaderPath.size() >= 6 && shaderPath.substr(0, 6) == "res://")) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(shaderPath);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }
    SetPropertyValue<std::string>("materialOverride", resPath);
}

const std::string& Shape2D::GetShaderParameters() const {
    static std::string cachedParams;
    cachedParams = GetPropertyValue<std::string>("shaderParameters");
    return cachedParams;
}

void Shape2D::SetShaderParameters(const std::string& parametersJson) {
    SetPropertyValue<std::string>("shaderParameters", parametersJson);
}

bool Shape2D::RenderFillCustomShader(RenderContext& ctx, const Vec2& position, float rotation) {
    const std::string& shaderPath = GetShader();
    if (shaderPath.empty()) {
        return false;
    }

    MaterialHandle material = ctx.getOrCreateLshMaterial(shaderPath, 0);
    if (!material.isValid()) {
        return false;
    }

    ShapeType type = GetShapeType();
    Color fillColor = GetColor();
    Color borderColor = GetBorderColor();
    bool borderEnabled = GetBorderEnabled();
    float borderWidth = borderEnabled ? GetBorderWidth() : 0.0f;
    bool filled = GetFilled();

    // Margin so the bounding quad comfortably contains the shape, border, and AA edge.
    const float antiAlias = 2.0f;
    float margin = borderWidth + antiAlias;

    float radius = 0.0f;
    int sides = 0;
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
    Vec2 quadSize;

    switch (type) {
        case ShapeType::Square: {
            halfWidth = GetWidth() * 0.5f;
            halfHeight = GetHeight() * 0.5f;
            quadSize = Vec2(GetWidth() + 2.0f * margin, GetHeight() + 2.0f * margin);
            break;
        }
        case ShapeType::Triangle:
            sides = 3;
            radius = GetRadius();
            quadSize = Vec2(2.0f * (radius + margin), 2.0f * (radius + margin));
            break;
        case ShapeType::Pentagon:
            sides = 5;
            radius = GetRadius();
            quadSize = Vec2(2.0f * (radius + margin), 2.0f * (radius + margin));
            break;
        case ShapeType::Hexagon:
            sides = 6;
            radius = GetRadius();
            quadSize = Vec2(2.0f * (radius + margin), 2.0f * (radius + margin));
            break;
        case ShapeType::Circle:
        default:
            radius = GetRadius();
            quadSize = Vec2(2.0f * (radius + margin), 2.0f * (radius + margin));
            break;
    }

    MaterialPropertyBlock params;
    m_ShaderParams.BuildBlock(ctx, GetShaderParameters(), params);

    // Shape description provided to the custom shader so it can perform the SDF itself.
    // u_ShapeType is a float (not int) to match the property-block upload path used by the
    // other 2D-UI uniforms.
    params.setFloat("u_ShapeType", static_cast<float>(static_cast<int>(type)));
    params.setVec4("u_ShapeParams", Vec4(radius, static_cast<float>(sides), halfWidth, halfHeight));
    params.setColor("u_BorderColor", borderColor);
    params.setVec4("u_BorderParams", Vec4(borderWidth, borderEnabled ? 1.0f : 0.0f, filled ? 1.0f : 0.0f, 0.0f));

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRectShader(position, quadSize, Vec4(0.0f), fillColor, rotation, material, params);
    } else {
        Vec2 topLeft = Vec2(position.x - quadSize.x * 0.5f, position.y - quadSize.y * 0.5f);
        ctx.drawRoundedRectShader(topLeft, quadSize, Vec4(0.0f), fillColor, 0.0f, material, params);
    }

    return true;
}

int Shape2D::ShapeTypeToInt(ShapeType type) const {
    return static_cast<int>(type);
}

Shape2D::ShapeType Shape2D::IntToShapeType(int value) const {
    if (value < 0 || value > 4) return ShapeType::Circle;
    return static_cast<ShapeType>(value);
}

void Shape2D::RenderCircle(RenderContext& ctx, const Vec2& position, float radius, float) {
    Color fillColor = GetColor();
    Color borderColor = GetBorderColor();
    float borderWidth = GetBorderWidth();
    bool hasBorder = GetBorderEnabled() && borderWidth > 0.0f;
    Vec3 pos3D(position.x, position.y, 0.0f);

    // Draw border first (larger circle behind the fill)
    if (hasBorder) {
        float outerRadius = radius + borderWidth * 0.5f;
        ctx.drawCircle(pos3D, outerRadius, borderColor, true);
    }

    // Draw fill on top
    if (GetFilled()) {
        ctx.drawCircle(pos3D, radius, fillColor, true);
    }
}

void Shape2D::RenderSquare(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    Color fillColor = GetColor();
    Color borderColor = GetBorderColor();
    float borderWidth = GetBorderWidth();
    bool hasBorder = GetBorderEnabled() && borderWidth > 0.0f;
    Vec4 cornerRadius(0.0f, 0.0f, 0.0f, 0.0f);

    // Draw border first (larger rect behind the fill)
    if (hasBorder) {
        Vec2 borderSize = Vec2(size.x + borderWidth, size.y + borderWidth);

        if (std::abs(rotation) > 0.0001f) {
            ctx.drawRoundedRect(position, borderSize, cornerRadius, borderColor, rotation, 0);
        } else {
            Vec2 topLeft(position.x - borderSize.x * 0.5f, position.y - borderSize.y * 0.5f);
            ctx.drawRoundedRect(topLeft, borderSize, cornerRadius, borderColor, 0);
        }
    }

    // Draw fill on top
    if (GetFilled()) {
        if (std::abs(rotation) > 0.0001f) {
            ctx.drawRoundedRect(position, size, cornerRadius, fillColor, rotation, 0);
        } else {
            Vec2 topLeft(position.x - size.x * 0.5f, position.y - size.y * 0.5f);
            ctx.drawRoundedRect(topLeft, size, cornerRadius, fillColor, 0);
        }
    }
}

void Shape2D::RenderTriangle(RenderContext& ctx, const Vec2& position, float radius, float rotation) {
    RenderPolygon(ctx, position, radius, 3, rotation);
}

void Shape2D::RenderPolygon(RenderContext& ctx, const Vec2& position, float radius, int sides, float rotation) {
    Color fillColor = GetColor();
    Color borderColor = GetBorderColor();
    float borderWidth = GetBorderWidth();
    bool hasBorder = GetBorderEnabled() && borderWidth > 0.0f;

    // Draw border first (larger polygon behind the fill)
    if (hasBorder) {
        float outerRadius = radius + borderWidth * 0.5f;
        ctx.drawPolygon(position, outerRadius, sides, borderColor, rotation, 0);
    }

    // Draw fill on top
    if (GetFilled()) {
        // Use the SDF-based polygon shader for smooth antialiased edges
        // The rotation is applied inside the shader for better quality
        ctx.drawPolygon(position, radius, sides, fillColor, rotation, 0);
    }
}

void Shape2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 position;
    float rotation = 0.0f;
    Vec2 scale(1.0f, 1.0f);

    if (node2D) {
        position = node2D->GetGlobalPosition();
        rotation = node2D->GetGlobalRotation();
        scale = node2D->GetGlobalScale();
    } else {
        position = Vec2(0.0f, 0.0f);
    }

    ShapeType type = GetShapeType();

    // Radius-based shapes can only be uniformly scaled; use the average axis scale.
    float radiusScale = (std::abs(scale.x) + std::abs(scale.y)) * 0.5f;

    // A custom .lsh shader, when attached and compiled, renders the whole shape (fill + border).
    if (!GetShader().empty() && RenderFillCustomShader(ctx, position, rotation)) {
        return;
    }

    switch (type) {
        case ShapeType::Circle:
            RenderCircle(ctx, position, GetRadius() * radiusScale, rotation);
            break;

        case ShapeType::Square:
            RenderSquare(ctx, position, Vec2(GetWidth() * scale.x, GetHeight() * scale.y), rotation);
            break;

        case ShapeType::Triangle:
            RenderTriangle(ctx, position, GetRadius() * radiusScale, rotation);
            break;

        case ShapeType::Pentagon:
            RenderPolygon(ctx, position, GetRadius() * radiusScale, 5, rotation);
            break;

        case ShapeType::Hexagon:
            RenderPolygon(ctx, position, GetRadius() * radiusScale, 6, rotation);
            break;
    }
}

AABB Shape2D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 position;
    float rotation = 0.0f;

    if (node2D) {
        position = node2D->GetGlobalPosition();
        rotation = node2D->GetGlobalRotation();
    } else {
        position = Vec2(0.0f, 0.0f);
    }

    Vec2 scale(1.0f, 1.0f);
    if (node2D) {
        scale = node2D->GetGlobalScale();
    }
    float radiusScale = (std::abs(scale.x) + std::abs(scale.y)) * 0.5f;

    ShapeType type = GetShapeType();
    float radius = GetRadius() * radiusScale;
    Vec2 size = Vec2(GetWidth() * scale.x, GetHeight() * scale.y);

    if (GetBorderEnabled()) {
        float borderWidth = GetBorderWidth();
        size += Vec2(borderWidth * 2.0f, borderWidth * 2.0f);
        radius += borderWidth;
    }

    Vec2 min, max;

    if (type == ShapeType::Square) {

        Vec2 halfSize = size * 0.5f;

        if (std::abs(rotation) < 0.0001f) {
            min = position - halfSize;
            max = position + halfSize;
        } else {

            float cosR = std::cos(rotation);
            float sinR = std::sin(rotation);

            Vec2 localCorners[4] = {
                Vec2(-halfSize.x, -halfSize.y),
                Vec2( halfSize.x, -halfSize.y),
                Vec2( halfSize.x,  halfSize.y),
                Vec2(-halfSize.x,  halfSize.y)
            };

            min = Vec2(FLT_MAX, FLT_MAX);
            max = Vec2(-FLT_MAX, -FLT_MAX);

            for (int i = 0; i < 4; ++i) {

                Vec2 rotated(
                    localCorners[i].x * cosR - localCorners[i].y * sinR,
                    localCorners[i].x * sinR + localCorners[i].y * cosR
                );

                Vec2 worldCorner = position + rotated;

                min.x = std::min(min.x, worldCorner.x);
                min.y = std::min(min.y, worldCorner.y);
                max.x = std::max(max.x, worldCorner.x);
                max.y = std::max(max.y, worldCorner.y);
            }
        }
    } else {

        if (type == ShapeType::Circle || std::abs(rotation) < 0.0001f) {

            min = position - Vec2(radius, radius);
            max = position + Vec2(radius, radius);
        } else {

            int sides = 3;
            if (type == ShapeType::Pentagon) sides = 5;
            else if (type == ShapeType::Hexagon) sides = 6;

            float angleStep = (2.0f * static_cast<float>(M_PI)) / sides;
            float startAngle = -static_cast<float>(M_PI) / 2.0f;
            float cosR = std::cos(rotation);
            float sinR = std::sin(rotation);

            min = Vec2(FLT_MAX, FLT_MAX);
            max = Vec2(-FLT_MAX, -FLT_MAX);

            for (int i = 0; i < sides; ++i) {
                float angle = startAngle + i * angleStep;
                float x = radius * std::cos(angle);
                float y = radius * std::sin(angle);

                float rotatedX = x * cosR - y * sinR;
                float rotatedY = x * sinR + y * cosR;

                Vec2 worldVertex = position + Vec2(rotatedX, rotatedY);

                min.x = std::min(min.x, worldVertex.x);
                min.y = std::min(min.y, worldVertex.y);
                max.x = std::max(max.x, worldVertex.x);
                max.y = std::max(max.y, worldVertex.y);
            }
        }
    }

    return AABB(
        Vec3(min.x, min.y, -0.1f),
        Vec3(max.x, max.y, 0.1f)
    );
}

RenderLayer Shape2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType Shape2D::getSpatialType() const {
    return SpatialType::World2D;
}

}
}
