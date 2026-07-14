#include "lupine/components/GifPlayer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/MathCommon.hpp"

#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace math;

GifPlayer::GifPlayer()
    : core::Component("GifPlayer") {
}

GifPlayer::~GifPlayer() {
    ReleaseTexture();
}

void GifPlayer::DefineProperties() {
    DefineProperty(PROPERTY_FILE_GROUP(gifPath, std::string(""), "*.gif", "Gif"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(autoPlay, Bool, true, "Playback"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(playing, Bool, true, "Playback"));
    DefineProperty(PROPERTY_ENUM_GROUP(loopMode, 1, "Playback", OneShot, Loop, PingPong));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(speed, 1.0f, 0.0f, 10.0f, 0.05f, "Playback"));
    // fpsOverride: 0 = honor each GIF frame's own delay; > 0 = force a constant rate.
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fpsOverride, 0.0f, 0.0f, 240.0f, 1.0f, "Playback"));
    // previewInEditor: when true the GIF animates live in the editor viewport;
    // when false it stays on the first frame in the editor (overriding autoPlay).
    DefineProperty(PROPERTY_DEFAULT_GROUP(previewInEditor, Bool, true, "Playback"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, Vec2(0.0f, 0.0f), "Transform"));
    // size: (0,0) = use the GIF's native pixel dimensions.
    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec2, Vec2(0.0f, 0.0f), "Transform"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pixelSnap, Bool, false, "Rendering"));
}

void GifPlayer::DefineSignals() {
    RegisterSignal({"finished", {}, "Emitted when a one-shot GIF reaches its last frame."});
    RegisterSignal({"looped", {}, "Emitted each time playback wraps around (Loop / PingPong)."});
    RegisterSignal({"frame_changed",
                    {{"frame", core::PropertyValueType::Int}},
                    "Emitted when the displayed GIF frame changes."});
}

// ===== Getters =====

std::string GifPlayer::GetGifPath() const { return GetPropertyValue<std::string>("gifPath"); }
bool GifPlayer::IsPlaying() const { return GetPropertyValue<bool>("playing"); }
GifPlayer::LoopMode GifPlayer::GetLoopMode() const {
    return static_cast<LoopMode>(GetPropertyValue<int>("loopMode"));
}
bool GifPlayer::GetAutoPlay() const { return GetPropertyValue<bool>("autoPlay"); }
bool GifPlayer::GetPreviewInEditor() const { return GetPropertyValue<bool>("previewInEditor"); }
float GifPlayer::GetSpeed() const { return GetPropertyValue<float>("speed"); }
float GifPlayer::GetFpsOverride() const { return GetPropertyValue<float>("fpsOverride"); }
bool GifPlayer::GetFlipH() const { return GetPropertyValue<bool>("flipH"); }
bool GifPlayer::GetFlipV() const { return GetPropertyValue<bool>("flipV"); }
bool GifPlayer::GetPixelSnap() const { return GetPropertyValue<bool>("pixelSnap"); }

const Vec2& GifPlayer::GetOffset() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("offset");
    return cached;
}

const Vec2& GifPlayer::GetSize() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("size");
    return cached;
}

Color GifPlayer::GetModulate() const {
    return GetPropertyValue<Color>("modulate");
}

// ===== Setters =====

void GifPlayer::SetGifPath(const std::string& path) {
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
    SetPropertyValue<std::string>("gifPath", resPath);
    LoadGif();
}

void GifPlayer::SetLoopMode(LoopMode mode) {
    SetPropertyValue<int>("loopMode", static_cast<int>(mode));
}
void GifPlayer::SetAutoPlay(bool autoPlay) { SetPropertyValue<bool>("autoPlay", autoPlay); }
void GifPlayer::SetPreviewInEditor(bool enabled) { SetPropertyValue<bool>("previewInEditor", enabled); }
void GifPlayer::SetSpeed(float speed) { SetPropertyValue<float>("speed", math::Max(0.0f, speed)); }
void GifPlayer::SetFpsOverride(float fps) { SetPropertyValue<float>("fpsOverride", math::Max(0.0f, fps)); }
void GifPlayer::SetOffset(const Vec2& offset) { SetPropertyValue<Vec2>("offset", offset); }
void GifPlayer::SetSize(const Vec2& size) { SetPropertyValue<Vec2>("size", size); }
void GifPlayer::SetFlipH(bool flip) { SetPropertyValue<bool>("flipH", flip); }
void GifPlayer::SetFlipV(bool flip) { SetPropertyValue<bool>("flipV", flip); }
void GifPlayer::SetModulate(const Color& color) { SetPropertyValue<Color>("modulate", color); }
void GifPlayer::SetPixelSnap(bool snap) { SetPropertyValue<bool>("pixelSnap", snap); }

// ===== Lifecycle =====

void GifPlayer::OnAwake() {
    if (!GetGifPath().empty()) {
        LoadGif();
    }
}

void GifPlayer::OnReady() {
    bool inEditor = false;
    if (m_Owner) {
        core::Scene* scene = m_Owner->GetScene();
        inEditor = scene && scene->IsInEditor();
    }

    if (inEditor) {
        // Editor preview playback is governed solely by previewInEditor, which
        // overrides autoPlay.
        SetPropertyValue<bool>("playing", GetPreviewInEditor() && m_Loaded);
        m_FrameTimer = 0.0f;
    } else if (GetAutoPlay() && m_Loaded) {
        Play();
    } else {
        // Not auto-playing: keep playback paused on the first frame.
        SetPropertyValue<bool>("playing", false);
    }
}

void GifPlayer::OnUpdate(float deltaTime) {
    if (!IsPlaying() || !m_Loaded || m_Gif.FrameCount() <= 1) {
        return;
    }

    float speed = GetSpeed();
    if (speed <= 0.0f) {
        return;
    }

    m_FrameTimer += deltaTime * speed;

    float delay = CurrentFrameDelay();
    int guard = 0;
    while (delay > 0.0f && m_FrameTimer >= delay && guard < 4096) {
        m_FrameTimer -= delay;
        AdvanceFrame();
        ++guard;
        if (!IsPlaying()) {
            break;  // one-shot reached the end
        }
        delay = CurrentFrameDelay();
    }
}

void GifPlayer::OnDestroy() {
    ReleaseTexture();
}

void GifPlayer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "gifPath") {
        std::string newPath = newValue.is_string() ? newValue.get<std::string>() : std::string();
        ReleaseTexture();
        m_Loaded = false;
        m_Gif = video::GifData();
        m_CurrentFrame = 0;
        m_FrameTimer = 0.0f;
        m_PingPongReverse = false;
        m_Finished = false;
        if (!newPath.empty()) {
            LoadGif();
        }
    }
    else if (propertyName == "previewInEditor") {
        // Apply the editor-preview toggle immediately rather than only on the
        // next scene load. Runtime playback is unaffected.
        bool inEditor = false;
        if (m_Owner) {
            core::Scene* scene = m_Owner->GetScene();
            inEditor = scene && scene->IsInEditor();
        }
        if (inEditor) {
            bool preview = newValue.is_boolean() ? newValue.get<bool>() : true;
            SetPropertyValue<bool>("playing", preview && m_Loaded);
        }
    }
}

bool GifPlayer::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string currentPath = GetGifPath();
    if (currentPath.empty()) {
        return false;
    }

    std::string resolvedCurrent;
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        resolvedCurrent = assetDb.ResolveAsset(currentPath);
    }

    bool matches = (currentPath == changedPath) ||
                   (!resolvedCurrent.empty() && !resolvedChangedPath.empty() &&
                    resolvedCurrent == resolvedChangedPath);
    if (matches) {
        ReleaseTexture();
        m_Loaded = false;
        m_LoadedPath.clear();
        LoadGif();
        return true;
    }
    return false;
}

// ===== Playback control =====

void GifPlayer::Play() {
    if (!m_Loaded) {
        return;
    }
    if (m_Finished) {
        // Restart a finished one-shot from the beginning.
        m_CurrentFrame = 0;
        m_PingPongReverse = false;
        m_FrameDirty = true;
    }
    m_Finished = false;
    m_FrameTimer = 0.0f;
    SetPropertyValue<bool>("playing", true);
}

void GifPlayer::Stop() {
    SetPropertyValue<bool>("playing", false);
    m_CurrentFrame = 0;
    m_FrameTimer = 0.0f;
    m_PingPongReverse = false;
    m_Finished = false;
    m_FrameDirty = true;
}

void GifPlayer::Pause() {
    SetPropertyValue<bool>("playing", false);
}

void GifPlayer::Resume() {
    if (m_Loaded && !m_Finished) {
        SetPropertyValue<bool>("playing", true);
    }
}

void GifPlayer::Replay() {
    if (!m_Loaded) {
        return;
    }
    m_CurrentFrame = 0;
    m_FrameTimer = 0.0f;
    m_PingPongReverse = false;
    m_Finished = false;
    m_FrameDirty = true;
    SetPropertyValue<bool>("playing", true);
    if (on_frame_changed) on_frame_changed(m_CurrentFrame);
    Emit("frame_changed", { m_CurrentFrame });
}

void GifPlayer::SetFrame(int frameIndex) {
    if (!m_Loaded || m_Gif.FrameCount() == 0) {
        return;
    }
    int clamped = std::clamp(frameIndex, 0, m_Gif.FrameCount() - 1);
    if (clamped != m_CurrentFrame) {
        m_CurrentFrame = clamped;
        m_FrameTimer = 0.0f;
        m_FrameDirty = true;
        if (on_frame_changed) on_frame_changed(m_CurrentFrame);
        Emit("frame_changed", { m_CurrentFrame });
    }
}

// ===== Internal =====

void GifPlayer::LoadGif() {
    std::string path = GetGifPath();
    if (path.empty()) {
        return;
    }
    if (m_Loaded && path == m_LoadedPath) {
        return;
    }

    video::GifData data;
    if (!video::GifDecoder::LoadFromFile(path, data)) {
        LOG_ERROR(LogCategory::Core, "GifPlayer: failed to load {}", path);
        m_Loaded = false;
        return;
    }

    ReleaseTexture();
    m_Gif = std::move(data);
    m_Loaded = true;
    m_LoadedPath = path;
    m_CurrentFrame = 0;
    m_FrameTimer = 0.0f;
    m_PingPongReverse = false;
    m_Finished = false;
    m_FrameDirty = true;
}

void GifPlayer::ReleaseTexture() {
    // The owning RenderWorld/device frees GPU textures on teardown; we only drop
    // our handle so the next buildDrawCommands recreates it for the new GIF.
    m_TextureHandle = TextureHandle();
    m_TextureCreated = false;
    m_FrameDirty = true;
}

float GifPlayer::CurrentFrameDelay() const {
    float fpsOverride = GetFpsOverride();
    if (fpsOverride > 0.0f) {
        return 1.0f / fpsOverride;
    }
    if (m_CurrentFrame >= 0 && m_CurrentFrame < static_cast<int>(m_Gif.frameDelays.size())) {
        return m_Gif.frameDelays[m_CurrentFrame];
    }
    return 0.1f;
}

nlohmann::json GifPlayer::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto numArg = [&](float fallback) -> float {
        if (args.is_array() && !args.empty() && args[0].is_number()) {
            return args[0].get<float>();
        }
        return fallback;
    };

    if (method == "play") { Play(); return nlohmann::json(); }
    if (method == "stop") { Stop(); return nlohmann::json(); }
    if (method == "pause") { Pause(); return nlohmann::json(); }
    if (method == "resume") { Resume(); return nlohmann::json(); }
    if (method == "replay") { Replay(); return nlohmann::json(); }
    if (method == "set_frame") {
        if (args.is_array() && !args.empty() && args[0].is_number()) {
            SetFrame(args[0].get<int>());
        }
        return nlohmann::json();
    }
    if (method == "set_speed") { SetSpeed(numArg(GetSpeed())); return nlohmann::json(); }
    if (method == "set_fps") { SetFpsOverride(numArg(GetFpsOverride())); return nlohmann::json(); }
    if (method == "get_current_frame") { return GetCurrentFrame(); }
    if (method == "get_frame_count") { return GetFrameCount(); }
    if (method == "get_speed") { return GetSpeed(); }
    if (method == "is_playing") { return IsPlaying(); }
    if (method == "is_loaded") { return IsLoaded(); }
    return nlohmann::json();
}

void GifPlayer::AdvanceFrame() {
    int frameCount = m_Gif.FrameCount();
    if (frameCount <= 0) {
        return;
    }

    int oldFrame = m_CurrentFrame;
    LoopMode mode = GetLoopMode();

    if (mode == LoopMode::PingPong) {
        if (m_PingPongReverse) {
            m_CurrentFrame--;
            if (m_CurrentFrame < 0) {
                m_CurrentFrame = (frameCount > 1) ? 1 : 0;
                m_PingPongReverse = false;
                Emit("looped");
            }
        } else {
            m_CurrentFrame++;
            if (m_CurrentFrame >= frameCount) {
                m_CurrentFrame = (frameCount > 1) ? frameCount - 2 : 0;
                m_PingPongReverse = true;
                Emit("looped");
            }
        }
    } else if (mode == LoopMode::Loop) {
        m_CurrentFrame++;
        if (m_CurrentFrame >= frameCount) {
            m_CurrentFrame = 0;
            Emit("looped");
        }
    } else {  // OneShot
        m_CurrentFrame++;
        if (m_CurrentFrame >= frameCount) {
            m_CurrentFrame = frameCount - 1;
            SetPropertyValue<bool>("playing", false);
            m_Finished = true;
            if (on_finished) on_finished();
            Emit("finished");
        }
    }

    if (m_CurrentFrame != oldFrame) {
        m_FrameDirty = true;
        if (on_frame_changed) on_frame_changed(m_CurrentFrame);
        Emit("frame_changed", { m_CurrentFrame });
    }
}

void GifPlayer::EnsureTextureUploaded(RenderContext& ctx) {
    IGfxDevice* device = ctx.getDevice();
    if (!device || !m_Loaded || !m_Gif.IsValid()) {
        return;
    }

    int idx = std::clamp(m_CurrentFrame, 0, m_Gif.FrameCount() - 1);
    const std::vector<uint8_t>& frame = m_Gif.frames[idx];

    if (!m_TextureCreated) {
        TextureDesc desc;
        desc.width = static_cast<uint32_t>(m_Gif.width);
        desc.height = static_cast<uint32_t>(m_Gif.height);
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = frame.data();
        m_TextureHandle = device->createTexture(desc);
        m_TextureCreated = true;
        m_FrameDirty = false;
        return;
    }

    if (m_FrameDirty && m_TextureHandle.isValid()) {
        device->updateTexture(m_TextureHandle, frame.data());
        m_FrameDirty = false;
    }
}

// ===== Rendering =====

Vec2 GifPlayer::CalculateRenderSize() const {
    if (!m_Loaded || !m_Gif.IsValid()) {
        return Vec2(100.0f, 100.0f);
    }
    Vec2 explicitSize = GetSize();
    if (explicitSize.x > 0.0f && explicitSize.y > 0.0f) {
        return explicitSize;
    }
    return Vec2(static_cast<float>(m_Gif.width), static_cast<float>(m_Gif.height));
}

void GifPlayer::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }
    if (!m_Loaded || !m_Gif.IsValid()) {
        return;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    EnsureTextureUploaded(ctx);
    if (!m_TextureHandle.isValid()) {
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

    Vec2 renderSize = CalculateRenderSize();
    renderSize.x *= scale.x;
    renderSize.y *= scale.y;

    SpriteDrawData sprite;
    sprite.texture = m_TextureHandle;
    sprite.position = position;
    sprite.size = renderSize;
    sprite.rotation = rotation;
    sprite.tint = GetModulate();

    Vec2 uvMin(0.0f, 0.0f);
    Vec2 uvMax(1.0f, 1.0f);
    if (GetFlipH()) std::swap(uvMin.x, uvMax.x);
    if (GetFlipV()) std::swap(uvMin.y, uvMax.y);
    sprite.uvMin = uvMin;
    sprite.uvMax = uvMax;
    sprite.pivot = Vec2(0.5f, 0.5f);

    ctx.drawSprite(sprite);
}

AABB GifPlayer::getWorldBounds() const {
    if (!m_Owner || !m_Loaded || !m_Gif.IsValid()) {
        return AABB();
    }
    core::Node2D* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    Vec2 position2D = node2D->GetGlobalPosition();
    Vec2 scale2D = node2D->GetGlobalScale();
    Vec2 offset = GetOffset();

    Vec2 renderSize = CalculateRenderSize();
    float w = renderSize.x * std::abs(scale2D.x);
    float h = renderSize.y * std::abs(scale2D.y);

    Vec3 center(position2D.x + offset.x, position2D.y + offset.y, 0.0f);
    Vec3 half(w * 0.5f, h * 0.5f, 0.0f);
    return AABB(center - half, center + half);
}

RenderLayer GifPlayer::getRenderLayer() const {
    // GIFs commonly carry per-pixel transparency, so always sort them as
    // transparent (back-to-front) for correct compositing.
    return RenderLayer::Transparent;
}

} // namespace components
} // namespace lupine
