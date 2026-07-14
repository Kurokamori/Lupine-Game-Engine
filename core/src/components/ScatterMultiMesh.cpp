#include "lupine/components/ScatterMultiMesh.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>
#include <algorithm>
#include <ctime>
#include <vector>
#include <string>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

ScatterMultiMesh::ScatterMultiMesh()
    : MultiMeshGeneric("ScatterMultiMesh")
    , m_NeedsRegeneration(true)
    , m_LastScatterCount(0)
    , m_LastSeed(0)
    , m_LastShape(ScatterShapeType::Cube)
    , m_LastCubeSize(Vec3(10.0f, 10.0f, 10.0f))
    , m_LastRadius(5.0f)
{
    // Initialize random generator with time-based seed
    m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
}

ScatterMultiMesh::ScatterMultiMesh(const std::string& name)
    : MultiMeshGeneric(name)
    , m_NeedsRegeneration(true)
    , m_LastScatterCount(0)
    , m_LastSeed(0)
    , m_LastShape(ScatterShapeType::Cube)
    , m_LastCubeSize(Vec3(10.0f, 10.0f, 10.0f))
    , m_LastRadius(5.0f)
{
    m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
}

ScatterMultiMesh::~ScatterMultiMesh() {
}

void ScatterMultiMesh::DefineProperties() {
    // Call parent to inherit mesh/material/shadow properties
    MultiMeshGeneric::DefineProperties();

    // Scatter Shape group
    DefineProperty(PROPERTY_ENUM_GROUP(scatterShape, 0, "Scatter Shape", Cube, Sphere));

    // Cube dimensions (for Cube shape)
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 10.0f, 0.1f, 1000.0f, 0.5f, "Cube Dimensions"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 10.0f, 0.1f, 1000.0f, 0.5f, "Cube Dimensions"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(depth, 10.0f, 0.1f, 1000.0f, 0.5f, "Cube Dimensions"));

    // Sphere dimensions (for Sphere shape)
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 5.0f, 0.1f, 500.0f, 0.5f, "Sphere Dimensions"));

    // Scatter settings group
    DefineProperty(PROPERTY_INT_RANGE_GROUP(scatterCount, 100, 1, 100000, 10, "Scatter Settings"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(randomSeed, 12345, 0, 999999, 1, "Scatter Settings"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useRandomSeed, Bool, true, "Scatter Settings"));

    // Clumping group
    DefineProperty(PROPERTY_ENUM_GROUP(clumpingMode, 0, "Clumping", None, Clustered, Centered));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(clumpFactor, 0.5f, 0.0f, 1.0f, 0.05f, "Clumping"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(clumpCount, 5, 1, 100, 1, "Clumping"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(clumpRadius, 2.0f, 0.1f, 50.0f, 0.5f, "Clumping"));

    // Scale variation group
    DefineProperty(PROPERTY_DEFAULT_GROUP(scaleMin, Float, 0.8f, "Scale Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scaleMax, Float, 1.2f, "Scale Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uniformScale, Bool, true, "Scale Variation"));

    // Rotation variation group
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationX, Float, 0.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationY, Float, 360.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationZ, Float, 0.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(alignToSurface, Bool, false, "Rotation Variation"));

    // Color variation group
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorVariationEnabled, Bool, false, "Color Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(baseColor, Color, Color::White(), "Color Variation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(colorVariation, 0.1f, 0.0f, 1.0f, 0.05f, "Color Variation"));

    // Debug visualization group
    DefineProperty(PROPERTY_DEFAULT_GROUP(showDebugShape, Bool, true, "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, Color(0.2f, 0.8f, 0.2f, 0.8f), "Debug"));
}

void ScatterMultiMesh::OnAwake() {
    // Call parent OnAwake to load mesh
    MultiMeshGeneric::OnAwake();

    // Initial scatter generation
    m_NeedsRegeneration = true;
}

void ScatterMultiMesh::OnReady() {
    MultiMeshGeneric::OnReady();

    // Generate scatter on ready
    if (m_NeedsRegeneration) {
        RegenerateScatter();
    }
}

void ScatterMultiMesh::OnRender() {
    DrawDebugShape();
}

void ScatterMultiMesh::OnPropertyChanged(const std::string& propertyName, const nlohmann::json&) {
    // List of properties that require scatter regeneration
    static const std::vector<std::string> scatterProperties = {
        "scatterShape", "width", "height", "depth", "radius",
        "scatterCount", "randomSeed", "useRandomSeed",
        "clumpingMode", "clumpFactor", "clumpCount", "clumpRadius",
        "scaleMin", "scaleMax", "uniformScale",
        "rotationVariationX", "rotationVariationY", "rotationVariationZ", "alignToSurface",
        "colorVariationEnabled", "baseColor", "colorVariation"
    };

    // Check if this property requires regeneration
    for (const auto& prop : scatterProperties) {
        if (propertyName == prop) {
            m_NeedsRegeneration = true;
            return;
        }
    }
}

void ScatterMultiMesh::buildDrawCommands(RenderContext& ctx) {
    // Check if any scatter parameters changed and regenerate if needed
    CheckForChanges();

    if (m_NeedsRegeneration) {
        RegenerateScatter();
    }

    // Call parent to render the actual mesh instances
    MultiMeshGeneric::buildDrawCommands(ctx);
}

void ScatterMultiMesh::DrawDebugShape() {
    if (!DebugDraw::IsAvailable()) {
        return;
    }

    if (!GetShowDebugShape()) {
        return;
    }

    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera3D) {
        return;
    }

    if (!m_Owner) {
        return;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return;
    }

    Vec3 position = node3D->GetGlobalPosition();
    Color debugColor = GetDebugColor();
    ScatterShapeType shape = GetScatterShape();

    if (shape == ScatterShapeType::Cube) {
        Vec3 size(GetWidth(), GetHeight(), GetDepth());
        DebugDraw::Box(position, size, debugColor, true, 0.0f, true);
    } else {
        float radius = GetRadius();
        DebugDraw::Sphere(position, radius, debugColor, true, 24, 0.0f, true);
    }
}

void ScatterMultiMesh::CheckForChanges() {
    int currentCount = GetScatterCount();
    int currentSeed = GetRandomSeed();
    ScatterShapeType currentShape = GetScatterShape();
    Vec3 currentCubeSize = GetCubeSize();
    float currentRadius = GetRadius();

    if (currentCount != m_LastScatterCount ||
        currentSeed != m_LastSeed ||
        currentShape != m_LastShape ||
        currentCubeSize.x != m_LastCubeSize.x ||
        currentCubeSize.y != m_LastCubeSize.y ||
        currentCubeSize.z != m_LastCubeSize.z ||
        currentRadius != m_LastRadius) {
        m_NeedsRegeneration = true;
    }
}

void ScatterMultiMesh::RegenerateScatter() {
    int count = GetScatterCount();
    if (count <= 0) {
        SetInstanceCount(0);
        m_NeedsRegeneration = false;
        return;
    }

    // Seed the random generator
    if (GetUseRandomSeed()) {
        m_RandomGenerator.seed(static_cast<unsigned int>(GetRandomSeed()));
    } else {
        m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
    }

    // Set instance count in parent
    SetInstanceCount(count);

    // Generate clump centers if using clustered mode
    std::vector<Vec3> clumpCenters;
    if (GetClumpingMode() == ClumpingMode::Clustered) {
        clumpCenters = GenerateClumpCenters();
    }

    // Generate instances
    auto& instances = GetInstances();
    for (int i = 0; i < count; ++i) {
        // Generate position
        Vec3 position = GenerateRandomPoint();

        // Apply clumping
        position = ApplyClumping(position, clumpCenters);

        // Generate scale
        Vec3 scale = GenerateRandomScale();

        // Generate rotation
        Vec3 rotation = GenerateRandomRotation();

        // Build transform matrix using TRS (Translation-Rotation-Scale)
        // rotation is already in radians from GenerateRandomRotation()
        Mat4 transform = Mat4::TRS(position, rotation, scale);

        // Set instance transform
        instances[i].transform = transform;

        // Set instance color
        instances[i].color = GenerateRandomColor();

        // Clear custom data (can be used by shaders)
        instances[i].customData = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Update tracking variables
    m_LastScatterCount = count;
    m_LastSeed = GetRandomSeed();
    m_LastShape = GetScatterShape();
    m_LastCubeSize = GetCubeSize();
    m_LastRadius = GetRadius();

    m_NeedsRegeneration = false;
}

Vec3 ScatterMultiMesh::GenerateRandomPoint() {
    ScatterShapeType shape = GetScatterShape();

    if (shape == ScatterShapeType::Cube) {
        return GeneratePointInCube(GetWidth(), GetHeight(), GetDepth());
    } else {
        return GeneratePointInSphere(GetRadius());
    }
}

Vec3 ScatterMultiMesh::GeneratePointInCube(float width, float height, float depth) {
    std::uniform_real_distribution<float> distX(-width * 0.5f, width * 0.5f);
    std::uniform_real_distribution<float> distY(-height * 0.5f, height * 0.5f);
    std::uniform_real_distribution<float> distZ(-depth * 0.5f, depth * 0.5f);

    return Vec3(distX(m_RandomGenerator), distY(m_RandomGenerator), distZ(m_RandomGenerator));
}

Vec3 ScatterMultiMesh::GeneratePointInSphere(float radius) {
    // Use rejection sampling for uniform distribution within sphere
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    Vec3 point;
    do {
        point = Vec3(dist(m_RandomGenerator), dist(m_RandomGenerator), dist(m_RandomGenerator));
    } while (point.x * point.x + point.y * point.y + point.z * point.z > 1.0f);

    return point * radius;
}

Vec3 ScatterMultiMesh::ApplyClumping(const Vec3& point, const std::vector<Vec3>& clumpCenters) {
    ClumpingMode mode = GetClumpingMode();
    float factor = GetClumpFactor();

    if (mode == ClumpingMode::None || factor <= 0.0f) {
        return point;
    }

    if (mode == ClumpingMode::Centered) {
        // Move toward center (0, 0, 0)
        return point * (1.0f - factor);
    }

    if (mode == ClumpingMode::Clustered && !clumpCenters.empty()) {
        // Find nearest clump center
        float minDist = FLT_MAX;
        Vec3 nearestCenter = clumpCenters[0];

        for (const auto& center : clumpCenters) {
            Vec3 diff = point - center;
            float dist = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            if (dist < minDist) {
                minDist = dist;
                nearestCenter = center;
            }
        }

        // Move toward nearest clump center
        Vec3 direction = nearestCenter - point;
        return point + direction * factor;
    }

    return point;
}

std::vector<Vec3> ScatterMultiMesh::GenerateClumpCenters() {
    int clumpCount = GetClumpCount();
    std::vector<Vec3> centers;
    centers.reserve(clumpCount);

    float clumpRadius = GetClumpRadius();
    ScatterShapeType shape = GetScatterShape();

    for (int i = 0; i < clumpCount; ++i) {
        Vec3 center;
        if (shape == ScatterShapeType::Cube) {
            // Generate clump center within cube, but with some margin
            float margin = clumpRadius;
            float w = std::max(0.1f, GetWidth() - margin * 2.0f);
            float h = std::max(0.1f, GetHeight() - margin * 2.0f);
            float d = std::max(0.1f, GetDepth() - margin * 2.0f);
            center = GeneratePointInCube(w, h, d);
        } else {
            // Generate clump center within sphere, but with some margin
            float r = std::max(0.1f, GetRadius() - clumpRadius);
            center = GeneratePointInSphere(r);
        }
        centers.push_back(center);
    }

    return centers;
}

Vec3 ScatterMultiMesh::GenerateRandomScale() {
    Vec2 range = GetScaleRange();
    std::uniform_real_distribution<float> dist(range.x, range.y);

    if (GetUniformScale()) {
        float s = dist(m_RandomGenerator);
        return Vec3(s, s, s);
    } else {
        return Vec3(dist(m_RandomGenerator), dist(m_RandomGenerator), dist(m_RandomGenerator));
    }
}

Vec3 ScatterMultiMesh::GenerateRandomRotation() {
    Vec3 variation = GetRotationVariation();

    // Variation is in degrees, convert to radians for output
    std::uniform_real_distribution<float> distX(-variation.x * 0.5f, variation.x * 0.5f);
    std::uniform_real_distribution<float> distY(-variation.y * 0.5f, variation.y * 0.5f);
    std::uniform_real_distribution<float> distZ(-variation.z * 0.5f, variation.z * 0.5f);

    // Return rotation in radians (TRS expects radians)
    return Vec3(
        distX(m_RandomGenerator) * DEG_TO_RAD,
        distY(m_RandomGenerator) * DEG_TO_RAD,
        distZ(m_RandomGenerator) * DEG_TO_RAD
    );
}

Color ScatterMultiMesh::GenerateRandomColor() {
    if (!GetColorVariationEnabled()) {
        return GetBaseColor();
    }

    Color base = GetBaseColor();
    float variation = GetColorVariation();

    std::uniform_real_distribution<float> dist(-variation, variation);

    float r = std::clamp(base.r + dist(m_RandomGenerator), 0.0f, 1.0f);
    float g = std::clamp(base.g + dist(m_RandomGenerator), 0.0f, 1.0f);
    float b = std::clamp(base.b + dist(m_RandomGenerator), 0.0f, 1.0f);

    return Color(r, g, b, base.a);
}

// Property accessors

ScatterShapeType ScatterMultiMesh::GetScatterShape() const {
    int mode = GetPropertyValue<int>("scatterShape");
    return static_cast<ScatterShapeType>(mode);
}

void ScatterMultiMesh::SetScatterShape(ScatterShapeType shape) {
    SetPropertyValue<int>("scatterShape", static_cast<int>(shape));
    m_NeedsRegeneration = true;
}

float ScatterMultiMesh::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void ScatterMultiMesh::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
    m_NeedsRegeneration = true;
}

float ScatterMultiMesh::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void ScatterMultiMesh::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    m_NeedsRegeneration = true;
}

float ScatterMultiMesh::GetDepth() const {
    return GetPropertyValue<float>("depth");
}

void ScatterMultiMesh::SetDepth(float depth) {
    SetPropertyValue<float>("depth", depth);
    m_NeedsRegeneration = true;
}

Vec3 ScatterMultiMesh::GetCubeSize() const {
    return Vec3(GetWidth(), GetHeight(), GetDepth());
}

void ScatterMultiMesh::SetCubeSize(const Vec3& size) {
    SetWidth(size.x);
    SetHeight(size.y);
    SetDepth(size.z);
}

float ScatterMultiMesh::GetRadius() const {
    return GetPropertyValue<float>("radius");
}

void ScatterMultiMesh::SetRadius(float radius) {
    SetPropertyValue<float>("radius", radius);
    m_NeedsRegeneration = true;
}

int ScatterMultiMesh::GetScatterCount() const {
    return GetPropertyValue<int>("scatterCount");
}

void ScatterMultiMesh::SetScatterCount(int count) {
    SetPropertyValue<int>("scatterCount", count);
    m_NeedsRegeneration = true;
}

ClumpingMode ScatterMultiMesh::GetClumpingMode() const {
    int mode = GetPropertyValue<int>("clumpingMode");
    return static_cast<ClumpingMode>(mode);
}

void ScatterMultiMesh::SetClumpingMode(ClumpingMode mode) {
    SetPropertyValue<int>("clumpingMode", static_cast<int>(mode));
    m_NeedsRegeneration = true;
}

float ScatterMultiMesh::GetClumpFactor() const {
    return GetPropertyValue<float>("clumpFactor");
}

void ScatterMultiMesh::SetClumpFactor(float factor) {
    SetPropertyValue<float>("clumpFactor", factor);
    m_NeedsRegeneration = true;
}

int ScatterMultiMesh::GetClumpCount() const {
    return GetPropertyValue<int>("clumpCount");
}

void ScatterMultiMesh::SetClumpCount(int count) {
    SetPropertyValue<int>("clumpCount", count);
    m_NeedsRegeneration = true;
}

float ScatterMultiMesh::GetClumpRadius() const {
    return GetPropertyValue<float>("clumpRadius");
}

void ScatterMultiMesh::SetClumpRadius(float radius) {
    SetPropertyValue<float>("clumpRadius", radius);
    m_NeedsRegeneration = true;
}

Vec2 ScatterMultiMesh::GetScaleRange() const {
    float min = GetPropertyValue<float>("scaleMin");
    float max = GetPropertyValue<float>("scaleMax");
    return Vec2(min, max);
}

void ScatterMultiMesh::SetScaleRange(const Vec2& range) {
    SetPropertyValue<float>("scaleMin", range.x);
    SetPropertyValue<float>("scaleMax", range.y);
    m_NeedsRegeneration = true;
}

bool ScatterMultiMesh::GetUniformScale() const {
    return GetPropertyValue<bool>("uniformScale");
}

void ScatterMultiMesh::SetUniformScale(bool uniform) {
    SetPropertyValue<bool>("uniformScale", uniform);
    m_NeedsRegeneration = true;
}

Vec3 ScatterMultiMesh::GetRotationVariation() const {
    float x = GetPropertyValue<float>("rotationVariationX");
    float y = GetPropertyValue<float>("rotationVariationY");
    float z = GetPropertyValue<float>("rotationVariationZ");
    return Vec3(x, y, z);
}

void ScatterMultiMesh::SetRotationVariation(const Vec3& variation) {
    SetPropertyValue<float>("rotationVariationX", variation.x);
    SetPropertyValue<float>("rotationVariationY", variation.y);
    SetPropertyValue<float>("rotationVariationZ", variation.z);
    m_NeedsRegeneration = true;
}

bool ScatterMultiMesh::GetAlignToSurface() const {
    return GetPropertyValue<bool>("alignToSurface");
}

void ScatterMultiMesh::SetAlignToSurface(bool align) {
    SetPropertyValue<bool>("alignToSurface", align);
    m_NeedsRegeneration = true;
}

bool ScatterMultiMesh::GetColorVariationEnabled() const {
    return GetPropertyValue<bool>("colorVariationEnabled");
}

void ScatterMultiMesh::SetColorVariationEnabled(bool enabled) {
    SetPropertyValue<bool>("colorVariationEnabled", enabled);
    m_NeedsRegeneration = true;
}

Color ScatterMultiMesh::GetBaseColor() const {
    return GetPropertyValue<Color>("baseColor");
}

void ScatterMultiMesh::SetBaseColor(const Color& color) {
    SetPropertyValue<Color>("baseColor", color);
    m_NeedsRegeneration = true;
}

float ScatterMultiMesh::GetColorVariation() const {
    return GetPropertyValue<float>("colorVariation");
}

void ScatterMultiMesh::SetColorVariation(float variation) {
    SetPropertyValue<float>("colorVariation", variation);
    m_NeedsRegeneration = true;
}

int ScatterMultiMesh::GetRandomSeed() const {
    return GetPropertyValue<int>("randomSeed");
}

void ScatterMultiMesh::SetRandomSeed(int seed) {
    SetPropertyValue<int>("randomSeed", seed);
    m_NeedsRegeneration = true;
}

bool ScatterMultiMesh::GetUseRandomSeed() const {
    return GetPropertyValue<bool>("useRandomSeed");
}

void ScatterMultiMesh::SetUseRandomSeed(bool use) {
    SetPropertyValue<bool>("useRandomSeed", use);
    m_NeedsRegeneration = true;
}

bool ScatterMultiMesh::GetShowDebugShape() const {
    return GetPropertyValue<bool>("showDebugShape");
}

void ScatterMultiMesh::SetShowDebugShape(bool show) {
    SetPropertyValue<bool>("showDebugShape", show);
}

Color ScatterMultiMesh::GetDebugColor() const {
    return GetPropertyValue<Color>("debugColor");
}

void ScatterMultiMesh::SetDebugColor(const Color& color) {
    SetPropertyValue<Color>("debugColor", color);
}

} // namespace components
} // namespace lupine
