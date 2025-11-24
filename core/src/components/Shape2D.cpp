#include "lupine/components/Shape2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
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

const Color& Shape2D::GetColor() const {
    static Color defaultColor = Color::White();
    const ComponentProperty* prop = m_CustomProperties.GetProperty("color");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return defaultColor;
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

const Color& Shape2D::GetBorderColor() const {
    static Color defaultColor = Color::Black();
    const ComponentProperty* prop = m_CustomProperties.GetProperty("borderColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return defaultColor;
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

int Shape2D::ShapeTypeToInt(ShapeType type) const {
    return static_cast<int>(type);
}

Shape2D::ShapeType Shape2D::IntToShapeType(int value) const {
    if (value < 0 || value > 4) return ShapeType::Circle;
    return static_cast<ShapeType>(value);
}

void Shape2D::RenderCircle(RenderContext& ctx, const Vec2& position, float radius, float rotation) {

    Color fillColor = GetColor();
    Vec3 pos3D(position.x, position.y, 0.0f);

    if (GetFilled()) {
        ctx.drawCircle(pos3D, radius, fillColor, true);
    }

    if (GetBorderEnabled()) {
        Color borderColor = GetBorderColor();
        float borderWidth = GetBorderWidth();

        float outerRadius = radius + borderWidth * 0.5f;
        ctx.drawCircle(pos3D, outerRadius, borderColor, false);

        if (GetFilled()) {
            float innerRadius = radius - borderWidth * 0.5f;
            if (innerRadius > 0) {
                ctx.drawCircle(pos3D, innerRadius, borderColor, false);
            }
        }
    }
}

void Shape2D::RenderSquare(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    Color fillColor = GetColor();
    Vec4 cornerRadius(0.0f, 0.0f, 0.0f, 0.0f);

    if (GetFilled()) {

        if (std::abs(rotation) > 0.0001f) {
            ctx.drawRoundedRect(position, size, cornerRadius, fillColor, rotation, 0);
        } else {

            Vec2 topLeft(position.x - size.x * 0.5f, position.y - size.y * 0.5f);
            ctx.drawRoundedRect(topLeft, size, cornerRadius, fillColor, 0);
        }
    }

    if (GetBorderEnabled()) {
        Color borderColor = GetBorderColor();
        float borderWidth = GetBorderWidth();

        Vec2 borderSize = Vec2(size.x + borderWidth, size.y + borderWidth);

        if (std::abs(rotation) > 0.0001f) {

            ctx.drawRoundedRect(position, borderSize, cornerRadius, borderColor, rotation, 0);

            if (GetFilled()) {
                ctx.drawRoundedRect(position, size, cornerRadius, fillColor, rotation, 0);
            }
        } else {

            Vec2 topLeft(position.x - size.x * 0.5f, position.y - size.y * 0.5f);
            Vec2 borderPos = Vec2(topLeft.x - borderWidth * 0.5f, topLeft.y - borderWidth * 0.5f);
            Vec4 borderWidths(borderWidth, borderWidth, borderWidth, borderWidth);
            ctx.drawRoundedRectBorder(borderPos, borderSize, cornerRadius, borderWidths, borderColor);
        }
    }
}

void Shape2D::RenderTriangle(RenderContext& ctx, const Vec2& position, float radius, float rotation) {
    RenderPolygon(ctx, position, radius, 3, rotation);
}

void Shape2D::RenderPolygon(RenderContext& ctx, const Vec2& position, float radius, int sides, float rotation) {
    Color fillColor = GetColor();
    IGfxDevice* device = ctx.getDevice();

    if (!device) {

        return;
    }

    std::vector<Vec2> vertices;
    float angleStep = (2.0f * static_cast<float>(M_PI)) / sides;
    float startAngle = -static_cast<float>(M_PI) / 2.0f;

    for (int i = 0; i < sides; ++i) {
        float angle = startAngle + i * angleStep;
        float x = radius * std::cos(angle);
        float y = radius * std::sin(angle);
        vertices.push_back(Vec2(x, y));
    }

    if (GetFilled()) {
        MeshData meshData;

        Vertex centerVert;
        centerVert.position = Vec3(0.0f, 0.0f, 0.0f);
        centerVert.normal = Vec3(0.0f, 0.0f, 1.0f);
        centerVert.texCoord = Vec2(0.5f, 0.5f);
        centerVert.color = Vec4(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        meshData.vertices.push_back(centerVert);

        for (int i = 0; i < sides; ++i) {
            Vertex v;
            v.position = Vec3(vertices[i].x, vertices[i].y, 0.0f);
            v.normal = Vec3(0.0f, 0.0f, 1.0f);
            float u = (vertices[i].x / radius + 1.0f) * 0.5f;
            float vCoord = (vertices[i].y / radius + 1.0f) * 0.5f;
            v.texCoord = Vec2(u, vCoord);
            v.color = Vec4(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
            meshData.vertices.push_back(v);
        }

        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            meshData.indices.push_back(0);
            meshData.indices.push_back(next + 1);
            meshData.indices.push_back(i + 1);
        }

        meshData.calculateBounds();

        MeshHandle mesh = device->createMesh(meshData);
        if (mesh.isValid()) {

            MaterialHandle material = ctx.getColoredMaterialForSpatialType();

            if (!material.isValid()) {

                material = ctx.getDefaultColoredMaterial();
            }

            Mat4 transform = Mat4::Translate(Vec3(position.x, position.y, 0.0f));
            if (std::abs(rotation) > 0.0001f) {

                Mat4 rotMat = Mat4::Rotate(rotation, Vec3(0.0f, 0.0f, 1.0f));
                transform = transform * rotMat;
            }

            MaterialPropertyBlock overrides;
            overrides.setColor("u_Color", fillColor);
            ctx.drawMesh(mesh, material, transform, overrides);

            device->destroyMesh(mesh);
        } else {

        }
    }

    if (GetBorderEnabled()) {
        Color borderColor = GetBorderColor();
        float borderWidth = GetBorderWidth();

        std::vector<Vec2> rotatedVertices = vertices;
        if (std::abs(rotation) > 0.0001f) {
            float cosR = std::cos(rotation);
            float sinR = std::sin(rotation);
            for (auto& v : rotatedVertices) {
                float rotatedX = v.x * cosR - v.y * sinR;
                float rotatedY = v.x * sinR + v.y * cosR;
                v.x = rotatedX;
                v.y = rotatedY;
            }
        }

        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            Vec3 start(position.x + rotatedVertices[i].x, position.y + rotatedVertices[i].y, 0.0f);
            Vec3 end(position.x + rotatedVertices[next].x, position.y + rotatedVertices[next].y, 0.0f);

            ctx.drawLine(start, end, borderColor, borderWidth);
        }

        for (const auto& vertex : rotatedVertices) {
            Vec3 vertexPos(position.x + vertex.x, position.y + vertex.y, 0.0f);
            float cornerRadius = borderWidth * 0.5f;
            ctx.drawCircle(vertexPos, cornerRadius, borderColor, true);
        }
    }
}

void Shape2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
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

    ShapeType type = GetShapeType();

    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {

    }

    switch (type) {
        case ShapeType::Circle:
            RenderCircle(ctx, position, GetRadius(), rotation);
            break;

        case ShapeType::Square:
            RenderSquare(ctx, position, Vec2(GetWidth(), GetHeight()), rotation);
            break;

        case ShapeType::Triangle:
            RenderTriangle(ctx, position, GetRadius(), rotation);
            break;

        case ShapeType::Pentagon:
            RenderPolygon(ctx, position, GetRadius(), 5, rotation);
            break;

        case ShapeType::Hexagon:
            RenderPolygon(ctx, position, GetRadius(), 6, rotation);
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

    ShapeType type = GetShapeType();
    float radius = GetRadius();
    Vec2 size = Vec2(GetWidth(), GetHeight());

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
