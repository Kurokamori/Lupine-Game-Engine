#include "lupine/components/AnimatedSprite3D.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/math/AABB.hpp"

namespace lupine {
namespace components {

using namespace math;
using namespace asset;

AnimatedSprite3D::AnimatedSprite3D()
    : core::Component("AnimatedSprite3D")
    , m_CurrentClip(nullptr)
    , m_CurrentFrameIndex(0)
    , m_FrameTimer(0.0f)
    , m_PingPongReverse(false)
    , m_TextureUploaded(false)
{
}

AnimatedSprite3D::~AnimatedSprite3D() {
}

void AnimatedSprite3D::DefineProperties() {

    DefineProperty(PROPERTY_FILE_GROUP(animationFilePath, std::string(""), "*.spriteanimation", "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(animationName, String, std::string(""), "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(playing, Bool, false, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, true, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoPlay, Bool, false, "Animation"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec3, Vec3(0.0f, 0.0f, 0.0f), "Transform"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Rendering"));
    DefineProperty(PROPERTY_ENUM_GROUP(billboard, 0, "Rendering", Disabled,Enabled,Y Axis Only));
    DefineProperty(PROPERTY_DEFAULT_GROUP(doubleSided, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(castShadows, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadows, Bool, true, "Rendering"));
}

std::string AnimatedSprite3D::GetAnimationFilePath() const {
    return GetPropertyValue<std::string>("animationFilePath");
}

std::string AnimatedSprite3D::GetAnimationName() const {
    return GetPropertyValue<std::string>("animationName");
}

bool AnimatedSprite3D::IsPlaying() const {
    return GetPropertyValue<bool>("playing");
}

bool AnimatedSprite3D::GetLoop() const {
    return GetPropertyValue<bool>("loop");
}

bool AnimatedSprite3D::GetAutoPlay() const {
    return GetPropertyValue<bool>("autoPlay");
}

const Vec3& AnimatedSprite3D::GetOffset() const {
    static Vec3 cachedOffset;
    const core::ComponentProperty* prop = m_CustomProperties.GetProperty("offset");
    if (prop) {
        cachedOffset = prop->GetValue<Vec3>();
        return cachedOffset;
    }
    static Vec3 defaultOffset(0.0f, 0.0f, 0.0f);
    return defaultOffset;
}

bool AnimatedSprite3D::GetFlipH() const {
    return GetPropertyValue<bool>("flipH");
}

bool AnimatedSprite3D::GetFlipV() const {
    return GetPropertyValue<bool>("flipV");
}

Color AnimatedSprite3D::GetModulate() const {
    const core::ComponentProperty* prop = m_CustomProperties.GetProperty("modulate");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

BillboardMode AnimatedSprite3D::GetBillboard() const {
    return static_cast<BillboardMode>(GetPropertyValue<int>("billboard"));
}

bool AnimatedSprite3D::GetCastShadows() const {
    return GetPropertyValue<bool>("castShadows");
}

bool AnimatedSprite3D::GetReceiveShadows() const {
    return GetPropertyValue<bool>("receiveShadows");
}

bool AnimatedSprite3D::GetDoubleSided() const {
    return GetPropertyValue<bool>("doubleSided");
}

void AnimatedSprite3D::SetAnimationFilePath(const std::string& filepath) {
    // Convert to res:// path if possible
    std::string resPath = filepath;
    if (!filepath.empty() && !(filepath.size() >= 6 && filepath.substr(0, 6) == "res://")) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(filepath);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }
    SetPropertyValue<std::string>("animationFilePath", resPath);

    m_TextureUploaded = false;
    m_TextureHandle = TextureHandle();
    m_CurrentClip = nullptr;
    m_CurrentFrameTexture.Reset();

    LoadAnimationAsset();
}

void AnimatedSprite3D::SetAnimationName(const std::string& name) {
    std::string currentName = GetAnimationName();
    if (currentName == name) {
        return;
    }

    SetPropertyValue<std::string>("animationName", name);

    if (!m_AnimationAsset.IsValid()) {
        return;
    }

    m_CurrentClip = m_AnimationAsset->GetAnimation(name);
    if (!m_CurrentClip) {

        m_CurrentFrameTexture.Reset();
        return;
    }

    m_CurrentFrameIndex = 0;
    m_FrameTimer = 0.0f;
    m_PingPongReverse = false;

    m_TextureUploaded = false;

    m_TextureHandle = TextureHandle();

    UpdateCurrentFrameTexture();
}

void AnimatedSprite3D::SetPlaying(bool playing) {
    SetPropertyValue<bool>("playing", playing);
}

void AnimatedSprite3D::SetLoop(bool loop) {
    SetPropertyValue<bool>("loop", loop);
}

void AnimatedSprite3D::SetAutoPlay(bool autoPlay) {
    SetPropertyValue<bool>("autoPlay", autoPlay);
}

void AnimatedSprite3D::SetOffset(const Vec3& offset) {
    SetPropertyValue<Vec3>("offset", offset);
}

void AnimatedSprite3D::SetFlipH(bool flip) {
    SetPropertyValue<bool>("flipH", flip);
}

void AnimatedSprite3D::SetFlipV(bool flip) {
    SetPropertyValue<bool>("flipV", flip);
}

void AnimatedSprite3D::SetModulate(const Color& color) {
    SetPropertyValue<Color>("modulate", color);
}

void AnimatedSprite3D::SetBillboard(BillboardMode mode) {
    SetPropertyValue<int>("billboard", static_cast<int>(mode));
}

void AnimatedSprite3D::SetCastShadows(bool cast) {
    SetPropertyValue<bool>("castShadows", cast);
}

void AnimatedSprite3D::SetReceiveShadows(bool receive) {
    SetPropertyValue<bool>("receiveShadows", receive);
}

void AnimatedSprite3D::SetDoubleSided(bool doubleSided) {
    SetPropertyValue<bool>("doubleSided", doubleSided);
}

void AnimatedSprite3D::OnAwake() {

    std::string animPath = GetAnimationFilePath();
    if (!animPath.empty()) {
        LoadAnimationAsset();
    }
}

void AnimatedSprite3D::OnReady() {

    if (GetAutoPlay() && m_AnimationAsset.IsValid()) {
        std::string animName = GetAnimationName();
        if (!animName.empty()) {
            Play(animName);
        }
    }
}

void AnimatedSprite3D::OnUpdate(float deltaTime) {
    if (IsPlaying() && m_CurrentClip) {
        UpdateAnimation(deltaTime);
    }
}

void AnimatedSprite3D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "animationFilePath") {

        std::string newPath = newValue.get<std::string>();

        m_TextureUploaded = false;
        m_TextureHandle = TextureHandle();
        m_CurrentClip = nullptr;
        m_CurrentFrameTexture.Reset();

        if (!newPath.empty()) {
            LoadAnimationAsset();
        }
    }
    else if (propertyName == "animationName") {

        std::string newName = newValue.get<std::string>();

        if (!m_AnimationAsset.IsValid()) {
            return;
        }

        m_CurrentClip = m_AnimationAsset->GetAnimation(newName);
        if (!m_CurrentClip) {

            m_CurrentFrameTexture.Reset();
            return;
        }

        m_CurrentFrameIndex = 0;
        m_FrameTimer = 0.0f;
        m_PingPongReverse = false;

        m_TextureUploaded = false;
        m_TextureHandle = TextureHandle();

        UpdateCurrentFrameTexture();
    }
    else if (propertyName == "playing") {

    }
    else if (propertyName == "loop") {

    }
    else if (propertyName == "autoPlay") {

    }
}

void AnimatedSprite3D::LoadAnimationAsset() {
    std::string filepath = GetAnimationFilePath();
    if (filepath.empty()) {
        return;
    }

    m_AnimationAsset = AssetRef<SpriteAnimationAsset>(new SpriteAnimationAsset());
    if (!m_AnimationAsset->LoadFromFile(filepath)) {

        m_AnimationAsset.Reset();
        m_CurrentClip = nullptr;
        m_CurrentFrameTexture.Reset();
        return;
    }

    m_CurrentFrameIndex = 0;
    m_FrameTimer = 0.0f;
    m_PingPongReverse = false;
    m_TextureUploaded = false;
    m_TextureHandle = TextureHandle();

    std::string animName = GetAnimationName();
    if (!animName.empty()) {
        m_CurrentClip = m_AnimationAsset->GetAnimation(animName);
        if (!m_CurrentClip) {

            m_CurrentFrameTexture.Reset();
        } else {
            UpdateCurrentFrameTexture();
        }
    } else {

        auto animNames = m_AnimationAsset->GetAnimationNames();
        if (!animNames.empty()) {
            m_CurrentClip = m_AnimationAsset->GetAnimation(animNames[0]);
            if (m_CurrentClip) {
                SetPropertyValue<std::string>("animationName", animNames[0]);
                UpdateCurrentFrameTexture();
            }
        }
    }
}

void AnimatedSprite3D::Play(const std::string& animationName) {
    if (!animationName.empty()) {
        SetAnimationName(animationName);

        if (!m_AnimationAsset.IsValid()) {
            return;
        }

        m_CurrentClip = m_AnimationAsset->GetAnimation(animationName);
        if (!m_CurrentClip) {

            return;
        }

        m_CurrentFrameIndex = 0;
        m_FrameTimer = 0.0f;
        m_PingPongReverse = false;
        m_TextureUploaded = false;
        UpdateCurrentFrameTexture();
    }

    if (!m_CurrentClip) {

        return;
    }

    SetPlaying(true);
    m_FrameTimer = 0.0f;
}

void AnimatedSprite3D::Stop() {
    SetPlaying(false);
    m_CurrentFrameIndex = 0;
    m_FrameTimer = 0.0f;
    m_PingPongReverse = false;
    UpdateCurrentFrameTexture();
}

void AnimatedSprite3D::Pause() {
    SetPlaying(false);
}

void AnimatedSprite3D::Resume() {
    SetPlaying(true);
}

void AnimatedSprite3D::SetFrame(int frameIndex) {
    if (!m_CurrentClip || m_CurrentClip->frames.empty()) {
        return;
    }

    m_CurrentFrameIndex = std::clamp(frameIndex, 0, static_cast<int>(m_CurrentClip->frames.size()) - 1);
    m_FrameTimer = 0.0f;
    m_TextureUploaded = false;
    UpdateCurrentFrameTexture();

    if (on_frame_changed) {
        on_frame_changed(m_CurrentFrameIndex);
    }
    Emit("frame_changed", { m_CurrentFrameIndex });
}

void AnimatedSprite3D::UpdateAnimation(float deltaTime) {
    if (!m_CurrentClip || m_CurrentClip->frames.empty()) {
        return;
    }

    float frameDuration = 1.0f / m_CurrentClip->speed;
    m_FrameTimer += deltaTime;

    while (m_FrameTimer >= frameDuration) {
        m_FrameTimer -= frameDuration;
        AdvanceFrame();
    }
}

void AnimatedSprite3D::AdvanceFrame() {
    if (!m_CurrentClip || m_CurrentClip->frames.empty()) {
        return;
    }

    int frameCount = static_cast<int>(m_CurrentClip->frames.size());
    int oldFrame = m_CurrentFrameIndex;

    AnimationLoopMode loopMode = m_CurrentClip->loopMode;
    if (!GetLoop()) {
        loopMode = AnimationLoopMode::None;
    }

    if (loopMode == AnimationLoopMode::PingPong) {
        if (m_PingPongReverse) {
            m_CurrentFrameIndex--;
            if (m_CurrentFrameIndex < 0) {
                m_CurrentFrameIndex = 1;
                m_PingPongReverse = false;
            }
        } else {
            m_CurrentFrameIndex++;
            if (m_CurrentFrameIndex >= frameCount) {
                m_CurrentFrameIndex = frameCount - 2;
                m_PingPongReverse = true;
                if (m_CurrentFrameIndex < 0) {
                    m_CurrentFrameIndex = 0;
                }
            }
        }
    } else if (loopMode == AnimationLoopMode::Linear) {
        m_CurrentFrameIndex++;
        if (m_CurrentFrameIndex >= frameCount) {
            m_CurrentFrameIndex = 0;
        }
    } else {
        m_CurrentFrameIndex++;
        if (m_CurrentFrameIndex >= frameCount) {
            m_CurrentFrameIndex = frameCount - 1;
            SetPlaying(false);

            if (on_animation_finished) {
                on_animation_finished();
            }
            Emit("animation_finished");
        }
    }

    if (oldFrame != m_CurrentFrameIndex) {
        m_TextureUploaded = false;
        UpdateCurrentFrameTexture();

        if (on_frame_changed) {
            on_frame_changed(m_CurrentFrameIndex);
        }
        Emit("frame_changed", { m_CurrentFrameIndex });
    }
}

void AnimatedSprite3D::DefineSignals() {
    RegisterSignal({"animation_finished", {}, "Emitted when a non-looping animation completes."});
    RegisterSignal({"frame_changed",
                    {{"frame", core::PropertyValueType::Int}},
                    "Emitted when the current animation frame changes."});
}

void AnimatedSprite3D::UpdateCurrentFrameTexture() {
    if (!m_AnimationAsset.IsValid() || !m_CurrentClip) {
        return;
    }

    if (m_CurrentFrameIndex < 0 || m_CurrentFrameIndex >= static_cast<int>(m_CurrentClip->frames.size())) {
        return;
    }

    const auto& imageAssets = m_AnimationAsset->GetImageAssets();
    if (imageAssets.empty() || !imageAssets[0].IsValid()) {
        return;
    }

    m_CurrentFrameTexture = imageAssets[0];
}

void AnimatedSprite3D::UploadTexture(RenderContext& ctx) {
    if (m_TextureUploaded || !m_CurrentFrameTexture.IsValid()) {
        return;
    }

    if (m_CurrentFrameTexture->GetWidth() == 0 || m_CurrentFrameTexture->GetHeight() == 0 ||
        m_CurrentFrameTexture->GetData() == nullptr) {
        return;
    }

    if (m_TextureHandle.isValid()) {
        IGfxDevice* device = ctx.getDevice();
        if (device) {
            device->destroyTexture(m_TextureHandle);
            m_TextureHandle = TextureHandle();
        }
    }

    IGfxDevice* device = ctx.getDevice();
    if (device) {
        m_TextureHandle = lupine::CreateTexture2DFromImage(device, *m_CurrentFrameTexture, TextureFormat::RGBA8_SRGB);
    }

    m_TextureUploaded = true;
}

Mat4 AnimatedSprite3D::CalculateBillboardTransform(const Mat4& viewMatrix) const {
    BillboardMode billboard = GetBillboard();

    if (billboard == BillboardMode::Disabled) {
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

    Vec3 offset = GetOffset();
    position = position + offset;

    if (billboard == BillboardMode::Enabled) {

        glm::mat4 glmView = viewMatrix.ToGLM();
        Vec3 right(glmView[0][0], glmView[1][0], glmView[2][0]);
        Vec3 up(glmView[0][1], glmView[1][1], glmView[2][1]);
        Vec3 forward = Cross(right, up).Normalized();

        right = right * scale.x;
        up = up * scale.y;
        forward = forward * scale.z;

        glm::mat4 billboardMatrix(1.0f);
        billboardMatrix[0] = glm::vec4(right.x, right.y, right.z, 0.0f);
        billboardMatrix[1] = glm::vec4(up.x, up.y, up.z, 0.0f);
        billboardMatrix[2] = glm::vec4(forward.x, forward.y, forward.z, 0.0f);
        billboardMatrix[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboardMatrix);

    } else if (billboard == BillboardMode::YAxisOnly) {

        glm::mat4 glmView = viewMatrix.ToGLM();
        glm::mat4 invView = glm::inverse(glmView);
        Vec3 cameraPos(invView[3][0], invView[3][1], invView[3][2]);

        Vec3 toCamera = cameraPos - position;
        toCamera.y = 0.0f;
        toCamera = toCamera.Normalized();

        Vec3 right = Cross(Vec3(0, 1, 0), toCamera).Normalized();
        Vec3 up(0, 1, 0);
        Vec3 forward = toCamera;

        right = right * scale.x;
        up = up * scale.y;
        forward = forward * scale.z;

        glm::mat4 billboardMatrix(1.0f);
        billboardMatrix[0] = glm::vec4(right.x, right.y, right.z, 0.0f);
        billboardMatrix[1] = glm::vec4(up.x, up.y, up.z, 0.0f);
        billboardMatrix[2] = glm::vec4(forward.x, forward.y, forward.z, 0.0f);
        billboardMatrix[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboardMatrix);
    }

    return Mat4::Identity();
}

void AnimatedSprite3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    if (!m_CurrentFrameTexture.IsValid() || !m_CurrentClip) {
        return;
    }

    if (m_CurrentFrameIndex < 0 || m_CurrentFrameIndex >= static_cast<int>(m_CurrentClip->frames.size())) {
        return;
    }

    UploadTexture(ctx);

    if (!m_TextureHandle.isValid()) {
        return;
    }

    core::Node3D* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (!node3D) {
        return;
    }

    const SpriteFrame& frame = m_CurrentClip->frames[m_CurrentFrameIndex];

    float spriteWidthPixels = m_CurrentFrameTexture->GetWidth() * (frame.uvRect.z - frame.uvRect.x);
    float spriteHeightPixels = m_CurrentFrameTexture->GetHeight() * (frame.uvRect.w - frame.uvRect.y);

    float maxDimension = std::max(spriteWidthPixels, spriteHeightPixels);
    float spriteWidth = (maxDimension > 0.0f) ? (spriteWidthPixels / maxDimension) : 1.0f;
    float spriteHeight = (maxDimension > 0.0f) ? (spriteHeightPixels / maxDimension) : 1.0f;

    Vec2 size(spriteWidth, spriteHeight);

    Mat4 transform;
    BillboardMode billboard = GetBillboard();

    if (billboard != BillboardMode::Disabled) {

        Mat4 billboardTransform = CalculateBillboardTransform(ctx.getViewMatrix());

        Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
        transform = billboardTransform * scale;
    } else {

        Mat4 nodeTransform = node3D->GetGlobalTransformMatrix();

        Vec3 offset = GetOffset();
        Mat4 offsetTransform = Mat4::Translate(offset);

        Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));

        transform = nodeTransform * offsetTransform * scale;
    }

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    Vec2 uvMin(frame.uvRect.x, frame.uvRect.y);
    Vec2 uvMax(frame.uvRect.z, frame.uvRect.w);

    bool flipH = GetFlipH();
    bool flipV = GetFlipV();

    if (flipH) {
        std::swap(uvMin.x, uvMax.x);
    }
    if (flipV) {
        std::swap(uvMin.y, uvMax.y);
    }

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_Texture", m_TextureHandle);
    overrides.setColor("u_TintColor", GetModulate());
    overrides.setVec4("u_UVRect", Vec4(uvMin.x, uvMin.y, uvMax.x, uvMax.y));
    overrides.setBool("u_UseTexture", true);

    MaterialHandle material = GetDoubleSided()
        ? ctx.getDefaultTexturedDoubleSidedMaterial()
        : ctx.getDefaultTexturedMaterial();

    ctx.drawMesh(quadMesh, material, transform, overrides, 0, GetCastShadows(), GetReceiveShadows());
}

AABB AnimatedSprite3D::getWorldBounds() const {
    if (!m_Owner || !m_CurrentFrameTexture.IsValid() || !m_CurrentClip) {
        return AABB();
    }

    core::Node3D* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (!node3D) {
        return AABB();
    }

    if (m_CurrentFrameIndex < 0 || m_CurrentFrameIndex >= static_cast<int>(m_CurrentClip->frames.size())) {
        return AABB();
    }

    const SpriteFrame& frame = m_CurrentClip->frames[m_CurrentFrameIndex];

    float spriteWidthPixels = m_CurrentFrameTexture->GetWidth() * (frame.uvRect.z - frame.uvRect.x);
    float spriteHeightPixels = m_CurrentFrameTexture->GetHeight() * (frame.uvRect.w - frame.uvRect.y);

    float maxDimension = std::max(spriteWidthPixels, spriteHeightPixels);
    float spriteWidth = (maxDimension > 0.0f) ? (spriteWidthPixels / maxDimension) : 1.0f;
    float spriteHeight = (maxDimension > 0.0f) ? (spriteHeightPixels / maxDimension) : 1.0f;

    Vec3 globalPos = node3D->GetGlobalPosition();
    Vec3 scale = node3D->GetGlobalScale();
    Vec3 offset = GetOffset();

    globalPos = globalPos + offset;

    spriteWidth *= std::abs(scale.x);
    spriteHeight *= std::abs(scale.y);

    Vec3 halfSize(spriteWidth * 0.5f, spriteHeight * 0.5f, 0.1f);

    return AABB(
        globalPos - halfSize,
        globalPos + halfSize
    );
}

bool AnimatedSprite3D::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string currentPath = GetAnimationFilePath();
    if (currentPath.empty()) {
        return false;
    }

    // Resolve our path for comparison
    std::string resolvedCurrentPath;
    auto& assetDb = AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        resolvedCurrentPath = assetDb.ResolveAsset(currentPath);
    }

    // Check if this is our animation file
    bool matches = (currentPath == changedPath) ||
                   (!resolvedCurrentPath.empty() && !resolvedChangedPath.empty() &&
                    resolvedCurrentPath == resolvedChangedPath);

    if (matches) {

        // Invalidate instance state - force reload on next update
        m_TextureHandle = TextureHandle();
        m_CurrentFrameTexture.Reset();
        m_AnimationAsset.Reset();
        m_CurrentClip = nullptr;
        m_TextureUploaded = false;

        return true;
    }

    return false;
}

}
}

