#include "lupine/components/ProgressBar3D.hpp"
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
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node3D;

ProgressBar3D::ProgressBar3D()
    : Component("ProgressBar3D")
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

ProgressBar3D::ProgressBar3D(const std::string& name)
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

ProgressBar3D::~ProgressBar3D() {

}

void ProgressBar3D::DefineProperties() {

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minValue, 0.0f, -10000.0f, 10000.0f, 0.1f, "Value"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxValue, 100.0f, -10000.0f, 10000.0f, 0.1f, "Value"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(value, 50.0f, -10000.0f, 10000.0f, 0.1f, "Value"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(step, 0.0f, 0.0f, 100.0f, 0.1f, "Value"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(smooth, Bool, false, "Smooth"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(smoothSpeed, 5.0f, 0.1f, 100.0f, 0.1f, "Smooth"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 5.0f, 0.0f, 10000.0f, 0.1f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 0.5f, 0.0f, 10000.0f, 0.1f, "Size"));

    DefineProperty(PROPERTY_ENUM_GROUP(orientation, 0, "Orientation", Horizontal, Vertical));
    DefineProperty(PROPERTY_ENUM_GROUP(fillDirection, 0, "Orientation", LeftToRight, RightToLeft, UpToDown, DownToUp));

    DefineProperty(PROPERTY_ENUM_GROUP(billboardMode, 1, "Transform", Disabled, Enabled, YAxisOnly));

    DefineProperty(PROPERTY_DEFAULT_GROUP(doubleSided, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(castShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Rendering"));

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

void ProgressBar3D::OnAwake() {

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

void ProgressBar3D::OnUpdate(float deltaTime) {
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

bool ProgressBar3D::OnGizmoScale(float scaleDelta, int axis, bool is3D) {

    if (is3D) {
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

float ProgressBar3D::GetMinValue() const {
    return GetPropertyValue<float>("minValue");
}

void ProgressBar3D::SetMinValue(float value) {
    SetPropertyValue<float>("minValue", value);
}

float ProgressBar3D::GetMaxValue() const {
    return GetPropertyValue<float>("maxValue");
}

void ProgressBar3D::SetMaxValue(float value) {
    SetPropertyValue<float>("maxValue", value);
}

float ProgressBar3D::GetValue() const {
    return GetPropertyValue<float>("value");
}

void ProgressBar3D::SetValue(float value) {
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

float ProgressBar3D::GetStep() const {
    return GetPropertyValue<float>("step");
}

void ProgressBar3D::SetStep(float step) {
    SetPropertyValue<float>("step", std::max(0.0f, step));
}

bool ProgressBar3D::GetSmooth() const {
    return GetPropertyValue<bool>("smooth");
}

void ProgressBar3D::SetSmooth(bool smooth) {
    SetPropertyValue<bool>("smooth", smooth);
}

float ProgressBar3D::GetSmoothSpeed() const {
    return GetPropertyValue<float>("smoothSpeed");
}

void ProgressBar3D::SetSmoothSpeed(float speed) {
    SetPropertyValue<float>("smoothSpeed", std::max(0.1f, speed));
}

float ProgressBar3D::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void ProgressBar3D::SetWidth(float width) {
    SetPropertyValue<float>("width", std::max(0.0f, width));
}

float ProgressBar3D::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void ProgressBar3D::SetHeight(float height) {
    SetPropertyValue<float>("height", std::max(0.0f, height));
}

int ProgressBar3D::GetOrientation() const {
    return GetPropertyValue<int>("orientation");
}

void ProgressBar3D::SetOrientation(int orientation) {
    SetPropertyValue<int>("orientation", std::max(0, std::min(1, orientation)));
}

int ProgressBar3D::GetFillDirection() const {
    return GetPropertyValue<int>("fillDirection");
}

void ProgressBar3D::SetFillDirection(int direction) {
    SetPropertyValue<int>("fillDirection", std::max(0, std::min(3, direction)));
}

ProgressBar3DBillboardMode ProgressBar3D::GetBillboardMode() const {
    int mode = GetPropertyValue<int>("billboardMode");
    return static_cast<ProgressBar3DBillboardMode>(mode);
}

void ProgressBar3D::SetBillboardMode(ProgressBar3DBillboardMode mode) {
    SetPropertyValue<int>("billboardMode", static_cast<int>(mode));
}

bool ProgressBar3D::GetDoubleSided() const {
    return GetPropertyValue<bool>("doubleSided");
}

void ProgressBar3D::SetDoubleSided(bool doubleSided) {
    SetPropertyValue<bool>("doubleSided", doubleSided);
}

bool ProgressBar3D::GetCastShadow() const {
    return GetPropertyValue<bool>("castShadow");
}

void ProgressBar3D::SetCastShadow(bool castShadow) {
    SetPropertyValue<bool>("castShadow", castShadow);
}

bool ProgressBar3D::GetReceiveShadow() const {
    return GetPropertyValue<bool>("receiveShadow");
}

void ProgressBar3D::SetReceiveShadow(bool receiveShadow) {
    SetPropertyValue<bool>("receiveShadow", receiveShadow);
}

const std::string& ProgressBar3D::GetBackgroundTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("backgroundTexturePath");
    return cachedPath;
}

void ProgressBar3D::SetBackgroundTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("backgroundTexturePath", path);
}

const Color& ProgressBar3D::GetBackgroundColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("backgroundColor");
    return cachedColor;
}

void ProgressBar3D::SetBackgroundColor(const Color& color) {
    SetPropertyValue<Color>("backgroundColor", color);
}

const std::string& ProgressBar3D::GetFillTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("fillTexturePath");
    return cachedPath;
}

void ProgressBar3D::SetFillTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("fillTexturePath", path);
}

const Color& ProgressBar3D::GetFillColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("fillColor");
    return cachedColor;
}

void ProgressBar3D::SetFillColor(const Color& color) {
    SetPropertyValue<Color>("fillColor", color);
}

const std::string& ProgressBar3D::GetBorderTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("borderTexturePath");
    return cachedPath;
}

void ProgressBar3D::SetBorderTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("borderTexturePath", path);
}

const Color& ProgressBar3D::GetBorderColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("borderColor");
    return cachedColor;
}

void ProgressBar3D::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
}

bool ProgressBar3D::GetShowValue() const {
    return GetPropertyValue<bool>("showValue");
}

void ProgressBar3D::SetShowValue(bool show) {
    SetPropertyValue<bool>("showValue", show);
    m_TextMeshNeedsRegeneration = true;
}

const std::string& ProgressBar3D::GetValueFontPath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("valueFontPath");
    return cachedPath;
}

void ProgressBar3D::SetValueFontPath(const std::string& path) {
    SetPropertyValue<std::string>("valueFontPath", path);
}

float ProgressBar3D::GetValueFontSize() const {
    return GetPropertyValue<float>("valueFontSize");
}

void ProgressBar3D::SetValueFontSize(float size) {
    SetPropertyValue<float>("valueFontSize", std::max(1.0f, size));
    m_TextMeshNeedsRegeneration = true;
}

const Color& ProgressBar3D::GetValueColor() const {
    static Color cachedColor;
    cachedColor = GetPropertyValue<Color>("valueColor");
    return cachedColor;
}

void ProgressBar3D::SetValueColor(const Color& color) {
    SetPropertyValue<Color>("valueColor", color);
}

const core::LinkedProperty4& ProgressBar3D::GetCornerRadius() const {
    return m_CornerRadius;
}

void ProgressBar3D::SetCornerRadius(const core::LinkedProperty4& radius) {
    m_CornerRadius = radius;
    SetPropertyValue<Vec4>("cornerRadius", radius.AsVec4());
    SetPropertyValue<bool>("cornerRadiusLinked", radius.IsLinked());
}

void ProgressBar3D::SetCornerRadiusAll(float radius) {
    m_CornerRadius.SetAll(radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

void ProgressBar3D::SetCornerRadiusIndividual(size_t index, float radius) {
    m_CornerRadius.Set(index, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

bool ProgressBar3D::IsCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void ProgressBar3D::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
}

const core::LinkedProperty4& ProgressBar3D::GetBorderWidth() const {
    return m_BorderWidth;
}

void ProgressBar3D::SetBorderWidth(const core::LinkedProperty4& width) {
    m_BorderWidth = width;
    SetPropertyValue<Vec4>("borderWidth", width.AsVec4());
    SetPropertyValue<bool>("borderWidthLinked", width.IsLinked());
}

void ProgressBar3D::SetBorderWidthAll(float width) {
    m_BorderWidth.SetAll(width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

void ProgressBar3D::SetBorderWidthIndividual(size_t index, float width) {
    m_BorderWidth.Set(index, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
}

bool ProgressBar3D::IsBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void ProgressBar3D::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue<bool>("borderWidthLinked", linked);
}

void ProgressBar3D::LoadBackgroundTexture(const std::string& path) {
    if (path.empty()) {
        return;
    }

    m_BackgroundTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_BackgroundTextureAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {
        m_BackgroundTextureAsset.Reset();
    }
}

void ProgressBar3D::LoadFillTexture(const std::string& path) {
    if (path.empty()) {
        return;
    }

    m_FillTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_FillTextureAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {
        m_FillTextureAsset.Reset();
    }
}

void ProgressBar3D::LoadBorderTexture(const std::string& path) {
    if (path.empty()) {
        return;
    }

    m_BorderTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_BorderTextureAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {
        m_BorderTextureAsset.Reset();
    }
}

void ProgressBar3D::LoadFont(const std::string& path) {
    if (path.empty()) {
        return;
    }

    m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());
    bool loaded = m_FontAsset->LoadFromFile(path);

    if (!loaded) {
        m_FontAsset.Reset();
    }
}

float ProgressBar3D::CalculateFillRatio() const {
    float minVal = GetMinValue();
    float maxVal = GetMaxValue();
    float range = maxVal - minVal;

    if (range <= 0.0f) {
        return 0.0f;
    }

    float normalizedValue = (m_DisplayValue - minVal) / range;
    return std::max(0.0f, std::min(1.0f, normalizedValue));
}

Mat4 ProgressBar3D::CalculateBillboardTransform(const Mat4& viewMatrix) const {
    ProgressBar3DBillboardMode mode = GetBillboardMode();

    if (mode == ProgressBar3DBillboardMode::Disabled) {
        return Mat4::Identity();
    }

    if (!m_Owner) {
        return Mat4::Identity();
    }

    core::Node3D* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (!node3D) {
        return Mat4::Identity();
    }

    Vec3 position = node3D->GetGlobalPosition();
    Vec3 scale = node3D->GetGlobalScale();

    if (mode == ProgressBar3DBillboardMode::Enabled) {

        glm::mat4 glmView = viewMatrix.ToGLM();
        Vec3 right(glmView[0][0], glmView[1][0], glmView[2][0]);
        Vec3 up(glmView[0][1], glmView[1][1], glmView[2][1]);
        Vec3 forward = Cross(right, up).Normalized();

        right = right * scale.x;
        up = up * scale.y;
        forward = forward * scale.z;

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(right.x, right.y, right.z, 0.0f);
        billboard[1] = glm::vec4(up.x, up.y, up.z, 0.0f);
        billboard[2] = glm::vec4(forward.x, forward.y, forward.z, 0.0f);
        billboard[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboard);
    } else if (mode == ProgressBar3DBillboardMode::YAxisOnly) {

        glm::mat4 glmView = viewMatrix.ToGLM();
        glm::mat4 invView = glm::inverse(glmView);
        Vec3 cameraPos(invView[3][0], invView[3][1], invView[3][2]);

        Vec3 toCamera = (cameraPos - position);
        toCamera.y = 0.0f;
        toCamera = toCamera.Normalized();

        Vec3 up(0, 1, 0);
        Vec3 right = Cross(up, toCamera).Normalized();
        Vec3 forward = toCamera;

        right = right * scale.x;
        up = up * scale.y;
        forward = forward * scale.z;

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(right.x, right.y, right.z, 0.0f);
        billboard[1] = glm::vec4(up.x, up.y, up.z, 0.0f);
        billboard[2] = glm::vec4(forward.x, forward.y, forward.z, 0.0f);
        billboard[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboard);
    }

    return Mat4::Identity();
}

void ProgressBar3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled()) {
        return;
    }

    Node* owner = GetOwner();
    if (!owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(owner);
    if (!node3D) return;

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    Vec2 size = Vec2(GetWidth(), GetHeight());

    ProgressBar3DBillboardMode billboardMode = GetBillboardMode();

    Mat4 transform;

    if (billboardMode != ProgressBar3DBillboardMode::Disabled) {

        transform = CalculateBillboardTransform(ctx.getViewMatrix());
    } else {

        transform = node3D->GetGlobalTransformMatrix();
    }

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

    bool doubleSided = GetDoubleSided();

    if (doubleSided) {

        RenderBorder(ctx, transform, size, true);
        RenderBackground(ctx, transform, size, true);
        RenderFill(ctx, transform, size, true);

        RenderFill(ctx, transform, size, false);
        RenderBackground(ctx, transform, size, false);
        RenderBorder(ctx, transform, size, false);
    } else {

        RenderBorder(ctx, transform, size, false);
        RenderBackground(ctx, transform, size, false);
        RenderFill(ctx, transform, size, false);
    }

    if (GetShowValue()) {
        RenderValue(ctx, transform, size);
    }
}

AABB ProgressBar3D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
    }

    Vec3 position = node3D->GetGlobalPosition();
    Vec2 size = Vec2(GetWidth(), GetHeight());
    Vec3 halfExtents = Vec3(size.x * 0.5f, size.y * 0.5f, 0.01f);

    return AABB(position - halfExtents, position + halfExtents);
}

RenderLayer ProgressBar3D::getRenderLayer() const {

    return RenderLayer::Transparent;
}

math::OBB ProgressBar3D::getOrientedBounds() const {
    if (!m_Owner) {
        return math::OBB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return math::OBB();
    }

    Vec3 position = node3D->GetGlobalPosition();
    Quat rotation = node3D->GetGlobalRotation();
    Vec2 size = Vec2(GetWidth(), GetHeight());
    Vec3 halfExtents = Vec3(size.x * 0.5f, size.y * 0.5f, 0.01f);

    return math::OBB(position, halfExtents, rotation);
}

bool ProgressBar3D::IntersectRay(const math::Ray& ray, float& outDistance) const {
    math::OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    math::AABB localAABB;
    localAABB.min = -obb.extents;
    localAABB.max = obb.extents;

    math::Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

void ProgressBar3D::RenderBackground(RenderContext& ctx, const Mat4& transform, const Vec2& size, bool isBackFace) {
    Color bgColor = GetBackgroundColor();
    if (bgColor.a <= 0.0f) return;

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    float zOffset = isBackFace ? -0.001f : 0.001f;

    Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
    Mat4 finalTransform = transform * scale * Mat4::Translate(Vec3(0, 0, zOffset));

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    bool hasRoundedCorners = (cornerRadius.x > 0.0f || cornerRadius.y > 0.0f ||
                              cornerRadius.z > 0.0f || cornerRadius.w > 0.0f);

    const float pixelScale = 20.0f;
    Vec2 scaledSize = size * pixelScale;
    Vec4 scaledCornerRadius = cornerRadius * pixelScale;

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", bgColor);
    overrides.setVec2("u_Size", scaledSize);
    overrides.setVec4("u_CornerRadius", scaledCornerRadius);

    bool useTexture = m_BackgroundTextureHandle.isValid();
    overrides.setBool("u_UseTexture", useTexture);
    if (useTexture) {
        overrides.setTexture("u_Texture", m_BackgroundTextureHandle);
    }

    MaterialHandle material;
    if (hasRoundedCorners) {
        material = ctx.getRoundedRect3DMaterial();
    } else {
        material = GetDoubleSided()
            ? ctx.getDefaultColoredDoubleSidedMaterial()
            : ctx.getDefaultColoredMaterial();
        overrides.setColor("u_TintColor", bgColor);
    }

    ctx.drawMesh(quadMesh, material, finalTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

void ProgressBar3D::RenderFill(RenderContext& ctx, const Mat4& transform, const Vec2& size, bool isBackFace) {
    float fillRatio = CalculateFillRatio();

    if (fillRatio <= 0.0f) {
        return;
    }

    Color fillColor = GetFillColor();
    if (fillColor.a <= 0.0f) return;

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    Vec2 fillSize = size;
    Vec3 fillOffset(0, 0, 0);

    int orientation = GetOrientation();
    int fillDirection = GetFillDirection();

    if (orientation == 0) {
        fillSize.x = size.x * fillRatio;

        if (fillDirection == 0) {
            fillOffset.x = -(size.x - fillSize.x) * 0.5f;
        } else if (fillDirection == 1) {
            fillOffset.x = (size.x - fillSize.x) * 0.5f;
        }
    } else {
        fillSize.y = size.y * fillRatio;

        if (fillDirection == 2) {
            fillOffset.y = (size.y - fillSize.y) * 0.5f;
        } else if (fillDirection == 3) {
            fillOffset.y = -(size.y - fillSize.y) * 0.5f;
        }
    }

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    bool hasRoundedCorners = (cornerRadius.x > 0.0f || cornerRadius.y > 0.0f ||
                              cornerRadius.z > 0.0f || cornerRadius.w > 0.0f);

    const float pixelScale = 20.0f;
    Vec2 scaledSize = fillSize * pixelScale;
    Vec4 scaledCornerRadius = cornerRadius * pixelScale;

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", fillColor);
    overrides.setVec2("u_Size", scaledSize);
    overrides.setVec4("u_CornerRadius", scaledCornerRadius);

    bool useTexture = m_FillTextureHandle.isValid();
    overrides.setBool("u_UseTexture", useTexture);
    if (useTexture) {
        overrides.setTexture("u_Texture", m_FillTextureHandle);
    }

    float zOffset = isBackFace ? -0.002f : 0.002f;

    Mat4 fillTransform = transform
        * Mat4::Translate(fillOffset)
        * Mat4::Scale(Vec3(fillSize.x, fillSize.y, 1.0f))
        * Mat4::Translate(Vec3(0, 0, zOffset));

    MaterialHandle material;
    if (hasRoundedCorners) {
        material = ctx.getRoundedRect3DMaterial();
    } else {
        material = GetDoubleSided()
            ? ctx.getDefaultColoredDoubleSidedMaterial()
            : ctx.getDefaultColoredMaterial();
        overrides.setColor("u_TintColor", fillColor);
    }

    ctx.drawMesh(quadMesh, material, fillTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

void ProgressBar3D::RenderBorder(RenderContext& ctx, const Mat4& transform, const Vec2& size, bool isBackFace) {

    Vec4 borderWidthVec = m_BorderWidth.AsVec4();
    if (borderWidthVec.x <= 0.0f && borderWidthVec.y <= 0.0f &&
        borderWidthVec.z <= 0.0f && borderWidthVec.w <= 0.0f) {
        return;
    }

    Color borderColor = GetBorderColor();
    if (borderColor.a <= 0.0f) return;

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    Vec4 cornerRadius = m_CornerRadius.AsVec4();

    float borderLeft = borderWidthVec.w;
    float borderRight = borderWidthVec.y;
    float borderTop = borderWidthVec.x;
    float borderBottom = borderWidthVec.z;

    Vec2 outerSize(
        size.x + borderLeft + borderRight,
        size.y + borderTop + borderBottom
    );

    const float pixelScale = 20.0f;
    Vec2 scaledOuterSize = outerSize * pixelScale;

    Vec4 outerCornerRadius = Vec4(
        cornerRadius.x + (borderTop + borderLeft) * 0.5f,
        cornerRadius.y + (borderTop + borderRight) * 0.5f,
        cornerRadius.z + (borderBottom + borderRight) * 0.5f,
        cornerRadius.w + (borderBottom + borderLeft) * 0.5f
    );

    Vec4 scaledOuterCornerRadius = outerCornerRadius * pixelScale;
    Vec4 scaledBorderWidth = borderWidthVec * pixelScale;

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", borderColor);
    overrides.setVec2("u_Size", scaledOuterSize);
    overrides.setVec4("u_CornerRadius", scaledOuterCornerRadius);
    overrides.setVec4("u_BorderWidth", scaledBorderWidth);
    overrides.setBool("u_EnableAntialiasing", false);

    bool useTexture = m_BorderTextureHandle.isValid();
    overrides.setBool("u_UseTexture", useTexture);
    if (useTexture) {
        overrides.setTexture("u_Texture", m_BorderTextureHandle);
    }

    float zOffset = isBackFace ? 0.0f : 0.0f;

    Mat4 borderTransform = transform
        * Mat4::Scale(Vec3(outerSize.x, outerSize.y, 1.0f))
        * Mat4::Translate(Vec3(0, 0, zOffset));

    MaterialHandle material = ctx.getRoundedRect3DBorderMaterial();

    ctx.drawMesh(quadMesh, material, borderTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

void ProgressBar3D::RenderValue(RenderContext& ctx, const Mat4& transform, const Vec2& size) {
    if (!GetShowValue()) {
        return;
    }

    if (m_TextMeshNeedsRegeneration) {
        RegenerateTextMesh(ctx, size);
        m_TextMeshNeedsRegeneration = false;
    }

    if (!m_TextMesh.isValid() || !m_FontHandle.isValid()) {
        return;
    }

    Color valueColor = GetValueColor();
    if (valueColor.a <= 0.0f) return;

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    MaterialHandle textMaterial = ctx.getDefaultText3DMaterial();
    if (!textMaterial.isValid()) {
        return;
    }

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_FontAtlas", fontAtlas->texture);
    overrides.setColor("u_TextColor", valueColor);
    overrides.setColor("u_Color", valueColor);

    Mat4 textTransform = transform * Mat4::Translate(Vec3(0, 0, 0.003f));

    ctx.drawMesh(m_TextMesh, textMaterial, textTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

void ProgressBar3D::RegenerateTextMesh(RenderContext& ctx, const Vec2& size) {

    if (m_TextMesh.isValid() && ctx.getDevice()) {
        ctx.getDevice()->destroyMesh(m_TextMesh);
        m_TextMesh = MeshHandle();
    }

    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        return;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << m_DisplayValue;
    std::string valueText = oss.str();

    if (valueText == m_CachedValueText && m_TextMesh.isValid()) {
        return;
    }

    m_CachedValueText = valueText;

    float fontSize = GetValueFontSize();

}

}
}

