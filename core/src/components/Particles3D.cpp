#include "lupine/components/Particles3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include "lupine/rendering/Material.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
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

std::unordered_map<std::string, TextureHandle>& GetParticle3DTextureCache() {
    static std::unordered_map<std::string, TextureHandle> s_Cache;
    return s_Cache;
}

std::mutex& GetParticle3DTextureCacheMutex() {
    static std::mutex s_Mutex;
    return s_Mutex;
}

void EnsureParticle3DCacheRegistered() {
    static bool s_Registered = false;
    if (s_Registered) {
        return;
    }
    s_Registered = true;

    rendering::TextureCache::RegisterCache(
        "Particles3D",
        [](const std::string& path) -> bool {
            std::lock_guard<std::mutex> lock(GetParticle3DTextureCacheMutex());
            return GetParticle3DTextureCache().erase(path) > 0;
        },
        []() {
            std::lock_guard<std::mutex> lock(GetParticle3DTextureCacheMutex());
            GetParticle3DTextureCache().clear();
        });
}

TextureHandle GetOrCreateParticle3DTexture(
    IGfxDevice* device,
    const std::string& path,
    asset::AssetRef<asset::ImageAsset>& assetRef)
{
    if (path.empty() || !device) {
        return TextureHandle();
    }

    EnsureParticle3DCacheRegistered();

    {
        std::lock_guard<std::mutex> lock(GetParticle3DTextureCacheMutex());
        auto it = GetParticle3DTextureCache().find(path);
        if (it != GetParticle3DTextureCache().end() && it->second.isValid()) {
            return it->second;
        }
    }

    if (!assetRef.IsValid()) {
        assetRef = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
        if (!assetRef->LoadFromFile(path, true, asset::ImageColorSpace::sRGB)) {
            LOG_ERROR(LogCategory::Render, "Particles3D FAILED to load texture: {}", path);
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
        std::lock_guard<std::mutex> lock(GetParticle3DTextureCacheMutex());
        GetParticle3DTextureCache()[path] = handle;
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

// Build an arbitrary orthonormal basis (t, b) perpendicular to a unit axis.
void BuildBasis(const Vec3& axis, Vec3& t, Vec3& b) {
    Vec3 helper = (std::abs(axis.y) < 0.999f) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
    t = Cross(helper, axis).Normalized();
    b = Cross(axis, t).Normalized();
}

} // anonymous namespace

Particles3D::Particles3D()
    : Component("Particles3D")
    , m_Rng(std::random_device{}())
    , m_Dist(0.0f, 1.0f)
{
}

Particles3D::Particles3D(const std::string& name)
    : Component(name)
    , m_Rng(std::random_device{}())
    , m_Dist(0.0f, 1.0f)
{
}

Particles3D::~Particles3D() = default;

void Particles3D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(emitting, Bool, true, "Emission"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(amount, 32, 1, 65536, 1, "Emission"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(oneShot, Bool, false, "Emission"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(explosiveness, 0.0f, 0.0f, 1.0f, 0.01f, "Emission"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(speedScale, 1.0f, 0.0f, 64.0f, 0.01f, "Emission"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(preprocess, 0.0f, 0.0f, 60.0f, 0.1f, "Emission"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(localSpace, Bool, false, "Emission"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lifetime, 1.0f, 0.01f, 600.0f, 0.01f, "Lifetime"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lifetimeRandomness, 0.0f, 0.0f, 1.0f, 0.01f, "Lifetime"));

    DefineProperty(PROPERTY_ENUM_GROUP(emissionShape, 0, "Shape", Point, Sphere, Box));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(emissionRadius, 0.0f, 0.0f, 8192.0f, 0.1f, "Shape"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(emissionExtents, Vec3, Vec3(0.0f, 0.0f, 0.0f), "Shape"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(direction, Vec3, Vec3(0.0f, 1.0f, 0.0f), "Velocity"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(spread, 20.0f, 0.0f, 180.0f, 1.0f, "Velocity"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(initialVelocityMin, 0.0f, 0.0f, 100000.0f, 0.1f, "Velocity"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(initialVelocityMax, 2.0f, 0.0f, 100000.0f, 0.1f, "Velocity"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(gravity, Vec3, Vec3(0.0f, -9.8f, 0.0f), "Velocity"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(linearDamping, 0.0f, 0.0f, 100.0f, 0.01f, "Velocity"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(initialAngleMin, 0.0f, -360.0f, 360.0f, 1.0f, "Rotation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(initialAngleMax, 0.0f, -360.0f, 360.0f, 1.0f, "Rotation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(angularVelocityMin, 0.0f, -3600.0f, 3600.0f, 1.0f, "Rotation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(angularVelocityMax, 0.0f, -3600.0f, 3600.0f, 1.0f, "Rotation"));

    DefineProperty(PROPERTY_ENUM_GROUP(billboardMode, 1, "Display", Disabled, Enabled, YAxisOnly));
    DefineProperty(PROPERTY_ENUM_GROUP(blendMode, 0, "Display", Alpha, Additive));
    DefineProperty(PROPERTY_ENUM_GROUP(particleShape, 0, "Display", Square, Circle));
    DefineProperty(PROPERTY_DEFAULT_GROUP(doubleSided, Bool, true, "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(particleSize, Vec2, Vec2(0.25f, 0.25f), "Display"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scaleMin, 1.0f, 0.0f, 64.0f, 0.01f, "Display"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scaleMax, 1.0f, 0.0f, 64.0f, 0.01f, "Display"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scaleEnd, 1.0f, 0.0f, 64.0f, 0.01f, "Display"));
    DefineProperty(PROPERTY_FILE_GROUP(texturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorStart, Color, Color::White(), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorEnd, Color, Color(1.0f, 1.0f, 1.0f, 0.0f), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorGradient, String, std::string(""), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scaleCurve, String, std::string(""), "Display"));
}

void Particles3D::OnReady() {
    EnsureCapacity();
    m_RuntimeEmitting = GetEmitting();
    m_NeedsPreprocess = true;
}

// `emitting` is mirrored by m_RuntimeEmitting, which a one-shot burst clears on its own
// once the cycle completes. A write that lands straight in the property registry — the
// editor inspector, or a script's component:set("emitting", ...) — bypasses SetEmitting
// and would leave the mirror stale, so the emitter would ignore the new value entirely.
// Routing the change back through SetEmitting keeps the mirror honest and preserves the
// false -> true restart edge that re-arms a one-shot.
void Particles3D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "emitting") {
        if (newValue.is_boolean()) {
            SetEmitting(newValue.get<bool>());
        }
    } else if (propertyName == "amount") {
        EnsureCapacity();
    }
}

void Particles3D::EnsureCapacity() {
    int amount = GetAmount();
    if (amount < 1) {
        amount = 1;
    }
    if (static_cast<int>(m_Particles.size()) != amount) {
        m_Particles.assign(static_cast<size_t>(amount), Particle());
        m_EmitCursor = std::min(m_EmitCursor, amount);
    }
}

float Particles3D::Random01() {
    return m_Dist(m_Rng);
}

float Particles3D::RandomRange(float minValue, float maxValue) {
    return minValue + (maxValue - minValue) * Random01();
}

void Particles3D::OnUpdate(float deltaTime) {
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
    if (scaledDt > kMaxFrameStep) {
        scaledDt = kMaxFrameStep;
    }
    if (scaledDt <= 0.0f) {
        return;
    }

    StepSimulation(scaledDt);
}

void Particles3D::StepSimulation(float deltaTime) {
    const Vec3 gravity = GetGravity();
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
        p.roll += p.angularVelocity * deltaTime;
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

int Particles3D::FindFreeSlot() {
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
    return oldestIndex;
}

Vec3 Particles3D::SampleEmissionOffset() {
    EmissionShape shape = GetEmissionShape();
    switch (shape) {
        case EmissionShape::Sphere: {
            float radius = GetEmissionRadius();
            // Rejection-free uniform point in a sphere: random direction * cube-root radius.
            float u = Random01();
            float v = Random01();
            float theta = u * 2.0f * kPi;
            float phi = std::acos(2.0f * v - 1.0f);
            float r = std::cbrt(Random01()) * radius;
            return Vec3(
                r * std::sin(phi) * std::cos(theta),
                r * std::sin(phi) * std::sin(theta),
                r * std::cos(phi));
        }
        case EmissionShape::Box: {
            const Vec3 extents = GetEmissionExtents();
            return Vec3(
                RandomRange(-extents.x, extents.x),
                RandomRange(-extents.y, extents.y),
                RandomRange(-extents.z, extents.z));
        }
        case EmissionShape::Point:
        default:
            return Vec3(0.0f, 0.0f, 0.0f);
    }
}

Vec3 Particles3D::SampleConeDirection(const Vec3& axis, float spreadRadians) {
    Vec3 normAxis = axis.Normalized();
    if (normAxis.Length() < 0.0001f) {
        normAxis = Vec3(0.0f, 1.0f, 0.0f);
    }
    if (spreadRadians <= 0.0001f) {
        return normAxis;
    }

    Vec3 t, b;
    BuildBasis(normAxis, t, b);

    float cosSpread = std::cos(spreadRadians);
    float cosPhi = 1.0f - Random01() * (1.0f - cosSpread);
    float sinPhi = std::sqrt(std::max(0.0f, 1.0f - cosPhi * cosPhi));
    float theta = Random01() * 2.0f * kPi;

    Vec3 dir = normAxis * cosPhi + (t * std::cos(theta) + b * std::sin(theta)) * sinPhi;
    return dir.Normalized();
}

void Particles3D::SpawnParticle() {
    int slot = FindFreeSlot();
    if (slot < 0) {
        return;
    }
    Particle& p = m_Particles[slot];

    const bool localSpace = GetLocalSpace();
    Vec3 origin(0.0f, 0.0f, 0.0f);
    if (!localSpace) {
        if (Node3D* node3D = dynamic_cast<Node3D*>(m_Owner)) {
            origin = node3D->GetGlobalPosition();
        }
    }

    p.position = origin + SampleEmissionOffset();

    float spreadRad = GetSpread() * kDegToRad;
    Vec3 dir = SampleConeDirection(GetDirection(), spreadRad);
    float speed = RandomRange(GetInitialVelocityMin(), GetInitialVelocityMax());
    p.velocity = dir * speed;

    p.roll = RandomRange(GetInitialAngleMin(), GetInitialAngleMax()) * kDegToRad;
    p.angularVelocity = RandomRange(GetAngularVelocityMin(), GetAngularVelocityMax()) * kDegToRad;
    p.scale = RandomRange(GetScaleMin(), GetScaleMax());

    float lifeRandom = std::clamp(GetLifetimeRandomness(), 0.0f, 1.0f);
    p.lifetime = std::max(0.0001f, GetLifetime() * (1.0f - lifeRandom * Random01()));
    p.age = 0.0f;
    p.alive = true;
}

void Particles3D::EmitBurst(int count) {
    EnsureCapacity();
    for (int i = 0; i < count; ++i) {
        SpawnParticle();
    }
}

TextureHandle Particles3D::ResolveTexture(RenderContext& ctx) {
    std::string currentPath = GetTexturePath();
    if (currentPath != m_CurrentTexturePath) {
        m_TextureHandle = TextureHandle();
        m_TextureAsset.Reset();
        m_CurrentTexturePath = currentPath;
    }
    if (!m_TextureHandle.isValid() && !currentPath.empty()) {
        if (IGfxDevice* device = ctx.getDevice()) {
            m_TextureHandle = GetOrCreateParticle3DTexture(device, currentPath, m_TextureAsset);
        }
    }
    return m_TextureHandle;
}

void Particles3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    Vec3 nodeOrigin(0.0f, 0.0f, 0.0f);
    const bool localSpace = GetLocalSpace();
    if (localSpace) {
        if (Node3D* node3D = dynamic_cast<Node3D*>(m_Owner)) {
            nodeOrigin = node3D->GetGlobalPosition();
        }
    }

    // Square particles render as a solid colored quad (no texture); circle
    // particles use a built-in soft-disc texture. A user texture overrides both.
    TextureHandle texture = ResolveTexture(ctx);
    if (!texture.isValid() && GetParticleShape() == ParticleShape::Circle) {
        texture = GetBuiltinParticleTexture(ctx.getDevice(), ParticleShape::Circle);
    }
    const bool useTexture = texture.isValid();

    const Color colorStart = GetColorStart();
    const Color colorEnd = GetColorEnd();
    const Color modulate = GetModulate();
    const Vec2 baseSize = GetParticleSize();
    const float scaleEnd = GetScaleEnd();
    const BillboardMode billboardMode = GetBillboardMode();

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

    MaterialHandle material = (GetBlendMode() == BlendMode::Additive)
        ? ctx.getParticle3DAdditiveMaterial()
        : ctx.getParticle3DAlphaMaterial();
    if (!material.isValid()) {
        material = ctx.getDefaultTexturedMaterial();
    }

    // Camera-facing basis vectors, shared by all full-billboard particles.
    const glm::mat4 glmView = ctx.getViewMatrix().ToGLM();
    Vec3 camRight(glmView[0][0], glmView[1][0], glmView[2][0]);
    Vec3 camUp(glmView[0][1], glmView[1][1], glmView[2][1]);

    glm::mat4 invView = glm::inverse(glmView);
    Vec3 cameraPos(invView[3][0], invView[3][1], invView[3][2]);

    size_t submitted = 0;
    Vec3 firstPos(0.0f, 0.0f, 0.0f);

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
        float halfW = baseSize.x * scaleFactor;
        float halfH = baseSize.y * scaleFactor;
        if (halfW <= 0.0f || halfH <= 0.0f || tint.a <= 0.0f) {
            continue;
        }

        Vec3 worldPos = localSpace ? (nodeOrigin + p.position) : p.position;
        if (submitted == 0) {
            firstPos = worldPos;
        }

        Vec3 right;
        Vec3 up;
        if (billboardMode == BillboardMode::Disabled) {
            right = Vec3(1.0f, 0.0f, 0.0f);
            up = Vec3(0.0f, 0.0f, -1.0f);
        } else if (billboardMode == BillboardMode::YAxisOnly) {
            Vec3 toCamera = cameraPos - worldPos;
            toCamera.y = 0.0f;
            if (toCamera.Length() < 0.0001f) {
                toCamera = Vec3(0.0f, 0.0f, 1.0f);
            }
            toCamera = toCamera.Normalized();
            right = Cross(Vec3(0.0f, 1.0f, 0.0f), toCamera).Normalized();
            up = Vec3(0.0f, 1.0f, 0.0f);
        } else {
            right = camRight;
            up = camUp;
        }

        // Apply per-particle roll around the view (forward) axis.
        if (std::abs(p.roll) > 0.0001f) {
            float c = std::cos(p.roll);
            float s = std::sin(p.roll);
            Vec3 rolledRight = right * c + up * s;
            Vec3 rolledUp = up * c - right * s;
            right = rolledRight;
            up = rolledUp;
        }

        Vec3 scaledRight = right * halfW;
        Vec3 scaledUp = up * halfH;
        Vec3 forward = Cross(scaledRight, scaledUp).Normalized();

        glm::mat4 m(1.0f);
        m[0] = glm::vec4(scaledRight.x, scaledRight.y, scaledRight.z, 0.0f);
        m[1] = glm::vec4(scaledUp.x, scaledUp.y, scaledUp.z, 0.0f);
        m[2] = glm::vec4(forward.x, forward.y, forward.z, 0.0f);
        m[3] = glm::vec4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
        Mat4 transform(m);

        MaterialPropertyBlock overrides;
        overrides.setColor("u_TintColor", tint);
        overrides.setVec4("u_UVRect", Vec4(0.0f, 0.0f, 1.0f, 1.0f));
        overrides.setBool("u_UseTexture", useTexture);
        if (useTexture) {
            overrides.setTexture("u_Texture", texture);
        }

        ctx.drawMesh(quadMesh, material, transform, overrides, 0, false, false);
        ++submitted;
    }

    static int s_DiagCount = 0;
    if (s_DiagCount < 8) {
        ++s_DiagCount;
        size_t alive = 0;
        for (const Particle& p : m_Particles) {
            if (p.alive) {
                ++alive;
            }
        }
        LOG_INFO(LogCategory::Render,
                 "PARTICLE3DDBG '{}': pool={} alive={} submitted={} mesh={} material={} "
                 "(alpha={} additive={} defaultTextured={}) useTexture={} texture={} "
                 "size=({},{}) first=({},{},{})",
                 m_Owner ? m_Owner->GetName() : "?",
                 m_Particles.size(), alive, submitted, quadMesh.id, material.id,
                 ctx.getParticle3DAlphaMaterial().id, ctx.getParticle3DAdditiveMaterial().id,
                 ctx.getDefaultTexturedMaterial().id,
                 useTexture, texture.id, baseSize.x, baseSize.y,
                 firstPos.x, firstPos.y, firstPos.z);
    }
}

AABB Particles3D::getWorldBounds() const {
    Vec3 nodeOrigin(0.0f, 0.0f, 0.0f);
    if (Node3D* node3D = dynamic_cast<Node3D*>(m_Owner)) {
        nodeOrigin = node3D->GetGlobalPosition();
    }

    const bool localSpace = GetLocalSpace();
    Vec3 minPt(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool any = false;

    for (const Particle& p : m_Particles) {
        if (!p.alive) {
            continue;
        }
        Vec3 worldPos = localSpace ? (nodeOrigin + p.position) : p.position;
        minPt.x = std::min(minPt.x, worldPos.x);
        minPt.y = std::min(minPt.y, worldPos.y);
        minPt.z = std::min(minPt.z, worldPos.z);
        maxPt.x = std::max(maxPt.x, worldPos.x);
        maxPt.y = std::max(maxPt.y, worldPos.y);
        maxPt.z = std::max(maxPt.z, worldPos.z);
        any = true;
    }

    if (!any) {
        return AABB(Vec3(nodeOrigin.x - 0.5f, nodeOrigin.y - 0.5f, nodeOrigin.z - 0.5f),
                    Vec3(nodeOrigin.x + 0.5f, nodeOrigin.y + 0.5f, nodeOrigin.z + 0.5f));
    }

    const Vec2 baseSize = GetParticleSize();
    float pad = std::max(baseSize.x, baseSize.y) * std::max(1.0f, GetScaleMax());
    return AABB(Vec3(minPt.x - pad, minPt.y - pad, minPt.z - pad),
                Vec3(maxPt.x + pad, maxPt.y + pad, maxPt.z + pad));
}

void Particles3D::Restart() {
    for (Particle& p : m_Particles) {
        p.alive = false;
    }
    m_CycleTime = 0.0f;
    m_EmitCursor = 0;
    m_RuntimeEmitting = GetEmitting();
    m_NeedsPreprocess = true;
}

int Particles3D::GetAliveCount() const {
    int count = 0;
    for (const Particle& p : m_Particles) {
        if (p.alive) {
            ++count;
        }
    }
    return count;
}

// ===== Property accessors =====

bool Particles3D::GetEmitting() const { return GetPropertyValue<bool>("emitting"); }
void Particles3D::SetEmitting(bool emitting) {
    SetPropertyValue<bool>("emitting", emitting);
    if (emitting && !m_RuntimeEmitting) {
        m_EmitCursor = 0;
        m_CycleTime = 0.0f;
    }
    m_RuntimeEmitting = emitting;
}

int Particles3D::GetAmount() const { return GetPropertyValue<int>("amount"); }
void Particles3D::SetAmount(int amount) { SetPropertyValue<int>("amount", std::max(1, amount)); }

const std::string& Particles3D::GetTexturePath() const {
    static std::string cached;
    cached = GetPropertyValue<std::string>("texturePath");
    return cached;
}
void Particles3D::SetTexturePath(const std::string& path) { SetPropertyValue<std::string>("texturePath", path); }

float Particles3D::GetLifetime() const { return GetPropertyValue<float>("lifetime"); }
void Particles3D::SetLifetime(float seconds) { SetPropertyValue<float>("lifetime", std::max(0.01f, seconds)); }

float Particles3D::GetLifetimeRandomness() const { return GetPropertyValue<float>("lifetimeRandomness"); }
void Particles3D::SetLifetimeRandomness(float value) { SetPropertyValue<float>("lifetimeRandomness", std::clamp(value, 0.0f, 1.0f)); }

bool Particles3D::GetOneShot() const { return GetPropertyValue<bool>("oneShot"); }
void Particles3D::SetOneShot(bool oneShot) { SetPropertyValue<bool>("oneShot", oneShot); }

float Particles3D::GetExplosiveness() const { return GetPropertyValue<float>("explosiveness"); }
void Particles3D::SetExplosiveness(float value) { SetPropertyValue<float>("explosiveness", std::clamp(value, 0.0f, 1.0f)); }

float Particles3D::GetSpeedScale() const { return GetPropertyValue<float>("speedScale"); }
void Particles3D::SetSpeedScale(float value) { SetPropertyValue<float>("speedScale", std::max(0.0f, value)); }

float Particles3D::GetPreprocess() const { return GetPropertyValue<float>("preprocess"); }
void Particles3D::SetPreprocess(float seconds) { SetPropertyValue<float>("preprocess", std::max(0.0f, seconds)); }

bool Particles3D::GetLocalSpace() const { return GetPropertyValue<bool>("localSpace"); }
void Particles3D::SetLocalSpace(bool localSpace) { SetPropertyValue<bool>("localSpace", localSpace); }

Particles3D::EmissionShape Particles3D::GetEmissionShape() const {
    return static_cast<EmissionShape>(GetPropertyValue<int>("emissionShape"));
}
void Particles3D::SetEmissionShape(EmissionShape shape) { SetPropertyValue<int>("emissionShape", static_cast<int>(shape)); }

float Particles3D::GetEmissionRadius() const { return GetPropertyValue<float>("emissionRadius"); }
void Particles3D::SetEmissionRadius(float radius) { SetPropertyValue<float>("emissionRadius", std::max(0.0f, radius)); }

const Vec3& Particles3D::GetEmissionExtents() const {
    static Vec3 cached;
    cached = GetPropertyValue<Vec3>("emissionExtents");
    return cached;
}
void Particles3D::SetEmissionExtents(const Vec3& extents) { SetPropertyValue<Vec3>("emissionExtents", extents); }

const Vec3& Particles3D::GetDirection() const {
    static Vec3 cached;
    cached = GetPropertyValue<Vec3>("direction");
    return cached;
}
void Particles3D::SetDirection(const Vec3& direction) { SetPropertyValue<Vec3>("direction", direction); }

float Particles3D::GetSpread() const { return GetPropertyValue<float>("spread"); }
void Particles3D::SetSpread(float degrees) { SetPropertyValue<float>("spread", std::clamp(degrees, 0.0f, 180.0f)); }

float Particles3D::GetInitialVelocityMin() const { return GetPropertyValue<float>("initialVelocityMin"); }
void Particles3D::SetInitialVelocityMin(float value) { SetPropertyValue<float>("initialVelocityMin", value); }

float Particles3D::GetInitialVelocityMax() const { return GetPropertyValue<float>("initialVelocityMax"); }
void Particles3D::SetInitialVelocityMax(float value) { SetPropertyValue<float>("initialVelocityMax", value); }

const Vec3& Particles3D::GetGravity() const {
    static Vec3 cached;
    cached = GetPropertyValue<Vec3>("gravity");
    return cached;
}
void Particles3D::SetGravity(const Vec3& gravity) { SetPropertyValue<Vec3>("gravity", gravity); }

float Particles3D::GetLinearDamping() const { return GetPropertyValue<float>("linearDamping"); }
void Particles3D::SetLinearDamping(float value) { SetPropertyValue<float>("linearDamping", std::max(0.0f, value)); }

float Particles3D::GetInitialAngleMin() const { return GetPropertyValue<float>("initialAngleMin"); }
void Particles3D::SetInitialAngleMin(float value) { SetPropertyValue<float>("initialAngleMin", value); }

float Particles3D::GetInitialAngleMax() const { return GetPropertyValue<float>("initialAngleMax"); }
void Particles3D::SetInitialAngleMax(float value) { SetPropertyValue<float>("initialAngleMax", value); }

float Particles3D::GetAngularVelocityMin() const { return GetPropertyValue<float>("angularVelocityMin"); }
void Particles3D::SetAngularVelocityMin(float value) { SetPropertyValue<float>("angularVelocityMin", value); }

float Particles3D::GetAngularVelocityMax() const { return GetPropertyValue<float>("angularVelocityMax"); }
void Particles3D::SetAngularVelocityMax(float value) { SetPropertyValue<float>("angularVelocityMax", value); }

Particles3D::BillboardMode Particles3D::GetBillboardMode() const {
    return static_cast<BillboardMode>(GetPropertyValue<int>("billboardMode"));
}
void Particles3D::SetBillboardMode(BillboardMode mode) { SetPropertyValue<int>("billboardMode", static_cast<int>(mode)); }

Particles3D::BlendMode Particles3D::GetBlendMode() const {
    return static_cast<BlendMode>(GetPropertyValue<int>("blendMode"));
}
void Particles3D::SetBlendMode(BlendMode mode) { SetPropertyValue<int>("blendMode", static_cast<int>(mode)); }

ParticleShape Particles3D::GetParticleShape() const {
    return static_cast<ParticleShape>(GetPropertyValue<int>("particleShape"));
}
void Particles3D::SetParticleShape(ParticleShape shape) { SetPropertyValue<int>("particleShape", static_cast<int>(shape)); }

std::string Particles3D::GetColorGradientJson() const {
    // Never let a wrong-typed property value (e.g. an object instead of a string)
    // throw out of the render gather pass.
    try { return GetPropertyValue<std::string>("colorGradient"); }
    catch (...) { return std::string(); }
}
void Particles3D::SetColorGradientJson(const std::string& json) { SetPropertyValue<std::string>("colorGradient", json); }
math::Gradient Particles3D::GetColorGradient() const { return math::Gradient::FromJsonString(GetColorGradientJson()); }
void Particles3D::SetColorGradient(const math::Gradient& gradient) { SetColorGradientJson(gradient.ToJsonString()); }

std::string Particles3D::GetScaleCurveJson() const {
    try { return GetPropertyValue<std::string>("scaleCurve"); }
    catch (...) { return std::string(); }
}
void Particles3D::SetScaleCurveJson(const std::string& json) { SetPropertyValue<std::string>("scaleCurve", json); }
math::Curve Particles3D::GetScaleCurve() const { return math::Curve::FromJsonString(GetScaleCurveJson()); }
void Particles3D::SetScaleCurve(const math::Curve& curve) { SetScaleCurveJson(curve.ToJsonString()); }

bool Particles3D::GetDoubleSided() const { return GetPropertyValue<bool>("doubleSided"); }
void Particles3D::SetDoubleSided(bool doubleSided) { SetPropertyValue<bool>("doubleSided", doubleSided); }

const Vec2& Particles3D::GetParticleSize() const {
    static Vec2 cached;
    cached = GetPropertyValue<Vec2>("particleSize");
    return cached;
}
void Particles3D::SetParticleSize(const Vec2& size) { SetPropertyValue<Vec2>("particleSize", size); }

float Particles3D::GetScaleMin() const { return GetPropertyValue<float>("scaleMin"); }
void Particles3D::SetScaleMin(float value) { SetPropertyValue<float>("scaleMin", std::max(0.0f, value)); }

float Particles3D::GetScaleMax() const { return GetPropertyValue<float>("scaleMax"); }
void Particles3D::SetScaleMax(float value) { SetPropertyValue<float>("scaleMax", std::max(0.0f, value)); }

float Particles3D::GetScaleEnd() const { return GetPropertyValue<float>("scaleEnd"); }
void Particles3D::SetScaleEnd(float value) { SetPropertyValue<float>("scaleEnd", std::max(0.0f, value)); }

Color Particles3D::GetColorStart() const {
    return GetPropertyValue<Color>("colorStart");
}
void Particles3D::SetColorStart(const Color& color) { SetPropertyValue<Color>("colorStart", color); }

Color Particles3D::GetColorEnd() const {
    return GetPropertyValue<Color>("colorEnd");
}
void Particles3D::SetColorEnd(const Color& color) { SetPropertyValue<Color>("colorEnd", color); }

Color Particles3D::GetModulate() const {
    return GetPropertyValue<Color>("modulate");
}
void Particles3D::SetModulate(const Color& color) { SetPropertyValue<Color>("modulate", color); }

} // namespace components
} // namespace lupine
