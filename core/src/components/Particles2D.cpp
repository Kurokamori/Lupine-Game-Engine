#include "lupine/components/Particles2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

constexpr float kFixedWarmupStep = 1.0f / 60.0f;
constexpr float kMaxFrameStep = 0.1f;

// Shared, hot-reloadable texture cache for all Particles2D instances. Mirrors
// the Sprite2D cache so particle textures are uploaded to the GPU exactly once
// and shared across every emitter that references the same path.
std::unordered_map<std::string, TextureHandle>& GetParticleTextureCache() {
    static std::unordered_map<std::string, TextureHandle> s_Cache;
    return s_Cache;
}

std::mutex& GetParticleTextureCacheMutex() {
    static std::mutex s_Mutex;
    return s_Mutex;
}

void EnsureParticleCacheRegistered() {
    static bool s_Registered = false;
    if (s_Registered) {
        return;
    }
    s_Registered = true;

    rendering::TextureCache::RegisterCache(
        "Particles2D",
        [](const std::string& path) -> bool {
            std::lock_guard<std::mutex> lock(GetParticleTextureCacheMutex());
            return GetParticleTextureCache().erase(path) > 0;
        },
        []() {
            std::lock_guard<std::mutex> lock(GetParticleTextureCacheMutex());
            GetParticleTextureCache().clear();
        });
}

TextureHandle GetOrCreateParticleTexture(
    IGfxDevice* device,
    const std::string& path,
    asset::AssetRef<asset::ImageAsset>& assetRef)
{
    if (path.empty() || !device) {
        return TextureHandle();
    }

    EnsureParticleCacheRegistered();

    {
        std::lock_guard<std::mutex> lock(GetParticleTextureCacheMutex());
        auto it = GetParticleTextureCache().find(path);
        if (it != GetParticleTextureCache().end() && it->second.isValid()) {
            return it->second;
        }
    }

    if (!assetRef.IsValid()) {
        assetRef = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
        if (!assetRef->LoadFromFile(path, true, asset::ImageColorSpace::sRGB)) {
            LOG_ERROR(LogCategory::Render, "Particles2D FAILED to load texture: {}", path);
            assetRef.Reset();
            return TextureHandle();
        }
    }

    if (!assetRef->IsLoaded() || assetRef->GetWidth() == 0 ||
        assetRef->GetHeight() == 0 || assetRef->GetData() == nullptr) {
        return TextureHandle();
    }

    TextureHandle handle = lupine::CreateTexture2DFromImage(device, *assetRef, TextureFormat::RGBA8_UNORM);

    {
        std::lock_guard<std::mutex> lock(GetParticleTextureCacheMutex());
        GetParticleTextureCache()[path] = handle;
    }

    return handle;
}

Color LerpColor(const Color& a, const Color& b, float t) {
    return Color(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t);
}

float LerpFloat(float a, float b, float t) {
    return a + (b - a) * t;
}

} // anonymous namespace

Particles2D::Particles2D()
    : Component("Particles2D")
    , m_Rng(std::random_device{}())
    , m_Dist(0.0f, 1.0f)
{
}

Particles2D::Particles2D(const std::string& name)
    : Component(name)
    , m_Rng(std::random_device{}())
    , m_Dist(0.0f, 1.0f)
{
}

Particles2D::~Particles2D() = default;

void Particles2D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(emitting, Bool, true, "Emission"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(amount, 32, 1, 65536, 1, "Emission"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(oneShot, Bool, false, "Emission"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(explosiveness, 0.0f, 0.0f, 1.0f, 0.01f, "Emission"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(speedScale, 1.0f, 0.0f, 64.0f, 0.01f, "Emission"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(preprocess, 0.0f, 0.0f, 60.0f, 0.1f, "Emission"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(localSpace, Bool, false, "Emission"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lifetime, 1.0f, 0.01f, 600.0f, 0.01f, "Lifetime"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lifetimeRandomness, 0.0f, 0.0f, 1.0f, 0.01f, "Lifetime"));

    DefineProperty(PROPERTY_ENUM_GROUP(emissionShape, 0, "Shape", Point, Circle, Rectangle));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(emissionRadius, 0.0f, 0.0f, 8192.0f, 1.0f, "Shape"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(emissionExtents, Vec2, Vec2(0.0f, 0.0f), "Shape"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(direction, Vec2, Vec2(0.0f, -1.0f), "Velocity"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(spread, 45.0f, 0.0f, 180.0f, 1.0f, "Velocity"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(initialVelocityMin, 0.0f, 0.0f, 100000.0f, 1.0f, "Velocity"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(initialVelocityMax, 100.0f, 0.0f, 100000.0f, 1.0f, "Velocity"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(gravity, Vec2, Vec2(0.0f, 200.0f), "Velocity"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(linearDamping, 0.0f, 0.0f, 100.0f, 0.01f, "Velocity"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(initialAngleMin, 0.0f, -360.0f, 360.0f, 1.0f, "Rotation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(initialAngleMax, 0.0f, -360.0f, 360.0f, 1.0f, "Rotation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(angularVelocityMin, 0.0f, -3600.0f, 3600.0f, 1.0f, "Rotation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(angularVelocityMax, 0.0f, -3600.0f, 3600.0f, 1.0f, "Rotation"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(particleSize, Vec2, Vec2(8.0f, 8.0f), "Display"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scaleMin, 1.0f, 0.0f, 64.0f, 0.01f, "Display"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scaleMax, 1.0f, 0.0f, 64.0f, 0.01f, "Display"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scaleEnd, 1.0f, 0.0f, 64.0f, 0.01f, "Display"));
    DefineProperty(PROPERTY_FILE_GROUP(texturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorStart, Color, Color::White(), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorEnd, Color, Color(1.0f, 1.0f, 1.0f, 0.0f), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Display"));
    DefineProperty(PROPERTY_ENUM_GROUP(blendMode, 0, "Display", Alpha, Additive));
    DefineProperty(PROPERTY_ENUM_GROUP(particleShape, 0, "Display", Square, Circle));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorGradient, String, std::string(""), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scaleCurve, String, std::string(""), "Display"));
}

void Particles2D::OnReady() {
    EnsureCapacity();
    m_RuntimeEmitting = GetEmitting();
    m_NeedsPreprocess = true;
}

void Particles2D::EnsureCapacity() {
    int amount = GetAmount();
    if (amount < 1) {
        amount = 1;
    }
    if (static_cast<int>(m_Particles.size()) != amount) {
        m_Particles.assign(static_cast<size_t>(amount), Particle());
        m_EmitCursor = std::min(m_EmitCursor, amount);
    }
}

float Particles2D::Random01() {
    return m_Dist(m_Rng);
}

float Particles2D::RandomRange(float minValue, float maxValue) {
    return minValue + (maxValue - minValue) * Random01();
}

void Particles2D::OnUpdate(float deltaTime) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    EnsureCapacity();

    float speedScale = GetSpeedScale();
    if (speedScale < 0.0f) {
        speedScale = 0.0f;
    }

    if (m_NeedsPreprocess) {
        m_NeedsPreprocess = false;
        float warmup = GetPreprocess();
        while (warmup > 0.0f) {
            float step = std::min(warmup, kFixedWarmupStep);
            StepSimulation(step);
            warmup -= step;
        }
    }

    float scaledDt = deltaTime * speedScale;
    // Clamp the per-frame step so a hitch or breakpoint cannot teleport every
    // particle through its whole lifetime in a single frame.
    if (scaledDt > kMaxFrameStep) {
        scaledDt = kMaxFrameStep;
    }
    if (scaledDt <= 0.0f) {
        return;
    }

    StepSimulation(scaledDt);
}

void Particles2D::StepSimulation(float deltaTime) {
    const Vec2 gravity = GetGravity();
    const float damping = GetLinearDamping();
    const float dampFactor = std::max(0.0f, 1.0f - damping * deltaTime);

    for (Particle& p : m_Particles) {
        if (!p.alive) {
            continue;
        }
        p.age += deltaTime;
        if (p.age >= p.lifetime) {
            p.alive = false;
            continue;
        }
        p.velocity = p.velocity + gravity * deltaTime;
        if (damping > 0.0f) {
            p.velocity = p.velocity * dampFactor;
        }
        p.position = p.position + p.velocity * deltaTime;
        p.rotation += p.angularVelocity * deltaTime;
    }

    if (!m_RuntimeEmitting) {
        return;
    }

    const int amount = static_cast<int>(m_Particles.size());
    const float cycleLength = std::max(GetLifetime(), 0.0001f);
    const float explosiveness = std::clamp(GetExplosiveness(), 0.0f, 1.0f);
    const float spreadTime = cycleLength * (1.0f - explosiveness);

    m_CycleTime += deltaTime;

    while (m_EmitCursor < amount) {
        float fraction = (amount > 1)
            ? static_cast<float>(m_EmitCursor) / static_cast<float>(amount)
            : 0.0f;
        float emitTime = fraction * spreadTime;
        if (emitTime <= m_CycleTime) {
            SpawnParticle();
            ++m_EmitCursor;
        } else {
            break;
        }
    }

    if (m_CycleTime >= cycleLength) {
        m_CycleTime = std::fmod(m_CycleTime, cycleLength);
        m_EmitCursor = 0;
        if (GetOneShot()) {
            m_RuntimeEmitting = false;
        }
    }
}

int Particles2D::FindFreeSlot() {
    int oldestIndex = -1;
    float oldestAgeFraction = -1.0f;
    for (int i = 0; i < static_cast<int>(m_Particles.size()); ++i) {
        const Particle& p = m_Particles[i];
        if (!p.alive) {
            return i;
        }
        float ageFraction = (p.lifetime > 0.0f) ? (p.age / p.lifetime) : 1.0f;
        if (ageFraction > oldestAgeFraction) {
            oldestAgeFraction = ageFraction;
            oldestIndex = i;
        }
    }
    // No free slot: recycle the particle nearest the end of its life so the
    // emitter never silently drops emission when overlapping cycles fill the
    // pool (e.g. with high lifetime randomness).
    return oldestIndex;
}

Vec2 Particles2D::SampleEmissionOffset() {
    EmissionShape shape = GetEmissionShape();
    switch (shape) {
        case EmissionShape::Circle: {
            float radius = GetEmissionRadius();
            float angle = Random01() * 2.0f * kPi;
            float dist = std::sqrt(Random01()) * radius;
            return Vec2(std::cos(angle) * dist, std::sin(angle) * dist);
        }
        case EmissionShape::Rectangle: {
            const Vec2 extents = GetEmissionExtents();
            return Vec2(RandomRange(-extents.x, extents.x),
                        RandomRange(-extents.y, extents.y));
        }
        case EmissionShape::Point:
        default:
            return Vec2(0.0f, 0.0f);
    }
}

void Particles2D::SpawnParticle() {
    int slot = FindFreeSlot();
    if (slot < 0) {
        return;
    }
    Particle& p = m_Particles[slot];

    const bool localSpace = GetLocalSpace();
    Vec2 origin(0.0f, 0.0f);
    if (!localSpace) {
        if (Node2D* node2D = dynamic_cast<Node2D*>(m_Owner)) {
            origin = node2D->GetGlobalPosition();
        }
    }

    Vec2 offset = SampleEmissionOffset();
    p.position = origin + offset;

    Vec2 dir = GetDirection();
    float baseAngle = std::atan2(dir.y, dir.x);
    if (dir.x == 0.0f && dir.y == 0.0f) {
        baseAngle = -kPi * 0.5f;
    }
    float spreadRad = GetSpread() * kDegToRad;
    float angle = baseAngle + RandomRange(-spreadRad, spreadRad);
    float speed = RandomRange(GetInitialVelocityMin(), GetInitialVelocityMax());
    p.velocity = Vec2(std::cos(angle) * speed, std::sin(angle) * speed);

    p.rotation = RandomRange(GetInitialAngleMin(), GetInitialAngleMax()) * kDegToRad;
    p.angularVelocity = RandomRange(GetAngularVelocityMin(), GetAngularVelocityMax()) * kDegToRad;

    p.scale = RandomRange(GetScaleMin(), GetScaleMax());

    float lifeRandom = std::clamp(GetLifetimeRandomness(), 0.0f, 1.0f);
    p.lifetime = std::max(0.0001f, GetLifetime() * (1.0f - lifeRandom * Random01()));
    p.age = 0.0f;
    p.alive = true;
}

void Particles2D::EmitBurst(int count) {
    EnsureCapacity();
    for (int i = 0; i < count; ++i) {
        SpawnParticle();
    }
}

TextureHandle Particles2D::ResolveTexture(RenderContext& ctx) {
    std::string currentPath = GetTexturePath();
    if (currentPath != m_CurrentTexturePath) {
        m_TextureHandle = TextureHandle();
        m_TextureAsset.Reset();
        m_CurrentTexturePath = currentPath;
    }
    if (!m_TextureHandle.isValid() && !currentPath.empty()) {
        if (IGfxDevice* device = ctx.getDevice()) {
            m_TextureHandle = GetOrCreateParticleTexture(device, currentPath, m_TextureAsset);
        }
    }
    return m_TextureHandle;
}

void Particles2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Vec2 nodeOrigin(0.0f, 0.0f);
    const bool localSpace = GetLocalSpace();
    if (localSpace) {
        if (Node2D* node2D = dynamic_cast<Node2D*>(m_Owner)) {
            nodeOrigin = node2D->GetGlobalPosition();
        }
    }

    // Render every particle as a sprite so the per-particle color always flows
    // through the textured shader's u_TintColor. When no texture is assigned we
    // fall back to a built-in shape (solid square or soft circle); the built-in
    // texture is white so it tints cleanly.
    TextureHandle userTexture = ResolveTexture(ctx);
    TextureHandle texture = userTexture.isValid()
        ? userTexture
        : GetBuiltinParticleTexture(ctx.getDevice(), GetParticleShape());
    if (!texture.isValid()) {
        return;
    }

    const Color colorStart = GetColorStart();
    const Color colorEnd = GetColorEnd();
    const Color modulate = GetModulate();
    const Vec2 baseSize = GetParticleSize();
    const float scaleEnd = GetScaleEnd();
    const int blendMode = static_cast<int>(GetBlendMode());

    // Re-parse the gradient/curve only when their JSON changes. A valid gradient
    // (>=2 stops) or curve (>=1 point) overrides the simple color/scale ramps.
    std::string gradientJson = GetColorGradientJson();
    if (gradientJson != m_ColorGradientJson) {
        m_ColorGradientJson = gradientJson;
        m_ColorGradient = math::Gradient::FromJsonString(gradientJson);
    }
    std::string scaleCurveJson = GetScaleCurveJson();
    if (scaleCurveJson != m_ScaleCurveJson) {
        m_ScaleCurveJson = scaleCurveJson;
        m_ScaleCurve = math::Curve::FromJsonString(scaleCurveJson);
    }
    const bool useGradient = m_ColorGradient.IsValid();
    const bool useCurve = m_ScaleCurve.IsValid();

    for (const Particle& p : m_Particles) {
        if (!p.alive || p.lifetime <= 0.0f) {
            continue;
        }
        float lifeT = std::clamp(p.age / p.lifetime, 0.0f, 1.0f);

        Color baseColor = useGradient ? m_ColorGradient.Sample(lifeT)
                                      : LerpColor(colorStart, colorEnd, lifeT);
        Color tint = baseColor * modulate;
        float scaleOverLife = useCurve ? m_ScaleCurve.Sample(lifeT, 1.0f)
                                       : LerpFloat(1.0f, scaleEnd, lifeT);
        float scaleFactor = p.scale * scaleOverLife;
        Vec2 size(baseSize.x * scaleFactor, baseSize.y * scaleFactor);
        if (size.x <= 0.0f || size.y <= 0.0f || tint.a <= 0.0f) {
            continue;
        }

        Vec2 worldPos = localSpace ? (nodeOrigin + p.position) : p.position;

        SpriteDrawData sprite;
        sprite.texture = texture;
        sprite.position = worldPos;
        sprite.size = size;
        sprite.rotation = p.rotation;
        sprite.tint = tint;
        sprite.pivot = Vec2(0.5f, 0.5f);
        sprite.uvMin = Vec2(0.0f, 0.0f);
        sprite.uvMax = Vec2(1.0f, 1.0f);
        sprite.blendMode = blendMode;
        ctx.drawSprite(sprite);
    }
}

AABB Particles2D::getWorldBounds() const {
    Vec2 nodeOrigin(0.0f, 0.0f);
    if (Node2D* node2D = dynamic_cast<Node2D*>(m_Owner)) {
        nodeOrigin = node2D->GetGlobalPosition();
    }

    const bool localSpace = GetLocalSpace();
    Vec2 minPt(FLT_MAX, FLT_MAX);
    Vec2 maxPt(-FLT_MAX, -FLT_MAX);
    bool any = false;

    for (const Particle& p : m_Particles) {
        if (!p.alive) {
            continue;
        }
        Vec2 worldPos = localSpace ? (nodeOrigin + p.position) : p.position;
        minPt.x = std::min(minPt.x, worldPos.x);
        minPt.y = std::min(minPt.y, worldPos.y);
        maxPt.x = std::max(maxPt.x, worldPos.x);
        maxPt.y = std::max(maxPt.y, worldPos.y);
        any = true;
    }

    if (!any) {
        return AABB(Vec3(nodeOrigin.x - 1.0f, nodeOrigin.y - 1.0f, -0.1f),
                    Vec3(nodeOrigin.x + 1.0f, nodeOrigin.y + 1.0f, 0.1f));
    }

    const Vec2 baseSize = GetParticleSize();
    float pad = std::max(baseSize.x, baseSize.y) * std::max(1.0f, GetScaleMax());
    return AABB(Vec3(minPt.x - pad, minPt.y - pad, -0.1f),
                Vec3(maxPt.x + pad, maxPt.y + pad, 0.1f));
}

void Particles2D::Restart() {
    for (Particle& p : m_Particles) {
        p.alive = false;
    }
    m_CycleTime = 0.0f;
    m_EmitCursor = 0;
    m_RuntimeEmitting = GetEmitting();
    m_NeedsPreprocess = true;
}

int Particles2D::GetAliveCount() const {
    int count = 0;
    for (const Particle& p : m_Particles) {
        if (p.alive) {
            ++count;
        }
    }
    return count;
}

// ===== Property accessors =====

bool Particles2D::GetEmitting() const { return GetPropertyValue<bool>("emitting"); }
void Particles2D::SetEmitting(bool emitting) {
    SetPropertyValue<bool>("emitting", emitting);
    if (emitting && !m_RuntimeEmitting) {
        m_EmitCursor = 0;
        m_CycleTime = 0.0f;
    }
    m_RuntimeEmitting = emitting;
}

int Particles2D::GetAmount() const { return GetPropertyValue<int>("amount"); }
void Particles2D::SetAmount(int amount) { SetPropertyValue<int>("amount", std::max(1, amount)); }

const std::string& Particles2D::GetTexturePath() const {
    static std::string cached;
    cached = GetPropertyValue<std::string>("texturePath");
    return cached;
}
void Particles2D::SetTexturePath(const std::string& path) { SetPropertyValue<std::string>("texturePath", path); }

float Particles2D::GetLifetime() const { return GetPropertyValue<float>("lifetime"); }
void Particles2D::SetLifetime(float seconds) { SetPropertyValue<float>("lifetime", std::max(0.01f, seconds)); }

float Particles2D::GetLifetimeRandomness() const { return GetPropertyValue<float>("lifetimeRandomness"); }
void Particles2D::SetLifetimeRandomness(float value) { SetPropertyValue<float>("lifetimeRandomness", std::clamp(value, 0.0f, 1.0f)); }

bool Particles2D::GetOneShot() const { return GetPropertyValue<bool>("oneShot"); }
void Particles2D::SetOneShot(bool oneShot) { SetPropertyValue<bool>("oneShot", oneShot); }

float Particles2D::GetExplosiveness() const { return GetPropertyValue<float>("explosiveness"); }
void Particles2D::SetExplosiveness(float value) { SetPropertyValue<float>("explosiveness", std::clamp(value, 0.0f, 1.0f)); }

float Particles2D::GetSpeedScale() const { return GetPropertyValue<float>("speedScale"); }
void Particles2D::SetSpeedScale(float value) { SetPropertyValue<float>("speedScale", std::max(0.0f, value)); }

float Particles2D::GetPreprocess() const { return GetPropertyValue<float>("preprocess"); }
void Particles2D::SetPreprocess(float seconds) { SetPropertyValue<float>("preprocess", std::max(0.0f, seconds)); }

bool Particles2D::GetLocalSpace() const { return GetPropertyValue<bool>("localSpace"); }
void Particles2D::SetLocalSpace(bool localSpace) { SetPropertyValue<bool>("localSpace", localSpace); }

Particles2D::EmissionShape Particles2D::GetEmissionShape() const {
    return static_cast<EmissionShape>(GetPropertyValue<int>("emissionShape"));
}
void Particles2D::SetEmissionShape(EmissionShape shape) { SetPropertyValue<int>("emissionShape", static_cast<int>(shape)); }

float Particles2D::GetEmissionRadius() const { return GetPropertyValue<float>("emissionRadius"); }
void Particles2D::SetEmissionRadius(float radius) { SetPropertyValue<float>("emissionRadius", std::max(0.0f, radius)); }

const Vec2& Particles2D::GetEmissionExtents() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("emissionExtents");
    return cached;
}
void Particles2D::SetEmissionExtents(const Vec2& extents) { SetPropertyValue<Vec2>("emissionExtents", extents); }

const Vec2& Particles2D::GetDirection() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("direction");
    return cached;
}
void Particles2D::SetDirection(const Vec2& direction) { SetPropertyValue<Vec2>("direction", direction); }

float Particles2D::GetSpread() const { return GetPropertyValue<float>("spread"); }
void Particles2D::SetSpread(float degrees) { SetPropertyValue<float>("spread", std::clamp(degrees, 0.0f, 180.0f)); }

float Particles2D::GetInitialVelocityMin() const { return GetPropertyValue<float>("initialVelocityMin"); }
void Particles2D::SetInitialVelocityMin(float value) { SetPropertyValue<float>("initialVelocityMin", value); }

float Particles2D::GetInitialVelocityMax() const { return GetPropertyValue<float>("initialVelocityMax"); }
void Particles2D::SetInitialVelocityMax(float value) { SetPropertyValue<float>("initialVelocityMax", value); }

const Vec2& Particles2D::GetGravity() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("gravity");
    return cached;
}
void Particles2D::SetGravity(const Vec2& gravity) { SetPropertyValue<Vec2>("gravity", gravity); }

float Particles2D::GetLinearDamping() const { return GetPropertyValue<float>("linearDamping"); }
void Particles2D::SetLinearDamping(float value) { SetPropertyValue<float>("linearDamping", std::max(0.0f, value)); }

float Particles2D::GetAngularVelocityMin() const { return GetPropertyValue<float>("angularVelocityMin"); }
void Particles2D::SetAngularVelocityMin(float value) { SetPropertyValue<float>("angularVelocityMin", value); }

float Particles2D::GetAngularVelocityMax() const { return GetPropertyValue<float>("angularVelocityMax"); }
void Particles2D::SetAngularVelocityMax(float value) { SetPropertyValue<float>("angularVelocityMax", value); }

float Particles2D::GetInitialAngleMin() const { return GetPropertyValue<float>("initialAngleMin"); }
void Particles2D::SetInitialAngleMin(float value) { SetPropertyValue<float>("initialAngleMin", value); }

float Particles2D::GetInitialAngleMax() const { return GetPropertyValue<float>("initialAngleMax"); }
void Particles2D::SetInitialAngleMax(float value) { SetPropertyValue<float>("initialAngleMax", value); }

float Particles2D::GetScaleMin() const { return GetPropertyValue<float>("scaleMin"); }
void Particles2D::SetScaleMin(float value) { SetPropertyValue<float>("scaleMin", std::max(0.0f, value)); }

float Particles2D::GetScaleMax() const { return GetPropertyValue<float>("scaleMax"); }
void Particles2D::SetScaleMax(float value) { SetPropertyValue<float>("scaleMax", std::max(0.0f, value)); }

float Particles2D::GetScaleEnd() const { return GetPropertyValue<float>("scaleEnd"); }
void Particles2D::SetScaleEnd(float value) { SetPropertyValue<float>("scaleEnd", std::max(0.0f, value)); }

const Vec2& Particles2D::GetParticleSize() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("particleSize");
    return cached;
}
void Particles2D::SetParticleSize(const Vec2& size) { SetPropertyValue<Vec2>("particleSize", size); }

Color Particles2D::GetColorStart() const {
    return GetPropertyValue<Color>("colorStart");
}
void Particles2D::SetColorStart(const Color& color) { SetPropertyValue<Color>("colorStart", color); }

Color Particles2D::GetColorEnd() const {
    return GetPropertyValue<Color>("colorEnd");
}
void Particles2D::SetColorEnd(const Color& color) { SetPropertyValue<Color>("colorEnd", color); }

Color Particles2D::GetModulate() const {
    return GetPropertyValue<Color>("modulate");
}
void Particles2D::SetModulate(const Color& color) { SetPropertyValue<Color>("modulate", color); }

Particles2D::BlendMode Particles2D::GetBlendMode() const {
    return static_cast<BlendMode>(GetPropertyValue<int>("blendMode"));
}
void Particles2D::SetBlendMode(BlendMode mode) { SetPropertyValue<int>("blendMode", static_cast<int>(mode)); }

ParticleShape Particles2D::GetParticleShape() const {
    return static_cast<ParticleShape>(GetPropertyValue<int>("particleShape"));
}
void Particles2D::SetParticleShape(ParticleShape shape) { SetPropertyValue<int>("particleShape", static_cast<int>(shape)); }

std::string Particles2D::GetColorGradientJson() const {
    // Never let a wrong-typed property value (e.g. an object instead of a string)
    // throw out of the render gather pass.
    try { return GetPropertyValue<std::string>("colorGradient"); }
    catch (...) { return std::string(); }
}
void Particles2D::SetColorGradientJson(const std::string& json) { SetPropertyValue<std::string>("colorGradient", json); }
math::Gradient Particles2D::GetColorGradient() const { return math::Gradient::FromJsonString(GetColorGradientJson()); }
void Particles2D::SetColorGradient(const math::Gradient& gradient) { SetColorGradientJson(gradient.ToJsonString()); }

std::string Particles2D::GetScaleCurveJson() const {
    try { return GetPropertyValue<std::string>("scaleCurve"); }
    catch (...) { return std::string(); }
}
void Particles2D::SetScaleCurveJson(const std::string& json) { SetPropertyValue<std::string>("scaleCurve", json); }
math::Curve Particles2D::GetScaleCurve() const { return math::Curve::FromJsonString(GetScaleCurveJson()); }
void Particles2D::SetScaleCurve(const math::Curve& curve) { SetScaleCurveJson(curve.ToJsonString()); }

} // namespace components
} // namespace lupine
