#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/video/GifDecoder.hpp"
#include <functional>
#include <string>

namespace lupine {
namespace components {

/**
 * GifPlayer Component
 *
 * Plays back an animated GIF on a 2D node. The whole GIF is decoded up front
 * (via stb_image) and its frames are streamed to a GPU texture as playback
 * advances. Each GIF frame carries its own delay, but a fixed frame rate can be
 * forced with `fpsOverride`.
 *
 * Loop modes:
 *  - OneShot  : play through once and stop on the last frame
 *  - Loop     : repeat from the first frame forever
 *  - PingPong : bounce forward then backward, repeatedly
 *
 * Signals:
 *  - finished()            : emitted when a OneShot playback reaches the end
 *  - looped()              : emitted each time playback wraps (Loop / PingPong)
 *  - frame_changed(frame)  : emitted whenever the displayed frame changes
 */
class GifPlayer : public core::Component, public IRenderableComponent {
public:
    enum class LoopMode {
        OneShot = 0,
        Loop = 1,
        PingPong = 2
    };

    GifPlayer();
    virtual ~GifPlayer();

    // ISerializable interface
    std::string GetTypeName() const override { return "GifPlayer"; }
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
    void SetFrame(int frameIndex);

    // Restart from the first frame and play, even for a finished one-shot.
    // Exposed for editor preview ("replay" button) and for scripting.
    void Replay();

    // Getters
    std::string GetGifPath() const;
    bool IsPlaying() const;
    LoopMode GetLoopMode() const;
    bool GetAutoPlay() const;
    bool GetPreviewInEditor() const;
    float GetSpeed() const;
    float GetFpsOverride() const;
    const math::Vec2& GetOffset() const;
    const math::Vec2& GetSize() const;
    bool GetFlipH() const;
    bool GetFlipV() const;
    math::Color GetModulate() const;
    bool GetPixelSnap() const;
    int GetCurrentFrame() const { return m_CurrentFrame; }
    int GetFrameCount() const { return m_Gif.FrameCount(); }
    bool IsLoaded() const { return m_Loaded; }

    // Setters
    void SetGifPath(const std::string& path);
    void SetLoopMode(LoopMode mode);
    void SetAutoPlay(bool autoPlay);
    void SetPreviewInEditor(bool enabled);
    void SetSpeed(float speed);
    void SetFpsOverride(float fps);
    void SetOffset(const math::Vec2& offset);
    void SetSize(const math::Vec2& size);
    void SetFlipH(bool flip);
    void SetFlipV(bool flip);
    void SetModulate(const math::Color& color);
    void SetPixelSnap(bool snap);

    // Render size of the current frame (native frame size unless overridden).
    math::Vec2 CalculateRenderSize() const;

    // Optional native callbacks (mirrors AnimatedSprite2D; scripts use signals).
    std::function<void()> on_finished;
    std::function<void(int)> on_frame_changed;

private:
    void LoadGif();
    void ReleaseTexture();
    void AdvanceFrame();
    float CurrentFrameDelay() const;
    void EnsureTextureUploaded(RenderContext& ctx);

    video::GifData m_Gif;
    bool m_Loaded{false};
    std::string m_LoadedPath;

    int m_CurrentFrame{0};
    float m_FrameTimer{0.0f};
    bool m_PingPongReverse{false};
    bool m_Finished{false};

    TextureHandle m_TextureHandle;
    bool m_TextureCreated{false};
    bool m_FrameDirty{true};
};

} // namespace components
} // namespace lupine
