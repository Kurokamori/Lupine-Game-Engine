#include "lupine/components/Panel.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;
using core::Node3D;

Panel::Panel()
    : Component("Panel")
    , m_StyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_StyleBox = std::make_shared<StyleBoxFlat>();
}

Panel::Panel(const std::string& name)
    : Component(name)
    , m_StyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_StyleBox = std::make_shared<StyleBoxFlat>();
}

Panel::~Panel() {

}

void Panel::DefineProperties() {

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 100.0f, 0.0f, 10000.0f, 1.0f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 100.0f, 0.0f, 10000.0f, 1.0f, "Size"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useUISpace, Bool, true, "Rendering"));

    DefineProperty(PROPERTY_FILE_GROUP(stylePath, std::string(""), "*.style", "Style"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.8f, 0.8f, 0.8f, 1.0f), "Background"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Background"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, false, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(1.0f, 1.0f, 1.0f, 1.0f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color::Black(), "Border"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "CornerRadius"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "CornerRadius"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(cornerDetail, 8, 1, 32, 1, "CornerDetail"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(antiAliasing, Bool, true, "CornerDetail"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(antiAliasingSize, 1.0f, 0.0f, 4.0f, 0.1f, "CornerDetail"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowEnabled, Bool, false, "Shadow"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowColor, Color, Color(0.0f, 0.0f, 0.0f, 0.6f), "Shadow"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(shadowSize, 4.0f, 0.0f, 100.0f, 0.5f, "Shadow"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowOffset, Vec2, Vec2(0.0f, 0.0f), "Shadow"));

    DefineProperty(PROPERTY_FILE_GROUP(customShaderPath, std::string(""), "*.glsl,*.shader", "Material"));
}

void Panel::OnAwake() {
    EnsureStyleBox();

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    std::string stylePath = GetStylePath();
    if (!stylePath.empty()) {
        LoadStyleFromFile(stylePath);
    }
}

void Panel::OnReady() {
    m_MeshNeedsRegeneration = true;
}

void Panel::OnRender() {

}

bool Panel::OnGizmoScale(float scaleDelta, int axis, bool is3D) {

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

        m_MeshNeedsRegeneration = true;

        return true;
    }

    return false;
}

float Panel::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void Panel::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
    m_MeshNeedsRegeneration = true;
}

float Panel::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void Panel::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    m_MeshNeedsRegeneration = true;
}

int Panel::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Panel::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int Panel::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Panel::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

bool Panel::GetUseUISpace() const {
    return GetPropertyValue<bool>("useUISpace");
}

void Panel::SetUseUISpace(bool useUISpace) {
    SetPropertyValue<bool>("useUISpace", useUISpace);
}

void Panel::SetStyleBox(std::shared_ptr<StyleBox> styleBox) {
    m_StyleBox = styleBox;
    m_MeshNeedsRegeneration = true;

    if (auto flatStyle = GetStyleBoxFlat()) {
        SetBackgroundColor(flatStyle->GetBackgroundColor());
        SetOpacity(flatStyle->GetOpacity());
        SetBorderWidthLinked(flatStyle->GetBorderWidthLinked());

    }
}

bool Panel::LoadStyleFromFile(const std::string& filepath) {
    auto styleBox = StyleBox::LoadFromFile(filepath);
    if (styleBox) {
        SetStyleBox(styleBox);
        SetStylePath(filepath);

        return true;
    }
    return false;
}

bool Panel::SaveStyleToFile(const std::string& filepath) const {
    if (m_StyleBox) {
        bool success = m_StyleBox->SaveToFile(filepath);
        if (success) {

        }
        return success;
    }
    return false;
}

const std::string& Panel::GetStylePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("stylePath");
    return cachedPath;
}

void Panel::SetStylePath(const std::string& path) {
    SetPropertyValue<std::string>("stylePath", path);
}

const Color& Panel::GetBackgroundColor() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("backgroundColor");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    return Color::White();
}

void Panel::SetBackgroundColor(const Color& color) {
    SetPropertyValue<Color>("backgroundColor", color);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetBackgroundColor(color);
    }
    m_MeshNeedsRegeneration = true;
}

float Panel::GetOpacity() const {
    return GetPropertyValue<float>("opacity");
}

void Panel::SetOpacity(float opacity) {
    SetPropertyValue<float>("opacity", opacity);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetOpacity(opacity);
    }
    m_MeshNeedsRegeneration = true;
}

bool Panel::GetBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void Panel::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue<bool>("borderWidthLinked", linked);
}

float Panel::GetBorderWidthLeft() const {
    return m_BorderWidth.Get(3);
}

void Panel::SetBorderWidthLeft(float width) {
    m_BorderWidth.Set(3, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetBorderWidthRight() const {
    return m_BorderWidth.Get(1);
}

void Panel::SetBorderWidthRight(float width) {
    m_BorderWidth.Set(1, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetBorderWidthTop() const {
    return m_BorderWidth.Get(0);
}

void Panel::SetBorderWidthTop(float width) {
    m_BorderWidth.Set(0, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetBorderWidthBottom() const {
    return m_BorderWidth.Get(2);
}

void Panel::SetBorderWidthBottom(float width) {
    m_BorderWidth.Set(2, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

bool Panel::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void Panel::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
    m_MeshNeedsRegeneration = true;
}

const Color& Panel::GetBorderColor() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("borderColor");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    return Color::Black();
}

void Panel::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
    m_MeshNeedsRegeneration = true;
}

bool Panel::GetCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void Panel::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
}

float Panel::GetCornerRadiusTopLeft() const {
    return m_CornerRadius.Get(0);
}

void Panel::SetCornerRadiusTopLeft(float radius) {
    m_CornerRadius.Set(0, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetCornerRadiusTopRight() const {
    return m_CornerRadius.Get(1);
}

void Panel::SetCornerRadiusTopRight(float radius) {
    m_CornerRadius.Set(1, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetCornerRadiusBottomLeft() const {
    return m_CornerRadius.Get(3);
}

void Panel::SetCornerRadiusBottomLeft(float radius) {
    m_CornerRadius.Set(3, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetCornerRadiusBottomRight() const {
    return m_CornerRadius.Get(2);
}

void Panel::SetCornerRadiusBottomRight(float radius) {
    m_CornerRadius.Set(2, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

int Panel::GetCornerDetail() const {
    return GetPropertyValue<int>("cornerDetail");
}

void Panel::SetCornerDetail(int detail) {
    SetPropertyValue<int>("cornerDetail", detail);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetCornerDetail(detail);
    }
    m_MeshNeedsRegeneration = true;
}

bool Panel::GetAntiAliasing() const {
    return GetPropertyValue<bool>("antiAliasing");
}

void Panel::SetAntiAliasing(bool aa) {
    SetPropertyValue<bool>("antiAliasing", aa);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetAntiAliasing(aa);
    }
    m_MeshNeedsRegeneration = true;
}

float Panel::GetAntiAliasingSize() const {
    return GetPropertyValue<float>("antiAliasingSize");
}

void Panel::SetAntiAliasingSize(float size) {
    SetPropertyValue<float>("antiAliasingSize", size);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetAntiAliasingSize(size);
    }
    m_MeshNeedsRegeneration = true;
}

bool Panel::GetShadowEnabled() const {
    return GetPropertyValue<bool>("shadowEnabled");
}

void Panel::SetShadowEnabled(bool enabled) {
    SetPropertyValue<bool>("shadowEnabled", enabled);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetShadowEnabled(enabled);
    }
    m_MeshNeedsRegeneration = true;
}

const Color& Panel::GetShadowColor() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowColor");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    return Color(0.0f, 0.0f, 0.0f, 0.6f);
}

void Panel::SetShadowColor(const Color& color) {
    SetPropertyValue<Color>("shadowColor", color);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetShadowColor(color);
    }
    m_MeshNeedsRegeneration = true;
}

float Panel::GetShadowSize() const {
    return GetPropertyValue<float>("shadowSize");
}

void Panel::SetShadowSize(float size) {
    SetPropertyValue<float>("shadowSize", size);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetShadowSize(size);
    }
    m_MeshNeedsRegeneration = true;
}

const Vec2& Panel::GetShadowOffset() const {
    static Vec2 cachedOffset;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("shadowOffset");
    if (prop) {
        cachedOffset = prop->GetValue<Vec2>();
        return cachedOffset;
    }
    return Vec2::Zero();
}

void Panel::SetShadowOffset(const Vec2& offset) {
    SetPropertyValue<Vec2>("shadowOffset", offset);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetShadowOffset(offset);
    }
    m_MeshNeedsRegeneration = true;
}

const std::string& Panel::GetCustomShaderPath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("customShaderPath");
    return cachedPath;
}

void Panel::SetCustomShaderPath(const std::string& path) {
    SetPropertyValue<std::string>("customShaderPath", path);
    if (auto flatStyle = GetStyleBoxFlat()) {
        flatStyle->SetCustomShaderPath(path);
    }
}

void Panel::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled()) {
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

    Node* owner = GetOwner();
    if (!owner) return;

    Node2D* node2D = dynamic_cast<Node2D*>(owner);
    if (!node2D) return;

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = Vec2(GetWidth(), GetHeight());
    float rotation = node2D->GetGlobalRotation();

    RenderBorder(ctx, position, size, rotation);
    RenderFill(ctx, position, size, rotation);
}

void Panel::RenderFill(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    Color fillColor = GetBackgroundColor();
    fillColor.a *= GetOpacity();

    if (fillColor.a <= 0.0f) return;

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int blendMode = 0;

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRect(position, size, cornerRadius, fillColor, rotation, blendMode);
    } else {
        ctx.drawRoundedRect(position, size, cornerRadius, fillColor, blendMode);
    }
}

void Panel::RenderBorder(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    if (!GetBorderEnabled()) {
        return;
    }

    float borderTop = m_BorderWidth.Get(0);
    float borderRight = m_BorderWidth.Get(1);
    float borderBottom = m_BorderWidth.Get(2);
    float borderLeft = m_BorderWidth.Get(3);

    if (borderTop <= 0.0f && borderRight <= 0.0f && borderBottom <= 0.0f && borderLeft <= 0.0f) {
        return;
    }

    Color borderColor = GetBorderColor();

    Vec4 innerRadius = m_CornerRadius.AsVec4();

    Vec2 outerSize = Vec2(size.x + borderLeft + borderRight, size.y + borderTop + borderBottom);
    Vec2 outerPosition = Vec2(position.x, position.y);

    Vec4 outerRadius = Vec4(
        innerRadius.x + std::max(borderTop, borderLeft),
        innerRadius.y + std::max(borderTop, borderRight),
        innerRadius.z + std::max(borderBottom, borderRight),
        innerRadius.w + std::max(borderBottom, borderLeft)
    );

    Vec4 borderWidthVec = Vec4(borderTop, borderRight, borderBottom, borderLeft);

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRectBorder(outerPosition, outerSize, outerRadius, borderWidthVec, borderColor, rotation);
    } else {
        Vec2 outerPosNoRot = Vec2(position.x - borderLeft, position.y - borderTop);
        ctx.drawRoundedRectBorder(outerPosNoRot, outerSize, outerRadius, borderWidthVec, borderColor);
    }
}

AABB Panel::getWorldBounds() const {
    if (!GetOwner()) {
        return AABB();
    }

    float width = GetWidth();
    float height = GetHeight();

    Node2D* node2d = dynamic_cast<Node2D*>(GetOwner());
    if (!node2d) {
        return AABB();
    }

    Vec2 globalPos = node2d->GetGlobalPosition();
    Vec2 globalScale = node2d->GetGlobalScale();
    float rotation = node2d->GetGlobalRotation();

    Vec2 size(width * globalScale.x, height * globalScale.y);

    if (std::abs(rotation) > 0.0001f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        Vec2 halfSize = size * 0.5f;

        Vec2 localCorners[4] = {
            Vec2(-halfSize.x, -halfSize.y),
            Vec2( halfSize.x, -halfSize.y),
            Vec2( halfSize.x,  halfSize.y),
            Vec2(-halfSize.x,  halfSize.y)
        };

        Vec2 min(FLT_MAX, FLT_MAX);
        Vec2 max(-FLT_MAX, -FLT_MAX);

        for (int i = 0; i < 4; ++i) {
            Vec2 rotated(
                localCorners[i].x * cosR - localCorners[i].y * sinR,
                localCorners[i].x * sinR + localCorners[i].y * cosR
            );
            Vec2 worldCorner = globalPos + rotated;
            min.x = std::min(min.x, worldCorner.x);
            min.y = std::min(min.y, worldCorner.y);
            max.x = std::max(max.x, worldCorner.x);
            max.y = std::max(max.y, worldCorner.y);
        }

        return AABB(Vec3(min.x, min.y, -0.1f), Vec3(max.x, max.y, 0.1f));
    }

    Vec3 min(globalPos.x - size.x * 0.5f, globalPos.y - size.y * 0.5f, -0.1f);
    Vec3 max(globalPos.x + size.x * 0.5f, globalPos.y + size.y * 0.5f, 0.1f);
    return AABB(min, max);
}

math::OBB Panel::getOrientedBounds() const {
    if (!GetOwner()) {
        return math::OBB();
    }

    Node2D* node2d = dynamic_cast<Node2D*>(GetOwner());
    if (!node2d) {
        return math::OBB();
    }

    Vec2 position = node2d->GetGlobalPosition();
    Vec2 size(GetWidth(), GetHeight());
    Vec2 globalScale = node2d->GetGlobalScale();
    float rotation = node2d->GetGlobalRotation();

    size.x *= globalScale.x;
    size.y *= globalScale.y;

    Vec3 center = Vec3(position.x, position.y, 0.0f);
    Vec3 extents = Vec3(size.x * 0.5f, size.y * 0.5f, 0.1f);
    Quat quatRotation = Quat::FromAxisAngle(Vec3::UnitZ(), rotation);

    return math::OBB(center, extents, quatRotation);
}

bool Panel::IntersectRay(const math::Ray& ray, float& outDistance) const {
    math::OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(
        Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
        Vec3(obb.extents.x, obb.extents.y, obb.extents.z)
    );

    math::Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

RenderLayer Panel::getRenderLayer() const {
    return GetUseUISpace() ? RenderLayer::UI : RenderLayer::Opaque;
}

SpatialType Panel::getSpatialType() const {

    return SpatialType::World2D;
}

void Panel::EnsureStyleBox() {
    if (!m_StyleBox) {
        m_StyleBox = std::make_shared<StyleBoxFlat>();
    }
}

std::shared_ptr<StyleBoxFlat> Panel::GetStyleBoxFlat() const {
    return std::dynamic_pointer_cast<StyleBoxFlat>(m_StyleBox);
}

}
}
