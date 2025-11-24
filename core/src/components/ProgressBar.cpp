#include "lupine/components/ProgressBar.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cfloat>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;

ProgressBar::ProgressBar()
    : Component("ProgressBar")
    , m_BackgroundTextureAsset()
    , m_BackgroundTextureHandle()
    , m_CurrentBackgroundTexturePath("")
    , m_FillTextureAsset()
    , m_FillTextureHandle()
    , m_CurrentFillTexturePath("")
    , m_BorderTextureAsset()
    , m_BorderTextureHandle()
    , m_CurrentBorderTexturePath("")
    , m_FontAsset()
    , m_FontHandle()
    , m_CurrentFontPath("")
    , m_CurrentFontSize(16.0f)
    , m_TextMesh()
    , m_TextMeshNeedsRegeneration(true)
    , m_CachedValueText("")
    , m_DisplayValue(0.0f)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

ProgressBar::ProgressBar(const std::string& name)
    : Component(name)
    , m_BackgroundTextureAsset()
    , m_BackgroundTextureHandle()
    , m_CurrentBackgroundTexturePath("")
    , m_FillTextureAsset()
    , m_FillTextureHandle()
    , m_CurrentFillTexturePath("")
    , m_BorderTextureAsset()
    , m_BorderTextureHandle()
    , m_CurrentBorderTexturePath("")
    , m_FontAsset()
    , m_FontHandle()
    , m_CurrentFontPath("")
    , m_CurrentFontSize(16.0f)
    , m_TextMesh()
    , m_TextMeshNeedsRegeneration(true)
    , m_CachedValueText("")
    , m_DisplayValue(0.0f)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

ProgressBar::~ProgressBar() {

}

void ProgressBar::DefineProperties() {

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minValue, 0.0f, -10000.0f, 10000.0f, 0.1f, "Value"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxValue, 100.0f, -10000.0f, 10000.0f, 0.1f, "Value"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(value, 50.0f, -10000.0f, 10000.0f, 0.1f, "Value"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(step, 0.0f, 0.0f, 100.0f, 0.1f, "Value"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(smooth, Bool, false, "Smooth"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(smoothSpeed, 5.0f, 0.1f, 100.0f, 0.1f, "Smooth"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 200.0f, 0.0f, 10000.0f, 1.0f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 20.0f, 0.0f, 10000.0f, 1.0f, "Size"));

    DefineProperty(PROPERTY_ENUM_GROUP(orientation, 0, "Orientation", Horizontal, Vertical));
    DefineProperty(PROPERTY_ENUM_GROUP(fillDirection, 0, "Orientation", LeftToRight, RightToLeft, UpToDown, DownToUp));

    DefineProperty(PROPERTY_FILE_GROUP(backgroundTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Background"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.2f, 0.2f, 0.2f, 1.0f), "Background"));

    DefineProperty(PROPERTY_FILE_GROUP(fillTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Fill"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fillColor, Color, Color(0.0f, 0.8f, 0.0f, 1.0f), "Fill"));

    DefineProperty(PROPERTY_FILE_GROUP(borderTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color::White(), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Border"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(showValue, Bool, false, "ValueDisplay"));
    DefineProperty(PROPERTY_FILE_GROUP(valueFontPath, std::string(""), "*.ttf,*.otf", "ValueDisplay"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(valueFontSize, 14.0f, 1.0f, 256.0f, 1.0f, "ValueDisplay"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(valueColor, Color, Color::White(), "ValueDisplay"));
}

void ProgressBar::OnAwake() {

    m_DisplayValue = GetValue();

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    std::string bgTexPath = GetBackgroundTexturePath();
    if (!bgTexPath.empty()) {
        LoadBackgroundTexture(bgTexPath);
    }

    std::string fillTexPath = GetFillTexturePath();
    if (!fillTexPath.empty()) {
        LoadFillTexture(fillTexPath);
    }

    std::string borderTexPath = GetBorderTexturePath();
    if (!borderTexPath.empty()) {
        LoadBorderTexture(borderTexPath);
    }

    std::string fontPath = GetValueFontPath();
    if (!fontPath.empty()) {
        LoadFont(fontPath);
    }
}

void ProgressBar::OnUpdate(float deltaTime) {
    if (!GetSmooth()) {
        m_DisplayValue = GetValue();
        return;
    }

    float targetValue = GetValue();
    float speed = GetSmoothSpeed();

    if (std::abs(m_DisplayValue - targetValue) > 0.001f) {
        m_DisplayValue = math::Lerp(m_DisplayValue, targetValue, deltaTime * speed);
        m_TextMeshNeedsRegeneration = true;
    } else {
        m_DisplayValue = targetValue;
    }
}

bool ProgressBar::OnGizmoScale(float scaleDelta, int axis, bool is3D) {

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

float ProgressBar::GetMinValue() const {
    return GetPropertyValue<float>("minValue");
}

void ProgressBar::SetMinValue(float value) {
    SetPropertyValue<float>("minValue", value);
}

float ProgressBar::GetMaxValue() const {
    return GetPropertyValue<float>("maxValue");
}

void ProgressBar::SetMaxValue(float value) {
    SetPropertyValue<float>("maxValue", value);
}

float ProgressBar::GetValue() const {
    return GetPropertyValue<float>("value");
}

void ProgressBar::SetValue(float value) {
    float minVal = GetMinValue();
    float maxVal = GetMaxValue();
    float step = GetStep();

    value = std::max(minVal, std::min(maxVal, value));

    if (step > 0.0f) {
        value = std::round((value - minVal) / step) * step + minVal;
    }

    SetPropertyValue<float>("value", value);
    m_TextMeshNeedsRegeneration = true;
}

float ProgressBar::GetStep() const {
    return GetPropertyValue<float>("step");
}

void ProgressBar::SetStep(float step) {
    SetPropertyValue<float>("step", std::max(0.0f, step));
}

bool ProgressBar::GetSmooth() const {
    return GetPropertyValue<bool>("smooth");
}

void ProgressBar::SetSmooth(bool smooth) {
    SetPropertyValue<bool>("smooth", smooth);
}

float ProgressBar::GetSmoothSpeed() const {
    return GetPropertyValue<float>("smoothSpeed");
}

void ProgressBar::SetSmoothSpeed(float speed) {
    SetPropertyValue<float>("smoothSpeed", std::max(0.1f, speed));
}

float ProgressBar::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void ProgressBar::SetWidth(float width) {
    SetPropertyValue<float>("width", std::max(0.0f, width));
}

float ProgressBar::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void ProgressBar::SetHeight(float height) {
    SetPropertyValue<float>("height", std::max(0.0f, height));
}

int ProgressBar::GetOrientation() const {
    return GetPropertyValue<int>("orientation");
}

void ProgressBar::SetOrientation(int orientation) {
    SetPropertyValue<int>("orientation", std::max(0, std::min(1, orientation)));
}

int ProgressBar::GetFillDirection() const {
    return GetPropertyValue<int>("fillDirection");
}

void ProgressBar::SetFillDirection(int direction) {
    SetPropertyValue<int>("fillDirection", std::max(0, std::min(3, direction)));
}

const std::string& ProgressBar::GetBackgroundTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("backgroundTexturePath");
    return cachedPath;
}

void ProgressBar::SetBackgroundTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("backgroundTexturePath", path);
}

const Color& ProgressBar::GetBackgroundColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("backgroundColor");
    return cachedColor;
}

void ProgressBar::SetBackgroundColor(const Color& color) {
    SetPropertyValue<Color>("backgroundColor", color);
}

const std::string& ProgressBar::GetFillTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("fillTexturePath");
    return cachedPath;
}

void ProgressBar::SetFillTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("fillTexturePath", path);
}

const Color& ProgressBar::GetFillColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("fillColor");
    return cachedColor;
}

void ProgressBar::SetFillColor(const Color& color) {
    SetPropertyValue<Color>("fillColor", color);
}

const std::string& ProgressBar::GetBorderTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("borderTexturePath");
    return cachedPath;
}

void ProgressBar::SetBorderTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("borderTexturePath", path);
}

const Color& ProgressBar::GetBorderColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("borderColor");
    return cachedColor;
}

void ProgressBar::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
}

bool ProgressBar::GetShowValue() const {
    return GetPropertyValue<bool>("showValue");
}

void ProgressBar::SetShowValue(bool show) {
    SetPropertyValue<bool>("showValue", show);
    m_TextMeshNeedsRegeneration = true;
}

const std::string& ProgressBar::GetValueFontPath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("valueFontPath");
    return cachedPath;
}

void ProgressBar::SetValueFontPath(const std::string& path) {
    SetPropertyValue<std::string>("valueFontPath", path);
}

float ProgressBar::GetValueFontSize() const {
    return GetPropertyValue<float>("valueFontSize");
}

void ProgressBar::SetValueFontSize(float size) {
    SetPropertyValue<float>("valueFontSize", std::max(1.0f, size));
    m_TextMeshNeedsRegeneration = true;
}

const Color& ProgressBar::GetValueColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("valueColor");
    return cachedColor;
}

void ProgressBar::SetValueColor(const Color& color) {
    SetPropertyValue<Color>("valueColor", color);
}

const core::LinkedProperty4& ProgressBar::GetCornerRadius() const {
    return m_CornerRadius;
}

void ProgressBar::SetCornerRadius(const core::LinkedProperty4& radius) {
    m_CornerRadius = radius;
    SetPropertyValue<Vec4>("cornerRadius", radius.AsVec4());
    SetPropertyValue<bool>("cornerRadiusLinked", radius.IsLinked());
}

void ProgressBar::SetCornerRadiusAll(float radius) {
    m_CornerRadius.SetAll(radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

void ProgressBar::SetCornerRadiusIndividual(size_t index, float radius) {
    m_CornerRadius.Set(index, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

bool ProgressBar::IsCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void ProgressBar::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
}

const core::LinkedProperty4& ProgressBar::GetBorderWidth() const {
    return m_BorderWidth;
}

void ProgressBar::SetBorderWidth(const core::LinkedProperty4& width) {
    m_BorderWidth = width;
    SetPropertyValue<Vec4>("borderWidth", width.AsVec4());
    SetPropertyValue<bool>("borderWidthLinked", width.IsLinked());
}

void ProgressBar::SetBorderWidthAll(float width) {
    m_BorderWidth.SetAll(width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

void ProgressBar::SetBorderWidthIndividual(size_t index, float width) {
    m_BorderWidth.Set(index, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

bool ProgressBar::IsBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void ProgressBar::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue<bool>("borderWidthLinked", linked);
}

void ProgressBar::LoadBackgroundTexture(const std::string& path) {
    if (path.empty()) {
        return;
    }

    m_BackgroundTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_BackgroundTextureAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {
        m_BackgroundTextureAsset.Reset();
    }
}

void ProgressBar::LoadFillTexture(const std::string& path) {
    if (path.empty()) {
        return;
    }

    m_FillTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_FillTextureAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {
        m_FillTextureAsset.Reset();
    }
}

void ProgressBar::LoadBorderTexture(const std::string& path) {
    if (path.empty()) {
        return;
    }

    m_BorderTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_BorderTextureAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {
        m_BorderTextureAsset.Reset();
    }
}

void ProgressBar::LoadFont(const std::string& path) {
    if (path.empty()) {
        return;
    }

    m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());
    bool loaded = m_FontAsset->LoadFromFile(path);

    if (!loaded) {
        m_FontAsset.Reset();
    }
}

float ProgressBar::CalculateFillRatio() const {
    float minVal = GetMinValue();
    float maxVal = GetMaxValue();
    float range = maxVal - minVal;

    if (range <= 0.0f) {
        return 0.0f;
    }

    float normalizedValue = (m_DisplayValue - minVal) / range;
    return std::max(0.0f, std::min(1.0f, normalizedValue));
}

void ProgressBar::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled()) {
        return;
    }

    Node* owner = GetOwner();
    if (!owner) return;

    Node2D* node2D = dynamic_cast<Node2D*>(owner);
    if (!node2D) return;

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    Vec2 globalPos = node2D->GetGlobalPosition();
    Vec2 size = Vec2(GetWidth(), GetHeight());
    float rotation = node2D->GetGlobalRotation();

    Vec2 position = Vec2(globalPos.x - size.x * 0.5f, globalPos.y - size.y * 0.5f);

    std::string bgTexPath = GetBackgroundTexturePath();
    if (bgTexPath != m_CurrentBackgroundTexturePath) {
        if (m_BackgroundTextureHandle.isValid()) {
            ctx.getDevice()->destroyTexture(m_BackgroundTextureHandle);
            m_BackgroundTextureHandle = TextureHandle();
        }
        m_BackgroundTextureAsset.Reset();
        if (!bgTexPath.empty()) {
            LoadBackgroundTexture(bgTexPath);
        }
        m_CurrentBackgroundTexturePath = bgTexPath;
    }

    if (!m_BackgroundTextureHandle.isValid() && m_BackgroundTextureAsset.IsValid() && m_BackgroundTextureAsset->IsLoaded()) {
        TextureDesc desc;
        desc.width = m_BackgroundTextureAsset->GetWidth();
        desc.height = m_BackgroundTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_SRGB;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_BackgroundTextureAsset->GetData();
        m_BackgroundTextureHandle = ctx.getDevice()->createTexture(desc);
    }

    std::string fillTexPath = GetFillTexturePath();
    if (fillTexPath != m_CurrentFillTexturePath) {
        if (m_FillTextureHandle.isValid()) {
            ctx.getDevice()->destroyTexture(m_FillTextureHandle);
            m_FillTextureHandle = TextureHandle();
        }
        m_FillTextureAsset.Reset();
        if (!fillTexPath.empty()) {
            LoadFillTexture(fillTexPath);
        }
        m_CurrentFillTexturePath = fillTexPath;
    }

    if (!m_FillTextureHandle.isValid() && m_FillTextureAsset.IsValid() && m_FillTextureAsset->IsLoaded()) {
        TextureDesc desc;
        desc.width = m_FillTextureAsset->GetWidth();
        desc.height = m_FillTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_SRGB;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_FillTextureAsset->GetData();
        m_FillTextureHandle = ctx.getDevice()->createTexture(desc);
    }

    std::string borderTexPath = GetBorderTexturePath();
    if (borderTexPath != m_CurrentBorderTexturePath) {
        if (m_BorderTextureHandle.isValid()) {
            ctx.getDevice()->destroyTexture(m_BorderTextureHandle);
            m_BorderTextureHandle = TextureHandle();
        }
        m_BorderTextureAsset.Reset();
        if (!borderTexPath.empty()) {
            LoadBorderTexture(borderTexPath);
        }
        m_CurrentBorderTexturePath = borderTexPath;
    }

    if (!m_BorderTextureHandle.isValid() && m_BorderTextureAsset.IsValid() && m_BorderTextureAsset->IsLoaded()) {
        TextureDesc desc;
        desc.width = m_BorderTextureAsset->GetWidth();
        desc.height = m_BorderTextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_SRGB;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_BorderTextureAsset->GetData();
        m_BorderTextureHandle = ctx.getDevice()->createTexture(desc);
    }

    std::string fontPath = GetValueFontPath();
    float fontSize = GetValueFontSize();
    if (fontPath != m_CurrentFontPath || fontSize != m_CurrentFontSize) {
        m_CurrentFontPath = fontPath;
        m_CurrentFontSize = fontSize;

        if (m_FontHandle.isValid()) {
            m_FontHandle = FontHandle();
        }

        if (m_TextMesh.isValid() && ctx.getDevice()) {
            ctx.getDevice()->destroyMesh(m_TextMesh);
            m_TextMesh = MeshHandle();
        }

        if (!fontPath.empty()) {
            LoadFont(fontPath);
        } else {
            m_FontAsset.Reset();
        }
        m_TextMeshNeedsRegeneration = true;
    }

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {
        FontDesc fontDesc;
        fontDesc.fontPath = m_FontAsset->GetPath();
        fontDesc.fontSize = fontSize;
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();

        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_FontHandle = device->createFontAtlas(fontDesc);
        }
        m_TextMeshNeedsRegeneration = true;
    }

    RenderBorder(ctx, position, size, rotation);
    RenderBackground(ctx, position, size, rotation);
    RenderFill(ctx, position, size, rotation);

    if (GetShowValue()) {
        RenderValue(ctx, position, size);
    }
}

void ProgressBar::RenderBackground(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    Color bgColor = GetBackgroundColor();
    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int blendMode = 0;

    if (m_BackgroundTextureHandle.isValid()) {

        Vec2 uvMin = Vec2(0.0f, 0.0f);
        Vec2 uvMax = Vec2(1.0f, 1.0f);

        if (std::abs(rotation) > 0.0001f) {
            Vec2 center = Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
            ctx.drawRoundedRect(center, size, cornerRadius, bgColor, m_BackgroundTextureHandle, rotation, uvMin, uvMax, blendMode);
        } else {
            ctx.drawRoundedRect(position, size, cornerRadius, bgColor, m_BackgroundTextureHandle, uvMin, uvMax, blendMode);
        }
    } else {

        if (std::abs(rotation) > 0.0001f) {
            Vec2 center = Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
            ctx.drawRoundedRect(center, size, cornerRadius, bgColor, rotation, blendMode);
        } else {
            ctx.drawRoundedRect(position, size, cornerRadius, bgColor, blendMode);
        }
    }
}

void ProgressBar::RenderFill(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    float fillRatio = CalculateFillRatio();

    if (fillRatio <= 0.0f) {
        return;
    }

    Color fillColor = GetFillColor();
    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int orientation = GetOrientation();
    int fillDirection = GetFillDirection();
    int blendMode = 0;

    Vec2 fillSize = size;
    Vec2 uvMin = Vec2(0.0f, 0.0f);
    Vec2 uvMax = Vec2(1.0f, 1.0f);

    Vec2 localOffset = Vec2(0.0f, 0.0f);

    if (orientation == 0) {
        fillSize.x *= fillRatio;

        if (fillDirection == 0) {

            localOffset.x = -(size.x - fillSize.x) * 0.5f;
            uvMax.x = fillRatio;
        } else {

            localOffset.x = (size.x - fillSize.x) * 0.5f;
            uvMin.x = 1.0f - fillRatio;
        }
    } else {
        fillSize.y *= fillRatio;

        if (fillDirection == 2) {

            localOffset.y = -(size.y - fillSize.y) * 0.5f;
            uvMax.y = fillRatio;
        } else {

            localOffset.y = (size.y - fillSize.y) * 0.5f;
            uvMin.y = 1.0f - fillRatio;
        }
    }

    Vec2 bgCenter = Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);

    if (m_FillTextureHandle.isValid()) {

        if (std::abs(rotation) > 0.0001f) {

            float cosR = std::cos(rotation);
            float sinR = std::sin(rotation);
            Vec2 rotatedOffset = Vec2(
                localOffset.x * cosR - localOffset.y * sinR,
                localOffset.x * sinR + localOffset.y * cosR
            );
            Vec2 fillCenter = Vec2(bgCenter.x + rotatedOffset.x, bgCenter.y + rotatedOffset.y);
            ctx.drawRoundedRect(fillCenter, fillSize, cornerRadius, fillColor, m_FillTextureHandle, rotation, uvMin, uvMax, blendMode);
        } else {

            Vec2 fillPosition = Vec2(position.x + localOffset.x + (size.x - fillSize.x) * 0.5f,
                                     position.y + localOffset.y + (size.y - fillSize.y) * 0.5f);
            ctx.drawRoundedRect(fillPosition, fillSize, cornerRadius, fillColor, m_FillTextureHandle, uvMin, uvMax, blendMode);
        }
    } else {

        if (std::abs(rotation) > 0.0001f) {

            float cosR = std::cos(rotation);
            float sinR = std::sin(rotation);
            Vec2 rotatedOffset = Vec2(
                localOffset.x * cosR - localOffset.y * sinR,
                localOffset.x * sinR + localOffset.y * cosR
            );
            Vec2 fillCenter = Vec2(bgCenter.x + rotatedOffset.x, bgCenter.y + rotatedOffset.y);
            ctx.drawRoundedRect(fillCenter, fillSize, cornerRadius, fillColor, rotation, blendMode);
        } else {

            Vec2 fillPosition = Vec2(position.x + localOffset.x + (size.x - fillSize.x) * 0.5f,
                                     position.y + localOffset.y + (size.y - fillSize.y) * 0.5f);
            ctx.drawRoundedRect(fillPosition, fillSize, cornerRadius, fillColor, blendMode);
        }
    }
}

void ProgressBar::RenderBorder(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {

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

    if (std::abs(rotation) > 0.0001f) {

        Vec2 center = Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
        ctx.drawRoundedRectBorder(center, outerSize, outerRadius, borderWidthVec, borderColor, rotation);
    } else {
        Vec2 outerPosition = Vec2(position.x - borderLeft, position.y - borderTop);
        ctx.drawRoundedRectBorder(outerPosition, outerSize, outerRadius, borderWidthVec, borderColor);
    }
}

void ProgressBar::RenderValue(RenderContext& ctx, const Vec2& position, const Vec2& size) {
    if (!m_FontHandle.isValid()) {
        return;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << m_DisplayValue;
    std::string valueText = oss.str();

    if (valueText != m_CachedValueText || m_TextMeshNeedsRegeneration) {
        RegenerateTextMesh(ctx, position, size);
        m_CachedValueText = valueText;
    }

    if (!m_TextMesh.isValid()) {
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_FontAtlas", fontAtlas->texture);
    overrides.setColor("u_TextColor", GetValueColor());

    Mat4 transform = Mat4::Identity();
    ctx.drawMesh(m_TextMesh, ctx.getDefaultTextMaterial(), transform, overrides);
}

void ProgressBar::RegenerateTextMesh(RenderContext& ctx, const Vec2& position, const Vec2& size) {

    if (m_TextMesh.isValid() && ctx.getDevice()) {
        ctx.getDevice()->destroyMesh(m_TextMesh);
        m_TextMesh = MeshHandle();
    }

    if (!m_FontHandle.isValid()) {
        m_TextMeshNeedsRegeneration = false;
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        m_TextMeshNeedsRegeneration = false;
        return;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << m_DisplayValue;
    std::string valueText = oss.str();

    if (valueText.empty()) {
        m_TextMeshNeedsRegeneration = false;
        return;
    }

    float fontSize = GetValueFontSize();
    float scale = fontSize / fontAtlas->fontSize;
    Vec2 textSize = fontAtlas->measureText(valueText, scale);

    Vec2 textPosition = Vec2(
        position.x - textSize.x * 0.5f,
        position.y - textSize.y * 0.5f
    );

    MeshData textMeshData;
    Vec2 cursor = textPosition;
    Color color = GetValueColor();

    for (size_t i = 0; i < valueText.length(); ++i) {
        char c = valueText[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        const Glyph* glyph = fontAtlas->getGlyph(codepoint);
        if (!glyph) {
            if (c == ' ') {
                cursor.x += fontAtlas->fontSize * 0.25f * scale;
            }
            continue;
        }

        float glyphX = cursor.x + glyph->bearing.x * scale;
        float glyphY = cursor.y - glyph->bearing.y * scale;
        float glyphW = glyph->size.x * scale;
        float glyphH = glyph->size.y * scale;

        Vertex v0, v1, v2, v3;

        v0.position = Vec3(glyphX, glyphY - glyphH, 0.0f);
        v0.texCoord = Vec2(glyph->uvMin.x, glyph->uvMax.y);
        v0.color = Vec4(color.r, color.g, color.b, color.a);

        v1.position = Vec3(glyphX + glyphW, glyphY - glyphH, 0.0f);
        v1.texCoord = Vec2(glyph->uvMax.x, glyph->uvMax.y);
        v1.color = Vec4(color.r, color.g, color.b, color.a);

        v2.position = Vec3(glyphX + glyphW, glyphY, 0.0f);
        v2.texCoord = Vec2(glyph->uvMax.x, glyph->uvMin.y);
        v2.color = Vec4(color.r, color.g, color.b, color.a);

        v3.position = Vec3(glyphX, glyphY, 0.0f);
        v3.texCoord = Vec2(glyph->uvMin.x, glyph->uvMin.y);
        v3.color = Vec4(color.r, color.g, color.b, color.a);

        uint32_t baseIndex = static_cast<uint32_t>(textMeshData.vertices.size());
        textMeshData.vertices.push_back(v0);
        textMeshData.vertices.push_back(v1);
        textMeshData.vertices.push_back(v2);
        textMeshData.vertices.push_back(v3);

        textMeshData.indices.push_back(baseIndex + 0);
        textMeshData.indices.push_back(baseIndex + 1);
        textMeshData.indices.push_back(baseIndex + 2);

        textMeshData.indices.push_back(baseIndex + 0);
        textMeshData.indices.push_back(baseIndex + 2);
        textMeshData.indices.push_back(baseIndex + 3);

        cursor.x += glyph->advance * scale;
    }

    if (!textMeshData.vertices.empty()) {
        m_TextMesh = ctx.getDevice()->createMesh(textMeshData);
    }

    m_TextMeshNeedsRegeneration = false;
}

AABB ProgressBar::getWorldBounds() const {
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

RenderLayer ProgressBar::getRenderLayer() const {

    return RenderLayer::UI;
}

SpatialType ProgressBar::getSpatialType() const {

    return SpatialType::World2D;
}

}
}

