#include "lupine/components/AnimatedSprite2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/RenderWorld.hpp"

namespace lupine {
namespace components {

using namespace math;
using namespace asset;

AnimatedSprite2D::AnimatedSprite2D()
    : core::Component("AnimatedSprite2D")
    , m_CurrentClip(nullptr)
    , m_CurrentFrameIndex(0)
    , m_FrameTimer(0.0f)
    , m_PingPongReverse(false)
    , m_TextureUploaded(false)
{
}

AnimatedSprite2D::~AnimatedSprite2D() {
}

void AnimatedSprite2D::DefineProperties() {

    DefineProperty(PROPERTY_FILE_GROUP(animationFilePath, std::string(""), "*.spriteanimation", "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(animationName, String, std::string(""), "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(playing, Bool, false, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, true, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoPlay, Bool, false, "Animation"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, Vec2(0.0f, 0.0f), "Transform"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pixelSnap, Bool, false, "Rendering"));
}

std::string AnimatedSprite2D::GetAnimationFilePath() const {
    return GetPropertyValue<std::string>("animationFilePath");
}

std::string AnimatedSprite2D::GetAnimationName() const {
    return GetPropertyValue<std::string>("animationName");
}

bool AnimatedSprite2D::IsPlaying() const {
    return GetPropertyValue<bool>("playing");
}

bool AnimatedSprite2D::GetLoop() const {
    return GetPropertyValue<bool>("loop");
}

bool AnimatedSprite2D::GetAutoPlay() const {
    return GetPropertyValue<bool>("autoPlay");
}

const Vec2& AnimatedSprite2D::GetOffset() const {
    static Vec2 cachedOffset;
    const core::ComponentProperty* prop = m_CustomProperties.GetProperty("offset");
    if (prop) {
        cachedOffset = prop->GetValue<Vec2>();
        return cachedOffset;
    }
    static Vec2 defaultOffset(0.0f, 0.0f);
    return defaultOffset;
}

bool AnimatedSprite2D::GetFlipH() const {
    return GetPropertyValue<bool>("flipH");
}

bool AnimatedSprite2D::GetFlipV() const {
    return GetPropertyValue<bool>("flipV");
}

const Color& AnimatedSprite2D::GetModulate() const {
    static Color cachedColor;
    const core::ComponentProperty* prop = m_CustomProperties.GetProperty("modulate");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    static Color defaultColor = Color::White();
    return defaultColor;
}

bool AnimatedSprite2D::GetPixelSnap() const {
    return GetPropertyValue<bool>("pixelSnap");
}

void AnimatedSprite2D::SetAnimationFilePath(const std::string& filepath) {
    SetPropertyValue<std::string>("animationFilePath", filepath);

    m_TextureUploaded = false;
    m_TextureHandle = TextureHandle();
    m_CurrentClip = nullptr;
    m_CurrentFrameTexture.Reset();

    LoadAnimationAsset();
}

void AnimatedSprite2D::SetAnimationName(const std::string& name) {
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

void AnimatedSprite2D::SetPlaying(bool playing) {
    bool currentlyPlaying = IsPlaying();
    if (playing && !currentlyPlaying) {
        Play();
    } else if (!playing && currentlyPlaying) {
        Pause();
    }
    SetPropertyValue<bool>("playing", playing);
}

void AnimatedSprite2D::SetLoop(bool loop) {
    SetPropertyValue<bool>("loop", loop);
}

void AnimatedSprite2D::SetAutoPlay(bool autoPlay) {
    SetPropertyValue<bool>("autoPlay", autoPlay);
}

void AnimatedSprite2D::SetOffset(const Vec2& offset) {
    SetPropertyValue<Vec2>("offset", offset);
}

void AnimatedSprite2D::SetFlipH(bool flip) {
    SetPropertyValue<bool>("flipH", flip);
}

void AnimatedSprite2D::SetFlipV(bool flip) {
    SetPropertyValue<bool>("flipV", flip);
}

void AnimatedSprite2D::SetModulate(const Color& color) {
    SetPropertyValue<Color>("modulate", color);
}

void AnimatedSprite2D::SetPixelSnap(bool snap) {
    SetPropertyValue<bool>("pixelSnap", snap);
}

void AnimatedSprite2D::OnAwake() {

    std::string animPath = GetAnimationFilePath();
    if (!animPath.empty()) {
        LoadAnimationAsset();
    }
}

void AnimatedSprite2D::OnReady() {

    if (GetAutoPlay() && m_AnimationAsset.IsValid()) {
        std::string animName = GetAnimationName();
        if (!animName.empty()) {
            Play(animName);
        }
    }
}

void AnimatedSprite2D::OnUpdate(float deltaTime) {
    if (IsPlaying() && m_CurrentClip) {
        UpdateAnimation(deltaTime);
    }
}

void AnimatedSprite2D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
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

void AnimatedSprite2D::LoadAnimationAsset() {
    std::string animPath = GetAnimationFilePath();
    if (animPath.empty()) {
        return;
    }

    m_AnimationAsset = AssetRef<SpriteAnimationAsset>(new SpriteAnimationAsset());
    if (!m_AnimationAsset->LoadFromFile(animPath)) {

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

void AnimatedSprite2D::Play(const std::string& animationName) {
    if (!animationName.empty()) {
        SetAnimationName(animationName);
    }

    if (!m_CurrentClip) {

        return;
    }

    SetPropertyValue<bool>("playing", true);
    m_FrameTimer = 0.0f;
}

void AnimatedSprite2D::Stop() {
    SetPropertyValue<bool>("playing", false);
    m_CurrentFrameIndex = 0;
    m_FrameTimer = 0.0f;
    m_PingPongReverse = false;
    UpdateCurrentFrameTexture();
}

void AnimatedSprite2D::Pause() {
    SetPropertyValue<bool>("playing", false);
}

void AnimatedSprite2D::Resume() {
    SetPropertyValue<bool>("playing", true);
}

void AnimatedSprite2D::SetFrame(int frameIndex) {
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
}

void AnimatedSprite2D::UpdateAnimation(float deltaTime) {
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

void AnimatedSprite2D::AdvanceFrame() {
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
            SetPropertyValue<bool>("playing", false);

            if (on_animation_finished) {
                on_animation_finished();
            }
        }
    }

    if (oldFrame != m_CurrentFrameIndex) {
        m_TextureUploaded = false;
        UpdateCurrentFrameTexture();

        if (on_frame_changed) {
            on_frame_changed(m_CurrentFrameIndex);
        }
    }
}

void AnimatedSprite2D::UpdateCurrentFrameTexture() {
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

void AnimatedSprite2D::UploadTexture(RenderContext& ctx) {
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

    TextureDesc desc;
    desc.width = m_CurrentFrameTexture->GetWidth();
    desc.height = m_CurrentFrameTexture->GetHeight();
    desc.format = TextureFormat::RGBA8_SRGB;
    desc.usage = TextureUsage::Sampled;
    desc.initialData = m_CurrentFrameTexture->GetData();

    IGfxDevice* device = ctx.getDevice();
    if (device) {
        m_TextureHandle = device->createTexture(desc);
        m_TextureUploaded = true;
    }
}

void AnimatedSprite2D::buildDrawCommands(RenderContext& ctx) {
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

    const SpriteFrame& frame = m_CurrentClip->frames[m_CurrentFrameIndex];

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 scale = node2D->GetGlobalScale();
    float rotation = node2D->GetGlobalRotation();

    Vec2 offset = GetOffset();
    position.x += offset.x;
    position.y += offset.y;

    if (GetPixelSnap()) {
        position.x = std::round(position.x);
        position.y = std::round(position.y);
    }

    float spriteWidth = m_CurrentFrameTexture->GetWidth() * (frame.uvRect.z - frame.uvRect.x);
    float spriteHeight = m_CurrentFrameTexture->GetHeight() * (frame.uvRect.w - frame.uvRect.y);

    spriteWidth *= scale.x;
    spriteHeight *= scale.y;

    SpriteDrawData sprite;
    sprite.texture = m_TextureHandle;
    sprite.position = position;
    sprite.size = Vec2(spriteWidth, spriteHeight);
    sprite.rotation = rotation;
    sprite.tint = GetModulate();

    Vec2 uvMin(frame.uvRect.x, frame.uvRect.y);
    Vec2 uvMax(frame.uvRect.z, frame.uvRect.w);

    if (GetFlipH()) {
        std::swap(uvMin.x, uvMax.x);
    }
    if (GetFlipV()) {
        std::swap(uvMin.y, uvMax.y);
    }

    sprite.uvMin = uvMin;
    sprite.uvMax = uvMax;

    sprite.pivot = Vec2(0.5f, 0.5f);

    ctx.drawSprite(sprite);
}

AABB AnimatedSprite2D::getWorldBounds() const {

    if (!m_Owner || !m_CurrentFrameTexture.IsValid() || !m_CurrentClip) {
        return AABB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    if (m_CurrentFrameIndex < 0 || m_CurrentFrameIndex >= static_cast<int>(m_CurrentClip->frames.size())) {
        return AABB();
    }

    const SpriteFrame& frame = m_CurrentClip->frames[m_CurrentFrameIndex];

    Vec2 position2D = node2D->GetGlobalPosition();
    Vec2 scale2D = node2D->GetGlobalScale();
    Vec3 position(position2D.x, position2D.y, 0.0f);
    Vec3 scale(scale2D.x, scale2D.y, 1.0f);

    Vec2 offset = GetOffset();
    position.x += offset.x;
    position.y += offset.y;

    float spriteWidth = m_CurrentFrameTexture->GetWidth() * (frame.uvRect.z - frame.uvRect.x);
    float spriteHeight = m_CurrentFrameTexture->GetHeight() * (frame.uvRect.w - frame.uvRect.y);

    spriteWidth *= std::abs(scale.x);
    spriteHeight *= std::abs(scale.y);

    Vec3 min = position - Vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 0.0f);
    Vec3 max = position + Vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 0.0f);

    return AABB(min, max);
}

Vec2 AnimatedSprite2D::CalculateRenderSize() const {
    // Default size if no texture or animation loaded
    if (!m_CurrentFrameTexture.IsValid() || !m_CurrentClip) {
        return Vec2(100.0f, 100.0f);
    }

    // Get current frame
    if (m_CurrentFrameIndex < 0 || m_CurrentFrameIndex >= static_cast<int>(m_CurrentClip->frames.size())) {
        return Vec2(100.0f, 100.0f);
    }

    const SpriteFrame& frame = m_CurrentClip->frames[m_CurrentFrameIndex];

    // Calculate sprite size based on texture and UV rect
    float spriteWidth = m_CurrentFrameTexture->GetWidth() * (frame.uvRect.z - frame.uvRect.x);
    float spriteHeight = m_CurrentFrameTexture->GetHeight() * (frame.uvRect.w - frame.uvRect.y);

    // Don't apply scale here - this should return the base size
    // The scale is applied during rendering
    return Vec2(spriteWidth, spriteHeight);
}

}
}

