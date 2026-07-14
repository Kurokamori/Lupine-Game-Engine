#include "lupine/components/ColorRect.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/ui/Theme.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

ColorRect::ColorRect()
    : UIControl("ColorRect")
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

ColorRect::ColorRect(const std::string& name)
    : UIControl(name)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

ColorRect::~ColorRect() {

}

void ColorRect::DefineProperties() {

    DefineUIControlProperties(100.0f, 100.0f, "uiSpace", "Transform");

    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_ENUM_GROUP(blendMode, 0, "Appearance", Alpha, Additive, Multiply, Opaque, Overlay));
    DefineProperty(PROPERTY_FILE_GROUP(materialOverride, std::string(""), "*.lsh,*.mat,*.material", "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shaderParameters, String, std::string(""), "Appearance"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, 0, 100, 1, "Layering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Layering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, false, "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color::Black(), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(1.0f, 1.0f, 1.0f, 1.0f), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Style"));

    DefineMouseFilterProperty(MouseFilter::Ignore);
}

void ColorRect::DefineSignals() {
    DefineMouseFilterSignals();
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

void ColorRect::OnInput(float) {
    if (!IsEnabled()) {
        return;
    }

    UpdateMouseFilterState();
}

void ColorRect::OnRender() {

}

bool ColorRect::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // Resizing is delegated to the base, which writes whichever property actually drives
    // the axis: width/height when point-anchored, the offsets when anchor-stretched (where
    // width/height are never read at all), and nothing when a container owns the rect.
    const bool handled = UIControl::OnGizmoScale(scaleDelta, axis, is3D);
    return handled;
}

const std::vector<UIControl::ThemeBinding>& ColorRect::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "color",        "background",    ThemeBinding::Kind::Color },
        { "borderColor",  "border_color",  ThemeBinding::Kind::Color },
        { "cornerRadius", "corner_radius", ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color ColorRect::GetColor() const {
    return ResolveThemedColor("color", "background");
}

void ColorRect::SetColor(const Color& color) {
    SetThemedProperty<Color>("color", color);
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

const core::LinkedProperty4& ColorRect::GetCornerRadius() const {
    return m_CornerRadius;
}

void ColorRect::SetCornerRadius(const core::LinkedProperty4& radius) {
    m_CornerRadius = radius;
    SetThemedProperty<Vec4>("cornerRadius", radius.AsVec4());
    SetPropertyValue<bool>("cornerRadiusLinked", radius.IsLinked());
}

void ColorRect::SetCornerRadiusAll(float radius) {
    m_CornerRadius.SetAll(radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

void ColorRect::SetCornerRadiusIndividual(size_t index, float radius) {
    m_CornerRadius.Set(index, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
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

Color ColorRect::GetBorderColor() const {
    return ResolveThemedColor("borderColor", "border_color");
}

void ColorRect::SetBorderColor(const Color& color) {
    SetThemedProperty<Color>("borderColor", color);
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

const std::string& ColorRect::GetMaterialOverride() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("materialOverride");
    return cachedPath;
}

void ColorRect::SetMaterialOverride(const std::string& materialPath) {
    SetPropertyValue<std::string>("materialOverride", materialPath);
}

const std::string& ColorRect::GetShader() const {
    // Custom .lsh shaders live in the material override slot. Only treat a `.lsh`
    // material override as a custom shader (other material types are ignored here).
    static std::string cachedPath;
    std::string materialPath = GetPropertyValue<std::string>("materialOverride");
    if (materialPath.size() >= 4 && materialPath.compare(materialPath.size() - 4, 4, ".lsh") == 0) {
        cachedPath = materialPath;
    } else {
        cachedPath = std::string();
    }
    return cachedPath;
}

void ColorRect::SetShader(const std::string& shaderPath) {
    // Stored in the material override slot; normalize to a res:// path when possible.
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

const std::string& ColorRect::GetShaderParameters() const {
    static std::string cachedParams;
    cachedParams = GetPropertyValue<std::string>("shaderParameters");
    return cachedParams;
}

void ColorRect::SetShaderParameters(const std::string& parametersJson) {
    SetPropertyValue<std::string>("shaderParameters", parametersJson);
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

void ColorRect::RenderFill(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
    Color fillColor = GetColor();

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int blendMode = static_cast<int>(GetBlendMode());

    if (std::abs(rotation) > 0.0001f) {
        // Rotated: pass center directly
        ctx.drawRoundedRect(center, size, cornerRadius, fillColor, rotation, blendMode);
    } else {
        // Non-rotated: calculate top-left from center
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRect(topLeft, size, cornerRadius, fillColor, blendMode);
    }
}

bool ColorRect::RenderFillCustomShader(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
    const std::string& shaderPath = GetShader();
    if (shaderPath.empty()) {
        return false;
    }

    int blendMode = static_cast<int>(GetBlendMode());
    MaterialHandle material = ctx.getOrCreateLshMaterial(shaderPath, blendMode);
    if (!material.isValid()) {
        // Translation/compilation failed; caller renders the default fill instead.
        return false;
    }

    Color fillColor = GetColor();
    Vec4 cornerRadius = m_CornerRadius.AsVec4();

    MaterialPropertyBlock params;
    m_ShaderParams.BuildBlock(ctx, GetShaderParameters(), params);

    if (std::abs(rotation) > 0.0001f) {
        // Rotated: pass center directly.
        ctx.drawRoundedRectShader(center, size, cornerRadius, fillColor, rotation, material, params);
    } else {
        // Non-rotated: derive top-left from the center.
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRectShader(topLeft, size, cornerRadius, fillColor, 0.0f, material, params);
    }

    return true;
}

void ColorRect::RenderBorder(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
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
        // Rotated: pass center directly
        ctx.drawRoundedRectBorder(center, outerSize, outerRadius, borderWidthVec, borderColor, rotation);
    } else {
        // Non-rotated: calculate top-left from center
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        Vec2 outerPosition = Vec2(topLeft.x - borderLeft, topLeft.y - borderTop);
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

    // A theme may supply a uniform corner radius. Applied only when the theme
    // defines it and the property is not a local override (preserving per-corner
    // values otherwise).
    if (!IsThemeOverridden("cornerRadius")) {
        const ui::ThemeAsset* theme = GetEffectiveTheme();
        ui::ThemeManager& tm = ui::ThemeManager::GetInstance();
        if (tm.HasConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), "corner_radius")) {
            float r = tm.ResolveConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), "corner_radius", 0.0f);
            m_CornerRadius.FromVec4(Vec4(r, r, r, r));
            m_CornerRadius.SetLinked(true);
        }
    }

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    const Rect rect = GetResolvedRect();
    Vec2 position = rect.GetCenter();
    Vec2 size = rect.size;

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    float rotation = 0.0f;
    if (node2D) {
        rotation = node2D->GetGlobalRotation();
    }

    RenderBorder(ctx, position, size, rotation);

    // When a custom .lsh shader is attached and compiles, it renders the fill; otherwise
    // fall back to the built-in rounded-rect fill.
    if (!GetShader().empty() && RenderFillCustomShader(ctx, position, size, rotation)) {
        return;
    }

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

    Vec2 center = node2D->GetGlobalPosition();
    Vec2 globalScale = node2D->GetGlobalScale();
    Vec2 size = GetBoundsSize();

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

    // Calculate bounds from center
    Vec2 halfSize = size * 0.5f;
    Vec2 min = center - halfSize - Vec2(maxBorder, maxBorder);
    Vec2 max = center + halfSize + Vec2(maxBorder, maxBorder);

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
    return GetUISpatialType();
}

}
}
