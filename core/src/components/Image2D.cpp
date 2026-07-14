#include "lupine/components/Image2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/ui/Theme.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/asset/ImageCache.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Image2D::Image2D()
    : UIControl("Image2D")
    , m_TextureHandle()
    , m_TextureNeedsUpload(false)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

Image2D::Image2D(const std::string& name)
    : UIControl(name)
    , m_TextureHandle()
    , m_TextureNeedsUpload(false)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

Image2D::~Image2D() {

}

void Image2D::DefineProperties() {

    DefineUIControlProperties(0.0f, 0.0f, "uiSpace", "Size");

    DefineProperty(PROPERTY_FILE_GROUP(texturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Texture"));
    DefineProperty(PROPERTY_FILE_GROUP(materialOverride, std::string(""), "*.lsh,*.mat,*.material", "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shaderParameters, String, std::string(""), "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Texture"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(preserveAspect, Bool, true, "Appearance"));
    DefineProperty(PROPERTY_ENUM_GROUP(aspectMode, 0, "Appearance", Fit, Letterbox, Fill, Stretch));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Appearance"));
    DefineProperty(PROPERTY_ENUM_GROUP(blendMode, 0, "Appearance", Alpha, Additive, Multiply, Opaque, Overlay));
    DefineProperty(PROPERTY_ENUM_GROUP(stretchMode, 0, "Appearance", Stretch, KeepCentered, NineSlice));

    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceMargins, Vec4, Vec4(8.0f, 8.0f, 8.0f, 8.0f), "NineSlice"));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisH, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisV, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceDrawCenter, Bool, true, "NineSlice"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, false, "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color::Black(), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(1.0f, 1.0f, 1.0f, 1.0f), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Style"));

    DefineProperty(PROPERTY_ENUM_GROUP(mouseBehaviour, 0, "Input", Ignore, PropagateUp, Stop));
}

void Image2D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    UIControl::OnPropertyChanged(propertyName, newValue);

    if (propertyName == "texturePath") {
        std::string newPath = newValue.get<std::string>();

        // Destroy old texture handle
        m_TextureHandle = TextureHandle();
        m_TextureAsset.Reset();
        m_CurrentTexturePath.clear();

        if (!newPath.empty()) {
            // Load the texture immediately, via the shared decode cache so a
            // preloaded (warmed) image is a cache hit instead of a re-decode.
            m_TextureAsset = asset::ImageCache::GetInstance().GetOrLoad(newPath);
            bool loaded = m_TextureAsset.IsValid() && m_TextureAsset->IsLoaded();

            if (loaded) {
                m_CurrentTexturePath = newPath;
                m_TextureNeedsUpload = true;

                // Set width/height from texture if not already set
                if (GetWidth() <= 0.0f && m_TextureAsset->GetWidth() > 0) {
                    SetPropertyValue<float>("width", static_cast<float>(m_TextureAsset->GetWidth()));
                }
                if (GetHeight() <= 0.0f && m_TextureAsset->GetHeight() > 0) {
                    SetPropertyValue<float>("height", static_cast<float>(m_TextureAsset->GetHeight()));
                }
            } else {
                m_TextureAsset.Reset();
            }
        }
    }
}

void Image2D::OnAwake() {

    std::string texPath = GetTexturePath();
    if (!texPath.empty()) {
        LoadTexture(texPath);
    }

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);
}

void Image2D::OnReady() {

    if (m_TextureNeedsUpload) {
        UploadTextureToGPU();
    }
}

void Image2D::OnRender() {

}

bool Image2D::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // Resizing is delegated to the base, which writes whichever property actually drives
    // the axis: width/height when point-anchored, the offsets when anchor-stretched (where
    // width/height are never read at all), and nothing when a container owns the rect.
    const bool handled = UIControl::OnGizmoScale(scaleDelta, axis, is3D);
    return handled;
}

bool Image2D::LoadTexture(const std::string& filepath) {
    if (filepath.empty()) {

        return false;
    }

    m_TextureAsset = asset::ImageCache::GetInstance().GetOrLoad(filepath);

    bool loaded = m_TextureAsset.IsValid() && m_TextureAsset->IsLoaded();

    if (!loaded) {

        m_TextureAsset.Reset();
        return false;
    }

    m_TextureNeedsUpload = true;

    SetTexturePath(filepath);

    // Initialize width/height from texture if not already set
    if (GetWidth() <= 0.0f && m_TextureAsset->GetWidth() > 0) {
        SetWidth(static_cast<float>(m_TextureAsset->GetWidth()));
    }
    if (GetHeight() <= 0.0f && m_TextureAsset->GetHeight() > 0) {
        SetHeight(static_cast<float>(m_TextureAsset->GetHeight()));
    }

    return true;
}

void Image2D::SetTexture(const asset::AssetRef<asset::ImageAsset>& texture) {
    m_TextureAsset = texture;
    m_TextureNeedsUpload = true;

    if (texture.IsValid()) {
        SetTexturePath(texture->GetPath());

        // Initialize width/height from texture if not already set
        if (GetWidth() <= 0.0f && texture->GetWidth() > 0) {
            SetWidth(static_cast<float>(texture->GetWidth()));
        }
        if (GetHeight() <= 0.0f && texture->GetHeight() > 0) {
            SetHeight(static_cast<float>(texture->GetHeight()));
        }
    }
}

void Image2D::UploadTextureToGPU() {
    if (!m_TextureAsset.IsValid() || !m_TextureAsset->IsLoaded()) {
        m_TextureNeedsUpload = false;
        return;
    }

    m_TextureNeedsUpload = false;
}

const std::string& Image2D::GetTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("texturePath");
    return cachedPath;
}

void Image2D::SetTexturePath(const std::string& path) {
    // Convert to res:// path if possible
    std::string resPath = path;
    if (!path.empty() && !(path.size() >= 6 && path.substr(0, 6) == "res://")) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(path);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }
    SetPropertyValue<std::string>("texturePath", resPath);
}

const std::string& Image2D::GetMaterialOverride() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("materialOverride");
    return cachedPath;
}

void Image2D::SetMaterialOverride(const std::string& materialPath) {
    SetPropertyValue<std::string>("materialOverride", materialPath);
}

const std::string& Image2D::GetShader() const {
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

void Image2D::SetShader(const std::string& shaderPath) {
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

const std::string& Image2D::GetShaderParameters() const {
    static std::string cachedParams;
    cachedParams = GetPropertyValue<std::string>("shaderParameters");
    return cachedParams;
}

void Image2D::SetShaderParameters(const std::string& parametersJson) {
    SetPropertyValue<std::string>("shaderParameters", parametersJson);
}

const std::vector<UIControl::ThemeBinding>& Image2D::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "color",        "modulate",      ThemeBinding::Kind::Color },
        { "borderColor",  "border_color",  ThemeBinding::Kind::Color },
        { "cornerRadius", "corner_radius", ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color Image2D::GetColor() const {
    return ResolveThemedColor("color", "modulate");
}

void Image2D::SetColor(const Color& color) {
    SetThemedProperty<Color>("color", color);
}

bool Image2D::GetPreserveAspect() const {
    return GetPropertyValue<bool>("preserveAspect");
}

void Image2D::SetPreserveAspect(bool preserve) {
    SetPropertyValue<bool>("preserveAspect", preserve);
}

AspectMode Image2D::GetAspectMode() const {
    return IntToAspectMode(GetPropertyValue<int>("aspectMode"));
}

void Image2D::SetAspectMode(AspectMode mode) {
    SetPropertyValue<int>("aspectMode", AspectModeToInt(mode));
}

bool Image2D::GetFlipH() const {
    return GetPropertyValue<bool>("flipH");
}

void Image2D::SetFlipH(bool flip) {
    SetPropertyValue<bool>("flipH", flip);
}

bool Image2D::GetFlipV() const {
    return GetPropertyValue<bool>("flipV");
}

void Image2D::SetFlipV(bool flip) {
    SetPropertyValue<bool>("flipV", flip);
}

BlendMode Image2D::GetBlendMode() const {
    return IntToBlendMode(GetPropertyValue<int>("blendMode"));
}

void Image2D::SetBlendMode(BlendMode mode) {
    SetPropertyValue<int>("blendMode", BlendModeToInt(mode));
}

UIImageStretchMode Image2D::GetStretchMode() const {
    return UIImageStretchModeFromInt(GetPropertyValue<int>("stretchMode"));
}

void Image2D::SetStretchMode(UIImageStretchMode mode) {
    SetPropertyValue<int>("stretchMode", static_cast<int>(mode));
}

Vec4 Image2D::GetNineSliceMargins() const {
    return GetPropertyValue<Vec4>("nineSliceMargins");
}

void Image2D::SetNineSliceMargins(const Vec4& margins) {
    SetPropertyValue<Vec4>("nineSliceMargins", margins);
}

UINineSliceAxisMode Image2D::GetNineSliceAxisHorizontal() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisH"));
}

void Image2D::SetNineSliceAxisHorizontal(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisH", static_cast<int>(mode));
}

UINineSliceAxisMode Image2D::GetNineSliceAxisVertical() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisV"));
}

void Image2D::SetNineSliceAxisVertical(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisV", static_cast<int>(mode));
}

bool Image2D::GetNineSliceDrawCenter() const {
    return GetPropertyValue<bool>("nineSliceDrawCenter");
}

void Image2D::SetNineSliceDrawCenter(bool drawCenter) {
    SetPropertyValue<bool>("nineSliceDrawCenter", drawCenter);
}

MouseBehaviour Image2D::GetMouseBehaviour() const {
    return IntToMouseBehaviour(GetPropertyValue<int>("mouseBehaviour"));
}

void Image2D::SetMouseBehaviour(MouseBehaviour behaviour) {
    SetPropertyValue<int>("mouseBehaviour", MouseBehaviourToInt(behaviour));
}

const core::LinkedProperty4& Image2D::GetCornerRadius() const {
    return m_CornerRadius;
}

void Image2D::SetCornerRadius(const core::LinkedProperty4& radius) {
    m_CornerRadius = radius;
    SetThemedProperty<Vec4>("cornerRadius", radius.AsVec4());
    SetPropertyValue<bool>("cornerRadiusLinked", radius.IsLinked());
}

void Image2D::SetCornerRadiusAll(float radius) {
    m_CornerRadius.SetAll(radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

void Image2D::SetCornerRadiusIndividual(size_t index, float radius) {
    m_CornerRadius.Set(index, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

bool Image2D::IsCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void Image2D::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

bool Image2D::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void Image2D::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
}

Color Image2D::GetBorderColor() const {
    return ResolveThemedColor("borderColor", "border_color");
}

void Image2D::SetBorderColor(const Color& color) {
    SetThemedProperty<Color>("borderColor", color);
}

const core::LinkedProperty4& Image2D::GetBorderWidth() const {
    return m_BorderWidth;
}

void Image2D::SetBorderWidth(const core::LinkedProperty4& width) {
    m_BorderWidth = width;
    SetPropertyValue<Vec4>("borderWidth", width.AsVec4());
    SetPropertyValue<bool>("borderWidthLinked", width.IsLinked());
}

void Image2D::SetBorderWidthAll(float width) {
    m_BorderWidth.SetAll(width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

void Image2D::SetBorderWidthIndividual(size_t index, float width) {
    m_BorderWidth.Set(index, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

bool Image2D::IsBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void Image2D::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue<bool>("borderWidthLinked", linked);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

Vec2 Image2D::GetContentMinSize() const {
    Vec2 result(0.0f, 0.0f);
    if (m_TextureAsset.IsValid()) {
        // Width/height of 0 means "use the texture dimension" on that axis.
        if (GetWidth() <= 0.0f) {
            result.x = static_cast<float>(m_TextureAsset->GetWidth());
        }
        if (GetHeight() <= 0.0f) {
            result.y = static_cast<float>(m_TextureAsset->GetHeight());
        }
    }
    return result;
}

Vec2 Image2D::CalculatePosition() const {

    if (!m_Owner) {
        return Vec2(0.0f, 0.0f);
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (node2D) {
        return node2D->GetGlobalPosition();
    }

    return Vec2(0.0f, 0.0f);
}

Vec2 Image2D::CalculateSize() const {
    float w = GetWidth();
    float h = GetHeight();

    // If width/height are 0, use texture dimensions
    if (w <= 0.0f && m_TextureAsset.IsValid()) {
        w = static_cast<float>(m_TextureAsset->GetWidth());
    }
    if (h <= 0.0f && m_TextureAsset.IsValid()) {
        h = static_cast<float>(m_TextureAsset->GetHeight());
    }

    // Default to 100x100 if still zero
    if (w <= 0.0f) w = 100.0f;
    if (h <= 0.0f) h = 100.0f;

    return Vec2(w, h);
}

void Image2D::CalculateUVCoordinates(Vec2& uvMin, Vec2& uvMax) const {

    uvMin = Vec2(0.0f, 0.0f);
    uvMax = Vec2(1.0f, 1.0f);

    if (GetFlipH()) {
        std::swap(uvMin.x, uvMax.x);
    }

    if (GetFlipV()) {
        std::swap(uvMin.y, uvMax.y);
    }
}

void Image2D::ApplyAspectRatio(Vec2&, Vec2& size) const {
    if (!GetPreserveAspect() || !m_TextureAsset.IsValid()) {
        return;
    }

    float texWidth = static_cast<float>(m_TextureAsset->GetWidth());
    float texHeight = static_cast<float>(m_TextureAsset->GetHeight());

    if (texWidth <= 0.0f || texHeight <= 0.0f) {
        return;
    }

    float texAspect = texWidth / texHeight;
    float rectAspect = size.x / size.y;

    AspectMode mode = GetAspectMode();

    switch (mode) {
        case AspectMode::Fit:
        case AspectMode::Letterbox:
            // Show entire image, may have letterboxing
            if (texAspect > rectAspect) {
                // Texture is wider - fit to width, reduce height
                size.y = size.x / texAspect;
            } else {
                // Texture is taller - fit to height, reduce width
                size.x = size.y * texAspect;
            }
            // Center stays the same since we're center-based
            break;

        case AspectMode::Fill:
            // Fill rect, may crop image
            if (texAspect > rectAspect) {
                // Texture is wider - fill height, expand width
                size.x = size.y * texAspect;
            } else {
                // Texture is taller - fill width, expand height
                size.y = size.x / texAspect;
            }
            // Center stays the same since we're center-based
            break;

        case AspectMode::Stretch:
            // Ignore aspect ratio, use size as-is
            break;
    }
}

void Image2D::RenderImage(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
    if (!m_TextureHandle.isValid()) {
        return;
    }

    Color tint = GetColor();

    // KeepCentered and NineSlice route through the shared UI image painter.
    // Stretch keeps the legacy textured rounded-rect path so flip, per-blend-mode
    // and aspect-ratio behavior are preserved unchanged.
    UIImageStretchMode stretchMode = GetStretchMode();
    if (stretchMode != UIImageStretchMode::Stretch && m_TextureAsset.IsValid()) {
        UINineSlice nineSlice;
        if (stretchMode == UIImageStretchMode::NineSlice) {
            Vec4 margins = GetNineSliceMargins();
            nineSlice.marginLeft = margins.x;
            nineSlice.marginTop = margins.y;
            nineSlice.marginRight = margins.z;
            nineSlice.marginBottom = margins.w;
            nineSlice.axisHorizontal = GetNineSliceAxisHorizontal();
            nineSlice.axisVertical = GetNineSliceAxisVertical();
            nineSlice.drawCenter = GetNineSliceDrawCenter();
        }

        DrawUIImage(ctx, center, size, rotation, m_TextureHandle,
                    m_TextureAsset->GetWidth(), m_TextureAsset->GetHeight(),
                    tint, m_CornerRadius.AsVec4(), stretchMode, nineSlice);
        return;
    }

    Vec2 uvMin, uvMax;
    CalculateUVCoordinates(uvMin, uvMax);

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int blendMode = static_cast<int>(GetBlendMode());

    if (std::abs(rotation) > 0.0001f) {
        // Rotated: pass center directly
        ctx.drawRoundedRect(center, size, cornerRadius, tint, m_TextureHandle, rotation, uvMin, uvMax, blendMode);
    } else {
        // Non-rotated: calculate top-left from center
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRect(topLeft, size, cornerRadius, tint, m_TextureHandle, uvMin, uvMax, blendMode);
    }
}

void Image2D::RenderBorder(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
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

int Image2D::AspectModeToInt(AspectMode mode) const {
    return static_cast<int>(mode);
}

AspectMode Image2D::IntToAspectMode(int value) const {
    if (value < 0 || value > 3) return AspectMode::Fit;
    return static_cast<AspectMode>(value);
}

int Image2D::BlendModeToInt(BlendMode mode) const {
    return static_cast<int>(mode);
}

BlendMode Image2D::IntToBlendMode(int value) const {
    if (value < 0 || value > 4) return BlendMode::Alpha;
    return static_cast<BlendMode>(value);
}

int Image2D::MouseBehaviourToInt(MouseBehaviour behaviour) const {
    return static_cast<int>(behaviour);
}

MouseBehaviour Image2D::IntToMouseBehaviour(int value) const {
    if (value < 0 || value > 2) return MouseBehaviour::Ignore;
    return static_cast<MouseBehaviour>(value);
}

void Image2D::buildDrawCommands(RenderContext& ctx) {
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

    std::string currentPath = GetTexturePath();
    if (currentPath != m_CurrentTexturePath) {

        if (m_TextureHandle.isValid()) {
            IGfxDevice* device = ctx.getDevice();
            if (device) {
                device->destroyTexture(m_TextureHandle);
                m_TextureHandle = TextureHandle();
            }
        }

        m_TextureAsset.Reset();

        if (!currentPath.empty()) {
            m_TextureAsset = asset::ImageCache::GetInstance().GetOrLoad(currentPath);
            bool loaded = m_TextureAsset.IsValid() && m_TextureAsset->IsLoaded();

            if (!loaded) {
                m_TextureAsset.Reset();
            } else {
                // Initialize width/height from texture if not already set
                if (GetWidth() <= 0.0f && m_TextureAsset->GetWidth() > 0) {
                    SetWidth(static_cast<float>(m_TextureAsset->GetWidth()));
                }
                if (GetHeight() <= 0.0f && m_TextureAsset->GetHeight() > 0) {
                    SetHeight(static_cast<float>(m_TextureAsset->GetHeight()));
                }
            }
        }

        m_CurrentTexturePath = currentPath;
    }

    if (!m_TextureHandle.isValid() && m_TextureAsset.IsValid() && m_TextureAsset->IsLoaded()) {
        // Only create the texture if the asset has valid data
        if (m_TextureAsset->GetWidth() > 0 && m_TextureAsset->GetHeight() > 0 &&
            m_TextureAsset->GetData() != nullptr) {

            IGfxDevice* device = ctx.getDevice();
            if (device) {
                m_TextureHandle = lupine::CreateTexture2DFromImage(device, *m_TextureAsset, TextureFormat::RGBA8_UNORM);
            }
        }
    }

    const Rect __rect = GetResolvedRect();
    Vec2 position = __rect.GetCenter();
    Vec2 size = __rect.size;
    if (size.x <= 0.0f || size.y <= 0.0f) {
        // Auto-size from the texture when width/height are 0 (point-anchor case).
        size = CalculateSize();
    }

    ApplyAspectRatio(position, size);

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    float rotation = 0.0f;
    if (node2D) {
        rotation = node2D->GetGlobalRotation();
    }

    RenderBorder(ctx, position, size, rotation);

    // A custom .lsh shader, when attached and compiled, renders the image.
    if (!GetShader().empty() && RenderImageCustomShader(ctx, position, size, rotation)) {
        return;
    }

    RenderImage(ctx, position, size, rotation);
}

bool Image2D::RenderImageCustomShader(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation) {
    const std::string& shaderPath = GetShader();
    if (shaderPath.empty()) {
        return false;
    }

    int blendMode = static_cast<int>(GetBlendMode());
    MaterialHandle material = ctx.getOrCreateLshMaterial(shaderPath, blendMode);
    if (!material.isValid()) {
        return false;
    }

    Color tint = GetColor();
    Vec4 cornerRadius = m_CornerRadius.AsVec4();

    Vec2 uvMin, uvMax;
    CalculateUVCoordinates(uvMin, uvMax);

    MaterialPropertyBlock params;
    m_ShaderParams.BuildBlock(ctx, GetShaderParameters(), params);

    // Expose the image to the custom shader as u_Texture (overrides the standard defaults).
    bool hasTexture = m_TextureHandle.isValid();
    params.setBool("u_UseTexture", hasTexture);
    if (hasTexture) {
        params.setTexture("u_Texture", m_TextureHandle);
    }
    params.setVec4("u_UVRect", Vec4(uvMin.x, uvMin.y, uvMax.x, uvMax.y));

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRectShader(center, size, cornerRadius, tint, rotation, material, params);
    } else {
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRectShader(topLeft, size, cornerRadius, tint, 0.0f, material, params);
    }

    return true;
}

AABB Image2D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Vec2 center = CalculatePosition();
    Vec2 size = GetBoundsSize();

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

    return AABB(Vec3(min.x, min.y, -0.1f), Vec3(max.x, max.y, 0.1f));
}

RenderLayer Image2D::getRenderLayer() const {
    BlendMode blend = GetBlendMode();

    if (blend == BlendMode::Opaque) {
        return RenderLayer::Opaque;
    }

    return RenderLayer::Transparent;
}

SpatialType Image2D::getSpatialType() const {
    return GetUISpatialType();
}

bool Image2D::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string currentPath = GetTexturePath();
    if (currentPath.empty()) {
        return false;
    }

    // Resolve our path for comparison
    std::string resolvedCurrentPath;
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        resolvedCurrentPath = assetDb.ResolveAsset(currentPath);
    }

    // Check if this is our texture
    bool matches = (currentPath == changedPath) ||
                   (!resolvedCurrentPath.empty() && !resolvedChangedPath.empty() &&
                    resolvedCurrentPath == resolvedChangedPath);

    if (matches) {

        // Invalidate instance state - force reload on next render
        m_TextureHandle = TextureHandle();
        m_TextureAsset.Reset();
        m_CurrentTexturePath.clear();
        m_TextureNeedsUpload = true;

        return true;
    }

    return false;
}

}
}
