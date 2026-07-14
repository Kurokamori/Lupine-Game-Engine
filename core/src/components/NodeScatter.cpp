#include "lupine/components/NodeScatter.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/logger/Logger.hpp"
#include <cmath>
#include <algorithm>
#include <ctime>
#include <limits>
#include <functional>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {
    // Helper to regenerate UUIDs for a node tree
    void RegenerateNodeUUIDs(std::shared_ptr<Node> node) {
        if (!node) return;

        // Generate new UUID for the node
        const_cast<UUID&>(node->GetUUID()) = UUID();

        // Regenerate UUIDs for all components
        for (const auto& component : node->GetComponents()) {
            if (component) {
                const_cast<UUID&>(component->GetUUID()) = UUID();
            }
        }

        // Recursively process children
        for (const auto& child : node->GetChildren()) {
            RegenerateNodeUUIDs(child);
        }
    }
}

NodeScatter::NodeScatter()
    : Component("NodeScatter")
    , m_NeedsRegeneration(true)
    , m_IsInitialized(false)
    , m_IsProgressivelyLoading(false)
    , m_ProgressiveLoadIndex(0)
    , m_LastNodePosition(Vec3::Zero())
    , m_LastNodeRotation(Quat::Identity())
    , m_LastNodeScale(Vec3(1.0f, 1.0f, 1.0f))
    , m_TrackingInitialized(false)
{
    m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
}

NodeScatter::NodeScatter(const std::string& name)
    : Component(name)
    , m_NeedsRegeneration(true)
    , m_IsInitialized(false)
    , m_IsProgressivelyLoading(false)
    , m_ProgressiveLoadIndex(0)
    , m_LastNodePosition(Vec3::Zero())
    , m_LastNodeRotation(Quat::Identity())
    , m_LastNodeScale(Vec3(1.0f, 1.0f, 1.0f))
    , m_TrackingInitialized(false)
{
    m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
}

NodeScatter::~NodeScatter() {
    ClearScatteredInstances();
}

void NodeScatter::DefineProperties() {
    // Scatter Mode group
    DefineProperty(PROPERTY_ENUM_GROUP(scatterMode, 0, "Scatter Mode", Simple, Collision));

    // Collision Layers group (only used in Collision mode)
    DefineProperty(PROPERTY_GROUP(scatterLayers, Int, 1, Layers3D, "", "Collision Layers"));
    DefineProperty(PROPERTY_GROUP(cutoutLayers, Int, 0, Layers3D, "", "Collision Layers"));

    // Distribution group
    DefineProperty(PROPERTY_ENUM_GROUP(distributionMode, 0, "Distribution", Random, Grid));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(instanceCount, 100, 1, 10000, 10, "Distribution"));

    // Grid Settings group
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gridSpacingX, 2.0f, 0.1f, 100.0f, 0.1f, "Grid Settings"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gridSpacingZ, 2.0f, 0.1f, 100.0f, 0.1f, "Grid Settings"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gridJitter, 0.0f, 0.0f, 1.0f, 0.05f, "Grid Settings"));

    // Scatter Area group
    DefineProperty(PROPERTY_DEFAULT_GROUP(scatterAreaSize, Vec3, Vec3(20.0f, 50.0f, 20.0f), "Scatter Area"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scatterAreaOffset, Vec3, Vec3(0.0f, 25.0f, 0.0f), "Scatter Area"));

    // Surface Settings group (for Collision mode)
    DefineProperty(PROPERTY_DEFAULT_GROUP(alignToNormal, Bool, true, "Surface Settings"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalAlignmentStrength, 1.0f, 0.0f, 1.0f, 0.05f, "Surface Settings"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(surfaceOffset, 0.0f, -10.0f, 10.0f, 0.1f, "Surface Settings"));

    // Slope Filtering group (for Collision mode)
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minSlope, 0.0f, 0.0f, 90.0f, 1.0f, "Slope Filtering"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxSlope, 45.0f, 0.0f, 90.0f, 1.0f, "Slope Filtering"));

    // Scale Variation group
    DefineProperty(PROPERTY_DEFAULT_GROUP(scaleMin, Float, 0.8f, "Scale Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(scaleMax, Float, 1.2f, "Scale Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uniformScale, Bool, true, "Scale Variation"));

    // Rotation Variation group
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationX, Float, 0.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationY, Float, 360.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(rotationVariationZ, Float, 0.0f, "Rotation Variation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(randomYRotation, Bool, true, "Rotation Variation"));

    // Randomization group
    DefineProperty(PROPERTY_INT_RANGE_GROUP(randomSeed, 12345, 0, 999999, 1, "Randomization"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useRandomSeed, Bool, true, "Randomization"));

    // Debug group
    DefineProperty(PROPERTY_DEFAULT_GROUP(showDebugShape, Bool, true, "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, Color(0.2f, 1.0f, 0.6f, 0.8f), "Debug"));

    // Performance / Optimization group
    DefineProperty(PROPERTY_DEFAULT_GROUP(progressiveLoading, Bool, true, "Performance"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(instancesPerFrame, 10, 1, 100, 5, "Performance"));
    DefineProperty(PROPERTY_ENUM_GROUP(collisionOptimization, 0, "Performance", Full, SimplifiedBox, SimplifiedSphere, None));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(collisionDistance, 50.0f, 0.0f, 500.0f, 10.0f, "Performance"));
}

void NodeScatter::OnAwake() {
    m_NeedsRegeneration = true;
    m_IsInitialized = false;
}

void NodeScatter::OnReady() {
    m_IsInitialized = true;
    if (m_NeedsRegeneration) {
        RegenerateScatter();
    }
}

void NodeScatter::OnRender() {
    DrawDebugShape();
}

void NodeScatter::OnUpdate(float deltaTime) {
    (void)deltaTime;

    if (!m_IsInitialized) return;

    // Handle progressive loading
    if (m_IsProgressivelyLoading) {
        int batchSize = GetInstancesPerFrame();
        CreateInstanceBatch(batchSize);
        return; // Don't check for changes while loading
    }

    CheckForChanges();

    if (m_NeedsRegeneration) {
        RegenerateScatter();
    }
}

void NodeScatter::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    (void)newValue;

    // Properties that require regeneration
    static const std::vector<std::string> scatterProperties = {
        "scatterMode",
        "scatterLayers", "cutoutLayers",
        "distributionMode", "instanceCount",
        "gridSpacingX", "gridSpacingZ", "gridJitter",
        "scatterAreaSize", "scatterAreaOffset",
        "alignToNormal", "normalAlignmentStrength", "surfaceOffset",
        "minSlope", "maxSlope",
        "scaleMin", "scaleMax", "uniformScale",
        "rotationVariationX", "rotationVariationY", "rotationVariationZ", "randomYRotation",
        "randomSeed", "useRandomSeed"
    };

    for (const auto& prop : scatterProperties) {
        if (propertyName == prop) {
            m_NeedsRegeneration = true;
            return;
        }
    }
}

void NodeScatter::CheckForChanges() {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 currentPos = node3D->GetGlobalPosition();
    Quat currentRot = node3D->GetGlobalRotation();
    Vec3 currentScale = node3D->GetGlobalScale();

    if (!m_TrackingInitialized) {
        m_LastNodePosition = currentPos;
        m_LastNodeRotation = currentRot;
        m_LastNodeScale = currentScale;
        m_TrackingInitialized = true;
        return;
    }

    const float posThreshold = 0.001f;
    Vec3 posDiff = currentPos - m_LastNodePosition;
    if (posDiff.LengthSquared() > posThreshold * posThreshold) {
        m_NeedsRegeneration = true;
        m_LastNodePosition = currentPos;
    }

    const float rotThreshold = 0.001f;
    float rotDiff = std::abs(currentRot.x() - m_LastNodeRotation.x()) +
                    std::abs(currentRot.y() - m_LastNodeRotation.y()) +
                    std::abs(currentRot.z() - m_LastNodeRotation.z()) +
                    std::abs(currentRot.w() - m_LastNodeRotation.w());
    if (rotDiff > rotThreshold) {
        m_NeedsRegeneration = true;
        m_LastNodeRotation = currentRot;
    }

    const float scaleThreshold = 0.001f;
    Vec3 scaleDiff = currentScale - m_LastNodeScale;
    if (scaleDiff.LengthSquared() > scaleThreshold * scaleThreshold) {
        m_NeedsRegeneration = true;
        m_LastNodeScale = currentScale;
    }
}

std::shared_ptr<Node3D> NodeScatter::FindTemplateNode() {
    if (!m_Owner) return nullptr;

    // Find first Node3D child that is NOT a scattered instance
    for (const auto& child : m_Owner->GetChildren()) {
        Node3D* node3D = dynamic_cast<Node3D*>(child.get());
        if (node3D) {
            // Check if this is one of our scattered instances
            bool isScatteredInstance = false;
            for (const auto& instance : m_ScatteredInstances) {
                if (instance.get() == node3D) {
                    isScatteredInstance = true;
                    break;
                }
            }

            if (!isScatteredInstance) {
                return std::dynamic_pointer_cast<Node3D>(child);
            }
        }
    }

    return nullptr;
}

void NodeScatter::RegenerateScatter() {
    if (!m_IsInitialized) return;

    // Seed random generator
    if (GetUseRandomSeed()) {
        m_RandomGenerator.seed(static_cast<unsigned int>(GetRandomSeed()));
    } else {
        m_RandomGenerator.seed(static_cast<unsigned int>(std::time(nullptr)));
    }

    // Find template node
    auto templateNode = FindTemplateNode();
    if (!templateNode) {
        
        ClearScatteredInstances();
        m_NeedsRegeneration = false;
        return;
    }

    m_TemplateNode = templateNode;

    // Hide the template node (it's just for cloning)
    templateNode->SetVisible(false);

    // Find collision shapes if in collision mode
    if (GetScatterMode() == NodeScatterMode::Collision) {
        FindCollisionShapes();
    }

    // Generate scatter points
    m_CachedScatterPoints.clear();
    GenerateScatterPoints();

    // Create scattered instances
    CreateScatteredInstances();

    m_NeedsRegeneration = false;
}

void NodeScatter::GenerateScatterPoints() {
    NodeScatterMode mode = GetScatterMode();
    NodeScatterDistribution dist = GetDistributionMode();

    if (mode == NodeScatterMode::Simple) {
        if (dist == NodeScatterDistribution::Random) {
            GenerateSimpleRandomPoints(m_CachedScatterPoints);
        } else {
            GenerateSimpleGridPoints(m_CachedScatterPoints);
        }
    } else {
        if (dist == NodeScatterDistribution::Random) {
            GenerateCollisionRandomPoints(m_CachedScatterPoints);
        } else {
            GenerateCollisionGridPoints(m_CachedScatterPoints);
        }
    }
}

void NodeScatter::GenerateSimpleRandomPoints(std::vector<ScatterPoint>& points) {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 areaSize = GetScatterAreaSize();
    Vec3 areaOffset = GetScatterAreaOffset();
    Vec3 areaCenter = nodePos + areaOffset;

    int count = GetInstanceCount();

    std::uniform_real_distribution<float> distX(-areaSize.x * 0.5f, areaSize.x * 0.5f);
    std::uniform_real_distribution<float> distY(-areaSize.y * 0.5f, areaSize.y * 0.5f);
    std::uniform_real_distribution<float> distZ(-areaSize.z * 0.5f, areaSize.z * 0.5f);

    for (int i = 0; i < count; ++i) {
        ScatterPoint point;
        point.position = areaCenter + Vec3(distX(m_RandomGenerator), distY(m_RandomGenerator), distZ(m_RandomGenerator));
        point.normal = Vec3(0.0f, 1.0f, 0.0f);
        point.valid = true;
        points.push_back(point);
    }
}

void NodeScatter::GenerateSimpleGridPoints(std::vector<ScatterPoint>& points) {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 areaSize = GetScatterAreaSize();
    Vec3 areaOffset = GetScatterAreaOffset();
    Vec3 areaCenter = nodePos + areaOffset;

    float spacingX = GetGridSpacingX();
    float spacingZ = GetGridSpacingZ();
    float jitter = GetGridJitter();
    int maxCount = GetInstanceCount();

    std::uniform_real_distribution<float> jitterDist(-0.5f, 0.5f);
    std::uniform_real_distribution<float> yDist(-areaSize.y * 0.5f, areaSize.y * 0.5f);

    int gridCountX = static_cast<int>(areaSize.x / spacingX) + 1;
    int gridCountZ = static_cast<int>(areaSize.z / spacingZ) + 1;

    float startX = areaCenter.x - (gridCountX - 1) * spacingX * 0.5f;
    float startZ = areaCenter.z - (gridCountZ - 1) * spacingZ * 0.5f;

    for (int ix = 0; ix < gridCountX && static_cast<int>(points.size()) < maxCount; ++ix) {
        for (int iz = 0; iz < gridCountZ && static_cast<int>(points.size()) < maxCount; ++iz) {
            float x = startX + ix * spacingX;
            float z = startZ + iz * spacingZ;

            if (jitter > 0.0f) {
                x += jitterDist(m_RandomGenerator) * spacingX * jitter;
                z += jitterDist(m_RandomGenerator) * spacingZ * jitter;
            }

            ScatterPoint point;
            point.position = Vec3(x, areaCenter.y + yDist(m_RandomGenerator), z);
            point.normal = Vec3(0.0f, 1.0f, 0.0f);
            point.valid = true;
            points.push_back(point);
        }
    }
}

void NodeScatter::GenerateCollisionRandomPoints(std::vector<ScatterPoint>& points) {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 scanSize = GetScatterAreaSize();
    Vec3 scanOffset = GetScatterAreaOffset();
    Vec3 scanCenter = nodePos + scanOffset;

    int maxCount = GetInstanceCount();
    uint32_t scatterLayers = GetScatterLayers();
    uint32_t cutoutLayers = GetCutoutLayers();

    int numAttempts = maxCount * 3;

    std::uniform_real_distribution<float> distX(-scanSize.x * 0.5f, scanSize.x * 0.5f);
    std::uniform_real_distribution<float> distZ(-scanSize.z * 0.5f, scanSize.z * 0.5f);

    for (int i = 0; i < numAttempts && static_cast<int>(points.size()) < maxCount; ++i) {
        float x = distX(m_RandomGenerator);
        float z = distZ(m_RandomGenerator);

        Vec3 rayOrigin = scanCenter + Vec3(x, scanSize.y * 0.5f, z);
        Vec3 rayDir(0.0f, -1.0f, 0.0f);

        Vec3 hitPoint, hitNormal;
        uint32_t hitLayers = 0;
        if (RaycastAgainstShapes(rayOrigin, rayDir, scanSize.y, hitPoint, hitNormal, hitLayers)) {
            bool isScatterLayer = (hitLayers & scatterLayers) != 0;
            bool isCutoutLayer = (hitLayers & cutoutLayers) != 0;

            if (isScatterLayer && !isCutoutLayer && IsSlopeAcceptable(hitNormal)) {
                float offset = GetSurfaceOffset();
                ScatterPoint point;
                point.position = hitPoint + hitNormal * offset;
                point.normal = hitNormal;
                point.valid = true;
                points.push_back(point);
            }
        }
    }
}

void NodeScatter::GenerateCollisionGridPoints(std::vector<ScatterPoint>& points) {
    if (!m_Owner) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 scanSize = GetScatterAreaSize();
    Vec3 scanOffset = GetScatterAreaOffset();
    Vec3 scanCenter = nodePos + scanOffset;

    float spacingX = GetGridSpacingX();
    float spacingZ = GetGridSpacingZ();
    float jitter = GetGridJitter();
    int maxCount = GetInstanceCount();
    uint32_t scatterLayers = GetScatterLayers();
    uint32_t cutoutLayers = GetCutoutLayers();

    std::uniform_real_distribution<float> jitterDist(-0.5f, 0.5f);

    int gridCountX = static_cast<int>(scanSize.x / spacingX) + 1;
    int gridCountZ = static_cast<int>(scanSize.z / spacingZ) + 1;

    float startX = scanCenter.x - (gridCountX - 1) * spacingX * 0.5f;
    float startZ = scanCenter.z - (gridCountZ - 1) * spacingZ * 0.5f;

    for (int ix = 0; ix < gridCountX && static_cast<int>(points.size()) < maxCount; ++ix) {
        for (int iz = 0; iz < gridCountZ && static_cast<int>(points.size()) < maxCount; ++iz) {
            float x = startX + ix * spacingX;
            float z = startZ + iz * spacingZ;

            if (jitter > 0.0f) {
                x += jitterDist(m_RandomGenerator) * spacingX * jitter;
                z += jitterDist(m_RandomGenerator) * spacingZ * jitter;
            }

            Vec3 rayOrigin = Vec3(x, scanCenter.y + scanSize.y * 0.5f, z);
            Vec3 rayDir(0.0f, -1.0f, 0.0f);

            Vec3 hitPoint, hitNormal;
            uint32_t hitLayers = 0;
            if (RaycastAgainstShapes(rayOrigin, rayDir, scanSize.y, hitPoint, hitNormal, hitLayers)) {
                bool isScatterLayer = (hitLayers & scatterLayers) != 0;
                bool isCutoutLayer = (hitLayers & cutoutLayers) != 0;

                if (isScatterLayer && !isCutoutLayer && IsSlopeAcceptable(hitNormal)) {
                    float offset = GetSurfaceOffset();
                    ScatterPoint point;
                    point.position = hitPoint + hitNormal * offset;
                    point.normal = hitNormal;
                    point.valid = true;
                    points.push_back(point);
                }
            }
        }
    }
}

namespace {
    void FindCollisionComponentsRecursive(Node* node,
                                           std::vector<NodeScatter::CollisionShapeInfo>& shapes,
                                           const std::vector<std::shared_ptr<Node3D>>& excludeInstances) {
        if (!node) return;

        // Check if this node should be excluded (is a scattered instance)
        for (const auto& instance : excludeInstances) {
            if (instance.get() == node) return;
        }

        auto collisionComp = node->GetComponent<CollisionMesh3DComponent>();
        if (collisionComp) {
            NodeScatter::CollisionShapeInfo info;
            info.component = collisionComp.get();
            info.node = node;

            Node3D* node3D = dynamic_cast<Node3D*>(node);
            if (node3D) {
                info.worldTransform = node3D->GetGlobalTransformMatrix();
            } else {
                info.worldTransform = Mat4::Identity();
            }

            info.layers = collisionComp->GetCollisionLayers();
            shapes.push_back(info);
        }

        for (const auto& child : node->GetChildren()) {
            FindCollisionComponentsRecursive(child.get(), shapes, excludeInstances);
        }
    }
}

void NodeScatter::FindCollisionShapes() {
    m_CachedCollisionShapes.clear();

    Scene* scene = nullptr;
    if (m_Owner) {
        scene = m_Owner->GetScene();
    }

    if (!scene) {
        auto* sceneManager = SceneManager::GetInstance();
        if (sceneManager) {
            scene = sceneManager->GetCurrentScene();
        }
    }

    if (!scene) return;

    auto root = scene->GetRoot();
    if (!root) return;

    FindCollisionComponentsRecursive(root.get(), m_CachedCollisionShapes, m_ScatteredInstances);
}

bool NodeScatter::RaycastAgainstShapes(const Vec3& origin, const Vec3& direction, float maxDist,
                                        Vec3& outPoint, Vec3& outNormal, uint32_t& outLayers) {
    float closestDist = maxDist;
    bool hitAny = false;

    for (const auto& shapeInfo : m_CachedCollisionShapes) {
        if (!shapeInfo.component) continue;

        CollisionShape3DType shapeType = shapeInfo.component->GetShapeType();
        Vec3 hitNormal;
        float hitDist = -1.0f;

        Vec3 shapeOffset = shapeInfo.component->GetOffset();
        Vec3 worldPos = Vec3(
            shapeInfo.worldTransform[3][0],
            shapeInfo.worldTransform[3][1],
            shapeInfo.worldTransform[3][2]
        );
        Vec4 offsetTransformed = shapeInfo.worldTransform * Vec4(shapeOffset.x, shapeOffset.y, shapeOffset.z, 0.0f);
        Vec3 shapeCenter = worldPos + Vec3(offsetTransformed.x, offsetTransformed.y, offsetTransformed.z);

        Vec3 worldScale = Vec3(
            Vec3(shapeInfo.worldTransform[0][0], shapeInfo.worldTransform[0][1], shapeInfo.worldTransform[0][2]).Length(),
            Vec3(shapeInfo.worldTransform[1][0], shapeInfo.worldTransform[1][1], shapeInfo.worldTransform[1][2]).Length(),
            Vec3(shapeInfo.worldTransform[2][0], shapeInfo.worldTransform[2][1], shapeInfo.worldTransform[2][2]).Length()
        );

        switch (shapeType) {
            case CollisionShape3DType::Box: {
                Vec3 size = shapeInfo.component->GetSize();
                size = Vec3(size.x * worldScale.x, size.y * worldScale.y, size.z * worldScale.z);
                hitDist = RayBoxIntersect(origin, direction, shapeCenter, size,
                                          shapeInfo.worldTransform, hitNormal);
                break;
            }
            case CollisionShape3DType::Sphere: {
                float radius = shapeInfo.component->GetRadius();
                radius *= std::max({worldScale.x, worldScale.y, worldScale.z});
                hitDist = RaySphereIntersect(origin, direction, shapeCenter, radius, hitNormal);
                break;
            }
            default: {
                Vec3 size = shapeInfo.component->GetSize();
                if (size.LengthSquared() < 0.001f) {
                    size = Vec3(1.0f, 1.0f, 1.0f);
                }
                size = Vec3(size.x * worldScale.x, size.y * worldScale.y, size.z * worldScale.z);
                hitDist = RayBoxIntersect(origin, direction, shapeCenter, size,
                                          shapeInfo.worldTransform, hitNormal);
                break;
            }
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

float NodeScatter::RayBoxIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                                    const Vec3& boxCenter, const Vec3& boxSize,
                                    const Mat4& boxTransform, Vec3& outNormal) {
    Vec3 axisX = Vec3(boxTransform[0][0], boxTransform[0][1], boxTransform[0][2]).Normalized();
    Vec3 axisY = Vec3(boxTransform[1][0], boxTransform[1][1], boxTransform[1][2]).Normalized();
    Vec3 axisZ = Vec3(boxTransform[2][0], boxTransform[2][1], boxTransform[2][2]).Normalized();

    Vec3 halfSize = boxSize * 0.5f;
    Vec3 delta = rayOrigin - boxCenter;

    float tMin = -std::numeric_limits<float>::max();
    float tMax = std::numeric_limits<float>::max();
    Vec3 hitNormalLocal(0.0f, 0.0f, 0.0f);

    Vec3 axes[3] = { axisX, axisY, axisZ };
    float extents[3] = { halfSize.x, halfSize.y, halfSize.z };

    for (int i = 0; i < 3; ++i) {
        const Vec3& axis = axes[i];
        float extent = extents[i];

        float e = axis.Dot(delta);
        float f = axis.Dot(rayDir);

        if (std::abs(f) > 0.0001f) {
            float tNeg = (-e - extent) / f;
            float tPos = (-e + extent) / f;

            float tEntry, tExit;
            Vec3 entryNormal;

            if (tNeg < tPos) {
                tEntry = tNeg;
                tExit = tPos;
                entryNormal = axis * -1.0f;
            } else {
                tEntry = tPos;
                tExit = tNeg;
                entryNormal = axis;
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
            if (std::abs(e) > extent) {
                return -1.0f;
            }
        }
    }

    if (tMin < 0.0f) return -1.0f;

    outNormal = hitNormalLocal;
    return tMin;
}

float NodeScatter::RaySphereIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
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

bool NodeScatter::IsSlopeAcceptable(const Vec3& normal) const {
    float dotUp = normal.y;
    float slopeAngle = std::acos(std::clamp(dotUp, -1.0f, 1.0f)) * 180.0f / 3.14159265f;

    float minSlope = GetMinSlope();
    float maxSlope = GetMaxSlope();

    return slopeAngle >= minSlope && slopeAngle <= maxSlope;
}

void NodeScatter::CreateScatteredInstances() {
    ClearScatteredInstances();

    auto templateNode = m_TemplateNode.lock();
    if (!templateNode) return;
    if (m_CachedScatterPoints.empty()) return;

    Node3D* ownerNode3D = dynamic_cast<Node3D*>(m_Owner);
    if (!ownerNode3D) return;

    // Check if we should use progressive loading
    if (GetProgressiveLoading() && m_CachedScatterPoints.size() > static_cast<size_t>(GetInstancesPerFrame())) {
        // Start progressive loading
        m_PendingScatterPoints = m_CachedScatterPoints;
        m_ProgressiveLoadIndex = 0;
        m_IsProgressivelyLoading = true;

        // Create first batch immediately
        CreateInstanceBatch(GetInstancesPerFrame());
        return;
    }

    // Non-progressive mode: create all at once
    Mat4 ownerWorldTransform = ownerNode3D->GetGlobalTransformMatrix();
    Mat4 ownerInverseTransform = ownerWorldTransform.Inverse();

    bool alignToNormal = GetAlignToNormal();
    float alignStrength = GetNormalAlignmentStrength();

    for (const auto& point : m_CachedScatterPoints) {
        if (!point.valid) continue;

        // Clone the template node tree
        auto instance = CloneNodeTree(templateNode);
        if (!instance) continue;

        // Calculate transform
        Vec3 scale = GenerateRandomScale();
        Vec3 rotation = GenerateRandomRotation(point.normal);

        // Convert world position to local space relative to owner
        Vec4 worldPos4(point.position.x, point.position.y, point.position.z, 1.0f);
        Vec4 localPos4 = ownerInverseTransform * worldPos4;
        Vec3 localPosition(localPos4.x, localPos4.y, localPos4.z);

        // Build transform
        Mat4 transform;

        if (alignToNormal && alignStrength > 0.0f && GetScatterMode() == NodeScatterMode::Collision) {
            Vec3 up(0.0f, 1.0f, 0.0f);
            Vec3 targetUp = up.Lerp(point.normal, alignStrength).Normalized();

            Vec3 axis = up.Cross(targetUp);
            float dot = up.Dot(targetUp);

            if (axis.LengthSquared() > 0.0001f) {
                axis = axis.Normalized();
                float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

                Mat4 normalAlign = Mat4::Rotate(angle, axis);
                Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));

                transform = Mat4::Translate(localPosition) * normalAlign * yRot * Mat4::Scale(scale);
            } else {
                if (dot < 0.0f) {
                    Mat4 flipRot = Mat4::Rotate(3.14159265f, Vec3(1.0f, 0.0f, 0.0f));
                    Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));
                    transform = Mat4::Translate(localPosition) * flipRot * yRot * Mat4::Scale(scale);
                } else {
                    Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));
                    transform = Mat4::Translate(localPosition) * yRot * Mat4::Scale(scale);
                }
            }
        } else {
            transform = Mat4::TRS(localPosition, rotation, scale);
        }

        // Decompose transform and apply to instance
        Vec3 instancePos(transform[3][0], transform[3][1], transform[3][2]);

        Vec3 col0(transform[0][0], transform[0][1], transform[0][2]);
        Vec3 col1(transform[1][0], transform[1][1], transform[1][2]);
        Vec3 col2(transform[2][0], transform[2][1], transform[2][2]);
        Vec3 instanceScale(col0.Length(), col1.Length(), col2.Length());

        Mat4 rotMat = Mat4::Identity();
        if (instanceScale.x > 0.0001f) {
            rotMat[0][0] = col0.x / instanceScale.x;
            rotMat[0][1] = col0.y / instanceScale.x;
            rotMat[0][2] = col0.z / instanceScale.x;
        }
        if (instanceScale.y > 0.0001f) {
            rotMat[1][0] = col1.x / instanceScale.y;
            rotMat[1][1] = col1.y / instanceScale.y;
            rotMat[1][2] = col1.z / instanceScale.y;
        }
        if (instanceScale.z > 0.0001f) {
            rotMat[2][0] = col2.x / instanceScale.z;
            rotMat[2][1] = col2.y / instanceScale.z;
            rotMat[2][2] = col2.z / instanceScale.z;
        }
        Quat instanceRot = Quat::FromMatrix(rotMat);

        instance->SetPosition(instancePos);
        instance->SetRotation(instanceRot);
        instance->SetScale(instanceScale);
        instance->SetVisible(true);

        // Optimize collision on this instance
        OptimizeInstanceCollision(instance);

        // Add to owner as child
        m_Owner->AddChild(instance);
        m_ScatteredInstances.push_back(instance);
    }

}

std::shared_ptr<Node3D> NodeScatter::CloneNodeTree(std::shared_ptr<Node3D> source) {
    if (!source) return nullptr;

    // Serialize the source node to JSON (includes all components and children)
    nlohmann::json nodeJson = source->Serialize();

    // Get the type name
    if (!nodeJson.contains("type")) {
        return nullptr;
    }

    std::string typeName = nodeJson["type"].get<std::string>();

    // Create a new instance using TypeRegistry
    auto node = std::dynamic_pointer_cast<Node>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));

    if (!node) {
        // Fallback to creating a basic Node3D
        node = std::make_shared<Node3D>(source->GetName() + "_instance");
    }

    // Deserialize the node from JSON (this restores components and children)
    node->Deserialize(nodeJson);

    // Regenerate UUIDs to make this a unique instance
    RegenerateNodeUUIDs(node);

    return std::dynamic_pointer_cast<Node3D>(node);
}

void NodeScatter::ClearScatteredInstances() {
    for (auto& instance : m_ScatteredInstances) {
        if (instance && m_Owner) {
            m_Owner->RemoveChild(instance);
        }
    }
    m_ScatteredInstances.clear();
}

Vec3 NodeScatter::GenerateRandomScale() {
    Vec2 range = GetScaleRange();
    std::uniform_real_distribution<float> dist(range.x, range.y);

    if (GetUniformScale()) {
        float s = dist(m_RandomGenerator);
        return Vec3(s, s, s);
    } else {
        return Vec3(dist(m_RandomGenerator), dist(m_RandomGenerator), dist(m_RandomGenerator));
    }
}

Vec3 NodeScatter::GenerateRandomRotation(const Vec3& surfaceNormal) {
    (void)surfaceNormal;

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

void NodeScatter::DrawDebugShape() {
    if (!DebugDraw::IsAvailable()) return;
    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera3D) return;
    if (!m_Owner) return;
    if (!GetShowDebugShape()) return;

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 nodePos = node3D->GetGlobalPosition();
    Vec3 areaSize = GetScatterAreaSize();
    Vec3 areaOffset = GetScatterAreaOffset();
    Vec3 areaCenter = nodePos + areaOffset;

    Color debugColor = GetDebugColor();

    DebugDraw::Box(areaCenter, areaSize, debugColor, true, 0.0f, true);
}

// Property accessors

NodeScatterMode NodeScatter::GetScatterMode() const {
    return static_cast<NodeScatterMode>(GetPropertyValue<int>("scatterMode"));
}

void NodeScatter::SetScatterMode(NodeScatterMode mode) {
    SetPropertyValue<int>("scatterMode", static_cast<int>(mode));
    m_NeedsRegeneration = true;
}

uint32_t NodeScatter::GetScatterLayers() const {
    return static_cast<uint32_t>(GetPropertyValue<int>("scatterLayers"));
}

void NodeScatter::SetScatterLayers(uint32_t layers) {
    SetPropertyValue<int>("scatterLayers", static_cast<int>(layers));
    m_NeedsRegeneration = true;
}

uint32_t NodeScatter::GetCutoutLayers() const {
    return static_cast<uint32_t>(GetPropertyValue<int>("cutoutLayers"));
}

void NodeScatter::SetCutoutLayers(uint32_t layers) {
    SetPropertyValue<int>("cutoutLayers", static_cast<int>(layers));
    m_NeedsRegeneration = true;
}

NodeScatterDistribution NodeScatter::GetDistributionMode() const {
    return static_cast<NodeScatterDistribution>(GetPropertyValue<int>("distributionMode"));
}

void NodeScatter::SetDistributionMode(NodeScatterDistribution mode) {
    SetPropertyValue<int>("distributionMode", static_cast<int>(mode));
    m_NeedsRegeneration = true;
}

int NodeScatter::GetInstanceCount() const {
    return GetPropertyValue<int>("instanceCount");
}

void NodeScatter::SetInstanceCount(int count) {
    SetPropertyValue<int>("instanceCount", count);
    m_NeedsRegeneration = true;
}

float NodeScatter::GetGridSpacingX() const {
    return GetPropertyValue<float>("gridSpacingX");
}

void NodeScatter::SetGridSpacingX(float spacing) {
    SetPropertyValue<float>("gridSpacingX", spacing);
    m_NeedsRegeneration = true;
}

float NodeScatter::GetGridSpacingZ() const {
    return GetPropertyValue<float>("gridSpacingZ");
}

void NodeScatter::SetGridSpacingZ(float spacing) {
    SetPropertyValue<float>("gridSpacingZ", spacing);
    m_NeedsRegeneration = true;
}

float NodeScatter::GetGridJitter() const {
    return GetPropertyValue<float>("gridJitter");
}

void NodeScatter::SetGridJitter(float jitter) {
    SetPropertyValue<float>("gridJitter", jitter);
    m_NeedsRegeneration = true;
}

Vec3 NodeScatter::GetScatterAreaSize() const {
    return GetPropertyValue<Vec3>("scatterAreaSize");
}

void NodeScatter::SetScatterAreaSize(const Vec3& size) {
    SetPropertyValue<Vec3>("scatterAreaSize", size);
    m_NeedsRegeneration = true;
}

Vec3 NodeScatter::GetScatterAreaOffset() const {
    return GetPropertyValue<Vec3>("scatterAreaOffset");
}

void NodeScatter::SetScatterAreaOffset(const Vec3& offset) {
    SetPropertyValue<Vec3>("scatterAreaOffset", offset);
    m_NeedsRegeneration = true;
}

bool NodeScatter::GetAlignToNormal() const {
    return GetPropertyValue<bool>("alignToNormal");
}

void NodeScatter::SetAlignToNormal(bool align) {
    SetPropertyValue<bool>("alignToNormal", align);
    m_NeedsRegeneration = true;
}

float NodeScatter::GetNormalAlignmentStrength() const {
    return GetPropertyValue<float>("normalAlignmentStrength");
}

void NodeScatter::SetNormalAlignmentStrength(float strength) {
    SetPropertyValue<float>("normalAlignmentStrength", strength);
    m_NeedsRegeneration = true;
}

float NodeScatter::GetSurfaceOffset() const {
    return GetPropertyValue<float>("surfaceOffset");
}

void NodeScatter::SetSurfaceOffset(float offset) {
    SetPropertyValue<float>("surfaceOffset", offset);
    m_NeedsRegeneration = true;
}

float NodeScatter::GetMinSlope() const {
    return GetPropertyValue<float>("minSlope");
}

void NodeScatter::SetMinSlope(float slope) {
    SetPropertyValue<float>("minSlope", slope);
    m_NeedsRegeneration = true;
}

float NodeScatter::GetMaxSlope() const {
    return GetPropertyValue<float>("maxSlope");
}

void NodeScatter::SetMaxSlope(float slope) {
    SetPropertyValue<float>("maxSlope", slope);
    m_NeedsRegeneration = true;
}

Vec2 NodeScatter::GetScaleRange() const {
    float min = GetPropertyValue<float>("scaleMin");
    float max = GetPropertyValue<float>("scaleMax");
    return Vec2(min, max);
}

void NodeScatter::SetScaleRange(const Vec2& range) {
    SetPropertyValue<float>("scaleMin", range.x);
    SetPropertyValue<float>("scaleMax", range.y);
    m_NeedsRegeneration = true;
}

bool NodeScatter::GetUniformScale() const {
    return GetPropertyValue<bool>("uniformScale");
}

void NodeScatter::SetUniformScale(bool uniform) {
    SetPropertyValue<bool>("uniformScale", uniform);
    m_NeedsRegeneration = true;
}

Vec3 NodeScatter::GetRotationVariation() const {
    float x = GetPropertyValue<float>("rotationVariationX");
    float y = GetPropertyValue<float>("rotationVariationY");
    float z = GetPropertyValue<float>("rotationVariationZ");
    return Vec3(x, y, z);
}

void NodeScatter::SetRotationVariation(const Vec3& variation) {
    SetPropertyValue<float>("rotationVariationX", variation.x);
    SetPropertyValue<float>("rotationVariationY", variation.y);
    SetPropertyValue<float>("rotationVariationZ", variation.z);
    m_NeedsRegeneration = true;
}

bool NodeScatter::GetRandomYRotation() const {
    return GetPropertyValue<bool>("randomYRotation");
}

void NodeScatter::SetRandomYRotation(bool random) {
    SetPropertyValue<bool>("randomYRotation", random);
    m_NeedsRegeneration = true;
}

int NodeScatter::GetRandomSeed() const {
    return GetPropertyValue<int>("randomSeed");
}

void NodeScatter::SetRandomSeed(int seed) {
    SetPropertyValue<int>("randomSeed", seed);
    m_NeedsRegeneration = true;
}

bool NodeScatter::GetUseRandomSeed() const {
    return GetPropertyValue<bool>("useRandomSeed");
}

void NodeScatter::SetUseRandomSeed(bool use) {
    SetPropertyValue<bool>("useRandomSeed", use);
    m_NeedsRegeneration = true;
}

bool NodeScatter::GetShowDebugShape() const {
    return GetPropertyValue<bool>("showDebugShape");
}

void NodeScatter::SetShowDebugShape(bool show) {
    SetPropertyValue<bool>("showDebugShape", show);
}

Color NodeScatter::GetDebugColor() const {
    return GetPropertyValue<Color>("debugColor");
}

void NodeScatter::SetDebugColor(const Color& color) {
    SetPropertyValue<Color>("debugColor", color);
}

// Performance / Optimization property accessors

int NodeScatter::GetInstancesPerFrame() const {
    return GetPropertyValue<int>("instancesPerFrame");
}

void NodeScatter::SetInstancesPerFrame(int count) {
    SetPropertyValue<int>("instancesPerFrame", count);
}

ScatterCollisionMode NodeScatter::GetCollisionOptimization() const {
    return static_cast<ScatterCollisionMode>(GetPropertyValue<int>("collisionOptimization"));
}

void NodeScatter::SetCollisionOptimization(ScatterCollisionMode mode) {
    SetPropertyValue<int>("collisionOptimization", static_cast<int>(mode));
    m_NeedsRegeneration = true;
}

float NodeScatter::GetCollisionDistance() const {
    return GetPropertyValue<float>("collisionDistance");
}

void NodeScatter::SetCollisionDistance(float distance) {
    SetPropertyValue<float>("collisionDistance", distance);
}

bool NodeScatter::GetProgressiveLoading() const {
    return GetPropertyValue<bool>("progressiveLoading");
}

void NodeScatter::SetProgressiveLoading(bool enabled) {
    SetPropertyValue<bool>("progressiveLoading", enabled);
}

void NodeScatter::OptimizeInstanceCollision(std::shared_ptr<Node3D> instance) {
    if (!instance) return;

    ScatterCollisionMode mode = GetCollisionOptimization();

    // If mode is Full, do nothing - keep original collision
    if (mode == ScatterCollisionMode::Full) return;

    // Find all CollisionMesh3DComponent in this instance and its children
    std::function<void(Node*)> processNode = [&](Node* node) {
        if (!node) return;

        auto collisionComp = node->GetComponent<CollisionMesh3DComponent>();
        if (collisionComp) {
            if (mode == ScatterCollisionMode::None) {
                // Remove collision entirely
                node->RemoveComponent(collisionComp);
            } else if (mode == ScatterCollisionMode::SimplifiedBox) {
                // Change to box shape if it's a mesh collision
                if (collisionComp->GetShapeType() == CollisionShape3DType::Mesh) {
                    collisionComp->SetShapeType(CollisionShape3DType::Box);
                    // Size should already be set based on mesh bounds
                }
            } else if (mode == ScatterCollisionMode::SimplifiedSphere) {
                // Change to sphere shape if it's a mesh collision
                if (collisionComp->GetShapeType() == CollisionShape3DType::Mesh) {
                    collisionComp->SetShapeType(CollisionShape3DType::Sphere);
                    // Calculate radius from size
                    Vec3 size = collisionComp->GetSize();
                    float maxDim = std::max({size.x, size.y, size.z});
                    collisionComp->SetRadius(maxDim * 0.5f);
                }
            }
        }

        // Process children
        for (const auto& child : node->GetChildren()) {
            processNode(child.get());
        }
    };

    processNode(instance.get());
}

bool NodeScatter::CreateInstanceBatch(int batchSize) {
    auto templateNode = m_TemplateNode.lock();
    if (!templateNode) {
        m_IsProgressivelyLoading = false;
        return false;
    }

    Node3D* ownerNode3D = dynamic_cast<Node3D*>(m_Owner);
    if (!ownerNode3D) {
        m_IsProgressivelyLoading = false;
        return false;
    }

    Mat4 ownerWorldTransform = ownerNode3D->GetGlobalTransformMatrix();
    Mat4 ownerInverseTransform = ownerWorldTransform.Inverse();

    bool alignToNormal = GetAlignToNormal();
    float alignStrength = GetNormalAlignmentStrength();

    int created = 0;
    while (m_ProgressiveLoadIndex < static_cast<int>(m_PendingScatterPoints.size()) && created < batchSize) {
        const auto& point = m_PendingScatterPoints[m_ProgressiveLoadIndex];
        m_ProgressiveLoadIndex++;

        if (!point.valid) continue;

        // Clone the template node tree
        auto instance = CloneNodeTree(templateNode);
        if (!instance) continue;

        // Calculate transform
        Vec3 scale = GenerateRandomScale();
        Vec3 rotation = GenerateRandomRotation(point.normal);

        // Convert world position to local space relative to owner
        Vec4 worldPos4(point.position.x, point.position.y, point.position.z, 1.0f);
        Vec4 localPos4 = ownerInverseTransform * worldPos4;
        Vec3 localPosition(localPos4.x, localPos4.y, localPos4.z);

        // Build transform
        Mat4 transform;

        if (alignToNormal && alignStrength > 0.0f && GetScatterMode() == NodeScatterMode::Collision) {
            Vec3 up(0.0f, 1.0f, 0.0f);
            Vec3 targetUp = up.Lerp(point.normal, alignStrength).Normalized();

            Vec3 axis = up.Cross(targetUp);
            float dot = up.Dot(targetUp);

            if (axis.LengthSquared() > 0.0001f) {
                axis = axis.Normalized();
                float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

                Mat4 normalAlign = Mat4::Rotate(angle, axis);
                Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));

                transform = Mat4::Translate(localPosition) * normalAlign * yRot * Mat4::Scale(scale);
            } else {
                if (dot < 0.0f) {
                    Mat4 flipRot = Mat4::Rotate(3.14159265f, Vec3(1.0f, 0.0f, 0.0f));
                    Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));
                    transform = Mat4::Translate(localPosition) * flipRot * yRot * Mat4::Scale(scale);
                } else {
                    Mat4 yRot = Mat4::Rotate(rotation.y, Vec3(0.0f, 1.0f, 0.0f));
                    transform = Mat4::Translate(localPosition) * yRot * Mat4::Scale(scale);
                }
            }
        } else {
            transform = Mat4::TRS(localPosition, rotation, scale);
        }

        // Decompose transform and apply to instance
        Vec3 instancePos(transform[3][0], transform[3][1], transform[3][2]);

        Vec3 col0(transform[0][0], transform[0][1], transform[0][2]);
        Vec3 col1(transform[1][0], transform[1][1], transform[1][2]);
        Vec3 col2(transform[2][0], transform[2][1], transform[2][2]);
        Vec3 instanceScale(col0.Length(), col1.Length(), col2.Length());

        Mat4 rotMat = Mat4::Identity();
        if (instanceScale.x > 0.0001f) {
            rotMat[0][0] = col0.x / instanceScale.x;
            rotMat[0][1] = col0.y / instanceScale.x;
            rotMat[0][2] = col0.z / instanceScale.x;
        }
        if (instanceScale.y > 0.0001f) {
            rotMat[1][0] = col1.x / instanceScale.y;
            rotMat[1][1] = col1.y / instanceScale.y;
            rotMat[1][2] = col1.z / instanceScale.y;
        }
        if (instanceScale.z > 0.0001f) {
            rotMat[2][0] = col2.x / instanceScale.z;
            rotMat[2][1] = col2.y / instanceScale.z;
            rotMat[2][2] = col2.z / instanceScale.z;
        }
        Quat instanceRot = Quat::FromMatrix(rotMat);

        instance->SetPosition(instancePos);
        instance->SetRotation(instanceRot);
        instance->SetScale(instanceScale);
        instance->SetVisible(true);

        // Optimize collision on this instance
        OptimizeInstanceCollision(instance);

        // Add to owner as child
        m_Owner->AddChild(instance);
        m_ScatteredInstances.push_back(instance);

        created++;
    }

    // Check if we're done
    if (m_ProgressiveLoadIndex >= static_cast<int>(m_PendingScatterPoints.size())) {
        m_IsProgressivelyLoading = false;
        m_PendingScatterPoints.clear();
        
        return false;
    }

    return true; // More to create
}

} // namespace components
} // namespace lupine
