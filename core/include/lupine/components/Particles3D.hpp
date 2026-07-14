#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
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
 * Particles3D Component
 *
 * A CPU-simulated 3D particle emitter. Each live particle is rendered as a
 * camera-facing (billboarded) textured quad via the standard 3D mesh path, so
 * it reuses the engine's existing rendering on every graphics backend.
 *
 * Shares its simulation model with Particles2D but operates in 3D world space
 * with sphere/box emission volumes and cone-shaped directional emission.
 *
 * Features:
 * - Pooled, fixed-capacity particle buffer (no per-frame allocation)
 * - Continuous, one-shot, and explosive (burst) emission with explosiveness
 * - Point / Sphere / Box emission shapes
 * - Cone directional emission with angular spread
 * - Constant gravity and linear damping
 * - Per-particle roll (rotation around the view axis) with angular velocity
 * - Color and scale interpolation over each particle's lifetime
 * - Billboard modes (full / Y-axis only / disabled), alpha or additive blend
 * - Local-space (particles follow the node) or world-space simulation
 */
class Particles3D : public core::Component, public IRenderableComponent {
public:
    /**
     * Volume used to randomise the spawn position of new particles.
     */
    enum class EmissionShape {
        Point = 0,      ///< All particles spawn at the node origin.
        Sphere = 1,     ///< Spawn inside a solid sphere of emissionRadius.
        Box = 2         ///< Spawn inside a box of half-extents emissionExtents.
    };

    /**
     * How each particle quad is oriented relative to the camera.
     */
    enum class BillboardMode {
        Disabled = 0,   ///< Quad lies flat, facing +Y (e.g. ground decals).
        Enabled = 1,    ///< Full billboard: always faces the camera.
        YAxisOnly = 2   ///< Billboard around the Y axis only (upright sprites).
    };

    /**
     * Blend mode used when compositing particles.
     */
    enum class BlendMode {
        Alpha = 0,      ///< Standard alpha blending.
        Additive = 1    ///< Additive blending (fire, sparks, glows).
    };

    Particles3D();
    explicit Particles3D(const std::string& name);
    virtual ~Particles3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Particles3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnReady() override;
    void OnUpdate(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // IRenderableComponent interface
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override { return RenderLayer::Transparent; }
    SpatialType getSpatialType() const override { return SpatialType::World3D; }

    // ===== Runtime control =====

    /** Restart the emitter: clear particles, reset the cycle, re-run preprocess. */
    void Restart();
    /** Emit a one-off burst immediately, independent of the emission cycle. */
    void EmitBurst(int count);
    /** Number of particles currently alive. */
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

    const math::Vec3& GetEmissionExtents() const;
    void SetEmissionExtents(const math::Vec3& extents);

    const math::Vec3& GetDirection() const;
    void SetDirection(const math::Vec3& direction);

    float GetSpread() const;
    void SetSpread(float degrees);

    float GetInitialVelocityMin() const;
    void SetInitialVelocityMin(float value);

    float GetInitialVelocityMax() const;
    void SetInitialVelocityMax(float value);

    const math::Vec3& GetGravity() const;
    void SetGravity(const math::Vec3& gravity);

    float GetLinearDamping() const;
    void SetLinearDamping(float value);

    float GetInitialAngleMin() const;
    void SetInitialAngleMin(float value);

    float GetInitialAngleMax() const;
    void SetInitialAngleMax(float value);

    float GetAngularVelocityMin() const;
    void SetAngularVelocityMin(float value);

    float GetAngularVelocityMax() const;
    void SetAngularVelocityMax(float value);

    BillboardMode GetBillboardMode() const;
    void SetBillboardMode(BillboardMode mode);

    BlendMode GetBlendMode() const;
    void SetBlendMode(BlendMode mode);

    /**
     * Default shape used when no texture is assigned. Square renders a solid
     * quad; Circle renders a soft-edged disc via a built-in texture.
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

    bool GetDoubleSided() const;
    void SetDoubleSided(bool doubleSided);

    const math::Vec2& GetParticleSize() const;
    void SetParticleSize(const math::Vec2& size);

    float GetScaleMin() const;
    void SetScaleMin(float value);

    float GetScaleMax() const;
    void SetScaleMax(float value);

    float GetScaleEnd() const;
    void SetScaleEnd(float value);

    math::Color GetColorStart() const;
    void SetColorStart(const math::Color& color);

    math::Color GetColorEnd() const;
    void SetColorEnd(const math::Color& color);

    math::Color GetModulate() const;
    void SetModulate(const math::Color& color);

private:
    /**
     * One simulated particle. Position/velocity are world-space for world-space
     * emitters and relative to the node origin for local-space emitters.
     */
    struct Particle {
        bool alive = false;
        math::Vec3 position = math::Vec3(0.0f, 0.0f, 0.0f);
        math::Vec3 velocity = math::Vec3(0.0f, 0.0f, 0.0f);
        float roll = 0.0f;              ///< Current roll around the view axis (radians).
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
    math::Vec3 SampleEmissionOffset();
    math::Vec3 SampleConeDirection(const math::Vec3& axis, float spreadRadians);
    TextureHandle ResolveTexture(RenderContext& ctx);

    std::vector<Particle> m_Particles;
    std::mt19937 m_Rng;
    std::uniform_real_distribution<float> m_Dist;

    float m_CycleTime = 0.0f;
    int m_EmitCursor = 0;
    bool m_RuntimeEmitting = true;
    bool m_NeedsPreprocess = true;

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
