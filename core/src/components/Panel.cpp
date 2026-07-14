#include "lupine/components/Panel.hpp"
#include "lupine/components/StyleBoxRenderer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/ui/Theme.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;
using core::Node3D;

Panel::Panel()
    : UIControl("Panel")
    , m_StyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_StyleBox = std::make_shared<StyleBoxFlat>();
}

Panel::Panel(const std::string& name)
    : UIControl(name)
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

    DefineUIControlProperties(100.0f, 100.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    DefineProperty(PROPERTY_FILE_GROUP(stylePath, std::string(""), "*.style", "Style"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.8f, 0.8f, 0.8f, 1.0f), "Background"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Background"));
    DefineProperty(PROPERTY_FILE_GROUP(backgroundImagePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Background"));
    DefineProperty(PROPERTY_ENUM_GROUP(backgroundImageStretchMode, 0, "Background", Stretch, KeepCentered, NineSlice));

    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceMargins, Vec4, Vec4(8.0f, 8.0f, 8.0f, 8.0f), "NineSlice"));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisH, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisV, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceDrawCenter, Bool, true, "NineSlice"));

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
    DefineProperty(PROPERTY_FILE_GROUP(materialOverride, std::string(""), "*.lsh,*.mat,*.material", "Material"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shaderParameters, String, std::string(""), "Material"));

    DefineMouseFilterProperty(MouseFilter::Ignore);
}

void Panel::DefineSignals() {
    DefineMouseFilterSignals();
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

void Panel::OnInput(float) {
    if (!IsEnabled()) {
        return;
    }

    UpdateMouseFilterState();
}

void Panel::OnRender() {

}

bool Panel::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // Resizing is delegated to the base, which writes whichever property actually drives
    // the axis: width/height when point-anchored, the offsets when anchor-stretched (where
    // width/height are never read at all), and nothing when a container owns the rect.
    const bool handled = UIControl::OnGizmoScale(scaleDelta, axis, is3D);
    if (handled) {
        m_MeshNeedsRegeneration = true;
    }
    return handled;
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

const std::vector<UIControl::ThemeBinding>& Panel::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "backgroundColor",     "background",       ThemeBinding::Kind::Color },
        { "backgroundImagePath", "background_image", ThemeBinding::Kind::Image },
        { "borderColor",     "border_color",  ThemeBinding::Kind::Color },
        { "shadowColor",     "shadow_color",  ThemeBinding::Kind::Color },
        { "cornerRadius",    "corner_radius", ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color Panel::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}

void Panel::SetBackgroundColor(const Color& color) {
    SetThemedProperty<Color>("backgroundColor", color);
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

std::string Panel::GetBackgroundImagePath() const {
    return ResolveThemedImage("backgroundImagePath", "background_image");
}

void Panel::SetBackgroundImagePath(const std::string& path) {
    std::string resPath = path;
    if (!path.empty() && !(path.size() >= 6 && path.substr(0, 6) == "res://")) {
        asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(path);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }
    SetThemedProperty<std::string>("backgroundImagePath", resPath);
    m_MeshNeedsRegeneration = true;
}

UIImageStretchMode Panel::GetBackgroundImageStretchMode() const {
    return UIImageStretchModeFromInt(GetPropertyValue<int>("backgroundImageStretchMode"));
}

void Panel::SetBackgroundImageStretchMode(UIImageStretchMode mode) {
    SetPropertyValue<int>("backgroundImageStretchMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

Vec4 Panel::GetNineSliceMargins() const {
    return GetPropertyValue<Vec4>("nineSliceMargins");
}

void Panel::SetNineSliceMargins(const Vec4& margins) {
    SetPropertyValue<Vec4>("nineSliceMargins", margins);
    m_MeshNeedsRegeneration = true;
}

UINineSliceAxisMode Panel::GetNineSliceAxisHorizontal() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisH"));
}

void Panel::SetNineSliceAxisHorizontal(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisH", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

UINineSliceAxisMode Panel::GetNineSliceAxisVertical() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisV"));
}

void Panel::SetNineSliceAxisVertical(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisV", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

bool Panel::GetNineSliceDrawCenter() const {
    return GetPropertyValue<bool>("nineSliceDrawCenter");
}

void Panel::SetNineSliceDrawCenter(bool drawCenter) {
    SetPropertyValue<bool>("nineSliceDrawCenter", drawCenter);
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

Color Panel::GetBorderColor() const {
    return ResolveThemedColor("borderColor", "border_color");
}

void Panel::SetBorderColor(const Color& color) {
    SetThemedProperty<Color>("borderColor", color);
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
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetCornerRadiusTopRight() const {
    return m_CornerRadius.Get(1);
}

void Panel::SetCornerRadiusTopRight(float radius) {
    m_CornerRadius.Set(1, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetCornerRadiusBottomLeft() const {
    return m_CornerRadius.Get(3);
}

void Panel::SetCornerRadiusBottomLeft(float radius) {
    m_CornerRadius.Set(3, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Panel::GetCornerRadiusBottomRight() const {
    return m_CornerRadius.Get(2);
}

void Panel::SetCornerRadiusBottomRight(float radius) {
    m_CornerRadius.Set(2, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
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

Color Panel::GetShadowColor() const {
    return ResolveThemedColor("shadowColor", "shadow_color");
}

void Panel::SetShadowColor(const Color& color) {
    SetThemedProperty<Color>("shadowColor", color);
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
    cachedOffset = prop ? prop->GetValue<Vec2>() : Vec2::Zero();
    return cachedOffset;
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

const std::string& Panel::GetShader() const {
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

void Panel::SetShader(const std::string& shaderPath) {
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

const std::string& Panel::GetShaderParameters() const {
    static std::string cachedParams;
    cachedParams = GetPropertyValue<std::string>("shaderParameters");
    return cachedParams;
}

void Panel::SetShaderParameters(const std::string& parametersJson) {
    SetPropertyValue<std::string>("shaderParameters", parametersJson);
}

void Panel::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled()) {
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

    Node* owner = GetOwner();
    if (!owner) return;

    Node2D* node2D = dynamic_cast<Node2D*>(owner);
    if (!node2D) return;

    const Rect __rect = GetResolvedRect();
    Vec2 position = __rect.GetCenter();
    Vec2 size = __rect.size;
    float rotation = node2D->GetGlobalRotation();

    // A themed StyleBox entry ("panel"), when the effective theme defines one,
    // fully replaces the flat property-driven background+border — this is what
    // lets a theme supply a textured nine-patch / line / empty panel style. The
    // control's own opacity still modulates it. The local custom shader and
    // background image still draw over it.
    if (std::shared_ptr<StyleBox> themedBox = ResolveThemedStyleBox("panel")) {
        bool fillDrawn = false;
        if (!GetShader().empty()) {
            fillDrawn = RenderFillCustomShader(ctx, position, size, rotation);
        }
        if (!fillDrawn) {
            DrawStyleBox(ctx, themedBox.get(), position, size, rotation, Color(1.0f, 1.0f, 1.0f, GetOpacity()));
        }
        RenderBackgroundImage(ctx, position, size, rotation);
        return;
    }

    RenderBorder(ctx, position, size, rotation);

    // A custom .lsh shader, when attached and compiled, renders the background fill;
    // otherwise the flat StyleBox fill is used.
    bool fillDrawn = false;
    if (!GetShader().empty()) {
        fillDrawn = RenderFillCustomShader(ctx, position, size, rotation);
    }
    if (!fillDrawn) {
        RenderFill(ctx, position, size, rotation);
    }

    // Optional background image, painted over the fill (tinted by opacity).
    RenderBackgroundImage(ctx, position, size, rotation);
}

void Panel::RenderBackgroundImage(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    std::string path = ResolveThemedImage("backgroundImagePath", "background_image");

    if (path != m_CurrentBackgroundImagePath) {
        if (m_BackgroundImageHandle.isValid()) {
            if (IGfxDevice* device = ctx.getDevice()) {
                device->destroyTexture(m_BackgroundImageHandle);
            }
            m_BackgroundImageHandle = TextureHandle();
        }
        m_BackgroundImageAsset.Reset();
        m_CurrentBackgroundImagePath = path;

        if (!path.empty()) {
            m_BackgroundImageAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
            if (!m_BackgroundImageAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB)) {
                m_BackgroundImageAsset.Reset();
            }
        }
    }

    if (!m_BackgroundImageHandle.isValid() && m_BackgroundImageAsset.IsValid() &&
        m_BackgroundImageAsset->IsLoaded()) {
        if (m_BackgroundImageAsset->GetWidth() > 0 && m_BackgroundImageAsset->GetHeight() > 0 &&
            m_BackgroundImageAsset->GetData() != nullptr) {
            if (IGfxDevice* device = ctx.getDevice()) {
                m_BackgroundImageHandle = lupine::CreateTexture2DFromImage(device, *m_BackgroundImageAsset, TextureFormat::RGBA8_UNORM);
            }
        }
    }

    if (!m_BackgroundImageHandle.isValid() || !m_BackgroundImageAsset.IsValid()) {
        return;
    }

    Color tint = Color::White();
    tint.a *= GetOpacity();

    UIImageStretchMode stretchMode = GetBackgroundImageStretchMode();
    Vec4 margins = GetNineSliceMargins();
    UINineSlice nineSlice;
    nineSlice.marginLeft = margins.x;
    nineSlice.marginTop = margins.y;
    nineSlice.marginRight = margins.z;
    nineSlice.marginBottom = margins.w;
    nineSlice.axisHorizontal = GetNineSliceAxisHorizontal();
    nineSlice.axisVertical = GetNineSliceAxisVertical();
    nineSlice.drawCenter = GetNineSliceDrawCenter();

    // A themed "background_image" entry may also dictate the fit (stretch / nine-slice),
    // overriding this panel's own stretch + nine-slice properties.
    ui::ThemeImage themedImage;
    if (ResolveThemedImageEx("backgroundImagePath", "background_image", themedImage) && themedImage.hasStretch) {
        stretchMode = UIImageStretchModeFromInt(themedImage.stretchMode);
        if (themedImage.stretchMode == 2) {
            nineSlice.marginLeft = themedImage.marginLeft;
            nineSlice.marginTop = themedImage.marginTop;
            nineSlice.marginRight = themedImage.marginRight;
            nineSlice.marginBottom = themedImage.marginBottom;
            nineSlice.axisHorizontal = UINineSliceAxisModeFromInt(themedImage.axisH);
            nineSlice.axisVertical = UINineSliceAxisModeFromInt(themedImage.axisV);
            nineSlice.drawCenter = themedImage.drawCenter;
        }
    }

    DrawUIImage(ctx, position, size, rotation, m_BackgroundImageHandle,
                m_BackgroundImageAsset->GetWidth(), m_BackgroundImageAsset->GetHeight(),
                tint, m_CornerRadius.AsVec4(), stretchMode, nineSlice);
}

bool Panel::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string imagePath = GetBackgroundImagePath();
    if (imagePath.empty()) {
        return false;
    }

    asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();
    std::string resolved;
    if (assetDb.IsInitialized()) {
        resolved = assetDb.ResolveAsset(imagePath);
    }

    bool matches = (imagePath == changedPath) ||
                   (!resolved.empty() && !resolvedChangedPath.empty() && resolved == resolvedChangedPath);

    if (matches) {
        m_BackgroundImageHandle = TextureHandle();
        m_BackgroundImageAsset.Reset();
        m_CurrentBackgroundImagePath.clear();
        m_MeshNeedsRegeneration = true;
        return true;
    }

    return false;
}

bool Panel::RenderFillCustomShader(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
    const std::string& shaderPath = GetShader();
    if (shaderPath.empty()) {
        return false;
    }

    MaterialHandle material = ctx.getOrCreateLshMaterial(shaderPath, 0);
    if (!material.isValid()) {
        return false;
    }

    Color fillColor = GetBackgroundColor();
    fillColor.a *= GetOpacity();

    Vec4 cornerRadius = m_CornerRadius.AsVec4();

    MaterialPropertyBlock params;
    m_ShaderParams.BuildBlock(ctx, GetShaderParameters(), params);

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRectShader(center, size, cornerRadius, fillColor, rotation, material, params);
    } else {
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRectShader(topLeft, size, cornerRadius, fillColor, 0.0f, material, params);
    }

    return true;
}

void Panel::RenderFill(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
    Color fillColor = GetBackgroundColor();
    fillColor.a *= GetOpacity();

    if (fillColor.a <= 0.0f) return;

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int blendMode = 0;

    if (std::abs(rotation) > 0.0001f) {
        // Rotated: pass center directly
        ctx.drawRoundedRect(center, size, cornerRadius, fillColor, rotation, blendMode);
    } else {
        // Non-rotated: calculate top-left from center
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRect(topLeft, size, cornerRadius, fillColor, blendMode);
    }
}

void Panel::RenderBorder(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
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

    Vec4 outerRadius = Vec4(
        innerRadius.x + std::max(borderTop, borderLeft),
        innerRadius.y + std::max(borderTop, borderRight),
        innerRadius.z + std::max(borderBottom, borderRight),
        innerRadius.w + std::max(borderBottom, borderLeft)
    );

    Vec4 borderWidthVec = Vec4(borderTop, borderRight, borderBottom, borderLeft);

    // Account for asymmetric border offset from center
    float borderOffsetX = (borderRight - borderLeft) * 0.5f;
    float borderOffsetY = (borderBottom - borderTop) * 0.5f;
    Vec2 borderCenter = Vec2(center.x + borderOffsetX, center.y + borderOffsetY);

    if (std::abs(rotation) > 0.0001f) {
        // Rotated: pass center directly
        ctx.drawRoundedRectBorder(borderCenter, outerSize, outerRadius, borderWidthVec, borderColor, rotation);
    } else {
        // Non-rotated: calculate top-left from center
        Vec2 outerTopLeft = Vec2(borderCenter.x - outerSize.x * 0.5f, borderCenter.y - outerSize.y * 0.5f);
        ctx.drawRoundedRectBorder(outerTopLeft, outerSize, outerRadius, borderWidthVec, borderColor);
    }
}

AABB Panel::getWorldBounds() const {
    if (!GetOwner()) {
        return AABB();
    }

    Node2D* node2d = dynamic_cast<Node2D*>(GetOwner());
    if (!node2d) {
        return AABB();
    }

    Vec2 globalPos = node2d->GetGlobalPosition();
    Vec2 globalScale = node2d->GetGlobalScale();
    float rotation = node2d->GetGlobalRotation();

    Vec2 boundsSize = GetBoundsSize();
    Vec2 size(boundsSize.x * globalScale.x, boundsSize.y * globalScale.y);

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
    return GetUISpatialType();
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
