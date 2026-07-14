#pragma once

#include "lupine/math/Vec3.hpp"
#include "lupine/math/Vec4.hpp"
#include "lupine/math/Mat4.hpp"
#include "lupine/math/Color.hpp"
#include <cstdint>

namespace lupine {

using math::Vec3;
using math::Vec4;
using math::Color;

/**
 * Light type enumeration
 */
enum class LightType : uint32_t {
    Directional = 0,
    Point = 1,
    Spot = 2
};

/**
 * GPU-side light data structure.
 * Packed for efficient uniform buffer upload.
 *
 * Memory layout (std140):
 * - Vec4 positionOrDirection (16 bytes): xyz=position/direction, w=0 for directional, w=1 for point/spot
 * - Vec4 direction (16 bytes): xyz=direction for spot lights, w=unused
 * - Vec4 color (16 bytes): RGB + intensity in w
 * - Vec4 params (16 bytes): x=range, y=cos(innerConeAngle), z=cos(outerConeAngle), w=attenuation
 * - Vec4 flags (16 bytes): x=type, y=castShadows, z=shadowMapIndex, w=unused
 * Total: 80 bytes per light
 */
struct alignas(16) GPULightData {
    Vec4 positionOrDirection; // w=0 for directional, w=1 for point/spot
    Vec4 direction;           // xyz=direction for spot/directional lights, w=unused
    Vec4 color;               // RGB + intensity in w
    Vec4 params;              // x=range, y=cos(innerConeAngle), z=cos(outerConeAngle), w=attenuation
    Vec4 flags;               // x=type, y=castShadows, z=shadowMapIndex, w=unused

    GPULightData()
        : positionOrDirection(0.0f, 0.0f, 0.0f, 0.0f)
        , direction(0.0f, -1.0f, 0.0f, 0.0f)
        , color(1.0f, 1.0f, 1.0f, 1.0f)
        , params(0.0f, 0.0f, 0.0f, 0.0f)
        , flags(0.0f, 0.0f, -1.0f, 0.0f) // shadowMapIndex = -1 (no shadow)
    {}
};

/**
 * CPU-side light descriptor.
 * Used for scene graph light components.
 */
struct LightDescriptor {
    LightType type = LightType::Directional;
    
    // Transform
    Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 direction = Vec3(0.0f, -1.0f, 0.0f); // For directional and spot lights
    Vec3 up = Vec3(0.0f, 1.0f, 0.0f); // Up vector for spot lights (for correct shadow orientation)
    
    // Color and intensity
    Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    
    // Point/Spot light parameters
    float range = 10.0f;
    float attenuation = 2.0f; // Attenuation exponent (controls falloff curve)

    // Spot light parameters (in radians)
    float innerConeAngle = 0.785398f; // 45 degrees
    float outerConeAngle = 1.047198f; // 60 degrees
    
    // Shadow parameters
    bool castShadows = true;
    bool receiveShadows = true; // For objects lit by this light
    float shadowBias = 0.001f;
    float shadowBlur = 1.0f;
    float shadowOpacity = 1.0f;
    int shadowResolution = 2048;

    // Cascaded shadow maps (for directional lights)
    uint32_t shadowCascades = 4; // Number of cascades (1-8)
    float cascadeSplitLambda = 0.5f; // PSSM split lambda (0=uniform, 1=logarithmic)

    // Contact shadows (screen-space)
    bool contactShadows = false;
    float contactShadowLength = 0.1f;
    
    /**
     * Convert to GPU light data format
     */
    GPULightData toGPUData(int shadowMapIndex = -1) const;
};

/**
 * Shadow map descriptor
 */
struct ShadowMapDesc {
    uint32_t resolution = 1024;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float bias = 0.005f;
    float normalBias = 0.01f;
    
    // For cascaded shadow maps (directional lights)
    uint32_t cascadeCount = 4;
    float cascadeSplitLambda = 0.5f;
};

/**
 * Maximum number of lights supported per frame
 */
constexpr uint32_t MAX_LIGHTS_PER_FRAME = 16;

/**
 * Maximum number of shadow-casting lights
 * Increased to 8 to support multiple shadow-casting lights or cascaded shadow maps
 */
constexpr uint32_t MAX_SHADOW_MAPS = 8;

/**
 * Maximum number of cascades per directional light
 */
constexpr uint32_t MAX_CASCADES = 8;

/**
 * Shadow map data for a single light (non-cascaded or single cascade)
 */
struct alignas(16) ShadowMapData {
    math::Mat4 lightSpaceMatrix; // 64 bytes (4x Vec4)
    Vec4 shadowParams;           // x=bias, y=normalBias, z=shadowBlur, w=shadowOpacity
    Vec4 shadowParams2;          // x=shadowResolution, y=isCubeMap, z=lightRange (for cube maps), w=biasMultiplier

    ShadowMapData()
        : lightSpaceMatrix(math::Mat4::Identity())
        , shadowParams(0.005f, 0.01f, 1.0f, 1.0f)
        , shadowParams2(2048.0f, 0.0f, 0.0f, 1.0f)  // Default bias multiplier = 1.0
    {}
};

/**
 * Cascaded shadow map data for directional lights
 */
struct alignas(16) CascadedShadowMapData {
    math::Mat4 cascadeMatrices[MAX_CASCADES]; // Light space matrix for each cascade
    Vec4 cascadeSplits;                        // Split distances for up to 4 cascades (x, y, z, w)
    Vec4 cascadeSplits2;                       // Split distances for cascades 5-8 (x, y, z, w)
    Vec4 cascadeParams;                        // x=cascadeCount, y=bias, z=normalBias, w=baseShadowMapIndex
    Vec4 cascadeParams2;                       // x=biasMultiplier, y=unused, z=unused, w=unused

    CascadedShadowMapData() {
        for (int i = 0; i < MAX_CASCADES; ++i) {
            cascadeMatrices[i] = math::Mat4::Identity();
        }
        cascadeSplits = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        cascadeSplits2 = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        cascadeParams = Vec4(1.0f, 0.005f, 0.01f, 0.0f);
        cascadeParams2 = Vec4(1.0f, 0.0f, 0.0f, 0.0f);  // Default bias multiplier = 1.0
    }
};

/**
 * Light uniform buffer structure (std140 layout)
 */
struct alignas(16) LightUniformBuffer {
    GPULightData lights[MAX_LIGHTS_PER_FRAME];
    ShadowMapData shadowMaps[MAX_SHADOW_MAPS];
    CascadedShadowMapData cascadedShadowMaps[MAX_SHADOW_MAPS]; // For directional lights with CSM
    Vec4 ambientLight; // RGB + intensity
    Vec4 lightCounts;  // x=numLights, y=numShadowMaps, z=numCascadedShadowMaps, w=unused
    Vec4 fogColor;     // RGB + enabled flag in w (0=disabled, 1=enabled)
    Vec4 fogParams;    // x=density, y=start, z=end, w=mode (0=linear, 1=exp, 2=exp2)

    LightUniformBuffer() {
        ambientLight = Vec4(0.1f, 0.1f, 0.1f, 1.0f);
        lightCounts = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        fogColor = Vec4(0.5f, 0.5f, 0.5f, 0.0f); // Disabled by default
        fogParams = Vec4(0.01f, 10.0f, 100.0f, 0.0f); // Default: density=0.01, start=10, end=100, mode=linear
    }
};

} // namespace lupine

