#include "lupine/components/ColorRect.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

ColorRect::ColorRect()
    : Component("ColorRect")
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

ColorRect::ColorRect(const std::string& name)
    : Component(name)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

ColorRect::~ColorRect() {

}

void ColorRect::DefineProperties() {

    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_ENUM_GROUP(blendMode, 0, "Appearance", Alpha, Additive, Multiply, Opaque, Overlay));
    DefineProperty(PROPERTY_FILE_GROUP(materialOverride, std::string(""), "*.mat,*.material", "Appearance"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 100.0f, 0.0f, 10000.0f, 1.0f, "Transform"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 100.0f, 0.0f, 10000.0f, 1.0f, "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uiSpace, Bool, true, "Transform"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, 0, 100, 1, "Layering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Layering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, false, "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color::Black(), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(1.0f, 1.0f, 1.0f, 1.0f), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Style"));
}

void ColorRect::OnAwake() {

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);
}

void ColorRect::OnReady() {

}

void ColorRect::OnRender() {

}

bool ColorRect::OnGizmoScale(float scaleDelta, int axis, bool is3D) {

    if (!is3D) {
        float currentWidth = GetWidth();
        float currentHeight = GetHeight();

        if (axis == 0) {

            SetWidth(std::max(0.1f, currentWidth + scaleDelta * currentWidth));
        } else if (axis == 1) {

            SetHeight(std::max(0.1f, currentHeight + scaleDelta * currentHeight));
        } else if (axis == -1) {

            SetWidth(std::max(0.1f, currentWidth + scaleDelta * currentWidth));
            SetHeight(std::max(0.1f, currentHeight + scaleDelta * currentHeight));
        }

        return true;
    }

    return false;
}

const Color& ColorRect::GetColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("color");
    if (prop) {
        return prop->GetValue<Color>();
    }
    static Color defaultColor = Color::White();
    return defaultColor;
}

void ColorRect::SetColor(const Color& color) {
    SetPropertyValue<Color>("color", color);
}

int ColorRect::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void ColorRect::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int ColorRect::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void ColorRect::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

float ColorRect::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void ColorRect::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
}

float ColorRect::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void ColorRect::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
}

Vec2 ColorRect::GetSize() const {
    return Vec2(GetWidth(), GetHeight());
}

void ColorRect::SetSize(const Vec2& size) {
    SetWidth(size.x);
    SetHeight(size.y);
}

const core::LinkedProperty4& ColorRect::GetCornerRadius() const {
    return m_CornerRadius;
}

void ColorRect::SetCornerRadius(const core::LinkedProperty4& radius) {
    m_CornerRadius = radius;
    SetPropertyValue<Vec4>("cornerRadius", radius.AsVec4());
    SetPropertyValue<bool>("cornerRadiusLinked", radius.IsLinked());
}

void ColorRect::SetCornerRadiusAll(float radius) {
    m_CornerRadius.SetAll(radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

void ColorRect::SetCornerRadiusIndividual(size_t index, float radius) {
    m_CornerRadius.Set(index, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

bool ColorRect::IsCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void ColorRect::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
}

bool ColorRect::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void ColorRect::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
}

const Color& ColorRect::GetBorderColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("borderColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    static Color defaultColor = Color::Black();
    return defaultColor;
}

void ColorRect::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
}

const core::LinkedProperty4& ColorRect::GetBorderWidth() const {
    return m_BorderWidth;
}

void ColorRect::SetBorderWidth(const core::LinkedProperty4& width) {
    m_BorderWidth = width;
    SetPropertyValue<Vec4>("borderWidth", width.AsVec4());
    SetPropertyValue<bool>("borderWidthLinked", width.IsLinked());
}

void ColorRect::SetBorderWidthAll(float width) {
    m_BorderWidth.SetAll(width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

void ColorRect::SetBorderWidthIndividual(size_t index, float width) {
    m_BorderWidth.Set(index, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

bool ColorRect::IsBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void ColorRect::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue<bool>("borderWidthLinked", linked);
}

bool ColorRect::GetUISpace() const {
    return GetPropertyValue<bool>("uiSpace");
}

void ColorRect::SetUISpace(bool uiSpace) {
    SetPropertyValue<bool>("uiSpace", uiSpace);
}

const std::string& ColorRect::GetMaterialOverride() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("materialOverride");
    return cachedPath;
}

void ColorRect::SetMaterialOverride(const std::string& materialPath) {
    SetPropertyValue<std::string>("materialOverride", materialPath);
}

BlendMode ColorRect::GetBlendMode() const {
    int mode = GetPropertyValue<int>("blendMode");
    return IntToBlendMode(mode);
}

void ColorRect::SetBlendMode(BlendMode mode) {
    SetPropertyValue<int>("blendMode", BlendModeToInt(mode));
}

int ColorRect::BlendModeToInt(BlendMode mode) const {
    return static_cast<int>(mode);
}

BlendMode ColorRect::IntToBlendMode(int value) const {
    if (value < 0 || value > 4) return BlendMode::Alpha;
    return static_cast<BlendMode>(value);
}

Vec4 ColorRect::CornerRadiusToVec4() const {
    return m_CornerRadius.AsVec4();
}

Vec4 ColorRect::BorderWidthToVec4() const {
    return m_BorderWidth.AsVec4();
}

void ColorRect::Vec4ToCornerRadius(const Vec4& vec) {
    m_CornerRadius.FromVec4(vec);
}

void ColorRect::Vec4ToBorderWidth(const Vec4& vec) {
    m_BorderWidth.FromVec4(vec);
}

void ColorRect::RenderFill(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    Color fillColor = GetColor();

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int blendMode = static_cast<int>(GetBlendMode());

    if (std::abs(rotation) > 0.0001f) {

        Vec2 center = Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
        ctx.drawRoundedRect(center, size, cornerRadius, fillColor, rotation, blendMode);
    } else {
        ctx.drawRoundedRect(position, size, cornerRadius, fillColor, blendMode);
    }
}

void ColorRect::RenderBorder(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    if (!GetBorderEnabled()) {
        return;
    }

    Color borderColor = GetBorderColor();

    float borderTop = m_BorderWidth.Get(0);
    float borderRight = m_BorderWidth.Get(1);
    float borderBottom = m_BorderWidth.Get(2);
    float borderLeft = m_BorderWidth.Get(3);

    if (borderTop <= 0.0f && borderRight <= 0.0f && borderBottom <= 0.0f && borderLeft <= 0.0f) {
        return;
    }

    Vec4 innerRadius = m_CornerRadius.AsVec4();

    Vec2 outerSize = Vec2(size.x + borderLeft + borderRight, size.y + borderTop + borderBottom);

    Vec4 outerRadius = Vec4(
        innerRadius.x + std::max(borderTop, borderLeft),
        innerRadius.y + std::max(borderTop, borderRight),
        innerRadius.z + std::max(borderBottom, borderRight),
        innerRadius.w + std::max(borderBottom, borderLeft)
    );

    Vec4 borderWidthVec = Vec4(borderTop, borderRight, borderBottom, borderLeft);

    if (std::abs(rotation) > 0.0001f) {

        Vec2 center = Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
        ctx.drawRoundedRectBorder(center, outerSize, outerRadius, borderWidthVec, borderColor, rotation);
    } else {
        Vec2 outerPosition = Vec2(position.x - borderLeft, position.y - borderTop);
        ctx.drawRoundedRectBorder(outerPosition, outerSize, outerRadius, borderWidthVec, borderColor);
    }
}

void ColorRect::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 position;

    if (node2D) {
        position = node2D->GetGlobalPosition();
    } else {

        position = Vec2(0.0f, 0.0f);
    }

    Vec2 size = GetSize();
    float rotation = 0.0f;
    if (node2D) {
        rotation = node2D->GetGlobalRotation();
    }

    RenderBorder(ctx, position, size, rotation);

    RenderFill(ctx, position, size, rotation);
}

AABB ColorRect::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 globalScale = node2D->GetGlobalScale();
    Vec2 size = GetSize();

    size.x *= globalScale.x;
    size.y *= globalScale.y;

    float maxBorder = 0.0f;
    if (GetBorderEnabled()) {
        for (size_t i = 0; i < 4; ++i) {
            float borderWidth = m_BorderWidth.Get(i);
            if (borderWidth > maxBorder) {
                maxBorder = borderWidth;
            }
        }
    }

    Vec2 min = position - Vec2(maxBorder, maxBorder);
    Vec2 max = position + size + Vec2(maxBorder, maxBorder);

    return AABB(
        Vec3(min.x, min.y, -0.1f),
        Vec3(max.x, max.y, 0.1f)
    );
}

RenderLayer ColorRect::getRenderLayer() const {
    BlendMode blend = GetBlendMode();

    if (blend == BlendMode::Opaque) {
        return RenderLayer::Opaque;
    }

    return RenderLayer::Transparent;
}

SpatialType ColorRect::getSpatialType() const {

    return SpatialType::World2D;
}

}
}
