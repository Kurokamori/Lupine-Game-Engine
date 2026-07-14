#include "lupine/components/Sprite3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <unordered_map>
#include <mutex>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

// Static texture cache to share textures across all Sprite3D instances
// Use lazy initialization to avoid static initialization order issues
static std::unordered_map<std::string, TextureHandle>& GetSprite3DTextureCache() {
    static std::unordered_map<std::string, TextureHandle> s_Sprite3DTextureCache;
    return s_Sprite3DTextureCache;
}

static std::mutex& GetSprite3DTextureCacheMutex() {
    static std::mutex s_Sprite3DTextureCacheMutex;
    return s_Sprite3DTextureCacheMutex;
}

// Lazy registration with TextureCache system for hot-reloading
static void EnsureSprite3DCacheRegistered() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    rendering::TextureCache::RegisterCache(
        "Sprite3D",
        [](const std::string& path) -> bool {
            std::lock_guard<std::mutex> lock(GetSprite3DTextureCacheMutex());
            auto it = GetSprite3DTextureCache().find(path);
            if (it != GetSprite3DTextureCache().end()) {
                GetSprite3DTextureCache().erase(it);
                return true;
            }
            return false;
        },
        []() {
            std::lock_guard<std::mutex> lock(GetSprite3DTextureCacheMutex());
            GetSprite3DTextureCache().clear();
        }
    );
}

// Helper to get or create a cached texture for Sprite3D
static TextureHandle GetOrCreateSprite3DTexture(
    IGfxDevice* device,
    const std::string& path,
    asset::AssetRef<asset::ImageAsset>& asset)
{
    if (path.empty() || !device) {
        return TextureHandle();
    }

    // Ensure cache is registered with TextureCache system (lazy init)
    EnsureSprite3DCacheRegistered();

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(GetSprite3DTextureCacheMutex());
        auto it = GetSprite3DTextureCache().find(path);
        if (it != GetSprite3DTextureCache().end() && it->second.isValid()) {
            return it->second;
        }
    }

    // Not in cache, need to load
    if (!asset.IsValid()) {
        asset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
        
        if (!asset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB)) {
            LOG_ERROR(LogCategory::Render, "Sprite3D FAILED to load texture: {}", path);
            return TextureHandle();
        }
        
    }

    if (!asset->IsLoaded() || asset->GetWidth() == 0 || asset->GetHeight() == 0 || asset->GetData() == nullptr) {
        return TextureHandle();
    }

    // Create texture
    TextureHandle handle = lupine::CreateTexture2DFromImage(device, *asset, TextureFormat::RGBA8_SRGB);

    // Cache it
    {
        std::lock_guard<std::mutex> lock(GetSprite3DTextureCacheMutex());
        GetSprite3DTextureCache()[path] = handle;
    }

    return handle;
}

Sprite3D::Sprite3D()
    : Component("Sprite3D")
    , m_TextureHandle()
    , m_TextureNeedsUpload(false)
{
}

Sprite3D::Sprite3D(const std::string& name)
    : Component(name)
    , m_TextureHandle()
    , m_TextureNeedsUpload(false)
{
}

Sprite3D::~Sprite3D() {

}

void Sprite3D::DefineProperties() {

    DefineProperty(PROPERTY_FILE_GROUP(texturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uvRect, Vec4, Vec4(0.0f, 0.0f, 1.0f, 1.0f), "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Texture"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Texture"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec2, Vec2(1.0f, 1.0f), "Transform"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pixelSize, 0.0f, 0.0f, 1000.0f, 0.1f, "Transform"));
    DefineProperty(PROPERTY_ENUM_GROUP(billboardMode, 1, "Transform", Disabled, Enabled, YAxisOnly));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(alphaCutoff, 0.0f, 0.0f, 1.0f, 0.01f, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(doubleSided, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(castShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Rendering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(spriteSheetEnabled, Bool, false, "Sprite Sheet"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(spriteSize, Vec2, Vec2(0.0f, 0.0f), "Sprite Sheet"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(currentFrame, 0, 0, 10000, 1, "Sprite Sheet"));
}

void Sprite3D::OnAwake() {

    std::string texPath = GetTexturePath();
    if (!texPath.empty()) {
        LoadTexture(texPath);
    }
}

void Sprite3D::OnReady() {

    if (m_TextureNeedsUpload) {
        UploadTextureToGPU();
    }
}

void Sprite3D::OnRender() {

}

bool Sprite3D::LoadTexture(const std::string& filepath) {
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

void Sprite3D::SetTexture(const asset::AssetRef<asset::ImageAsset>& texture) {
    m_TextureAsset = texture;
    m_TextureNeedsUpload = true;

    if (texture.IsValid()) {
        SetTexturePath(texture->GetPath());
    }
}

void Sprite3D::UploadTextureToGPU() {
    if (!m_TextureAsset.IsValid() || !m_TextureAsset->IsLoaded()) {
        m_TextureNeedsUpload = false;
        return;
    }

    if (!m_Owner) {
        return;
    }

    m_TextureNeedsUpload = false;
}

Vec2 Sprite3D::CalculateRenderSize() const {
    Vec2 size = GetSize();
    float pixelSize = GetPixelSize();

    if (pixelSize > 0.0f) {
        if (m_TextureAsset.IsValid()) {
            float aspect = static_cast<float>(m_TextureAsset->GetWidth()) /
                          static_cast<float>(m_TextureAsset->GetHeight());
            size.x = pixelSize * aspect;
            size.y = pixelSize;
        }
    }

    return size;
}

bool Sprite3D::GetSpriteSheetEnabled() const {
    return GetPropertyValue<bool>("spriteSheetEnabled");
}

void Sprite3D::SetSpriteSheetEnabled(bool enabled) {
    SetPropertyValue<bool>("spriteSheetEnabled", enabled);
}

const Vec2& Sprite3D::GetSpriteSize() const {
    static Vec2 cachedSize;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("spriteSize");
    if (prop) {
        cachedSize = prop->GetValue<Vec2>();
        return cachedSize;
    }
    static Vec2 defaultSize(0.0f, 0.0f);
    return defaultSize;
}

void Sprite3D::SetSpriteSize(const Vec2& size) {
    SetPropertyValue<Vec2>("spriteSize", size);
}

int Sprite3D::GetCurrentFrame() const {
    return GetPropertyValue<int>("currentFrame");
}

void Sprite3D::SetCurrentFrame(int frame) {

    int frameCount = GetFrameCount();
    if (frameCount > 0) {
        frame = std::max(0, std::min(frame, frameCount - 1));
    } else {
        frame = 0;
    }
    SetPropertyValue<int>("currentFrame", frame);
}

int Sprite3D::GetFramesPerRow() const {
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

int Sprite3D::GetFramesPerColumn() const {
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

int Sprite3D::GetFrameCount() const {
    int framesPerRow = GetFramesPerRow();
    int framesPerColumn = GetFramesPerColumn();

    if (framesPerRow <= 0 || framesPerColumn <= 0) {
        return 0;
    }

    return framesPerRow * framesPerColumn;
}

Vec4 Sprite3D::CalculateFrameUVRect(int frameIndex) const {
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

void Sprite3D::CalculateUVCoordinates(Vec2& uvMin, Vec2& uvMax) const {
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

Mat4 Sprite3D::CalculateBillboardTransform(const Mat4& viewMatrix) const {
    BillboardMode mode = GetBillboardMode();

    if (mode == BillboardMode::Disabled) {
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

    if (mode == BillboardMode::Enabled) {

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
    } else if (mode == BillboardMode::YAxisOnly) {

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

const std::string& Sprite3D::GetTexturePath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("texturePath");
    return cachedPath;
}

void Sprite3D::SetTexturePath(const std::string& path) {
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

bool Sprite3D::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    // Get our current texture path
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

        // IMPORTANT: Remove from the static texture cache first!
        // This ensures the next buildDrawCommands will reload from disk
        {
            std::lock_guard<std::mutex> lock(GetSprite3DTextureCacheMutex());
            auto& cache = GetSprite3DTextureCache();
            auto it = cache.find(currentPath);
            if (it != cache.end()) {
                
                cache.erase(it);
            }
        }

        // Invalidate our cached texture handle - force reload on next render
        m_TextureHandle = TextureHandle();
        m_TextureAsset.Reset();
        m_CurrentTexturePath.clear();  // Force path comparison to trigger reload
        m_TextureNeedsUpload = true;

        return true;
    }

    return false;
}

Color Sprite3D::GetModulate() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("modulate");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

void Sprite3D::SetModulate(const Color& color) {
    SetPropertyValue<Color>("modulate", color);
}

const Vec2& Sprite3D::GetSize() const {
    static Vec2 cachedSize;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("size");
    if (prop) {
        cachedSize = prop->GetValue<Vec2>();
        return cachedSize;
    }
    static Vec2 defaultSize(1.0f, 1.0f);
    return defaultSize;
}

void Sprite3D::SetSize(const Vec2& size) {
    SetPropertyValue<Vec2>("size", size);
}

const Vec4& Sprite3D::GetUVRect() const {
    static Vec4 cachedUV;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("uvRect");
    if (prop) {
        cachedUV = prop->GetValue<Vec4>();
        return cachedUV;
    }
    static Vec4 defaultUV(0.0f, 0.0f, 1.0f, 1.0f);
    return defaultUV;
}

void Sprite3D::SetUVRect(const Vec4& uvRect) {
    SetPropertyValue<Vec4>("uvRect", uvRect);
}

float Sprite3D::GetAlphaCutoff() const {
    return GetPropertyValue<float>("alphaCutoff");
}

void Sprite3D::SetAlphaCutoff(float cutoff) {
    SetPropertyValue<float>("alphaCutoff", cutoff);
}

bool Sprite3D::GetFlipH() const {
    return GetPropertyValue<bool>("flipH");
}

void Sprite3D::SetFlipH(bool flip) {
    SetPropertyValue<bool>("flipH", flip);
}

bool Sprite3D::GetFlipV() const {
    return GetPropertyValue<bool>("flipV");
}

void Sprite3D::SetFlipV(bool flip) {
    SetPropertyValue<bool>("flipV", flip);
}

BillboardMode Sprite3D::GetBillboardMode() const {
    int mode = GetPropertyValue<int>("billboardMode");
    return static_cast<BillboardMode>(mode);
}

void Sprite3D::SetBillboardMode(BillboardMode mode) {
    SetPropertyValue<int>("billboardMode", static_cast<int>(mode));
}

float Sprite3D::GetPixelSize() const {
    return GetPropertyValue<float>("pixelSize");
}

void Sprite3D::SetPixelSize(float size) {
    SetPropertyValue<float>("pixelSize", size);
}

bool Sprite3D::GetDoubleSided() const {
    return GetPropertyValue<bool>("doubleSided");
}

void Sprite3D::SetDoubleSided(bool doubleSided) {
    SetPropertyValue<bool>("doubleSided", doubleSided);
}

bool Sprite3D::GetCastShadow() const {
    return GetPropertyValue<bool>("castShadow");
}

void Sprite3D::SetCastShadow(bool castShadow) {
    SetPropertyValue<bool>("castShadow", castShadow);
}

bool Sprite3D::GetReceiveShadow() const {
    return GetPropertyValue<bool>("receiveShadow");
}

void Sprite3D::SetReceiveShadow(bool receiveShadow) {
    SetPropertyValue<bool>("receiveShadow", receiveShadow);
}

void Sprite3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return;
    }

    std::string currentPath = GetTexturePath();

    // Check if we need to update the texture (path changed)
    if (currentPath != m_CurrentTexturePath) {
        // Don't destroy textures from cache - they're shared!
        // Just update our handle to point to the new cached texture
        m_TextureHandle = TextureHandle();
        m_TextureAsset.Reset();
        m_CurrentTexturePath = currentPath;
    }

    // Get or create cached texture if we don't have a valid handle
    if (!m_TextureHandle.isValid() && !currentPath.empty()) {
        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_TextureHandle = GetOrCreateSprite3DTexture(device, currentPath, m_TextureAsset);
        }
    }

    if (!m_TextureHandle.isValid()) {
        return;
    }

    BillboardMode billboardMode = GetBillboardMode();

    Mat4 transform;
    Vec2 size = CalculateRenderSize();

    if (billboardMode != BillboardMode::Disabled) {

        Mat4 billboardTransform = CalculateBillboardTransform(ctx.getViewMatrix());

        Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
        transform = billboardTransform * scale;
    } else {

        Mat4 nodeTransform = node3D->GetGlobalTransformMatrix();
        Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
        transform = nodeTransform * scale;
    }

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {

        return;
    }

    Vec2 uvMin, uvMax;
    CalculateUVCoordinates(uvMin, uvMax);

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_Texture", m_TextureHandle);
    overrides.setColor("u_TintColor", GetModulate());
    overrides.setVec4("u_UVRect", Vec4(uvMin.x, uvMin.y, uvMax.x, uvMax.y));
    overrides.setBool("u_UseTexture", true);

    MaterialHandle material = GetDoubleSided()
        ? ctx.getDefaultTexturedDoubleSidedMaterial()
        : ctx.getDefaultTexturedMaterial();

    ctx.drawMesh(quadMesh, material, transform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

AABB Sprite3D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return AABB();
    }

    Vec2 size = CalculateRenderSize();
    Vec3 globalPos = node3D->GetGlobalPosition();
    Vec3 scale = node3D->GetScale();

    size.x *= scale.x;
    size.y *= scale.y;

    Vec3 halfSize(size.x * 0.5f, size.y * 0.5f, 0.1f);

    return AABB(
        globalPos - halfSize,
        globalPos + halfSize
    );
}

RenderLayer Sprite3D::getRenderLayer() const {

    float alphaCutoff = GetAlphaCutoff();
    if (alphaCutoff > 0.0f) {
        return RenderLayer::Opaque;
    }
    return RenderLayer::Transparent;
}

bool Sprite3D::IntersectRay(const math::Ray& ray, float& outDistance) const {
    if (!m_Owner) {
        return false;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return false;
    }

    Vec2 size = CalculateRenderSize();

    BillboardMode billboardMode = GetBillboardMode();
    if (billboardMode != BillboardMode::Disabled) {

        AABB bounds = getWorldBounds();
        return ray.IntersectAABB(bounds, outDistance);
    }

    Mat4 worldTransform = node3D->GetGlobalTransformMatrix();
    Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
    Mat4 fullTransform = worldTransform * scale;
    Mat4 invTransform = fullTransform.Inverse();

    Vec4 localOrigin4 = invTransform * Vec4(ray.origin.x, ray.origin.y, ray.origin.z, 1.0f);
    Vec3 localOrigin(localOrigin4.x / localOrigin4.w, localOrigin4.y / localOrigin4.w, localOrigin4.z / localOrigin4.w);

    Vec4 localDir4 = invTransform * Vec4(ray.direction.x, ray.direction.y, ray.direction.z, 0.0f);
    Vec3 localDir(localDir4.x, localDir4.y, localDir4.z);
    localDir = localDir.Normalized();

    math::Ray localRay(localOrigin, localDir);

    Vec3 v0(-0.5f, -0.5f, 0.0f);
    Vec3 v1( 0.5f, -0.5f, 0.0f);
    Vec3 v2( 0.5f,  0.5f, 0.0f);
    Vec3 v3(-0.5f,  0.5f, 0.0f);

    float hitDist = 0.0f;
    bool hit = false;

    if (localRay.IntersectTriangle(v0, v1, v2, hitDist)) {
        hit = true;
    }

    float hitDist2 = 0.0f;
    if (localRay.IntersectTriangle(v0, v2, v3, hitDist2)) {
        if (!hit || hitDist2 < hitDist) {
            hitDist = hitDist2;
            hit = true;
        }
    }

    if (hit) {

        Vec3 localHitPoint = localRay.GetPoint(hitDist);
        Vec4 worldHitPoint4 = fullTransform * Vec4(localHitPoint.x, localHitPoint.y, localHitPoint.z, 1.0f);
        Vec3 worldHitPoint(worldHitPoint4.x / worldHitPoint4.w,
                          worldHitPoint4.y / worldHitPoint4.w,
                          worldHitPoint4.z / worldHitPoint4.w);

        outDistance = (worldHitPoint - ray.origin).Length();
        return true;
    }

    return false;
}

math::OBB Sprite3D::getOrientedBounds() const {
    if (!m_Owner) {
        return math::OBB::FromAABB(AABB());
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return math::OBB::FromAABB(AABB());
    }

    Vec2 size = CalculateRenderSize();
    Vec3 worldPos = node3D->GetGlobalPosition();
    Quat worldRot = node3D->GetGlobalRotation();
    Vec3 worldScale = node3D->GetGlobalScale();

    BillboardMode billboardMode = GetBillboardMode();
    if (billboardMode != BillboardMode::Disabled) {
        return math::OBB::FromAABB(getWorldBounds());
    }

    Vec3 halfExtents(
        size.x * 0.5f * std::abs(worldScale.x),
        size.y * 0.5f * std::abs(worldScale.y),
        0.05f * std::abs(worldScale.z)
    );

    return math::OBB(worldPos, halfExtents, worldRot);
}

}
}

