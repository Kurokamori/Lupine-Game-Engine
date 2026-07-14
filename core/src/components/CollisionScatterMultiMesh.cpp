#include "lupine/components/CollisionScatterMultiMesh.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>
#include <algorithm>
#include <ctime>
#include <vector>
#include <string>
#include <limits>

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics3d;

CollisionScatterMultiMesh::CollisionScatterMultiMesh()
    : MultiMeshGeneric("CollisionScatterMultiMesh")
    , m_NeedsRegeneration(true)
    , m_LastNodePosition(Vec3::Zero())
    , m_LastNodeRotation(Quat::Identity())
    , m_LastNodeScale(Vec3(1.0f, 1.0f, 1.0f))
    , m_TrackingInitialized(false)
{
    m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
}

CollisionScatterMultiMesh::CollisionScatterMultiMesh(const std::string& name)
    : MultiMeshGeneric(name)
    , m_NeedsRegeneration(true)
    , m_LastNodePosition(Vec3::Zero())
    , m_LastNodeRotation(Quat::Identity())
    , m_LastNodeScale(Vec3(1.0f, 1.0f, 1.0f))
    , m_TrackingInitialized(false)
{
    m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
}

CollisionScatterMultiMesh::~CollisionScatterMultiMesh() {
}

void CollisionScatterMultiMesh::DefineProperties() {
    // Call parent to inherit mesh/material/shadow properties
    MultiMeshGeneric::DefineProperties();

    // Collision Layers group
    DefineProperty(PROPERTY_GROUP(scatterLayers, Int, 1, Layers3D, "", "Collision Layers"));
    DefineProperty(PROPERTY_GROUP(cutoutLayers, Int, 0, Layers3D, "", "Collision Layers"));

    // Distribution group
    DefineProperty(PROPERTY_ENUM_GROUP(distributionMode, 0, "Distribution", Random, Grid, Outline));
    DefineProperty(PROPERTY_ENUM_GROUP(placementMode, 0, "Distribution", Surface, Volume));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(density, 1.0f, 0.01f, 100.0f, 0.1f, "Distribution"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(maxInstances, 1000, 1, 100000, 100, "Distribution"));

    // Grid Settings group
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gridSpacingX, 1.0f, 0.1f, 100.0f, 0.1f, "Grid Settings"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gridSpacingZ, 1.0f, 0.1f, 100.0f, 0.1f, "Grid Settings"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gridJitter, 0.0f, 0.0f, 1.0f, 0.05f, "Grid Settings"));

    // Surface Settings group
    DefineProperty(PROPERTY_DEFAULT_GROUP(alignToNormal, Bool, true, "Surface Settings"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalAlignmentStrength, 1.0f, 0.0f, 1.0f, 0.05f, "Surface Settings"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(surfaceOffset, 0.0f, -10.0f, 10.0f, 0.1f, "Surface Settings"));

    // Slope Filtering group
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minSlope, 0.0f, 0.0f, 90.0f, 1.0f, "Slope Filtering"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxSlope, 45.0f, 0.0f, 90.0f, 1.0f, "Slope Filtering"));

    // Scan Area group
    DefineProperty(PROPERTY_DEFAULT_GROUP(scanAreaSize, Vec3, Vec3(20.0f, 50.0f, 20.0f), "Scan Area"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scanAreaOffset, Vec3, Vec3(0.0f, 25.0f, 0.0f), "Scan Area"));

    // Scale Variation group
    DefineProperty(PROPERTY_DEFAULT_GROUP(scaleMin, Float, 0.8f, "Scale Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scaleMax, Float, 1.2f, "Scale Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uniformScale, Bool, true, "Scale Variation"));

    // Rotation Variation group
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationX, Float, 0.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationY, Float, 360.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationZ, Float, 0.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(randomYRotation, Bool, true, "Rotation Variation"));

    // Color Variation group
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorVariationEnabled, Bool, false, "Color Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(baseColor, Color, Color::White(), "Color Variation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(colorVariation, 0.1f, 0.0f, 1.0f, 0.05f, "Color Variation"));

    // Randomization group
    DefineProperty(PROPERTY_INT_RANGE_GROUP(randomSeed, 12345, 0, 999999, 1, "Randomization"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useRandomSeed, Bool, true, "Randomization"));

    // Debug group
    DefineProperty(PROPERTY_DEFAULT_GROUP(showDebugShape, Bool, true, "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, Color(0.2f, 0.6f, 1.0f, 0.8f), "Debug"));
}

void CollisionScatterMultiMesh::OnAwake() {
    MultiMeshGeneric::OnAwake();
    m_NeedsRegeneration = true;
}

void CollisionScatterMultiMesh::OnReady() {
    MultiMeshGeneric::OnReady();

    if (m_NeedsRegeneration) {
        RegenerateScatter();
    }
}

void CollisionScatterMultiMesh::OnRender() {
    DrawDebugShape();
}

void CollisionScatterMultiMesh::OnPropertyChanged(const std::string& propertyName, const nlohmann::json&) {
    // Properties that require regeneration
    static const std::vector<std::string> scatterProperties = {
        "meshPath",  // Regenerate when mesh changes
        "scatterLayers", "cutoutLayers",
        "distributionMode", "placementMode", "density", "maxInstances",
        "gridSpacingX", "gridSpacingZ", "gridJitter",
        "alignToNormal", "normalAlignmentStrength", "surfaceOffset",
        "minSlope", "maxSlope",
        "scanAreaSize", "scanAreaOffset",
        "scaleMin", "scaleMax", "uniformScale",
        "rotationVariationX", "rotationVariationY", "rotationVariationZ", "randomYRotation",
        "colorVariationEnabled", "baseColor", "colorVariation",
        "randomSeed", "useRandomSeed"
    };

    for (const auto& prop : scatterProperties) {
        if (propertyName == prop) {
            m_NeedsRegeneration = true;
            return;
        }
    }
}

void CollisionScatterMultiMesh::buildDrawCommands(RenderContext& ctx) {
    CheckForChanges();

    if (m_NeedsRegeneration) {
        RegenerateScatter();
    }

    MultiMeshGeneric::buildDrawCommands(ctx);
}

void CollisionScatterMultiMesh::CheckForChanges() {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    // Check for node transform changes
    Vec3 currentPos = node3D->GetGlobalPosition();
    Quat currentRot = node3D->GetGlobalRotation();
    Vec3 currentScale = node3D->GetGlobalScale();

    if (!m_TrackingInitialized) {
        // First frame - initialize tracking
        m_LastNodePosition = currentPos;
        m_LastNodeRotation = currentRot;
        m_LastNodeScale = currentScale;
        m_LastMeshPath = GetMeshPath();
        m_TrackingInitialized = true;
        return;
    }

    // Check if node position changed
    const float posThreshold = 0.001f;
    Vec3 posDiff = currentPos - m_LastNodePosition;
    if (posDiff.LengthSquared() > posThreshold * posThreshold) {
        m_NeedsRegeneration = true;
        m_LastNodePosition = currentPos;
    }

    // Check if node rotation changed
    const float rotThreshold = 0.001f;
    float rotDiff = std::abs(currentRot.x() - m_LastNodeRotation.x()) +
                    std::abs(currentRot.y() - m_LastNodeRotation.y()) +
                    std::abs(currentRot.z() - m_LastNodeRotation.z()) +
                    std::abs(currentRot.w() - m_LastNodeRotation.w());
    if (rotDiff > rotThreshold) {
        m_NeedsRegeneration = true;
        m_LastNodeRotation = currentRot;
    }

    // Check if node scale changed
    const float scaleThreshold = 0.001f;
    Vec3 scaleDiff = currentScale - m_LastNodeScale;
    if (scaleDiff.LengthSquared() > scaleThreshold * scaleThreshold) {
        m_NeedsRegeneration = true;
        m_LastNodeScale = currentScale;
    }

    // Check if mesh path changed
    std::string currentMeshPath = GetMeshPath();
    if (currentMeshPath != m_LastMeshPath) {
        m_NeedsRegeneration = true;
        m_LastMeshPath = currentMeshPath;
    }
}

void CollisionScatterMultiMesh::RegenerateScatter() {
    // Seed random generator
    if (GetUseRandomSeed()) {
        m_RandomGenerator.seed(static_cast<unsigned int>(GetRandomSeed()));
    } else {
        m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
    }

    // Find collision shapes in the scene first
    FindCollisionShapes();

    // Debug: Log collision shapes info

    for (size_t i = 0; i < m_CachedCollisionShapes.size(); ++i) {
        const auto& shape = m_CachedCollisionShapes[i];
        Vec3 pos = Vec3(shape.worldTransform[3][0], shape.worldTransform[3][1], shape.worldTransform[3][2]);
        
    }

    // Debug: Log scan area info
    Node3D* node3D = m_Owner ? dynamic_cast<Node3D*>(m_Owner) : nullptr;
    if (node3D) {
        Vec3 nodePos = node3D->GetGlobalPosition();
        Vec3 scanSize = GetScanAreaSize();
        Vec3 scanOffset = GetScanAreaOffset();
        Vec3 scanCenter = nodePos + scanOffset;

    }

    // Generate scatter points based on distribution mode
    m_CachedScatterPoints.clear();

    ScatterDistributionMode mode = GetDistributionMode();
    switch (mode) {
        case ScatterDistributionMode::Random:
            GenerateRandomPoints(m_CachedScatterPoints);
            break;
        case ScatterDistributionMode::Grid:
            GenerateGridPoints(m_CachedScatterPoints);
            break;
        case ScatterDistributionMode::Outline:
            GenerateOutlinePoints(m_CachedScatterPoints);
            break;
    }

    // Build instances from scatter points
    BuildInstancesFromPoints();

    m_NeedsRegeneration = false;
}

void CollisionScatterMultiMesh::GenerateRandomPoints(std::vector<ScatterPoint>& points) {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 scanSize = GetScanAreaSize();
    Vec3 scanOffset = GetScanAreaOffset();
    Vec3 scanCenter = nodePos + scanOffset;

    int maxInstances = GetMaxInstances();
    float density = GetDensity();
    uint32_t scatterLayers = GetScatterLayers();
    uint32_t cutoutLayers = GetCutoutLayers();

    // Calculate number of attempts based on density and area
    float scanArea = scanSize.x * scanSize.z;
    int numAttempts = static_cast<int>(scanArea * density);
    numAttempts = std::min(numAttempts, maxInstances * 3); // Limit attempts

    std::uniform_real_distribution<float> distX(-scanSize.x * 0.5f, scanSize.x * 0.5f);
    std::uniform_real_distribution<float> distZ(-scanSize.z * 0.5f, scanSize.z * 0.5f);

    // Debug counters
    int raycastHits = 0;
    int layerPassed = 0;
    int slopePassed = 0;

    for (int i = 0; i < numAttempts && static_cast<int>(points.size()) < maxInstances; ++i) {
        float x = distX(m_RandomGenerator);
        float z = distZ(m_RandomGenerator);

        ScatterPoint point;
        point.valid = false;

        Vec3 rayOrigin = scanCenter + Vec3(x, scanSize.y * 0.5f, z);
        Vec3 rayDir(0.0f, -1.0f, 0.0f);

        Vec3 hitPoint, hitNormal;
        uint32_t hitLayers = 0;
        if (RaycastAgainstShapes(rayOrigin, rayDir, scanSize.y, hitPoint, hitNormal, hitLayers)) {
            raycastHits++;
            // Check layer filtering
            bool isScatterLayer = (hitLayers & scatterLayers) != 0;
            bool isCutoutLayer = (hitLayers & cutoutLayers) != 0;

            if (isScatterLayer && !isCutoutLayer) {
                layerPassed++;

                if (IsSlopeAcceptable(hitNormal)) {
                    slopePassed++;
                    // Apply surface offset along normal
                    float offset = GetSurfaceOffset();
                    point.worldPosition = hitPoint + hitNormal * offset;
                    point.position = point.worldPosition; // Legacy field
                    point.normal = hitNormal;
                    point.valid = true;
                }
            }
        }

        if (point.valid) {
            points.push_back(point);
        }
    }
}

void CollisionScatterMultiMesh::GenerateGridPoints(std::vector<ScatterPoint>& points) {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 scanSize = GetScanAreaSize();
    Vec3 scanOffset = GetScanAreaOffset();
    Vec3 scanCenter = nodePos + scanOffset;

    float spacingX = GetGridSpacingX();
    float spacingZ = GetGridSpacingZ();
    float jitter = GetGridJitter();
    int maxInstances = GetMaxInstances();
    uint32_t scatterLayers = GetScatterLayers();
    uint32_t cutoutLayers = GetCutoutLayers();

    std::uniform_real_distribution<float> jitterDist(-0.5f, 0.5f);

    int gridCountX = static_cast<int>(scanSize.x / spacingX) + 1;
    int gridCountZ = static_cast<int>(scanSize.z / spacingZ) + 1;

    float startX = scanCenter.x - (gridCountX - 1) * spacingX * 0.5f;
    float startZ = scanCenter.z - (gridCountZ - 1) * spacingZ * 0.5f;

    for (int ix = 0; ix < gridCountX && static_cast<int>(points.size()) < maxInstances; ++ix) {
        for (int iz = 0; iz < gridCountZ && static_cast<int>(points.size()) < maxInstances; ++iz) {
            float x = startX + ix * spacingX;
            float z = startZ + iz * spacingZ;

            // Apply jitter
            if (jitter > 0.0f) {
                x += jitterDist(m_RandomGenerator) * spacingX * jitter;
                z += jitterDist(m_RandomGenerator) * spacingZ * jitter;
            }

            Vec3 rayOrigin = Vec3(x, scanCenter.y + scanSize.y * 0.5f, z);
            Vec3 rayDir(0.0f, -1.0f, 0.0f);

            ScatterPoint point;
            Vec3 hitPoint, hitNormal;
            uint32_t hitLayers = 0;
            if (RaycastAgainstShapes(rayOrigin, rayDir, scanSize.y, hitPoint, hitNormal, hitLayers)) {
                bool isScatterLayer = (hitLayers & scatterLayers) != 0;
                bool isCutoutLayer = (hitLayers & cutoutLayers) != 0;

                if (isScatterLayer && !isCutoutLayer && IsSlopeAcceptable(hitNormal)) {
                    float offset = GetSurfaceOffset();
                    point.worldPosition = hitPoint + hitNormal * offset;
                    point.position = point.worldPosition; // Legacy field
                    point.normal = hitNormal;
                    point.valid = true;
                    points.push_back(point);
                }
            }
        }
    }
}

void CollisionScatterMultiMesh::GenerateOutlinePoints(std::vector<ScatterPoint>& points) {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 scanSize = GetScanAreaSize();
    Vec3 scanOffset = GetScanAreaOffset();
    Vec3 scanCenter = nodePos + scanOffset;

    int maxInstances = GetMaxInstances();
    float density = GetDensity();
    uint32_t scatterLayers = GetScatterLayers();
    uint32_t cutoutLayers = GetCutoutLayers();

    // Generate points around the edges
    float perimeter = 2.0f * (scanSize.x + scanSize.z);
    int numPoints = static_cast<int>(perimeter * density);
    numPoints = std::min(numPoints, maxInstances);

    float halfX = scanSize.x * 0.5f;
    float halfZ = scanSize.z * 0.5f;

    std::uniform_real_distribution<float> offsetDist(-0.1f, 0.1f);

    for (int i = 0; i < numPoints; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(numPoints);
        float perimPos = t * perimeter;

        float x, z;
        if (perimPos < scanSize.x) {
            // Top edge
            x = -halfX + perimPos;
            z = halfZ;
        } else if (perimPos < scanSize.x + scanSize.z) {
            // Right edge
            x = halfX;
            z = halfZ - (perimPos - scanSize.x);
        } else if (perimPos < 2 * scanSize.x + scanSize.z) {
            // Bottom edge
            x = halfX - (perimPos - scanSize.x - scanSize.z);
            z = -halfZ;
        } else {
            // Left edge
            x = -halfX;
            z = -halfZ + (perimPos - 2 * scanSize.x - scanSize.z);
        }

        // Add small random offset
        x += offsetDist(m_RandomGenerator) * GetGridSpacingX();
        z += offsetDist(m_RandomGenerator) * GetGridSpacingZ();

        Vec3 rayOrigin = scanCenter + Vec3(x, scanSize.y * 0.5f, z);
        Vec3 rayDir(0.0f, -1.0f, 0.0f);

        ScatterPoint point;
        Vec3 hitPoint, hitNormal;
        uint32_t hitLayers = 0;
        if (RaycastAgainstShapes(rayOrigin, rayDir, scanSize.y, hitPoint, hitNormal, hitLayers)) {
            bool isScatterLayer = (hitLayers & scatterLayers) != 0;
            bool isCutoutLayer = (hitLayers & cutoutLayers) != 0;

            if (isScatterLayer && !isCutoutLayer && IsSlopeAcceptable(hitNormal)) {
                float offset = GetSurfaceOffset();
                point.worldPosition = hitPoint + hitNormal * offset;
                point.position = point.worldPosition; // Legacy field
                point.normal = hitNormal;
                point.valid = true;
                points.push_back(point);
            }
        }
    }
}

bool CollisionScatterMultiMesh::RaycastToSurface(const Vec3& origin, const Vec3& direction, float maxDist,
                                                  Vec3& outPoint, Vec3& outNormal) {
    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) {
        return false;
    }

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) {
        return false;
    }

    // Try single raycast first (simpler, faster)
    RaycastHit3D hit;
    if (!physicsWorld->Raycast(origin, direction, maxDist, hit)) {
        return false;
    }

    if (!hit.hit) {
        return false;
    }

    // Use the hit point and normal
    outPoint = hit.point;
    outNormal = hit.normal;

    // Ensure normal is valid (pointing somewhat upward for ground surfaces)
    if (outNormal.LengthSquared() < 0.001f) {
        outNormal = Vec3(0.0f, 1.0f, 0.0f);
    }

    // Apply surface offset
    float offset = GetSurfaceOffset();
    outPoint = outPoint + outNormal * offset;

    return true;
}

bool CollisionScatterMultiMesh::IsScatterLayer(uint32_t bodyLayers) const {
    uint32_t scatterLayers = GetScatterLayers();
    return (bodyLayers & scatterLayers) != 0;
}

bool CollisionScatterMultiMesh::IsCutoutLayer(uint32_t bodyLayers) const {
    uint32_t cutoutLayers = GetCutoutLayers();
    return (bodyLayers & cutoutLayers) != 0;
}

bool CollisionScatterMultiMesh::IsSlopeAcceptable(const Vec3& normal) const {
    // Calculate slope angle from normal (0 = flat, 90 = vertical)
    float dotUp = normal.y; // Dot with up vector (0,1,0)
    float slopeAngle = std::acos(std::clamp(dotUp, -1.0f, 1.0f)) * 180.0f / 3.14159265f;

    float minSlope = GetMinSlope();
    float maxSlope = GetMaxSlope();

    return slopeAngle >= minSlope && slopeAngle <= maxSlope;
}

Vec3 CollisionScatterMultiMesh::GenerateRandomScale() {
    Vec2 range = GetScaleRange();
    std::uniform_real_distribution<float> dist(range.x, range.y);

    if (GetUniformScale()) {
        float s = dist(m_RandomGenerator);
        return Vec3(s, s, s);
    } else {
        return Vec3(dist(m_RandomGenerator), dist(m_RandomGenerator), dist(m_RandomGenerator));
    }
}

Vec3 CollisionScatterMultiMesh::GenerateRandomRotation(const Vec3&) {
    Vec3 variation = GetRotationVariation();

    std::uniform_real_distribution<float> distX(-variation.x * 0.5f, variation.x * 0.5f);
    std::uniform_real_distribution<float> distY(-variation.y * 0.5f, variation.y * 0.5f);
    std::uniform_real_distribution<float> distZ(-variation.z * 0.5f, variation.z * 0.5f);

    Vec3 rotation;

    if (GetRandomYRotation()) {
        std::uniform_real_distribution<float> fullY(0.0f, 360.0f);
        rotation.y = fullY(m_RandomGenerator) * DEG_TO_RAD;
    } else {
        rotation.y = distY(m_RandomGenerator) * DEG_TO_RAD;
    }

    rotation.x = distX(m_RandomGenerator) * DEG_TO_RAD;
    rotation.z = distZ(m_RandomGenerator) * DEG_TO_RAD;

    return rotation;
}

Color CollisionScatterMultiMesh::GenerateRandomColor() {
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

void CollisionScatterMultiMesh::BuildInstancesFromPoints() {
    int count = static_cast<int>(m_CachedScatterPoints.size());
    SetInstanceCount(count);

    if (count == 0) return;

    // Get node transform to convert world positions to local space
    Node3D* node3D = m_Owner ? dynamic_cast<Node3D*>(m_Owner) : nullptr;
    Mat4 nodeWorldTransform = node3D ? node3D->GetGlobalTransformMatrix() : Mat4::Identity();
    Mat4 nodeInverseTransform = nodeWorldTransform.Inverse();

    auto& instances = GetInstances();
    bool alignToNormal = GetAlignToNormal();
    float alignStrength = GetNormalAlignmentStrength();

    for (int i = 0; i < count; ++i) {
        const ScatterPoint& point = m_CachedScatterPoints[i];

        // Convert world position to local space
        // worldPosition is where the raycast hit in world coordinates
        Vec4 worldPos4(point.worldPosition.x, point.worldPosition.y, point.worldPosition.z, 1.0f);
        Vec4 localPos4 = nodeInverseTransform * worldPos4;
        Vec3 localPosition(localPos4.x, localPos4.y, localPos4.z);

        Vec3 scale = GenerateRandomScale();
        Vec3 rotation = GenerateRandomRotation(point.normal);

        // Build transform in local space
        Mat4 transform;

        if (alignToNormal && alignStrength > 0.0f) {
            // Calculate rotation to align Y-axis with surface normal
            Vec3 up(0.0f, 1.0f, 0.0f);
            Vec3 targetUp = up.Lerp(point.normal, alignStrength).Normalized();

            // Find rotation from up to targetUp
            Vec3 axis = up.Cross(targetUp);
            float dot = up.Dot(targetUp);

            if (axis.LengthSquared() > 0.0001f) {
                axis = axis.Normalized();
                float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

                // Create rotation matrix for normal alignment
                Mat4 normalAlign = Mat4::Rotate(angle, axis);

                // Apply additional random rotation around Y (in the aligned space)
                Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));

                // Build transform: translate to local position, then apply normal alignment, then Y rotation, then scale
                transform = Mat4::Translate(localPosition) * normalAlign * yRot * Mat4::Scale(scale);
            } else {
                // Normal is already up or down
                if (dot < 0.0f) {
                    // Normal points down - flip around X axis
                    Mat4 flipRot = Mat4::Rotate(3.14159265f, Vec3(1.0f, 0.0f, 0.0f));
                    Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));
                    transform = Mat4::Translate(localPosition) * flipRot * yRot * Mat4::Scale(scale);
                } else {
                    // Normal is up - just apply Y rotation
                    Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));
                    transform = Mat4::Translate(localPosition) * yRot * Mat4::Scale(scale);
                }
            }
        } else {
            // No normal alignment - just position, random rotation, scale
            transform = Mat4::TRS(localPosition, rotation, scale);
        }

        instances[i].transform = transform;
        instances[i].color = GenerateRandomColor();
        instances[i].customData = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

void CollisionScatterMultiMesh::DrawDebugShape() {
    if (!DebugDraw::IsAvailable()) return;
    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera3D) return;
    if (!m_Owner) return;
    if (!GetShowDebugShape()) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 scanSize = GetScanAreaSize();
    Vec3 scanOffset = GetScanAreaOffset();
    Vec3 scanCenter = nodePos + scanOffset;

    Color debugColor = GetDebugColor();

    // Draw scan area box only
    DebugDraw::Box(scanCenter, scanSize, debugColor, true, 0.0f, true);
}

// Property accessors

uint32_t CollisionScatterMultiMesh::GetScatterLayers() const {
    return static_cast<uint32_t>(GetPropertyValue<int>("scatterLayers"));
}

void CollisionScatterMultiMesh::SetScatterLayers(uint32_t layers) {
    SetPropertyValue<int>("scatterLayers", static_cast<int>(layers));
    m_NeedsRegeneration = true;
}

uint32_t CollisionScatterMultiMesh::GetCutoutLayers() const {
    return static_cast<uint32_t>(GetPropertyValue<int>("cutoutLayers"));
}

void CollisionScatterMultiMesh::SetCutoutLayers(uint32_t layers) {
    SetPropertyValue<int>("cutoutLayers", static_cast<int>(layers));
    m_NeedsRegeneration = true;
}

ScatterDistributionMode CollisionScatterMultiMesh::GetDistributionMode() const {
    return static_cast<ScatterDistributionMode>(GetPropertyValue<int>("distributionMode"));
}

void CollisionScatterMultiMesh::SetDistributionMode(ScatterDistributionMode mode) {
    SetPropertyValue<int>("distributionMode", static_cast<int>(mode));
    m_NeedsRegeneration = true;
}

ScatterPlacementMode CollisionScatterMultiMesh::GetPlacementMode() const {
    return static_cast<ScatterPlacementMode>(GetPropertyValue<int>("placementMode"));
}

void CollisionScatterMultiMesh::SetPlacementMode(ScatterPlacementMode mode) {
    SetPropertyValue<int>("placementMode", static_cast<int>(mode));
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetDensity() const {
    return GetPropertyValue<float>("density");
}

void CollisionScatterMultiMesh::SetDensity(float density) {
    SetPropertyValue<float>("density", density);
    m_NeedsRegeneration = true;
}

int CollisionScatterMultiMesh::GetMaxInstances() const {
    return GetPropertyValue<int>("maxInstances");
}

void CollisionScatterMultiMesh::SetMaxInstances(int max) {
    SetPropertyValue<int>("maxInstances", max);
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetGridSpacingX() const {
    return GetPropertyValue<float>("gridSpacingX");
}

void CollisionScatterMultiMesh::SetGridSpacingX(float spacing) {
    SetPropertyValue<float>("gridSpacingX", spacing);
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetGridSpacingZ() const {
    return GetPropertyValue<float>("gridSpacingZ");
}

void CollisionScatterMultiMesh::SetGridSpacingZ(float spacing) {
    SetPropertyValue<float>("gridSpacingZ", spacing);
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetGridJitter() const {
    return GetPropertyValue<float>("gridJitter");
}

void CollisionScatterMultiMesh::SetGridJitter(float jitter) {
    SetPropertyValue<float>("gridJitter", jitter);
    m_NeedsRegeneration = true;
}

bool CollisionScatterMultiMesh::GetAlignToNormal() const {
    return GetPropertyValue<bool>("alignToNormal");
}

void CollisionScatterMultiMesh::SetAlignToNormal(bool align) {
    SetPropertyValue<bool>("alignToNormal", align);
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetNormalAlignmentStrength() const {
    return GetPropertyValue<float>("normalAlignmentStrength");
}

void CollisionScatterMultiMesh::SetNormalAlignmentStrength(float strength) {
    SetPropertyValue<float>("normalAlignmentStrength", strength);
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetSurfaceOffset() const {
    return GetPropertyValue<float>("surfaceOffset");
}

void CollisionScatterMultiMesh::SetSurfaceOffset(float offset) {
    SetPropertyValue<float>("surfaceOffset", offset);
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetMinSlope() const {
    return GetPropertyValue<float>("minSlope");
}

void CollisionScatterMultiMesh::SetMinSlope(float slope) {
    SetPropertyValue<float>("minSlope", slope);
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetMaxSlope() const {
    return GetPropertyValue<float>("maxSlope");
}

void CollisionScatterMultiMesh::SetMaxSlope(float slope) {
    SetPropertyValue<float>("maxSlope", slope);
    m_NeedsRegeneration = true;
}

Vec2 CollisionScatterMultiMesh::GetScaleRange() const {
    float min = GetPropertyValue<float>("scaleMin");
    float max = GetPropertyValue<float>("scaleMax");
    return Vec2(min, max);
}

void CollisionScatterMultiMesh::SetScaleRange(const Vec2& range) {
    SetPropertyValue<float>("scaleMin", range.x);
    SetPropertyValue<float>("scaleMax", range.y);
    m_NeedsRegeneration = true;
}

bool CollisionScatterMultiMesh::GetUniformScale() const {
    return GetPropertyValue<bool>("uniformScale");
}

void CollisionScatterMultiMesh::SetUniformScale(bool uniform) {
    SetPropertyValue<bool>("uniformScale", uniform);
    m_NeedsRegeneration = true;
}

Vec3 CollisionScatterMultiMesh::GetRotationVariation() const {
    float x = GetPropertyValue<float>("rotationVariationX");
    float y = GetPropertyValue<float>("rotationVariationY");
    float z = GetPropertyValue<float>("rotationVariationZ");
    return Vec3(x, y, z);
}

void CollisionScatterMultiMesh::SetRotationVariation(const Vec3& variation) {
    SetPropertyValue<float>("rotationVariationX", variation.x);
    SetPropertyValue<float>("rotationVariationY", variation.y);
    SetPropertyValue<float>("rotationVariationZ", variation.z);
    m_NeedsRegeneration = true;
}

bool CollisionScatterMultiMesh::GetRandomYRotation() const {
    return GetPropertyValue<bool>("randomYRotation");
}

void CollisionScatterMultiMesh::SetRandomYRotation(bool random) {
    SetPropertyValue<bool>("randomYRotation", random);
    m_NeedsRegeneration = true;
}

bool CollisionScatterMultiMesh::GetColorVariationEnabled() const {
    return GetPropertyValue<bool>("colorVariationEnabled");
}

void CollisionScatterMultiMesh::SetColorVariationEnabled(bool enabled) {
    SetPropertyValue<bool>("colorVariationEnabled", enabled);
    m_NeedsRegeneration = true;
}

Color CollisionScatterMultiMesh::GetBaseColor() const {
    return GetPropertyValue<Color>("baseColor");
}

void CollisionScatterMultiMesh::SetBaseColor(const Color& color) {
    SetPropertyValue<Color>("baseColor", color);
    m_NeedsRegeneration = true;
}

float CollisionScatterMultiMesh::GetColorVariation() const {
    return GetPropertyValue<float>("colorVariation");
}

void CollisionScatterMultiMesh::SetColorVariation(float variation) {
    SetPropertyValue<float>("colorVariation", variation);
    m_NeedsRegeneration = true;
}

int CollisionScatterMultiMesh::GetRandomSeed() const {
    return GetPropertyValue<int>("randomSeed");
}

void CollisionScatterMultiMesh::SetRandomSeed(int seed) {
    SetPropertyValue<int>("randomSeed", seed);
    m_NeedsRegeneration = true;
}

bool CollisionScatterMultiMesh::GetUseRandomSeed() const {
    return GetPropertyValue<bool>("useRandomSeed");
}

void CollisionScatterMultiMesh::SetUseRandomSeed(bool use) {
    SetPropertyValue<bool>("useRandomSeed", use);
    m_NeedsRegeneration = true;
}

Vec3 CollisionScatterMultiMesh::GetScanAreaSize() const {
    return GetPropertyValue<Vec3>("scanAreaSize");
}

void CollisionScatterMultiMesh::SetScanAreaSize(const Vec3& size) {
    SetPropertyValue<Vec3>("scanAreaSize", size);
    m_NeedsRegeneration = true;
}

Vec3 CollisionScatterMultiMesh::GetScanAreaOffset() const {
    return GetPropertyValue<Vec3>("scanAreaOffset");
}

void CollisionScatterMultiMesh::SetScanAreaOffset(const Vec3& offset) {
    SetPropertyValue<Vec3>("scanAreaOffset", offset);
    m_NeedsRegeneration = true;
}

bool CollisionScatterMultiMesh::GetShowDebugShape() const {
    return GetPropertyValue<bool>("showDebugShape");
}

void CollisionScatterMultiMesh::SetShowDebugShape(bool show) {
    SetPropertyValue<bool>("showDebugShape", show);
}

Color CollisionScatterMultiMesh::GetDebugColor() const {
    return GetPropertyValue<Color>("debugColor");
}

void CollisionScatterMultiMesh::SetDebugColor(const Color& color) {
    SetPropertyValue<Color>("debugColor", color);
}

// ===== Direct Collision Shape Raycasting Implementation =====

namespace {
    // Helper to recursively find collision components
    void FindCollisionComponentsRecursive(core::Node* node,
                                           std::vector<CollisionScatterMultiMesh::CollisionShapeInfo>& shapes) {
        if (!node) return;

        // Check if this node has a CollisionMesh3DComponent
        auto collisionComp = node->GetComponent<CollisionMesh3DComponent>();
        if (collisionComp) {
            CollisionScatterMultiMesh::CollisionShapeInfo info;
            info.component = collisionComp.get();
            info.node = node;

            // Get world transform from Node3D
            core::Node3D* node3D = dynamic_cast<core::Node3D*>(node);
            if (node3D) {
                info.worldTransform = node3D->GetGlobalTransformMatrix();
            } else {
                info.worldTransform = Mat4::Identity();
            }

            info.layers = collisionComp->GetCollisionLayers();
            shapes.push_back(info);
        }

        // Recurse into children
        for (const auto& child : node->GetChildren()) {
            FindCollisionComponentsRecursive(child.get(), shapes);
        }
    }
}

void CollisionScatterMultiMesh::FindCollisionShapes() {
    m_CachedCollisionShapes.clear();

    // Try to get scene from owner node first (works in editor)
    core::Scene* scene = nullptr;
    if (m_Owner) {
        scene = m_Owner->GetScene();
    }

    // Fallback to SceneManager if owner doesn't have a scene
    if (!scene) {
        auto* sceneManager = SceneManager::GetInstance();
        if (sceneManager) {
            scene = sceneManager->GetCurrentScene();
        }
    }

    if (!scene) return;

    auto root = scene->GetRoot();
    if (!root) return;

    FindCollisionComponentsRecursive(root.get(), m_CachedCollisionShapes);
}

bool CollisionScatterMultiMesh::RaycastAgainstShapes(const Vec3& origin, const Vec3& direction, float maxDist,
                                                      Vec3& outPoint, Vec3& outNormal, uint32_t& outLayers) {
    float closestDist = maxDist;
    bool hitAny = false;

    for (const auto& shapeInfo : m_CachedCollisionShapes) {
        if (!shapeInfo.component) continue;

        CollisionShape3DType shapeType = shapeInfo.component->GetShapeType();
        Vec3 hitNormal;
        float hitDist = -1.0f;

        // Get shape center in world space (transform position + rotated offset)
        Vec3 shapeOffset = shapeInfo.component->GetOffset();
        // Translation is in column 3 of the matrix
        Vec3 worldPos = Vec3(
            shapeInfo.worldTransform[3][0],
            shapeInfo.worldTransform[3][1],
            shapeInfo.worldTransform[3][2]
        );
        // Transform the offset by the rotation (w=0 means no translation, just rotation+scale)
        Vec4 offsetTransformed = shapeInfo.worldTransform * Vec4(shapeOffset.x, shapeOffset.y, shapeOffset.z, 0.0f);
        Vec3 shapeCenter = worldPos + Vec3(offsetTransformed.x, offsetTransformed.y, offsetTransformed.z);

        // Extract scale from world transform matrix
        Vec3 worldScale = Vec3(
            Vec3(shapeInfo.worldTransform[0][0], shapeInfo.worldTransform[0][1], shapeInfo.worldTransform[0][2]).Length(),
            Vec3(shapeInfo.worldTransform[1][0], shapeInfo.worldTransform[1][1], shapeInfo.worldTransform[1][2]).Length(),
            Vec3(shapeInfo.worldTransform[2][0], shapeInfo.worldTransform[2][1], shapeInfo.worldTransform[2][2]).Length()
        );

        switch (shapeType) {
            case CollisionShape3DType::Box: {
                Vec3 size = shapeInfo.component->GetSize();
                // Apply world scale to size
                size = Vec3(size.x * worldScale.x, size.y * worldScale.y, size.z * worldScale.z);
                hitDist = RayBoxIntersect(origin, direction, shapeCenter, size,
                                          shapeInfo.worldTransform, hitNormal);
                break;
            }
            case CollisionShape3DType::Sphere: {
                float radius = shapeInfo.component->GetRadius();
                // Apply max scale component to radius
                radius *= std::max({worldScale.x, worldScale.y, worldScale.z});
                hitDist = RaySphereIntersect(origin, direction, shapeCenter, radius, hitNormal);
                break;
            }
            case CollisionShape3DType::Capsule: {
                float radius = shapeInfo.component->GetRadius();
                float height = shapeInfo.component->GetHeight();
                // Apply scale
                radius *= std::max(worldScale.x, worldScale.z);
                height *= worldScale.y;
                hitDist = RayCapsuleIntersect(origin, direction, shapeCenter, radius, height,
                                               shapeInfo.worldTransform, hitNormal);
                break;
            }
            case CollisionShape3DType::Cylinder: {
                float radius = shapeInfo.component->GetRadius();
                float height = shapeInfo.component->GetHeight();
                // Apply scale
                radius *= std::max(worldScale.x, worldScale.z);
                height *= worldScale.y;
                hitDist = RayCylinderIntersect(origin, direction, shapeCenter, radius, height,
                                                shapeInfo.worldTransform, hitNormal);
                break;
            }
            case CollisionShape3DType::Plane: {
                Vec3 planeNormal = shapeInfo.component->GetPlaneNormal();
                float planeWidth = shapeInfo.component->GetPlaneWidth();
                float planeLength = shapeInfo.component->GetPlaneLength();
                // Apply scale to plane dimensions
                planeWidth *= worldScale.x;
                planeLength *= worldScale.z;
                hitDist = RayPlaneIntersect(origin, direction, shapeCenter, planeNormal,
                                            planeWidth, planeLength, shapeInfo.worldTransform, hitNormal);
                break;
            }
            default:
                // Mesh and other types - use bounding box as approximation
                Vec3 size = shapeInfo.component->GetSize();
                if (size.LengthSquared() < 0.001f) {
                    size = Vec3(1.0f, 1.0f, 1.0f); // Default size
                }
                // Apply world scale
                size = Vec3(size.x * worldScale.x, size.y * worldScale.y, size.z * worldScale.z);
                hitDist = RayBoxIntersect(origin, direction, shapeCenter, size,
                                          shapeInfo.worldTransform, hitNormal);
                break;
        }

        if (hitDist > 0.0f && hitDist < closestDist) {
            closestDist = hitDist;
            outPoint = origin + direction * hitDist;
            outNormal = hitNormal;
            outLayers = shapeInfo.layers;
            hitAny = true;
        }
    }

    return hitAny;
}

float CollisionScatterMultiMesh::RayBoxIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                                                  const Vec3& boxCenter, const Vec3& boxSize,
                                                  const Mat4& boxTransform, Vec3& outNormal) {
    // Extract rotation from transform (columns 0-2 are the rotated axes)
    Vec3 axisX = Vec3(boxTransform[0][0], boxTransform[0][1], boxTransform[0][2]).Normalized();
    Vec3 axisY = Vec3(boxTransform[1][0], boxTransform[1][1], boxTransform[1][2]).Normalized();
    Vec3 axisZ = Vec3(boxTransform[2][0], boxTransform[2][1], boxTransform[2][2]).Normalized();

    Vec3 halfSize = boxSize * 0.5f;

    // Vector from box center to ray origin
    Vec3 delta = rayOrigin - boxCenter;

    float tMin = -std::numeric_limits<float>::max();
    float tMax = std::numeric_limits<float>::max();
    Vec3 hitNormalLocal(0.0f, 0.0f, 0.0f);

    // Test against each oriented slab
    Vec3 axes[3] = { axisX, axisY, axisZ };
    float extents[3] = { halfSize.x, halfSize.y, halfSize.z };

    for (int i = 0; i < 3; ++i) {
        const Vec3& axis = axes[i];
        float extent = extents[i];

        float e = axis.Dot(delta);
        float f = axis.Dot(rayDir);

        if (std::abs(f) > 0.0001f) {
            // t for negative face (-extent): tNeg
            // t for positive face (+extent): tPos
            float tNeg = (-e - extent) / f;
            float tPos = (-e + extent) / f;

            // Determine entry t and its normal
            float tEntry, tExit;
            Vec3 entryNormal;

            if (tNeg < tPos) {
                // Enter through negative face, exit through positive
                tEntry = tNeg;
                tExit = tPos;
                entryNormal = axis * -1.0f;  // Normal points outward from -extent face
            } else {
                // Enter through positive face, exit through negative
                tEntry = tPos;
                tExit = tNeg;
                entryNormal = axis;  // Normal points outward from +extent face
            }

            if (tEntry > tMin) {
                tMin = tEntry;
                hitNormalLocal = entryNormal;
            }
            if (tExit < tMax) {
                tMax = tExit;
            }

            if (tMin > tMax) return -1.0f;
            if (tMax < 0.0f) return -1.0f;
        } else {
            // Ray is parallel to slab
            if (std::abs(e) > extent) {
                return -1.0f;
            }
        }
    }

    if (tMin < 0.0f) return -1.0f;

    // The normal is already in world space since we used world-space axes
    outNormal = hitNormalLocal;

    return tMin;
}

float CollisionScatterMultiMesh::RaySphereIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                                                     const Vec3& sphereCenter, float radius, Vec3& outNormal) {
    Vec3 oc = rayOrigin - sphereCenter;

    float a = rayDir.Dot(rayDir);
    float b = 2.0f * oc.Dot(rayDir);
    float c = oc.Dot(oc) - radius * radius;

    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) return -1.0f;

    float sqrtDisc = std::sqrt(discriminant);
    float t1 = (-b - sqrtDisc) / (2.0f * a);
    float t2 = (-b + sqrtDisc) / (2.0f * a);

    float t = t1;
    if (t < 0.0f) t = t2;
    if (t < 0.0f) return -1.0f;

    Vec3 hitPoint = rayOrigin + rayDir * t;
    outNormal = (hitPoint - sphereCenter).Normalized();

    return t;
}

float CollisionScatterMultiMesh::RayCapsuleIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                                                      const Vec3& capsuleCenter, float radius, float height,
                                                      const Mat4& transform, Vec3& outNormal) {
    // Capsule is a cylinder with hemisphere caps
    // Simplify: treat as cylinder + check end caps
    float halfHeight = height * 0.5f;
    Vec3 topCenter = capsuleCenter + Vec3(0.0f, halfHeight, 0.0f);
    Vec3 bottomCenter = capsuleCenter - Vec3(0.0f, halfHeight, 0.0f);

    // Try sphere intersections at caps
    float tTop = RaySphereIntersect(rayOrigin, rayDir, topCenter, radius, outNormal);
    float tBottom = RaySphereIntersect(rayOrigin, rayDir, bottomCenter, radius, outNormal);

    // Try cylinder intersection
    float tCylinder = RayCylinderIntersect(rayOrigin, rayDir, capsuleCenter, radius, height, transform, outNormal);

    // Return closest hit
    float closest = -1.0f;
    if (tTop > 0.0f && (closest < 0.0f || tTop < closest)) {
        closest = tTop;
        Vec3 hitPoint = rayOrigin + rayDir * tTop;
        outNormal = (hitPoint - topCenter).Normalized();
    }
    if (tBottom > 0.0f && (closest < 0.0f || tBottom < closest)) {
        closest = tBottom;
        Vec3 hitPoint = rayOrigin + rayDir * tBottom;
        outNormal = (hitPoint - bottomCenter).Normalized();
    }
    if (tCylinder > 0.0f && (closest < 0.0f || tCylinder < closest)) {
        closest = tCylinder;
        // Normal already set by cylinder intersection
    }

    return closest;
}

float CollisionScatterMultiMesh::RayCylinderIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                                                       const Vec3& cylinderCenter, float radius, float height,
                                                       const Mat4&, Vec3& outNormal) {
    // Infinite cylinder along Y axis, then clip to height
    float halfHeight = height * 0.5f;

    // Project ray to XZ plane for infinite cylinder test
    Vec3 oc = rayOrigin - cylinderCenter;

    float a = rayDir.x * rayDir.x + rayDir.z * rayDir.z;
    float b = 2.0f * (oc.x * rayDir.x + oc.z * rayDir.z);
    float c = oc.x * oc.x + oc.z * oc.z - radius * radius;

    if (std::abs(a) < 0.0001f) return -1.0f; // Ray is parallel to cylinder axis

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return -1.0f;

    float sqrtDisc = std::sqrt(discriminant);
    float t1 = (-b - sqrtDisc) / (2.0f * a);
    float t2 = (-b + sqrtDisc) / (2.0f * a);

    // Check both intersection points for height constraint
    for (float t : {t1, t2}) {
        if (t < 0.0f) continue;

        Vec3 hitPoint = rayOrigin + rayDir * t;
        float hitY = hitPoint.y - cylinderCenter.y;

        if (hitY >= -halfHeight && hitY <= halfHeight) {
            // Valid cylinder hit
            outNormal = Vec3(hitPoint.x - cylinderCenter.x, 0.0f, hitPoint.z - cylinderCenter.z).Normalized();
            return t;
        }
    }

    return -1.0f;
}

float CollisionScatterMultiMesh::RayPlaneIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                                                    const Vec3& planeCenter, const Vec3& planeNormal,
                                                    float planeWidth, float planeLength,
                                                    const Mat4&, Vec3& outNormal) {
    Vec3 normal = planeNormal.Normalized();
    if (normal.LengthSquared() < 0.001f) {
        normal = Vec3(0.0f, 1.0f, 0.0f); // Default to up
    }

    float denom = normal.Dot(rayDir);

    // Check if ray is parallel to plane
    if (std::abs(denom) < 0.0001f) return -1.0f;

    Vec3 p0 = planeCenter;
    float t = normal.Dot(p0 - rayOrigin) / denom;

    if (t < 0.0f) return -1.0f;

    // Check if hit point is within finite plane bounds
    Vec3 hitPoint = rayOrigin + rayDir * t;
    Vec3 localHit = hitPoint - planeCenter;

    // Get tangent vectors for the plane
    Vec3 tangent1, tangent2;
    if (std::abs(normal.y) < 0.99f) {
        tangent1 = Vec3(0.0f, 1.0f, 0.0f).Cross(normal).Normalized();
    } else {
        tangent1 = Vec3(1.0f, 0.0f, 0.0f).Cross(normal).Normalized();
    }
    tangent2 = normal.Cross(tangent1).Normalized();

    float u = localHit.Dot(tangent1);
    float v = localHit.Dot(tangent2);

    float halfWidth = (planeWidth > 0.0f) ? planeWidth * 0.5f : 1000.0f;
    float halfLength = (planeLength > 0.0f) ? planeLength * 0.5f : 1000.0f;

    if (std::abs(u) > halfWidth || std::abs(v) > halfLength) {
        return -1.0f;
    }

    outNormal = normal;
    return t;
}

} // namespace components
} // namespace lupine
