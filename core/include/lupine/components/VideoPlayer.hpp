#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "lupine/video/VideoDecoder.hpp"
#include <functional>
#include <string>
#include <vector>

namespace lupine {
namespace components {

/**
 * VideoPlayer Component
 *
 * Plays a video file on a 2D node. Video frames are decoded on demand (FFmpeg
 * backend) and streamed to a GPU texture; the optional audio track is decoded
 * once and routed through the engine's AudioManager so it mixes with the rest
 * of the game audio (volume, bus, mute).
 *
 * Supported containers/codecs depend on the FFmpeg build, but the default
 * stripped build decodes the common web/game formats (H.264/MP4, VP8/VP9/WebM,
 * MPEG, etc.). When the engine is built without the video backend the component
 * loads nothing and renders nothing (it never crashes).
 *
 * Playback:
 *  - loop = false : play once and stop on the final frame (emits finished())
 *  - loop = true  : restart from the beginning automatically
 *  - speed        : playback rate multiplier. Audio is only played at speed 1.0
 *                   (other speeds would pitch/desync the audio track), so the
 *                   video plays silently when speed != 1.0.
 *
 * Signals:
 *  - finished()           : emitted when non-looping playback reaches the end
 *  - looped()             : emitted each time looping playback restarts
 *  - playback_started()   : emitted when Play() begins playback
 */
class VideoPlayer : public core::Component, public IRenderableComponent {
public:
    VideoPlayer();
    virtual ~VideoPlayer();

    // ISerializable interface
    std::string GetTypeName() const override { return "VideoPlayer"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Component interface
    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // Script-callable methods (Lua / MRuby / MicroPython / C-API via ComponentRef).
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // IRenderableComponent interface
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World2D; }

    // Playback control
    void Play();
    void Stop();
    void Pause();
    void Resume();
    void Restart();

    // Restart playback from the beginning and play, regardless of loop mode.
    // Useful to replay a finished one-shot (e.g. an editor preview or on demand
    // from a script). Alias-like to Restart(); kept as a distinct intent.
    void Replay();

    // Seek to a time position in seconds (no-op unless seekEnabled is true).
    // Audio is stopped on seek to avoid desync; it resumes on the next Play/Replay.
    void Seek(double seconds);

    // Getters
    std::string GetVideoPath() const;
    bool IsPlaying() const;
    bool GetLoop() const;
    bool GetAutoPlay() const;
    bool GetSeekEnabled() const;
    bool GetPreviewInEditor() const;
    float GetSpeed() const;
    bool GetAudioEnabled() const;
    float GetVolume() const;
    std::string GetBus() const;
    bool GetMuted() const;
    const math::Vec2& GetOffset() const;
    const math::Vec2& GetSize() const;
    bool GetFlipH() const;
    bool GetFlipV() const;
    math::Color GetModulate() const;

    bool IsLoaded() const { return m_Opened; }
    double GetDuration() const;          // seconds
    double GetPlaybackPosition() const { return m_Clock; }  // seconds
    int GetVideoWidth() const;
    int GetVideoHeight() const;
    bool HasAudio() const;

    // Setters
    void SetVideoPath(const std::string& path);
    void SetLoop(bool loop);
    void SetAutoPlay(bool autoPlay);
    void SetSeekEnabled(bool enabled);
    void SetPreviewInEditor(bool enabled);
    void SetSpeed(float speed);
    void SetAudioEnabled(bool enabled);
    void SetVolume(float volume);
    void SetBus(const std::string& bus);
    void SetMuted(bool muted);
    void SetOffset(const math::Vec2& offset);
    void SetSize(const math::Vec2& size);
    void SetFlipH(bool flip);
    void SetFlipV(bool flip);
    void SetModulate(const math::Color& color);

    math::Vec2 CalculateRenderSize() const;

    // Optional native callbacks (scripts use signals instead).
    std::function<void()> on_finished;

private:
    void OpenVideo();
    void CloseVideo();
    void ReleaseTexture();
    void PromoteNextFrame();          // make the decoded-ahead frame the display frame
    void DecodeAhead();               // decode the next frame into m_NextFrame
    void HandleEndOfStream();
    void EnsureTextureUploaded(RenderContext& ctx);

    void StartAudio();
    void StopAudio();
    bool ShouldPlayAudio() const;

    video::VideoDecoder m_Decoder;
    bool m_Opened{false};
    std::string m_OpenedPath;

    double m_Clock{0.0};              // playback position in seconds (video master clock)
    int m_Width{0};
    int m_Height{0};

    std::vector<uint8_t> m_DisplayPixels;  // RGBA of the frame currently shown
    video::VideoFrameRGBA m_NextFrame;     // decoded-ahead frame, presented when due
    bool m_HasNext{false};
    bool m_Finished{false};

    TextureHandle m_TextureHandle;
    bool m_TextureCreated{false};
    bool m_FrameDirty{false};

    // Audio track (decoded once, played through AudioManager).
    asset::AssetRef<asset::AudioAsset> m_AudioAsset;
    bool m_AudioDecoded{false};
    core::UUID m_AudioSource{0};
};

} // namespace components
} // namespace lupine
