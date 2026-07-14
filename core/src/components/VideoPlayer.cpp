#include "lupine/components/VideoPlayer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/MathCommon.hpp"

#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace math;

namespace {
constexpr uint32_t kVideoAudioSampleRate = 48000;  // matches the engine mixer default
constexpr uint32_t kVideoAudioChannels = 2;
}

VideoPlayer::VideoPlayer()
    : core::Component("VideoPlayer") {
}

VideoPlayer::~VideoPlayer() {
    StopAudio();
    ReleaseTexture();
}

void VideoPlayer::DefineProperties() {
    DefineProperty(PROPERTY_FILE_GROUP(videoPath, std::string(""),
        "*.mp4,*.m4v,*.mov,*.webm,*.mkv,*.avi,*.ogv,*.mpg,*.mpeg", "Video"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(autoPlay, Bool, true, "Playback"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(playing, Bool, true, "Playback"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, false, "Playback"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(speed, 1.0f, 0.1f, 4.0f, 0.05f, "Playback"));
    // seekEnabled: gate for Seek() — lets a game disable scrubbing entirely.
    DefineProperty(PROPERTY_DEFAULT_GROUP(seekEnabled, Bool, true, "Playback"));
    // previewInEditor: when true the video animates live in the editor viewport;
    // when false it stays paused on the first frame in the editor (overriding
    // autoPlay/playing). Has no effect at runtime.
    DefineProperty(PROPERTY_DEFAULT_GROUP(previewInEditor, Bool, true, "Playback"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(audioEnabled, Bool, true, "Audio"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(volume, 1.0f, 0.0f, 1.0f, 0.01f, "Audio"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(bus, String, std::string("Master"), "Audio"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(muted, Bool, false, "Audio"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, Vec2(0.0f, 0.0f), "Transform"));
    // size: (0,0) = use the video's native pixel dimensions.
    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec2, Vec2(0.0f, 0.0f), "Transform"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Rendering"));
}

void VideoPlayer::DefineSignals() {
    RegisterSignal({"finished", {}, "Emitted when non-looping playback reaches the end."});
    RegisterSignal({"looped", {}, "Emitted each time looping playback restarts."});
    RegisterSignal({"playback_started", {}, "Emitted when Play() begins playback."});
}

// ===== Getters =====

std::string VideoPlayer::GetVideoPath() const { return GetPropertyValue<std::string>("videoPath"); }
bool VideoPlayer::IsPlaying() const { return GetPropertyValue<bool>("playing"); }
bool VideoPlayer::GetLoop() const { return GetPropertyValue<bool>("loop"); }
bool VideoPlayer::GetAutoPlay() const { return GetPropertyValue<bool>("autoPlay"); }
bool VideoPlayer::GetSeekEnabled() const { return GetPropertyValue<bool>("seekEnabled"); }
bool VideoPlayer::GetPreviewInEditor() const { return GetPropertyValue<bool>("previewInEditor"); }
float VideoPlayer::GetSpeed() const { return GetPropertyValue<float>("speed"); }
bool VideoPlayer::GetAudioEnabled() const { return GetPropertyValue<bool>("audioEnabled"); }
float VideoPlayer::GetVolume() const { return GetPropertyValue<float>("volume"); }
std::string VideoPlayer::GetBus() const { return GetPropertyValue<std::string>("bus"); }
bool VideoPlayer::GetMuted() const { return GetPropertyValue<bool>("muted"); }
bool VideoPlayer::GetFlipH() const { return GetPropertyValue<bool>("flipH"); }
bool VideoPlayer::GetFlipV() const { return GetPropertyValue<bool>("flipV"); }

const Vec2& VideoPlayer::GetOffset() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("offset");
    return cached;
}
const Vec2& VideoPlayer::GetSize() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("size");
    return cached;
}
Color VideoPlayer::GetModulate() const {
    return GetPropertyValue<Color>("modulate");
}

double VideoPlayer::GetDuration() const { return m_Decoder.GetDuration(); }
int VideoPlayer::GetVideoWidth() const { return m_Width; }
int VideoPlayer::GetVideoHeight() const { return m_Height; }
bool VideoPlayer::HasAudio() const { return m_Decoder.HasAudio(); }

// ===== Setters =====

void VideoPlayer::SetVideoPath(const std::string& path) {
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
    SetPropertyValue<std::string>("videoPath", resPath);
    OpenVideo();
}

void VideoPlayer::SetLoop(bool loop) { SetPropertyValue<bool>("loop", loop); }
void VideoPlayer::SetAutoPlay(bool autoPlay) { SetPropertyValue<bool>("autoPlay", autoPlay); }
void VideoPlayer::SetSeekEnabled(bool enabled) { SetPropertyValue<bool>("seekEnabled", enabled); }
void VideoPlayer::SetPreviewInEditor(bool enabled) { SetPropertyValue<bool>("previewInEditor", enabled); }

void VideoPlayer::SetSpeed(float speed) {
    float clamped = math::Clamp(speed, 0.1f, 4.0f);
    bool wasOne = std::abs(GetSpeed() - 1.0f) <= 0.01f;
    bool nowOne = std::abs(clamped - 1.0f) <= 0.01f;
    SetPropertyValue<float>("speed", clamped);

    // Audio only tracks the video at 1x; (re)start or stop it when crossing 1x.
    if (IsPlaying()) {
        if (wasOne && !nowOne) {
            StopAudio();
        } else if (!wasOne && nowOne) {
            StartAudio();
        }
    }
}

void VideoPlayer::SetAudioEnabled(bool enabled) {
    SetPropertyValue<bool>("audioEnabled", enabled);
    if (!enabled) {
        StopAudio();
    } else if (IsPlaying()) {
        StartAudio();
    }
}

void VideoPlayer::SetVolume(float volume) {
    float clamped = math::Clamp(volume, 0.0f, 1.0f);
    SetPropertyValue<float>("volume", clamped);
    if (m_AudioSource.IsValid()) {
        audio::AudioManager::GetInstance().SetSourceVolume(
            m_AudioSource, GetMuted() ? 0.0f : clamped);
    }
}

void VideoPlayer::SetBus(const std::string& bus) {
    SetPropertyValue<std::string>("bus", bus);
    if (m_AudioSource.IsValid()) {
        audio::AudioManager::GetInstance().SetSourceBus(m_AudioSource, bus);
    }
}

void VideoPlayer::SetMuted(bool muted) {
    SetPropertyValue<bool>("muted", muted);
    if (m_AudioSource.IsValid()) {
        audio::AudioManager::GetInstance().SetSourceVolume(
            m_AudioSource, muted ? 0.0f : GetVolume());
    }
}

void VideoPlayer::SetOffset(const Vec2& offset) { SetPropertyValue<Vec2>("offset", offset); }
void VideoPlayer::SetSize(const Vec2& size) { SetPropertyValue<Vec2>("size", size); }
void VideoPlayer::SetFlipH(bool flip) { SetPropertyValue<bool>("flipH", flip); }
void VideoPlayer::SetFlipV(bool flip) { SetPropertyValue<bool>("flipV", flip); }
void VideoPlayer::SetModulate(const Color& color) { SetPropertyValue<Color>("modulate", color); }

// ===== Lifecycle =====

void VideoPlayer::OnAwake() {
    if (!GetVideoPath().empty()) {
        OpenVideo();
    }
}

void VideoPlayer::OnReady() {
    bool inEditor = false;
    if (m_Owner) {
        core::Scene* scene = m_Owner->GetScene();
        inEditor = scene && scene->IsInEditor();
    }

    if (inEditor) {
        // In the editor, playback is governed solely by previewInEditor — it
        // overrides autoPlay/playing. Audio never plays in the editor.
        SetPropertyValue<bool>("playing", GetPreviewInEditor() && m_Opened);
    } else if (GetAutoPlay() && m_Opened) {
        Play();
    } else {
        // Keep the first frame visible but paused.
        SetPropertyValue<bool>("playing", false);
    }
}

void VideoPlayer::OnUpdate(float deltaTime) {
    if (!m_Opened || !IsPlaying()) {
        return;
    }

    float speed = GetSpeed();
    if (speed <= 0.0f) {
        return;
    }

    m_Clock += static_cast<double>(deltaTime) * static_cast<double>(speed);

    int guard = 0;
    while (m_HasNext && m_NextFrame.pts <= m_Clock && guard < 16) {
        PromoteNextFrame();
        ++guard;
    }

    if (!m_HasNext) {
        HandleEndOfStream();
    }
}

void VideoPlayer::OnDestroy() {
    StopAudio();
    CloseVideo();
    ReleaseTexture();
}

void VideoPlayer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "videoPath") {
        StopAudio();
        m_AudioDecoded = false;
        m_AudioAsset.Reset();
        std::string newPath = newValue.is_string() ? newValue.get<std::string>() : std::string();
        if (!newPath.empty()) {
            OpenVideo();
        } else {
            CloseVideo();
        }
    } else if (propertyName == "muted") {
        if (m_AudioSource.IsValid()) {
            bool muted = newValue.is_boolean() ? newValue.get<bool>() : false;
            audio::AudioManager::GetInstance().SetSourceVolume(
                m_AudioSource, muted ? 0.0f : GetVolume());
        }
    } else if (propertyName == "previewInEditor") {
        // Apply the editor-preview toggle immediately (otherwise it would only
        // take effect on the next scene load). Runtime playback is unaffected.
        bool inEditor = false;
        if (m_Owner) {
            core::Scene* scene = m_Owner->GetScene();
            inEditor = scene && scene->IsInEditor();
        }
        if (inEditor) {
            bool preview = newValue.is_boolean() ? newValue.get<bool>() : true;
            if (preview && m_Opened) {
                SetPropertyValue<bool>("playing", true);
            } else {
                SetPropertyValue<bool>("playing", false);
                StopAudio();
            }
        }
    }
}

bool VideoPlayer::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string currentPath = GetVideoPath();
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
        bool wasPlaying = IsPlaying();
        StopAudio();
        m_AudioDecoded = false;
        m_AudioAsset.Reset();
        OpenVideo();
        if (wasPlaying && m_Opened) {
            Play();
        }
        return true;
    }
    return false;
}

// ===== Playback control =====

void VideoPlayer::Play() {
    if (!m_Opened) {
        OpenVideo();
        if (!m_Opened) {
            return;
        }
    }

    if (m_Finished) {
        m_Decoder.Rewind();
        m_Clock = 0.0;
        m_Finished = false;
        video::VideoFrameRGBA first;
        if (m_Decoder.DecodeNextFrame(first)) {
            m_DisplayPixels = std::move(first.pixels);
            m_Width = first.width;
            m_Height = first.height;
            m_FrameDirty = true;
        }
        DecodeAhead();
    }

    SetPropertyValue<bool>("playing", true);
    StartAudio();
    Emit("playback_started");
}

void VideoPlayer::Stop() {
    SetPropertyValue<bool>("playing", false);
    StopAudio();

    if (m_Opened) {
        m_Decoder.Rewind();
        m_Clock = 0.0;
        m_Finished = false;
        video::VideoFrameRGBA first;
        if (m_Decoder.DecodeNextFrame(first)) {
            m_DisplayPixels = std::move(first.pixels);
            m_Width = first.width;
            m_Height = first.height;
            m_FrameDirty = true;
        }
        DecodeAhead();
    }
}

void VideoPlayer::Pause() {
    SetPropertyValue<bool>("playing", false);
    if (m_AudioSource.IsValid()) {
        audio::AudioManager::GetInstance().Pause(m_AudioSource);
    }
}

void VideoPlayer::Resume() {
    if (!m_Opened || m_Finished) {
        return;
    }
    SetPropertyValue<bool>("playing", true);
    if (m_AudioSource.IsValid()) {
        audio::AudioManager::GetInstance().Resume(m_AudioSource);
    } else {
        StartAudio();
    }
}

void VideoPlayer::Restart() {
    if (!m_Opened) {
        OpenVideo();
        if (!m_Opened) {
            return;
        }
    }
    StopAudio();
    m_Decoder.Rewind();
    m_Clock = 0.0;
    m_Finished = false;
    video::VideoFrameRGBA first;
    if (m_Decoder.DecodeNextFrame(first)) {
        m_DisplayPixels = std::move(first.pixels);
        m_Width = first.width;
        m_Height = first.height;
        m_FrameDirty = true;
    }
    DecodeAhead();
    SetPropertyValue<bool>("playing", true);
    StartAudio();
    Emit("playback_started");
}

nlohmann::json VideoPlayer::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto numArg = [&](float fallback) -> float {
        if (args.is_array() && !args.empty() && args[0].is_number()) {
            return args[0].get<float>();
        }
        return fallback;
    };
    auto boolArg = [&](bool fallback) -> bool {
        if (args.is_array() && !args.empty() && args[0].is_boolean()) {
            return args[0].get<bool>();
        }
        return fallback;
    };

    if (method == "play") { Play(); return nlohmann::json(); }
    if (method == "stop") { Stop(); return nlohmann::json(); }
    if (method == "pause") { Pause(); return nlohmann::json(); }
    if (method == "resume") { Resume(); return nlohmann::json(); }
    if (method == "restart") { Restart(); return nlohmann::json(); }
    if (method == "replay") { Replay(); return nlohmann::json(); }
    if (method == "seek") { Seek(static_cast<double>(numArg(0.0f))); return nlohmann::json(); }
    if (method == "set_seek_enabled") { SetSeekEnabled(boolArg(GetSeekEnabled())); return nlohmann::json(); }
    if (method == "get_seek_enabled") { return GetSeekEnabled(); }
    if (method == "set_volume") { SetVolume(numArg(GetVolume())); return nlohmann::json(); }
    if (method == "set_muted") { SetMuted(boolArg(GetMuted())); return nlohmann::json(); }
    if (method == "set_speed") { SetSpeed(numArg(GetSpeed())); return nlohmann::json(); }
    if (method == "set_bus") {
        if (args.is_array() && !args.empty() && args[0].is_string()) {
            SetBus(args[0].get<std::string>());
        }
        return nlohmann::json();
    }
    if (method == "get_position") { return GetPlaybackPosition(); }
    if (method == "get_duration") { return GetDuration(); }
    if (method == "get_volume") { return GetVolume(); }
    if (method == "is_playing") { return IsPlaying(); }
    if (method == "is_loaded") { return IsLoaded(); }
    if (method == "has_audio") { return HasAudio(); }
    if (method == "get_video_width") { return GetVideoWidth(); }
    if (method == "get_video_height") { return GetVideoHeight(); }
    return nlohmann::json();
}

void VideoPlayer::Replay() {
    Restart();
}

void VideoPlayer::Seek(double seconds) {
    if (!m_Opened || !GetSeekEnabled()) {
        return;
    }
    // Audio cannot follow an arbitrary seek (it is a single preloaded source);
    // stop it so the picture and sound never drift. It resumes on Play/Replay.
    StopAudio();

    if (!m_Decoder.Seek(seconds)) {
        return;
    }
    m_Finished = false;
    m_Clock = seconds;

    video::VideoFrameRGBA first;
    if (m_Decoder.DecodeNextFrame(first)) {
        m_DisplayPixels = std::move(first.pixels);
        m_Width = first.width;
        m_Height = first.height;
        // Align the clock to the frame we actually landed on (nearest keyframe).
        m_Clock = first.pts;
        m_FrameDirty = true;
    }
    DecodeAhead();
}

// ===== Internal =====

void VideoPlayer::OpenVideo() {
    CloseVideo();

    std::string path = GetVideoPath();
    if (path.empty()) {
        return;
    }

    if (!video::VideoDecoder::IsBackendAvailable()) {
        LOG_WARN(LogCategory::Core,
                 "VideoPlayer: video backend not built (LUPINE_ENABLE_VIDEO off); cannot play {}", path);
        return;
    }

    if (!m_Decoder.Open(path)) {
        LOG_ERROR(LogCategory::Core, "VideoPlayer: failed to open {}", path);
        return;
    }

    m_Opened = true;
    m_OpenedPath = path;
    m_Width = m_Decoder.GetWidth();
    m_Height = m_Decoder.GetHeight();
    m_Clock = 0.0;
    m_Finished = false;
    m_AudioDecoded = false;
    m_AudioAsset.Reset();

    // Decode the first frame so the component shows content immediately (even
    // paused in the editor), then decode one frame ahead for presentation timing.
    video::VideoFrameRGBA first;
    if (m_Decoder.DecodeNextFrame(first)) {
        m_DisplayPixels = std::move(first.pixels);
        m_Width = first.width;
        m_Height = first.height;
        m_FrameDirty = true;
    }
    DecodeAhead();
}

void VideoPlayer::CloseVideo() {
    m_Decoder.Close();
    m_Opened = false;
    m_HasNext = false;
    m_Finished = false;
    m_Clock = 0.0;
    m_DisplayPixels.clear();
    m_NextFrame = video::VideoFrameRGBA();
    ReleaseTexture();
}

void VideoPlayer::ReleaseTexture() {
    m_TextureHandle = TextureHandle();
    m_TextureCreated = false;
    m_FrameDirty = true;
}

void VideoPlayer::DecodeAhead() {
    m_HasNext = m_Decoder.DecodeNextFrame(m_NextFrame);
}

void VideoPlayer::PromoteNextFrame() {
    m_DisplayPixels = std::move(m_NextFrame.pixels);
    m_Width = m_NextFrame.width;
    m_Height = m_NextFrame.height;
    m_FrameDirty = true;
    DecodeAhead();
}

void VideoPlayer::HandleEndOfStream() {
    if (GetLoop()) {
        m_Decoder.Rewind();
        m_Clock = 0.0;
        video::VideoFrameRGBA first;
        if (m_Decoder.DecodeNextFrame(first)) {
            m_DisplayPixels = std::move(first.pixels);
            m_Width = first.width;
            m_Height = first.height;
            m_FrameDirty = true;
        }
        DecodeAhead();
        // Audio is played in Loop mode for looping video, so it keeps running.
        Emit("looped");
    } else {
        SetPropertyValue<bool>("playing", false);
        m_Finished = true;
        StopAudio();
        if (on_finished) on_finished();
        Emit("finished");
    }
}

bool VideoPlayer::ShouldPlayAudio() const {
    if (!m_Opened || !GetAudioEnabled() || !m_Decoder.HasAudio()) {
        return false;
    }
    // Audio is only correct at native playback rate.
    if (std::abs(GetSpeed() - 1.0f) > 0.01f) {
        return false;
    }
    // Never play audio while editing in the editor viewport.
    if (m_Owner) {
        core::Scene* scene = m_Owner->GetScene();
        if (scene && scene->IsInEditor()) {
            return false;
        }
    }
    return true;
}

void VideoPlayer::StartAudio() {
    if (!ShouldPlayAudio()) {
        return;
    }
    if (m_AudioSource.IsValid()) {
        return;  // already playing
    }

    if (!m_AudioDecoded) {
        video::VideoAudioPCM pcm;
        if (m_Decoder.DecodeAudioPCM(kVideoAudioSampleRate, kVideoAudioChannels, pcm) && pcm.IsValid()) {
            auto* audioAsset = new asset::AudioAsset();
            if (audioAsset->LoadFromPCM(pcm.data.data(), pcm.data.size(),
                                        pcm.sampleRate, pcm.channels, 16)) {
                m_AudioAsset = asset::AssetRef<asset::AudioAsset>(audioAsset);
            } else {
                audioAsset->Release();
            }
        }
        m_AudioDecoded = true;
    }

    if (!m_AudioAsset || !m_AudioAsset->IsLoaded()) {
        return;
    }

    audio::AudioManager& mgr = audio::AudioManager::GetInstance();
    audio::PlaybackMode mode = GetLoop() ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;
    float volume = GetMuted() ? 0.0f : GetVolume();
    m_AudioSource = mgr.Play(m_AudioAsset, GetBus(), mode, volume);
}

void VideoPlayer::StopAudio() {
    if (m_AudioSource.IsValid()) {
        audio::AudioManager::GetInstance().Stop(m_AudioSource);
        m_AudioSource = core::UUID(0);
    }
}

void VideoPlayer::EnsureTextureUploaded(RenderContext& ctx) {
    IGfxDevice* device = ctx.getDevice();
    if (!device || m_Width <= 0 || m_Height <= 0) {
        return;
    }

    const size_t expectedBytes = static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) * 4u;
    if (m_DisplayPixels.size() < expectedBytes) {
        return;  // no frame decoded yet
    }

    if (!m_TextureCreated) {
        TextureDesc desc;
        desc.width = static_cast<uint32_t>(m_Width);
        desc.height = static_cast<uint32_t>(m_Height);
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = m_DisplayPixels.data();
        m_TextureHandle = device->createTexture(desc);
        m_TextureCreated = true;
        m_FrameDirty = false;
        return;
    }

    if (m_FrameDirty && m_TextureHandle.isValid()) {
        device->updateTexture(m_TextureHandle, m_DisplayPixels.data());
        m_FrameDirty = false;
    }
}

// ===== Rendering =====

Vec2 VideoPlayer::CalculateRenderSize() const {
    if (m_Width <= 0 || m_Height <= 0) {
        return Vec2(100.0f, 100.0f);
    }
    Vec2 explicitSize = GetSize();
    if (explicitSize.x > 0.0f && explicitSize.y > 0.0f) {
        return explicitSize;
    }
    return Vec2(static_cast<float>(m_Width), static_cast<float>(m_Height));
}

void VideoPlayer::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner || !m_Opened) {
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

AABB VideoPlayer::getWorldBounds() const {
    if (!m_Owner || !m_Opened) {
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

RenderLayer VideoPlayer::getRenderLayer() const {
    // Video frames are opaque RGB; modulate alpha can introduce transparency.
    return (GetModulate().a >= 1.0f) ? RenderLayer::Opaque : RenderLayer::Transparent;
}

} // namespace components
} // namespace lupine
