#include "lupine/components/Sprite2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Sprite2D::Sprite2D()
    : Component("Sprite2D")
    , m_TextureHandle()
    , m_TextureNeedsUpload(false)
{
}

Sprite2D::Sprite2D(const std::string& name)
    : Component(name)
    , m_TextureHandle()
    , m_TextureNeedsUpload(false)
{
}

Sprite2D::~Sprite2D() {

}

void Sprite2D::DefineProperties() {

    DefineProperty(PROPERTY_FILE_GROUP(texturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uvRect, Vec4, Vec4(0.0f, 0.0f, 1.0f, 1.0f), "Texture"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec2, Vec2(0.0f, 0.0f), "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(centered, Bool, true, "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, Vec2(0.0f, 0.0f), "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Transform"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(alphaCutoff, 0.0f, 0.0f, 1.0f, 0.01f, "Rendering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(spriteSheetEnabled, Bool, false, "Sprite Sheet"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(spriteSize, Vec2, Vec2(0.0f, 0.0f), "Sprite Sheet"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(currentFrame, 0, 0, 10000, 1, "Sprite Sheet"));
}

void Sprite2D::OnAwake() {

    std::string texPath = GetTexturePath();
    if (!texPath.empty()) {
        LoadTexture(texPath);
    }
}

void Sprite2D::OnReady() {

    if (m_TextureNeedsUpload) {
        UploadTextureToGPU();
    }
}

void Sprite2D::OnRender() {

}

bool Sprite2D::LoadTexture(const std::string& filepath) {
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

void Sprite2D::SetTexture(const asset::AssetRef<asset::ImageAsset>& texture) {
    m_TextureAsset = texture;
    m_TextureNeedsUpload = true;

    if (texture.IsValid()) {
        SetTexturePath(texture->GetPath());
    }
}

void Sprite2D::UploadTextureToGPU() {
    if (!m_TextureAsset.IsValid() || !m_TextureAsset->IsLoaded()) {
        m_TextureNeedsUpload = false;
        return;
    }

    if (!m_Owner) {
        return;
    }

    m_TextureNeedsUpload = false;
}

Vec2 Sprite2D::CalculateRenderSize() const {
    Vec2 size = GetSize();

    if (size.x == 0.0f && size.y == 0.0f) {
        if (m_TextureAsset.IsValid()) {
            size.x = static_cast<float>(m_TextureAsset->GetWidth());
            size.y = static_cast<float>(m_TextureAsset->GetHeight());
        } else {
            size = Vec2(100.0f, 100.0f);
        }
    }

    return size;
}

bool Sprite2D::GetSpriteSheetEnabled() const {
    return GetPropertyValue<bool>("spriteSheetEnabled");
}

void Sprite2D::SetSpriteSheetEnabled(bool enabled) {
    SetPropertyValue<bool>("spriteSheetEnabled", enabled);
}

const Vec2& Sprite2D::GetSpriteSize() const {
    static Vec2 cachedSize;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("spriteSize");
    if (prop) {
        cachedSize = prop->GetValue<Vec2>();
        return cachedSize;
    }
    static Vec2 defaultSize(0.0f, 0.0f);
    return defaultSize;
}

void Sprite2D::SetSpriteSize(const Vec2& size) {
    SetPropertyValue<Vec2>("spriteSize", size);
}

int Sprite2D::GetCurrentFrame() const {
    return GetPropertyValue<int>("currentFrame");
}

void Sprite2D::SetCurrentFrame(int frame) {

    int frameCount = GetFrameCount();
    if (frameCount > 0) {
        frame = std::max(0, std::min(frame, frameCount - 1));
    } else {
        frame = 0;
    }
    SetPropertyValue<int>("currentFrame", frame);
}

int Sprite2D::GetFramesPerRow() const {
    if (!m_TextureAsset.IsValid() || !GetSpriteSheetEnabled()) {
        return 0;
    }

    Vec2 spriteSize = GetSpriteSize();
    if (spriteSize.x <= 0.0f) {
        return 0;
    }

    int textureWidth = m_TextureAsset->GetWidth();
    return static_cast<int>(textureWidth / spriteSize.x);
}

int Sprite2D::GetFramesPerColumn() const {
    if (!m_TextureAsset.IsValid() || !GetSpriteSheetEnabled()) {
        return 0;
    }

    Vec2 spriteSize = GetSpriteSize();
    if (spriteSize.y <= 0.0f) {
        return 0;
    }

    int textureHeight = m_TextureAsset->GetHeight();
    return static_cast<int>(textureHeight / spriteSize.y);
}

int Sprite2D::GetFrameCount() const {
    int framesPerRow = GetFramesPerRow();
    int framesPerColumn = GetFramesPerColumn();

    if (framesPerRow <= 0 || framesPerColumn <= 0) {
        return 0;
    }

    return framesPerRow * framesPerColumn;
}

Vec4 Sprite2D::CalculateFrameUVRect(int frameIndex) const {
    if (!m_TextureAsset.IsValid() || !GetSpriteSheetEnabled()) {
        return Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    Vec2 spriteSize = GetSpriteSize();
    if (spriteSize.x <= 0.0f || spriteSize.y <= 0.0f) {
        return Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    int framesPerRow = GetFramesPerRow();
    if (framesPerRow <= 0) {
        return Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    int column = frameIndex % framesPerRow;
    int row = frameIndex / framesPerRow;

    float textureWidth = static_cast<float>(m_TextureAsset->GetWidth());
    float textureHeight = static_cast<float>(m_TextureAsset->GetHeight());

    float uMin = (column * spriteSize.x) / textureWidth;
    float vMin = (row * spriteSize.y) / textureHeight;
    float uMax = ((column + 1) * spriteSize.x) / textureWidth;
    float vMax = ((row + 1) * spriteSize.y) / textureHeight;

    return Vec4(uMin, vMin, uMax, vMax);
}

void Sprite2D::CalculateUVCoordinates(Vec2& uvMin, Vec2& uvMax) const {
    Vec4 uvRect;

    if (GetSpriteSheetEnabled()) {

        int currentFrame = GetCurrentFrame();
        uvRect = CalculateFrameUVRect(currentFrame);
    } else {

        uvRect = GetUVRect();
    }

    uvMin = Vec2(uvRect.x, uvRect.y);
    uvMax = Vec2(uvRect.z, uvRect.w);

    if (GetFlipH()) {
        std::swap(uvMin.x, uvMax.x);
    }

    if (GetFlipV()) {
        std::swap(uvMin.y, uvMax.y);
    }
}

const std::string& Sprite2D::GetTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("texturePath");
    return cachedPath;
}

void Sprite2D::SetTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("texturePath", path);
}

const Color& Sprite2D::GetModulate() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("modulate");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    static Color defaultColor = Color::White();
    return defaultColor;
}

void Sprite2D::SetModulate(const Color& color) {
    SetPropertyValue<Color>("modulate", color);
}

const Vec2& Sprite2D::GetSize() const {
    static Vec2 cachedSize;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("size");
    if (prop) {
        cachedSize = prop->GetValue<Vec2>();
        return cachedSize;
    }
    static Vec2 defaultSize(0.0f, 0.0f);
    return defaultSize;
}

void Sprite2D::SetSize(const Vec2& size) {
    SetPropertyValue<Vec2>("size", size);
}

const Vec4& Sprite2D::GetUVRect() const {
    static Vec4 cachedUV;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("uvRect");
    if (prop) {
        cachedUV = prop->GetValue<Vec4>();
        return cachedUV;
    }
    static Vec4 defaultUV(0.0f, 0.0f, 1.0f, 1.0f);
    return defaultUV;
}

void Sprite2D::SetUVRect(const Vec4& uvRect) {
    SetPropertyValue<Vec4>("uvRect", uvRect);
}

float Sprite2D::GetAlphaCutoff() const {
    return GetPropertyValue<float>("alphaCutoff");
}

void Sprite2D::SetAlphaCutoff(float cutoff) {
    SetPropertyValue<float>("alphaCutoff", cutoff);
}

bool Sprite2D::GetFlipH() const {
    return GetPropertyValue<bool>("flipH");
}

void Sprite2D::SetFlipH(bool flip) {
    SetPropertyValue<bool>("flipH", flip);
}

bool Sprite2D::GetFlipV() const {
    return GetPropertyValue<bool>("flipV");
}

void Sprite2D::SetFlipV(bool flip) {
    SetPropertyValue<bool>("flipV", flip);
}

bool Sprite2D::GetCentered() const {
    return GetPropertyValue<bool>("centered");
}

void Sprite2D::SetCentered(bool centered) {
    SetPropertyValue<bool>("centered", centered);
}

const Vec2& Sprite2D::GetOffset() const {
    static Vec2 cachedOffset;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("offset");
    if (prop) {
        cachedOffset = prop->GetValue<Vec2>();
        return cachedOffset;
    }
    static Vec2 defaultOffset(0.0f, 0.0f);
    return defaultOffset;
}

void Sprite2D::SetOffset(const Vec2& offset) {
    SetPropertyValue<Vec2>("offset", offset);
}

void Sprite2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

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

        if (m_TextureAsset->GetWidth() == 0 || m_TextureAsset->GetHeight() == 0 || m_TextureAsset->GetData() == nullptr) {

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

    if (!m_TextureHandle.isValid()) {
        return;
    }

    SpriteDrawData sprite;
    sprite.texture = m_TextureHandle;
    sprite.position = node2D->GetGlobalPosition();
    sprite.size = CalculateRenderSize();
    sprite.rotation = node2D->GetGlobalRotation();
    sprite.tint = GetModulate();

    CalculateUVCoordinates(sprite.uvMin, sprite.uvMax);

    if (GetCentered()) {
        sprite.pivot = Vec2(0.5f, 0.5f);
    } else {
        Vec2 offset = GetOffset();
        sprite.pivot = Vec2(0.5f + offset.x / sprite.size.x, 0.5f + offset.y / sprite.size.y);
    }

    ctx.drawSprite(sprite);
}

AABB Sprite2D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    Vec2 size = CalculateRenderSize();
    Vec2 globalPos = node2D->GetGlobalPosition();
    Vec2 scale = node2D->GetGlobalScale();
    float rotation = node2D->GetGlobalRotation();

    size.x *= scale.x;
    size.y *= scale.y;

    Vec2 pivot = GetCentered() ? Vec2(0.5f, 0.5f) : Vec2(0.5f, 0.5f);
    Vec2 halfSize = size * 0.5f;

    if (std::abs(rotation) < 0.0001f) {
        Vec2 min = globalPos - (size * pivot);
        Vec2 max = globalPos + (size * (Vec2(1.0f, 1.0f) - pivot));
        return AABB(
            Vec3(min.x, min.y, -0.1f),
            Vec3(max.x, max.y, 0.1f)
        );
    }

    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);

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

    return AABB(
        Vec3(min.x, min.y, -0.1f),
        Vec3(max.x, max.y, 0.1f)
    );
}

bool Sprite2D::OnGizmoScale(float scaleDelta, int axis, bool is3D) {

    if (!is3D) {

        Vec2 currentSize = GetSize();

        if (currentSize.x <= 0.0f || currentSize.y <= 0.0f) {
            currentSize = CalculateRenderSize();
        }

        if (axis == 0) {

            currentSize.x = std::max(0.1f, currentSize.x + scaleDelta * currentSize.x);
        } else if (axis == 1) {

            currentSize.y = std::max(0.1f, currentSize.y + scaleDelta * currentSize.y);
        } else if (axis == -1) {

            currentSize.x = std::max(0.1f, currentSize.x + scaleDelta * currentSize.x);
            currentSize.y = std::max(0.1f, currentSize.y + scaleDelta * currentSize.y);
        }

        SetSize(currentSize);

        return true;
    }

    return false;
}

RenderLayer Sprite2D::getRenderLayer() const {

    float alphaCutoff = GetAlphaCutoff();
    if (alphaCutoff > 0.0f) {
        return RenderLayer::Opaque;
    }
    return RenderLayer::Transparent;
}

}
}

