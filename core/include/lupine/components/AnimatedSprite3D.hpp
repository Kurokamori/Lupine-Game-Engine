#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/asset/SpriteAnimationAsset.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/math/AABB.hpp"
#include "lupine/components/Sprite3D.hpp"  // For BillboardMode enum
#include <string>
#include <functional>

namespace lupine {
namespace components {

/**
 * AnimatedSprite3D Component
 * 
 * Displays animated 3D sprites using sprite animation files (.spriteanimation).
 * Supports multiple named animations with configurable playback speed and loop modes.
 * Renders as a textured quad in 3D space with optional billboarding.
 * 
 * Properties:
 * - Animation: Name of the animation to play
 * - Playing: Whether the animation is currently playing
 * - Loop: Whether to loop the animation (overrides animation's loop mode)
 * - Auto Play: Whether to start playing automatically
 * - Offset: Position offset for rendering
 * - Flip H/V: Horizontal and vertical flipping
 * - Modulate: Color tint
 * - Billboard: Billboard mode (Disabled, Enabled, YAxisOnly)
 * - Cast Shadows: Whether to cast shadows
 * - Receive Shadows: Whether to receive shadows
 * 
 * Callbacks:
 * - on_animation_finished(): Called when animation completes (non-looping)
 * - on_frame_changed(int frameIndex): Called when frame changes
 */
class AnimatedSprite3D : public core::Component, public IRenderableComponent {
public:
    AnimatedSprite3D();
    virtual ~AnimatedSprite3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "AnimatedSprite3D"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Component interface
    void OnUpdate(float deltaTime) override;
    void OnAwake() override;
    void OnReady() override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // IRenderableComponent interface
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override { return RenderLayer::Opaque; }
    SpatialType getSpatialType() const override { return SpatialType::World3D; }
    
    // Getters
    std::string GetAnimationFilePath() const;
    std::string GetAnimationName() const;
    bool IsPlaying() const;
    bool GetLoop() const;
    bool GetAutoPlay() const;
    const math::Vec3& GetOffset() const;
    bool GetFlipH() const;
    bool GetFlipV() const;
    math::Color GetModulate() const;
    BillboardMode GetBillboard() const;
    bool GetCastShadows() const;
    bool GetReceiveShadows() const;
    bool GetDoubleSided() const;
    int GetCurrentFrame() const { return m_CurrentFrameIndex; }

    // Setters
    void SetAnimationFilePath(const std::string& filepath);
    void SetAnimationName(const std::string& name);
    void SetPlaying(bool playing);
    void SetLoop(bool loop);
    void SetAutoPlay(bool autoPlay);
    void SetOffset(const math::Vec3& offset);
    void SetFlipH(bool flip);
    void SetFlipV(bool flip);
    void SetModulate(const math::Color& color);
    void SetBillboard(BillboardMode mode);
    void SetCastShadows(bool cast);
    void SetReceiveShadows(bool receive);
    void SetDoubleSided(bool doubleSided);

    // Animation control (convenience methods)
    void Play(const std::string& animationName = "");
    void Stop();
    void Pause();
    void Resume();
    void SetFrame(int frameIndex);
    
    // Callbacks (can be overridden in scripts)
    std::function<void()> on_animation_finished;
    std::function<void(int)> on_frame_changed;
    
private:
    // Animation asset
    asset::AssetRef<asset::SpriteAnimationAsset> m_AnimationAsset;

    // Current animation state
    const asset::SpriteAnimationClip* m_CurrentClip;
    int m_CurrentFrameIndex;
    float m_FrameTimer;
    bool m_PingPongReverse;

    // Cached texture for current frame
    asset::AssetRef<asset::ImageAsset> m_CurrentFrameTexture;
    TextureHandle m_TextureHandle;
    bool m_TextureUploaded;

    // Helper methods
    void LoadAnimationAsset();
    void UpdateAnimation(float deltaTime);
    void AdvanceFrame();
    void UpdateCurrentFrameTexture();
    void UploadTexture(RenderContext& ctx);
    math::Mat4 CalculateBillboardTransform(const math::Mat4& viewMatrix) const;
};

} // namespace components
} // namespace lupine

