#include "lupine/components/Image2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Image2D::Image2D()
    : Component("Image2D")
    , m_TextureHandle()
    , m_TextureNeedsUpload(false)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

Image2D::Image2D(const std::string& name)
    : Component(name)
    , m_TextureHandle()
    , m_TextureNeedsUpload(false)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
{
}

Image2D::~Image2D() {

}

void Image2D::DefineProperties() {

    DefineProperty(PROPERTY_FILE_GROUP(texturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Texture"));
    DefineProperty(PROPERTY_FILE_GROUP(materialOverride, std::string(""), "*.mat,*.material", "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color::White(), "Texture"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(anchorMin, Vec2, Vec2(0.0f, 0.0f), "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(anchorMax, Vec2, Vec2(1.0f, 1.0f), "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offsetMin, Vec2, Vec2(0.0f, 0.0f), "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offsetMax, Vec2, Vec2(0.0f, 0.0f), "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uiSpace, Bool, true, "Layout"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(preserveAspect, Bool, true, "Appearance"));
    DefineProperty(PROPERTY_ENUM_GROUP(aspectMode, 0, "Appearance", Fit, Letterbox, Fill, Stretch));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Appearance"));
    DefineProperty(PROPERTY_ENUM_GROUP(blendMode, 0, "Appearance", Alpha, Additive, Multiply, Opaque, Overlay));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, false, "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color::Black(), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(1.0f, 1.0f, 1.0f, 1.0f), "Style"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Style"));

    DefineProperty(PROPERTY_ENUM_GROUP(mouseBehaviour, 0, "Input", Ignore, PropagateUp, Stop));
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
    if (!is3D) {

        Vec2 offsetMin = GetOffsetMin();
        Vec2 offsetMax = GetOffsetMax();

        Vec2 currentSize = offsetMax - offsetMin;

        if (currentSize.x <= 0.0f || currentSize.y <= 0.0f) {
            currentSize = CalculateSize();

            offsetMin = Vec2(0.0f, 0.0f);
            offsetMax = currentSize;
        }

        if (axis == 0) {

            currentSize.x = std::max(0.1f, currentSize.x + scaleDelta * currentSize.x);
        } else if (axis == 1) {

            currentSize.y = std::max(0.1f, currentSize.y + scaleDelta * currentSize.y);
        } else if (axis == -1) {

            currentSize.x = std::max(0.1f, currentSize.x + scaleDelta * currentSize.x);
            currentSize.y = std::max(0.1f, currentSize.y + scaleDelta * currentSize.y);
        }

        SetOffsetMax(offsetMin + currentSize);

        return true;
    }
    return false;
}

bool Image2D::LoadTexture(const std::string& filepath) {
    if (filepath.empty()) {

        return false;
    }

    m_TextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());

    bool loaded = m_TextureAsset->LoadFromFile(filepath, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {

        m_TextureAsset.Reset();
        return false;
    }

    m_TextureNeedsUpload = true;

    SetTexturePath(filepath);

    return true;
}

void Image2D::SetTexture(const asset::AssetRef<asset::ImageAsset>& texture) {
    m_TextureAsset = texture;
    m_TextureNeedsUpload = true;

    if (texture.IsValid()) {
        SetTexturePath(texture->GetPath());
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
    SetPropertyValue<std::string>("texturePath", path);
}

const std::string& Image2D::GetMaterialOverride() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("materialOverride");
    return cachedPath;
}

void Image2D::SetMaterialOverride(const std::string& materialPath) {
    SetPropertyValue<std::string>("materialOverride", materialPath);
}

const Color& Image2D::GetColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("color");
    if (prop) {
        return prop->GetValue<Color>();
    }
    static Color defaultColor = Color::White();
    return defaultColor;
}

void Image2D::SetColor(const Color& color) {
    SetPropertyValue<Color>("color", color);
}

const Vec2& Image2D::GetAnchorMin() const {
    static Vec2 cachedAnchor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("anchorMin");
    if (prop) {
        cachedAnchor = prop->GetValue<Vec2>();
        return cachedAnchor;
    }
    static Vec2 defaultAnchor(0.0f, 0.0f);
    return defaultAnchor;
}

void Image2D::SetAnchorMin(const Vec2& anchor) {
    SetPropertyValue<Vec2>("anchorMin", anchor);
}

const Vec2& Image2D::GetAnchorMax() const {
    static Vec2 cachedAnchor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("anchorMax");
    if (prop) {
        cachedAnchor = prop->GetValue<Vec2>();
        return cachedAnchor;
    }
    static Vec2 defaultAnchor(1.0f, 1.0f);
    return defaultAnchor;
}

void Image2D::SetAnchorMax(const Vec2& anchor) {
    SetPropertyValue<Vec2>("anchorMax", anchor);
}

const Vec2& Image2D::GetOffsetMin() const {
    static Vec2 cachedOffset;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("offsetMin");
    if (prop) {
        cachedOffset = prop->GetValue<Vec2>();
        return cachedOffset;
    }
    static Vec2 defaultOffset(0.0f, 0.0f);
    return defaultOffset;
}

void Image2D::SetOffsetMin(const Vec2& offset) {
    SetPropertyValue<Vec2>("offsetMin", offset);
}

const Vec2& Image2D::GetOffsetMax() const {
    static Vec2 cachedOffset;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("offsetMax");
    if (prop) {
        cachedOffset = prop->GetValue<Vec2>();
        return cachedOffset;
    }
    static Vec2 defaultOffset(0.0f, 0.0f);
    return defaultOffset;
}

void Image2D::SetOffsetMax(const Vec2& offset) {
    SetPropertyValue<Vec2>("offsetMax", offset);
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
    SetPropertyValue<Vec4>("cornerRadius", radius.AsVec4());
    SetPropertyValue<bool>("cornerRadiusLinked", radius.IsLinked());
}

void Image2D::SetCornerRadiusAll(float radius) {
    m_CornerRadius.SetAll(radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

void Image2D::SetCornerRadiusIndividual(size_t index, float radius) {
    m_CornerRadius.Set(index, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

bool Image2D::IsCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void Image2D::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
}

bool Image2D::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void Image2D::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
}

const Color& Image2D::GetBorderColor() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("borderColor");
    if (prop) {
        return prop->GetValue<Color>();
    }
    static Color defaultColor = Color::Black();
    return defaultColor;
}

void Image2D::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
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

bool Image2D::GetUISpace() const {
    return GetPropertyValue<bool>("uiSpace");
}

void Image2D::SetUISpace(bool uiSpace) {
    SetPropertyValue<bool>("uiSpace", uiSpace);
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

    Vec2 anchorMin = GetAnchorMin();
    Vec2 anchorMax = GetAnchorMax();
    Vec2 offsetMin = GetOffsetMin();
    Vec2 offsetMax = GetOffsetMax();

    if (m_TextureAsset.IsValid()) {
        float width = static_cast<float>(m_TextureAsset->GetWidth());
        float height = static_cast<float>(m_TextureAsset->GetHeight());
        return Vec2(width, height);
    }

    return Vec2(100.0f, 100.0f);
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

void Image2D::ApplyAspectRatio(Vec2& position, Vec2& size) const {
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

            if (texAspect > rectAspect) {

                float newHeight = size.x / texAspect;
                float offset = (size.y - newHeight) * 0.5f;
                position.y += offset;
                size.y = newHeight;
            } else {

                float newWidth = size.y * texAspect;
                float offset = (size.x - newWidth) * 0.5f;
                position.x += offset;
                size.x = newWidth;
            }
            break;

        case AspectMode::Fill:

            if (texAspect > rectAspect) {

                float newWidth = size.y * texAspect;
                float offset = (size.x - newWidth) * 0.5f;
                position.x += offset;
                size.x = newWidth;
            } else {

                float newHeight = size.x / texAspect;
                float offset = (size.y - newHeight) * 0.5f;
                position.y += offset;
                size.y = newHeight;
            }
            break;

        case AspectMode::Stretch:

            break;
    }
}

void Image2D::RenderImage(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    if (!m_TextureHandle.isValid()) {
        return;
    }

    Color tint = GetColor();

    Vec2 uvMin, uvMax;
    CalculateUVCoordinates(uvMin, uvMax);

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int blendMode = static_cast<int>(GetBlendMode());

    if (std::abs(rotation) > 0.0001f) {

        Vec2 center = Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
        ctx.drawRoundedRect(center, size, cornerRadius, tint, m_TextureHandle, rotation, uvMin, uvMax, blendMode);
    } else {
        ctx.drawRoundedRect(position, size, cornerRadius, tint, m_TextureHandle, uvMin, uvMax, blendMode);
    }
}

void Image2D::RenderBorder(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
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
            m_TextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
            bool loaded = m_TextureAsset->LoadFromFile(currentPath, true, asset::ImageColorSpace::sRGB);

            if (!loaded) {

                m_TextureAsset.Reset();
            } else {

            }
        }

        m_CurrentTexturePath = currentPath;
    }

    if (!m_TextureHandle.isValid() && m_TextureAsset.IsValid() && m_TextureAsset->IsLoaded()) {
        if (m_TextureAsset->GetWidth() == 0 || m_TextureAsset->GetHeight() == 0 ||
            m_TextureAsset->GetData() == nullptr) {

            return;
        }

        TextureDesc desc;
        desc.width = m_TextureAsset->GetWidth();
        desc.height = m_TextureAsset->GetHeight();
        desc.format = TextureFormat::RGBA8_SRGB;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_TextureAsset->GetData();

        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_TextureHandle = device->createTexture(desc);

        }
    }

    Vec2 position = CalculatePosition();
    Vec2 size = CalculateSize();

    ApplyAspectRatio(position, size);

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    float rotation = 0.0f;
    if (node2D) {
        rotation = node2D->GetGlobalRotation();
    }

    RenderBorder(ctx, position, size, rotation);

    RenderImage(ctx, position, size, rotation);
}

AABB Image2D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Vec2 position = CalculatePosition();
    Vec2 size = CalculateSize();

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

    return SpatialType::World2D;
}

}
}
