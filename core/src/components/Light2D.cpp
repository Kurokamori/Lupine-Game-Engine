#include "lupine/components/Light2D.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/components/LightOccluder2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/logger/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
// Radial subdivisions of each light spoke. More bands shape the falloff curve
// more accurately at the cost of triangles; 10 is smooth for additive lights.
constexpr int kRadialBands = 10;
// Small angular offset cast on either side of an occluder endpoint so the
// shadow boundary is crisp (one ray grazes the corner, one passes just beyond).
constexpr float kEndpointEpsilon = 0.0008f;

struct WorkEdge {
    Vec2 a;
    Vec2 b;
};

float DistancePointToSegment(const Vec2& p, const Vec2& a, const Vec2& b) {
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float apx = p.x - a.x;
    float apy = p.y - a.y;
    float lenSq = abx * abx + aby * aby;
    float t = (lenSq > 1e-9f) ? ((apx * abx + apy * aby) / lenSq) : 0.0f;
    t = std::max(0.0f, std::min(1.0f, t));
    float cx = a.x + abx * t;
    float cy = a.y + aby * t;
    float dx = p.x - cx;
    float dy = p.y - cy;
    return std::sqrt(dx * dx + dy * dy);
}

// Ray (origin o, unit dir d) against segment [a,b]. On hit within (0, maxT],
// writes the ray parameter (distance) to tOut and returns true.
bool RaySegmentHit(const Vec2& o, const Vec2& d, const Vec2& a, const Vec2& b,
                   float maxT, float& tOut) {
    float ex = b.x - a.x;
    float ey = b.y - a.y;
    float denom = d.x * ey - d.y * ex;
    if (std::fabs(denom) < 1e-9f) {
        return false; // parallel
    }
    float diffx = a.x - o.x;
    float diffy = a.y - o.y;
    float t = (diffx * ey - diffy * ex) / denom;
    float u = (diffx * d.y - diffy * d.x) / denom;
    if (t > 1e-4f && t <= maxT && u >= -1e-4f && u <= 1.0f + 1e-4f) {
        tOut = t;
        return true;
    }
    return false;
}

// Decide whether a directed edge a->b occludes light coming from center C,
// given the occluder cull mode.
bool EdgeCasts(const Vec2& a, const Vec2& b, const Vec2& center, OccluderCullMode2D mode) {
    if (mode == OccluderCullMode2D::Disabled) {
        return true;
    }
    float cross = (b.x - a.x) * (center.y - a.y) - (b.y - a.y) * (center.x - a.x);
    if (mode == OccluderCullMode2D::Clockwise) {
        return cross < 0.0f;
    }
    return cross > 0.0f; // CounterClockwise
}

void CollectOccluders(Node* node, std::vector<LightOccluder2D*>& out) {
    if (!node) {
        return;
    }
    for (const auto& comp : node->GetComponents()) {
        LightOccluder2D* occ = dynamic_cast<LightOccluder2D*>(comp.get());
        if (occ) {
            out.push_back(occ);
        }
    }
    for (const auto& child : node->GetChildren()) {
        CollectOccluders(child.get(), out);
    }
}

} // namespace

Light2D::Light2D()
    : Component("Light2D")
{
}

Light2D::Light2D(const std::string& name)
    : Component(name)
{
}

Light2D::~Light2D() {
}

void Light2D::DefineProperties() {
    // Light properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(lightEnabled, Bool, true, "Light"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color(1.0f, 1.0f, 0.9f, 1.0f), "Light"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(intensity, 1.0f, 0.0f, 10.0f, 0.1f, "Light"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(range, 100.0f, 1.0f, 2000.0f, 1.0f, "Light"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(falloff, 2.0f, 0.1f, 10.0f, 0.1f, "Light"));
    DefineProperty(PROPERTY_ENUM_GROUP(blendMode, 0, "Light", SoftLight, Overlay, Additive));

    // Shadows
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowEnabled, Bool, false, "Shadow"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(shadowSampleCount, 96, 16, 512, 1, "Shadow"));

    // Layering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, 0, 100, 1, "Layering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Layering"));
}

void Light2D::OnAwake() {
}

void Light2D::OnReady() {
}

void Light2D::OnRender() {
}

bool Light2D::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    if (!is3D) {
        float currentRange = GetRange();
        // Uniform scaling for range
        if (axis == -1 || axis == 0 || axis == 1) {
            SetRange(std::max(1.0f, currentRange + scaleDelta * currentRange));
        }
        return true;
    }
    return false;
}

Color Light2D::GetColor() const {
    Color color = GetPropertyValue<Color>("color");
    // A fully transparent black means the property was never authored; fall back to
    // the warm-white default rather than emitting no light at all.
    if (color.r == 0.0f && color.g == 0.0f && color.b == 0.0f && color.a == 0.0f) {
        return Color(1.0f, 1.0f, 0.9f, 1.0f);
    }
    return color;
}

void Light2D::SetColor(const Color& color) {
    SetPropertyValue<Color>("color", color);
}

float Light2D::GetRange() const {
    return GetPropertyValue<float>("range");
}

void Light2D::SetRange(float range) {
    SetPropertyValue<float>("range", range);
}

float Light2D::GetIntensity() const {
    return GetPropertyValue<float>("intensity");
}

void Light2D::SetIntensity(float intensity) {
    SetPropertyValue<float>("intensity", intensity);
}

float Light2D::GetFalloff() const {
    return GetPropertyValue<float>("falloff");
}

void Light2D::SetFalloff(float falloff) {
    SetPropertyValue<float>("falloff", falloff);
}

Light2DBlendMode Light2D::GetBlendMode() const {
    int mode = GetPropertyValue<int>("blendMode");
    return IntToBlendMode(mode);
}

void Light2D::SetBlendMode(Light2DBlendMode mode) {
    SetPropertyValue<int>("blendMode", BlendModeToInt(mode));
}

bool Light2D::GetLightEnabled() const {
    return GetPropertyValue<bool>("lightEnabled");
}

void Light2D::SetLightEnabled(bool enabled) {
    SetPropertyValue<bool>("lightEnabled", enabled);
}

bool Light2D::GetShadowEnabled() const {
    return GetPropertyValue<bool>("shadowEnabled");
}

void Light2D::SetShadowEnabled(bool enabled) {
    SetPropertyValue<bool>("shadowEnabled", enabled);
}

int Light2D::GetShadowSampleCount() const {
    return GetPropertyValue<int>("shadowSampleCount");
}

void Light2D::SetShadowSampleCount(int count) {
    SetPropertyValue<int>("shadowSampleCount", count);
}

int Light2D::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Light2D::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int Light2D::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Light2D::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

nlohmann::json Light2D::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argAt = [&](size_t i) -> nlohmann::json {
        if (args.is_array() && i < args.size()) {
            return args[i];
        }
        return nlohmann::json();
    };
    auto numAt = [&](size_t i, float fallback) -> float {
        nlohmann::json a = argAt(i);
        return a.is_number() ? a.get<float>() : fallback;
    };

    if (method == "set_range") {
        SetRange(numAt(0, GetRange()));
    } else if (method == "get_range") {
        return GetRange();
    } else if (method == "set_intensity") {
        SetIntensity(numAt(0, GetIntensity()));
    } else if (method == "get_intensity") {
        return GetIntensity();
    } else if (method == "set_falloff") {
        SetFalloff(numAt(0, GetFalloff()));
    } else if (method == "get_falloff") {
        return GetFalloff();
    } else if (method == "set_color") {
        nlohmann::json a = argAt(0);
        if (a.is_array() && a.size() >= 3) {
            float r = a[0].is_number() ? a[0].get<float>() : 1.0f;
            float g = a[1].is_number() ? a[1].get<float>() : 1.0f;
            float b = a[2].is_number() ? a[2].get<float>() : 1.0f;
            float al = (a.size() >= 4 && a[3].is_number()) ? a[3].get<float>() : 1.0f;
            SetColor(Color(r, g, b, al));
        }
    } else if (method == "get_color") {
        Color c = GetColor();
        return nlohmann::json::array({c.r, c.g, c.b, c.a});
    } else if (method == "set_light_enabled") {
        nlohmann::json a = argAt(0);
        SetLightEnabled(a.is_boolean() ? a.get<bool>() : true);
    } else if (method == "is_light_enabled") {
        return GetLightEnabled();
    } else if (method == "set_shadow_enabled") {
        nlohmann::json a = argAt(0);
        SetShadowEnabled(a.is_boolean() ? a.get<bool>() : false);
    } else if (method == "is_shadow_enabled") {
        return GetShadowEnabled();
    } else if (method == "set_shadow_sample_count") {
        nlohmann::json a = argAt(0);
        SetShadowSampleCount(a.is_number() ? a.get<int>() : 96);
    } else if (method == "get_shadow_sample_count") {
        return GetShadowSampleCount();
    }
    return nlohmann::json();
}

int Light2D::BlendModeToInt(Light2DBlendMode mode) const {
    return static_cast<int>(mode);
}

Light2DBlendMode Light2D::IntToBlendMode(int value) const {
    if (value < 0 || value > 2) return Light2DBlendMode::SoftLight;
    return static_cast<Light2DBlendMode>(value);
}

void Light2D::RenderLight(RenderContext& ctx, const Vec2& center, float range, float rotation) {
    if (!GetLightEnabled()) {
        return;
    }

    Color lightColor = GetColor();
    float intensity = GetIntensity();
    float falloff = GetFalloff();

    // Apply intensity to color (allowing HDR values)
    Color finalColor = Color(
        lightColor.r * intensity,
        lightColor.g * intensity,
        lightColor.b * intensity,
        lightColor.a
    );

    // Get the blend mode as an integer for the render context
    // Map our Light2DBlendMode to the render context blend modes:
    // SoftLight -> Alpha (0) with modified color
    // Overlay -> Alpha (0) - overlay not supported in radial gradient yet
    // Additive -> Additive (1)
    int ctxBlendMode = 0;
    switch (GetBlendMode()) {
        case Light2DBlendMode::SoftLight:
            ctxBlendMode = 0; // Alpha blend
            break;
        case Light2DBlendMode::Overlay:
            ctxBlendMode = 0; // Alpha blend (fallback)
            break;
        case Light2DBlendMode::Additive:
            ctxBlendMode = 1; // Additive
            break;
    }

    // Inner radius of 0 means gradient starts from center
    float innerRadius = 0.0f;

    // Draw radial gradient for the light effect
    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRadialGradient(center, range, finalColor, rotation, falloff, innerRadius, ctxBlendMode);
    } else {
        ctx.drawRadialGradient(center, range, finalColor, falloff, innerRadius, ctxBlendMode);
    }
}

void Light2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner || !GetLightEnabled()) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 position;

    if (node2D) {
        position = node2D->GetGlobalPosition();
    } else {
        position = Vec2(0.0f, 0.0f);
    }

    float range = GetRange();
    float rotation = 0.0f;
    if (node2D) {
        rotation = node2D->GetGlobalRotation();
    }

    if (GetShadowEnabled()) {
        RenderShadowedLight(ctx, position, range);
    } else {
        RenderLight(ctx, position, range, rotation);
    }
}

void Light2D::RenderShadowedLight(RenderContext& ctx, const Vec2& center, float range) {
    IGfxDevice* device = ctx.getDevice();
    if (!device || range <= 0.0f) {
        return;
    }

    Color lightColor = GetColor();
    float intensity = GetIntensity();
    float falloff = GetFalloff();
    Light2DBlendMode blendMode = GetBlendMode();

    // --- Gather occluder edges within the light's reach (with cull-mode applied) ---
    std::vector<WorkEdge> edges;
    Scene* scene = m_Owner ? m_Owner->GetScene() : nullptr;
    if (scene && scene->GetRoot()) {
        std::vector<LightOccluder2D*> occluders;
        CollectOccluders(scene->GetRoot().get(), occluders);

        std::vector<OccluderEdge2D> occEdges;
        for (LightOccluder2D* occ : occluders) {
            occEdges.clear();
            occ->AppendWorldEdges(occEdges);
            OccluderCullMode2D mode = occ->GetCullMode();
            for (const OccluderEdge2D& e : occEdges) {
                // Broadphase: skip edges entirely outside the light circle.
                if (DistancePointToSegment(center, e.a, e.b) > range) {
                    continue;
                }
                if (!EdgeCasts(e.a, e.b, center, mode)) {
                    continue;
                }
                edges.push_back(WorkEdge{e.a, e.b});
            }
        }
    }

    // --- Build the set of ray angles: a uniform ring for the round light body,
    //     plus ±epsilon rays at each occluder endpoint for crisp shadow edges. ---
    int uniformSamples = GetShadowSampleCount();
    uniformSamples = std::max(16, std::min(512, uniformSamples));

    std::vector<float> angles;
    angles.reserve(uniformSamples + edges.size() * 6);
    for (int i = 0; i < uniformSamples; ++i) {
        angles.push_back((static_cast<float>(i) / static_cast<float>(uniformSamples)) * kTwoPi);
    }
    for (const WorkEdge& e : edges) {
        for (const Vec2& endpoint : {e.a, e.b}) {
            float dx = endpoint.x - center.x;
            float dy = endpoint.y - center.y;
            if (dx * dx + dy * dy <= range * range) {
                float ang = std::atan2(dy, dx);
                angles.push_back(ang - kEndpointEpsilon);
                angles.push_back(ang);
                angles.push_back(ang + kEndpointEpsilon);
            }
        }
    }

    // Normalize to [0, 2pi) and sort so the fan winds consistently.
    for (float& a : angles) {
        a = std::fmod(a, kTwoPi);
        if (a < 0.0f) {
            a += kTwoPi;
        }
    }
    std::sort(angles.begin(), angles.end());

    // --- Cast each ray, clamping the lit distance to the nearest occluder. ---
    struct Spoke { Vec2 dir; float dist; };
    std::vector<Spoke> spokes;
    spokes.reserve(angles.size());
    for (float ang : angles) {
        Vec2 dir(std::cos(ang), std::sin(ang));
        float litDist = range;
        for (const WorkEdge& e : edges) {
            float t;
            if (RaySegmentHit(center, dir, e.a, e.b, litDist, t)) {
                litDist = t;
            }
        }
        spokes.push_back(Spoke{dir, litDist});
    }

    size_t spokeCount = spokes.size();
    if (spokeCount < 3) {
        return;
    }

    // --- Build the triangle-fan mesh: center -> rim, subdivided into radial
    //     bands so the falloff curve is honored, with the light color/intensity
    //     baked into vertex colors for additive (or alpha) compositing. ---
    MeshData meshData;
    const int bands = kRadialBands;
    meshData.vertices.reserve(spokeCount * (bands + 1));
    meshData.indices.reserve(spokeCount * bands * 6);

    auto vertexColorAt = [&](float normalizedDist) -> Vec4 {
        float fall = std::pow(std::max(0.0f, std::min(1.0f, 1.0f - normalizedDist)), falloff);
        return Vec4(lightColor.r * intensity,
                    lightColor.g * intensity,
                    lightColor.b * intensity,
                    lightColor.a * fall);
    };

    for (size_t s = 0; s < spokeCount; ++s) {
        const Spoke& sp = spokes[s];
        for (int b = 0; b <= bands; ++b) {
            float frac = static_cast<float>(b) / static_cast<float>(bands);
            float d = frac * sp.dist;
            Vertex v;
            v.position = Vec3(center.x + sp.dir.x * d, center.y + sp.dir.y * d, 0.0f);
            v.normal = Vec3(0.0f, 0.0f, -1.0f);
            v.texCoord = Vec2(0.0f, 0.0f);
            v.color = vertexColorAt((d) / range);
            meshData.vertices.push_back(v);
        }
    }

    auto vertexIndex = [&](size_t spoke, int band) -> uint32_t {
        return static_cast<uint32_t>(spoke * (bands + 1) + band);
    };

    for (size_t s = 0; s < spokeCount; ++s) {
        size_t next = (s + 1) % spokeCount;
        for (int b = 0; b < bands; ++b) {
            uint32_t i0 = vertexIndex(s, b);
            uint32_t i1 = vertexIndex(next, b);
            uint32_t i2 = vertexIndex(next, b + 1);
            uint32_t i3 = vertexIndex(s, b + 1);
            meshData.indices.push_back(i0);
            meshData.indices.push_back(i1);
            meshData.indices.push_back(i2);
            meshData.indices.push_back(i0);
            meshData.indices.push_back(i2);
            meshData.indices.push_back(i3);
        }
    }

    if (meshData.indices.empty()) {
        return;
    }
    meshData.calculateBounds();

    MeshHandle lightMesh = device->createMesh(meshData);
    if (!lightMesh.isValid()) {
        return;
    }

    MaterialHandle material = (blendMode == Light2DBlendMode::Additive)
        ? ctx.getColored2DAdditiveMaterial()
        : ctx.getColoredMaterialForSpatialType();
    if (!material.isValid()) {
        material = ctx.getColoredMaterialForSpatialType();
    }

    MaterialPropertyBlock overrides;
    overrides.setColor("u_TintColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
    overrides.setInt("u_UseTexture", 0);
    overrides.setVec4("u_UVRect", Vec4(0.0f, 0.0f, 1.0f, 1.0f));

    ctx.drawMesh(lightMesh, material, Mat4::Identity(), overrides);

    device->destroyMesh(lightMesh);
}

AABB Light2D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    Vec2 center = node2D->GetGlobalPosition();
    float range = GetRange();

    // Light bounds are circular with the given range
    Vec2 min = center - Vec2(range, range);
    Vec2 max = center + Vec2(range, range);

    return AABB(
        Vec3(min.x, min.y, -0.1f),
        Vec3(max.x, max.y, 0.1f)
    );
}

RenderLayer Light2D::getRenderLayer() const {
    // Lights should be rendered as transparent overlays
    return RenderLayer::Transparent;
}

SpatialType Light2D::getSpatialType() const {
    return SpatialType::World2D;
}

} // namespace components
} // namespace lupine

