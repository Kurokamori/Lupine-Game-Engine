#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Gradient.hpp"
#include "lupine/math/Curve.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/components/ParticleTextures.hpp"
#include <string>
#include <vector>
#include <random>

namespace lupine {
namespace components {

/**
 * Particles2D Component
 *
 * A CPU-simulated 2D particle emitter.
 *
 * Each live particle is submitted to the renderer as a textured sprite (or a
 * solid colored quad when no texture is set), so it reuses the engine's
 * existing 2D rendering path on every graphics backend without any
 * backend-specific code.
 *
 * Features:
 * - Pooled, fixed-capacity particle buffer (no per-frame allocation)
 * - Continuous, one-shot, and explosive (burst) emission with explosiveness
 * - Point / Circle / Rectangle emission shapes
 * - Randomised lifetime, initial velocity, angle, angular velocity and scale
 * - Directional emission with angular spread
 * - Constant gravity and linear damping
 * - Color and scale interpolation over each particle's lifetime
 * - Local-space (particles follow the node) or world-space simulation
 * - Simulation speed scaling and start-up preprocess (warm-up)
 * - Optional particle texture with shared, hot-reloadable texture cache
 */
class Particles2D : public core::Component, public IRenderableComponent {
public:
    /**
     * Shape used to randomise the spawn position of new particles.
     */
    enum class EmissionShape {
        Point = 0,      ///< All particles spawn at the node origin.
        Circle = 1,     ///< Spawn inside a filled circle of emissionRadius.
        Rectangle = 2   ///< Spawn inside a rectangle of half-extents emissionExtents.
    };

    /**
     * Blend mode used when compositing particles. Additive requires a texture
     * (textureless particles always use alpha blending).
     */
    enum class BlendMode {
        Alpha = 0,      ///< Standard alpha blending.
        Additive = 1    ///< Additive blending (fire, sparks, glows).
    };

    Particles2D();
    explicit Particles2D(const std::string& name);
    virtual ~Particles2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Particles2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnReady() override;
    void OnUpdate(float deltaTime) override;

    // IRenderableComponent interface
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override { return RenderLayer::Transparent; }
    SpatialType getSpatialType() const override { return SpatialType::World2D; }

    // ===== Runtime control =====

    /**
     * Restart the emitter: clears all live particles, resets the emission
     * cycle, and re-runs the preprocess warm-up on the next update.
     */
    void Restart();

    /**
     * Emit a one-off burst of particles immediately, independent of the
     * emitting flag and emission cycle. Useful for impact/explosion effects.
     */
    void EmitBurst(int count);

    /**
     * Number of particles currently alive.
     */
    int GetAliveCount() const;

    // ===== Property accessors =====

    bool GetEmitting() const;
    void SetEmitting(bool emitting);

    int GetAmount() const;
    void SetAmount(int amount);

    const std::string& GetTexturePath() const;
    void SetTexturePath(const std::string& path);

    float GetLifetime() const;
    void SetLifetime(float seconds);

    float GetLifetimeRandomness() const;
    void SetLifetimeRandomness(float value);

    bool GetOneShot() const;
    void SetOneShot(bool oneShot);

    float GetExplosiveness() const;
    void SetExplosiveness(float value);

    float GetSpeedScale() const;
    void SetSpeedScale(float value);

    float GetPreprocess() const;
    void SetPreprocess(float seconds);

    bool GetLocalSpace() const;
    void SetLocalSpace(bool localSpace);

    EmissionShape GetEmissionShape() const;
    void SetEmissionShape(EmissionShape shape);

    float GetEmissionRadius() const;
    void SetEmissionRadius(float radius);

    const math::Vec2& GetEmissionExtents() const;
    void SetEmissionExtents(const math::Vec2& extents);

    const math::Vec2& GetDirection() const;
    void SetDirection(const math::Vec2& direction);

    float GetSpread() const;
    void SetSpread(float degrees);

    float GetInitialVelocityMin() const;
    void SetInitialVelocityMin(float value);

    float GetInitialVelocityMax() const;
    void SetInitialVelocityMax(float value);

    const math::Vec2& GetGravity() const;
    void SetGravity(const math::Vec2& gravity);

    float GetLinearDamping() const;
    void SetLinearDamping(float value);

    float GetAngularVelocityMin() const;
    void SetAngularVelocityMin(float value);

    float GetAngularVelocityMax() const;
    void SetAngularVelocityMax(float value);

    float GetInitialAngleMin() const;
    void SetInitialAngleMin(float value);

    float GetInitialAngleMax() const;
    void SetInitialAngleMax(float value);

    float GetScaleMin() const;
    void SetScaleMin(float value);

    float GetScaleMax() const;
    void SetScaleMax(float value);

    float GetScaleEnd() const;
    void SetScaleEnd(float value);

    const math::Vec2& GetParticleSize() const;
    void SetParticleSize(const math::Vec2& size);

    math::Color GetColorStart() const;
    void SetColorStart(const math::Color& color);

    math::Color GetColorEnd() const;
    void SetColorEnd(const math::Color& color);

    math::Color GetModulate() const;
    void SetModulate(const math::Color& color);

    BlendMode GetBlendMode() const;
    void SetBlendMode(BlendMode mode);

    /**
     * Default shape used when no texture is assigned. Square renders a solid
     * quad; Circle renders a soft-edged disc.
     */
    ParticleShape GetParticleShape() const;
    void SetParticleShape(ParticleShape shape);

    /**
     * Multi-stop color ramp sampled over each particle's lifetime. When it has
     * two or more stops it overrides the simple colorStart/colorEnd lerp.
     */
    math::Gradient GetColorGradient() const;
    void SetColorGradient(const math::Gradient& gradient);
    std::string GetColorGradientJson() const;
    void SetColorGradientJson(const std::string& json);

    /**
     * Curve scaling each particle's size over its lifetime. When it has at
     * least one point it overrides the simple scaleEnd ramp.
     */
    math::Curve GetScaleCurve() const;
    void SetScaleCurve(const math::Curve& curve);
    std::string GetScaleCurveJson() const;
    void SetScaleCurveJson(const std::string& json);

private:
    /**
     * One simulated particle. Position/velocity are expressed in world space
     * for world-space emitters and relative to the node origin for
     * local-space emitters.
     */
    struct Particle {
        bool alive = false;
        math::Vec2 position = math::Vec2(0.0f, 0.0f);
        math::Vec2 velocity = math::Vec2(0.0f, 0.0f);
        float rotation = 0.0f;          ///< Current rotation in radians.
        float angularVelocity = 0.0f;   ///< Radians per second.
        float age = 0.0f;               ///< Seconds since spawn.
        float lifetime = 1.0f;          ///< Total lifetime in seconds.
        float scale = 1.0f;             ///< Random per-particle scale multiplier.
    };

    void EnsureCapacity();
    void StepSimulation(float deltaTime);
    void SpawnParticle();
    int FindFreeSlot();
    float RandomRange(float minValue, float maxValue);
    float Random01();
    math::Vec2 SampleEmissionOffset();
    TextureHandle ResolveTexture(RenderContext& ctx);

    std::vector<Particle> m_Particles;
    std::mt19937 m_Rng;
    std::uniform_real_distribution<float> m_Dist;

    float m_CycleTime = 0.0f;       ///< Time elapsed in the current emission cycle.
    int m_EmitCursor = 0;           ///< Index of the next particle to emit this cycle.
    bool m_RuntimeEmitting = true;  ///< Internal flag (one-shot stops emission without clearing the property).
    bool m_NeedsPreprocess = true;  ///< Run the warm-up on the next update.

    TextureHandle m_TextureHandle;
    asset::AssetRef<asset::ImageAsset> m_TextureAsset;
    std::string m_CurrentTexturePath;

    // Cached parsed gradient/curve, re-parsed only when the JSON string changes.
    math::Gradient m_ColorGradient;
    std::string m_ColorGradientJson;
    math::Curve m_ScaleCurve;
    std::string m_ScaleCurveJson;
};

} // namespace components
} // namespace lupine
