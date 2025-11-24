
#version 330 core

// Vertex inputs
in vec3 v_WorldPos;
in vec3 v_ViewPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_Color;
in vec3 v_ViewDir;

// Material textures
uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_MetallicRoughnessTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_EmissiveTexture;
uniform sampler2D u_AOTexture;

// Shadow map textures (up to 32 shadow maps for cascades)
uniform sampler2D u_ShadowMap0;
uniform sampler2D u_ShadowMap1;
uniform sampler2D u_ShadowMap2;
uniform sampler2D u_ShadowMap3;
uniform sampler2D u_ShadowMap4;
uniform sampler2D u_ShadowMap5;
uniform sampler2D u_ShadowMap6;
uniform sampler2D u_ShadowMap7;
uniform sampler2D u_ShadowMap8;
uniform sampler2D u_ShadowMap9;
uniform sampler2D u_ShadowMap10;
uniform sampler2D u_ShadowMap11;
uniform sampler2D u_ShadowMap12;
uniform sampler2D u_ShadowMap13;
uniform sampler2D u_ShadowMap14;
uniform sampler2D u_ShadowMap15;
uniform sampler2D u_ShadowMap16;
uniform sampler2D u_ShadowMap17;
uniform sampler2D u_ShadowMap18;
uniform sampler2D u_ShadowMap19;
uniform sampler2D u_ShadowMap20;
uniform sampler2D u_ShadowMap21;
uniform sampler2D u_ShadowMap22;
uniform sampler2D u_ShadowMap23;
uniform sampler2D u_ShadowMap24;
uniform sampler2D u_ShadowMap25;
uniform sampler2D u_ShadowMap26;
uniform sampler2D u_ShadowMap27;
uniform sampler2D u_ShadowMap28;
uniform sampler2D u_ShadowMap29;
uniform sampler2D u_ShadowMap30;
uniform sampler2D u_ShadowMap31;

// Cube map shadow textures for point lights (up to 8)
uniform samplerCube u_ShadowCubeMap0;
uniform samplerCube u_ShadowCubeMap1;
uniform samplerCube u_ShadowCubeMap2;
uniform samplerCube u_ShadowCubeMap3;
uniform samplerCube u_ShadowCubeMap4;
uniform samplerCube u_ShadowCubeMap5;
uniform samplerCube u_ShadowCubeMap6;
uniform samplerCube u_ShadowCubeMap7;

// Material properties
uniform vec4 u_AlbedoColor;
uniform vec4 u_EmissiveColor;
uniform vec4 u_MaterialParams1; // metallic, roughness, normalScale, emissiveStrength
uniform vec4 u_MaterialParams2; // alphaCutoff, aoStrength, heightScale, unused
uniform vec4 u_TextureFlags;    // useAlbedo, useMetallicRoughness, useNormal, useEmissive
uniform vec4 u_TintColor;

// Light data structure (matches GPULightData in Light.hpp)
struct Light {
    vec4 positionOrDirection; // w=0 for directional, w=1 for point/spot
    vec4 direction;           // xyz=direction for spot/directional lights, w=unused
    vec4 color;               // RGB + intensity in w
    vec4 params;              // x=range, y=innerConeAngle, z=outerConeAngle, w=attenuation
    vec4 flags;               // x=type, y=castShadows, z=shadowMapIndex, w=unused
};

// Shadow map data structure
struct ShadowMap {
    mat4 lightSpaceMatrix;   // Light space transformation matrix
    vec4 shadowParams;       // x=bias, y=normalBias, z=shadowBlur, w=shadowOpacity
    vec4 shadowParams2;      // x=shadowResolution, y=isCubeMap, z=lightRange (for cube maps), w=unused
};

// Cascaded shadow map data structure
struct CascadedShadowMap {
    mat4 cascadeMatrices[8]; // Light space matrix for each cascade
    vec4 cascadeSplits;      // Split distances for cascades 0-3 (x, y, z, w)
    vec4 cascadeSplits2;     // Split distances for cascades 4-7 (x, y, z, w)
    vec4 cascadeParams;      // x=cascadeCount, y=bias, z=normalBias, w=baseShadowMapIndex
};

// Light uniform buffer (std140 layout, binding point 0)
layout(std140) uniform LightData {
    Light lights[16];
    ShadowMap shadowMaps[8];  // Increased from 4 to 8 to match MAX_SHADOW_MAPS
    CascadedShadowMap cascadedShadowMaps[8];  // Increased from 4 to 8 to match MAX_SHADOW_MAPS
    vec4 ambientLight;       // RGB + intensity
    vec4 lightCounts;        // x=numLights, y=numShadowMaps, z=numCascadedShadowMaps, w=unused
} u_Lights;

out vec4 FragColor;

const float PI = 3.14159265359;

// Helper function to sample shadow map by index
float sampleShadowMap(int index, vec2 uv) {
    if (index == 0) return texture(u_ShadowMap0, uv).r;
    else if (index == 1) return texture(u_ShadowMap1, uv).r;
    else if (index == 2) return texture(u_ShadowMap2, uv).r;
    else if (index == 3) return texture(u_ShadowMap3, uv).r;
    else if (index == 4) return texture(u_ShadowMap4, uv).r;
    else if (index == 5) return texture(u_ShadowMap5, uv).r;
    else if (index == 6) return texture(u_ShadowMap6, uv).r;
    else if (index == 7) return texture(u_ShadowMap7, uv).r;
    else if (index == 8) return texture(u_ShadowMap8, uv).r;
    else if (index == 9) return texture(u_ShadowMap9, uv).r;
    else if (index == 10) return texture(u_ShadowMap10, uv).r;
    else if (index == 11) return texture(u_ShadowMap11, uv).r;
    else if (index == 12) return texture(u_ShadowMap12, uv).r;
    else if (index == 13) return texture(u_ShadowMap13, uv).r;
    else if (index == 14) return texture(u_ShadowMap14, uv).r;
    else if (index == 15) return texture(u_ShadowMap15, uv).r;
    else if (index == 16) return texture(u_ShadowMap16, uv).r;
    else if (index == 17) return texture(u_ShadowMap17, uv).r;
    else if (index == 18) return texture(u_ShadowMap18, uv).r;
    else if (index == 19) return texture(u_ShadowMap19, uv).r;
    else if (index == 20) return texture(u_ShadowMap20, uv).r;
    else if (index == 21) return texture(u_ShadowMap21, uv).r;
    else if (index == 22) return texture(u_ShadowMap22, uv).r;
    else if (index == 23) return texture(u_ShadowMap23, uv).r;
    else if (index == 24) return texture(u_ShadowMap24, uv).r;
    else if (index == 25) return texture(u_ShadowMap25, uv).r;
    else if (index == 26) return texture(u_ShadowMap26, uv).r;
    else if (index == 27) return texture(u_ShadowMap27, uv).r;
    else if (index == 28) return texture(u_ShadowMap28, uv).r;
    else if (index == 29) return texture(u_ShadowMap29, uv).r;
    else if (index == 30) return texture(u_ShadowMap30, uv).r;
    else if (index == 31) return texture(u_ShadowMap31, uv).r;
    return 1.0;
}

// Helper function to sample cube shadow map by index
float sampleShadowCubeMap(int index, vec3 dir) {
    // For GL_DEPTH_COMPONENT textures, texture() returns a vec4 where all components contain the depth
    // We can use .r, .x, or just the scalar value
    vec4 sample;
    if (index == 0) sample = texture(u_ShadowCubeMap0, dir);
    else if (index == 1) sample = texture(u_ShadowCubeMap1, dir);
    else if (index == 2) sample = texture(u_ShadowCubeMap2, dir);
    else if (index == 3) sample = texture(u_ShadowCubeMap3, dir);
    else if (index == 4) sample = texture(u_ShadowCubeMap4, dir);
    else if (index == 5) sample = texture(u_ShadowCubeMap5, dir);
    else if (index == 6) sample = texture(u_ShadowCubeMap6, dir);
    else if (index == 7) sample = texture(u_ShadowCubeMap7, dir);
    else return 1.0;

    // Return the red channel which contains the depth value
    return sample.r;
}

// DEBUG: Global variables for visualization
int g_debugCascadeIndex = -1;
int g_debugShadowMapIndex = -1;
float g_debugShadowValue = -1.0;
vec3 g_debugProjCoords = vec3(-1.0);
float g_debugSampledDepth = -1.0;

// Calculate shadow factor for a single shadow map with PCF
float calculateShadowPCF(mat4 lightSpaceMatrix, int shadowMapIndex, vec3 worldPos, vec3 normal, vec3 lightDir, float bias, float normalBias, float shadowBlur, float shadowOpacity, float shadowResolution) {
    // Transform world position to light space (no normal bias initially for debugging)
    vec4 lightSpacePos = lightSpaceMatrix * vec4(worldPos, 1.0);

    // Perform perspective divide
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // NOTE: Y-flip removed - framebuffer depth textures in OpenGL have matching coordinate systems
    // The Y-flip was causing inverted shadows (shadows pointing toward light instead of away)
    // projCoords.y = 1.0 - projCoords.y;

    // DEBUG: Store projected coordinates
    g_debugProjCoords = projCoords;

    // Check if outside shadow map bounds
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        g_debugSampledDepth = -2.0; // Mark as out of bounds
        return 1.0; // Outside shadow map, no shadow
    }

    float currentDepth = projCoords.z;

    // DEBUG: Sample shadow map and store for visualization
    float sampledDepth = sampleShadowMap(shadowMapIndex, projCoords.xy);
    g_debugSampledDepth = sampledDepth;

    // Calculate bias based on surface angle to light to prevent shadow acne
    float NdotL = max(dot(normal, lightDir), 0.0);
    // Angle-based bias: surfaces perpendicular to light need less bias
    // Use smaller bias multipliers to prevent over-biasing
    float minBias = bias;
    float maxBias = bias * 2.0;
    float finalBias = mix(maxBias, minBias, NdotL);

    // PCF (Percentage Closer Filtering) for soft shadows
    // shadowBlur controls the PCF kernel size (1.0 = 3x3, higher = larger kernel)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(shadowResolution, shadowResolution); // Dynamic shadow map resolution
    
    // Scale PCF kernel based on blur amount
    int kernelRadius = int(shadowBlur);
    kernelRadius = clamp(kernelRadius, 1, 3); // Limit to 1-3 for performance
    int sampleCount = 0;

    for(int x = -kernelRadius; x <= kernelRadius; ++x) {
        for(int y = -kernelRadius; y <= kernelRadius; ++y) {
            vec2 offset = vec2(x, y) * texelSize * shadowBlur;
            float pcfDepth = sampleShadowMap(shadowMapIndex, projCoords.xy + offset);

            // DEBUG: Store first sample depth
            if (x == 0 && y == 0) {
                g_debugSampledDepth = pcfDepth;
            }

            shadow += currentDepth - finalBias > pcfDepth ? 1.0 : 0.0;
            sampleCount++;
        }
    }
    shadow /= float(sampleCount);

    // Apply shadow opacity (1.0 = full shadow, 0.0 = no shadow)
    shadow *= shadowOpacity;

    return 1.0 - shadow;
}

// Calculate shadow factor for cube map (point lights)
float calculateShadowCube(int cubeMapIndex, vec3 lightPos, vec3 worldPos, vec3 normal, float bias, float shadowBlur, float shadowOpacity, float lightRange) {
    // Calculate direction from light to fragment
    vec3 fragToLight = worldPos - lightPos;
    float currentDepth = length(fragToLight);

    // Normalize current depth to [0,1] range
    float normalizedCurrentDepth = currentDepth / lightRange;

    // Normalize direction for sampling
    vec3 sampleDir = normalize(fragToLight);

    // Sample the cube map (already stores normalized linear depth in [0,1])
    float closestDepth = sampleShadowCubeMap(cubeMapIndex, sampleDir);

    // Calculate bias based on surface angle to light
    vec3 lightDir = -sampleDir;
    float NdotL = max(dot(normal, lightDir), 0.0);
    // Bias needs to be in normalized [0,1] space to match the depth values
    float normalizedBias = bias / lightRange;
    float minBias = normalizedBias * 0.5;
    float maxBias = normalizedBias * 3.0;
    float finalBias = mix(maxBias, minBias, NdotL);

    // Simple PCF for cube maps
    float shadow = 0.0;
    if (shadowBlur > 1.0) {
        // Sample neighboring texels for soft shadows
        vec3 sampleOffsetDirections[20] = vec3[]
        (
           vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
           vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
           vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
           vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
           vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
        );

        float diskRadius = 0.05 * shadowBlur;
        int samples = 20;
        for(int i = 0; i < samples; ++i) {
            vec3 offsetDir = sampleDir + sampleOffsetDirections[i] * diskRadius;
            float sampledDepth = sampleShadowCubeMap(cubeMapIndex, offsetDir);
            shadow += (normalizedCurrentDepth - finalBias) > sampledDepth ? 1.0 : 0.0;
        }
        shadow /= float(samples);
    } else {
        // No PCF, single sample
        shadow = (normalizedCurrentDepth - finalBias) > closestDepth ? 1.0 : 0.0;
    }

    // Apply shadow opacity
    shadow *= shadowOpacity;

    return 1.0 - shadow;
}

// Calculate shadow factor for cascaded shadow maps (directional lights)
float calculateCascadedShadow(int csmIndex, vec3 worldPos, vec3 viewPos, vec3 normal, vec3 lightDir) {
    if (csmIndex < 0 || csmIndex >= 4) {
        return 1.0; // No shadow
    }

    CascadedShadowMap csm = u_Lights.cascadedShadowMaps[csmIndex];
    int cascadeCount = int(csm.cascadeParams.x);
    float bias = csm.cascadeParams.y;
    float normalBias = csm.cascadeParams.z;
    int baseShadowMapIndex = int(csm.cascadeParams.w);

    // Calculate view-space depth
    float viewDepth = abs(viewPos.z);

    // Select cascade based on view depth
    int cascadeIndex = cascadeCount - 1;
    for (int i = 0; i < cascadeCount - 1; ++i) {
        float splitDistance = 0.0;
        if (i < 4) {
            splitDistance = csm.cascadeSplits[i];
        } else {
            splitDistance = csm.cascadeSplits2[i - 4];
        }

        if (viewDepth < splitDistance) {
            cascadeIndex = i;
            break;
        }
    }

    // DEBUG: Store cascade index for visualization
    g_debugCascadeIndex = cascadeIndex;

    // Calculate shadow map index (base index + cascade offset)
    int shadowMapIndex = baseShadowMapIndex + cascadeIndex;
    g_debugShadowMapIndex = shadowMapIndex;

    // Calculate shadow using selected cascade
    mat4 lightSpaceMatrix = csm.cascadeMatrices[cascadeIndex];
    // For cascaded shadows, use default values for blur/opacity/resolution (TODO: pass from CPU)
    float shadow = calculateShadowPCF(lightSpaceMatrix, shadowMapIndex, worldPos, normal, lightDir, bias, normalBias, 1.0, 1.0, 2048.0);
    g_debugShadowValue = shadow;
    return shadow;
}

// Calculate shadow factor for a given shadow map (non-cascaded)
float calculateShadow(int shadowMapIndex, vec3 worldPos, vec3 normal, vec3 lightDir, vec3 lightPos) {
    if (shadowMapIndex < 0 || shadowMapIndex >= 8) {
        return 1.0; // No shadow
    }

    ShadowMap shadowMap = u_Lights.shadowMaps[shadowMapIndex];

    // Check if this is a cube map shadow (for point lights)
    bool isCubeMap = shadowMap.shadowParams2.y > 0.5;

    if (isCubeMap) {
        // Use cube map shadow calculation for point lights
        float lightRange = shadowMap.shadowParams2.z;
        return calculateShadowCube(shadowMapIndex, lightPos, worldPos, normal,
                                  shadowMap.shadowParams.x, shadowMap.shadowParams.z,
                                  shadowMap.shadowParams.w, lightRange);
    } else {
        // Use standard 2D shadow map calculation
        return calculateShadowPCF(shadowMap.lightSpaceMatrix, shadowMapIndex, worldPos, normal, lightDir,
                                  shadowMap.shadowParams.x, shadowMap.shadowParams.y,
                                  shadowMap.shadowParams.z, shadowMap.shadowParams.w,
                                  shadowMap.shadowParams2.x);
    }
}

// Normal mapping
vec3 getNormalFromMap() {
    if (u_TextureFlags.z < 0.5) {
        return normalize(v_Normal);
    }

    vec3 tangentNormal = texture(u_NormalTexture, v_TexCoord).xyz * 2.0 - 1.0;
    tangentNormal.xy *= u_MaterialParams1.z; // normalScale

    // Construct TBN matrix
    vec3 Q1 = dFdx(v_WorldPos);
    vec3 Q2 = dFdy(v_WorldPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);

    vec3 N = normalize(v_Normal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz normal distribution
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Smith's Schlick-GGX geometry function
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

void main() {
    // Sample material properties
    vec4 albedo = u_AlbedoColor;
    if (u_TextureFlags.x > 0.5) {
        albedo *= texture(u_AlbedoTexture, v_TexCoord);
    }

    // Alpha test
    if (albedo.a < u_MaterialParams2.x) {
        discard;
    }

    float metallic = u_MaterialParams1.x;
    float roughness = u_MaterialParams1.y;
    if (u_TextureFlags.y > 0.5) {
        vec3 mrSample = texture(u_MetallicRoughnessTexture, v_TexCoord).rgb;
        roughness *= mrSample.g;
        metallic *= mrSample.b;
    }

    vec3 emissive = u_EmissiveColor.rgb * u_MaterialParams1.w;
    if (u_TextureFlags.w > 0.5) {
        emissive *= texture(u_EmissiveTexture, v_TexCoord).rgb;
    }

    float ao = 1.0;
    // AO texture support can be added here

    // Get normal
    vec3 N = getNormalFromMap();
    vec3 V = normalize(v_ViewDir);

    // Calculate reflectance at normal incidence
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    // Accumulate lighting from all lights
    vec3 Lo = vec3(0.0);
    int numLights = int(u_Lights.lightCounts.x);

    for (int i = 0; i < numLights && i < 16; ++i) {
        Light light = u_Lights.lights[i];
        int lightType = int(light.flags.x);
        bool castsShadows = light.flags.y > 0.5;
        int shadowMapIndex = int(light.flags.z);

        vec3 L;
        float attenuation = 1.0;

        // Calculate light direction and attenuation based on type
        if (lightType == 0) {
            // Directional light
            L = normalize(-light.direction.xyz);
        } else if (lightType == 1) {
            // Point light
            vec3 lightPos = light.positionOrDirection.xyz;
            vec3 lightDir = lightPos - v_WorldPos;
            float distance = length(lightDir);
            L = normalize(lightDir);

            float range = light.params.x;
            float attenuationExp = light.params.w;

            // Distance-based attenuation with configurable exponent
            // Using inverse square law as base, modified by attenuation exponent
            float distanceFactor = distance / range;
            attenuation = 1.0 / (1.0 + pow(distanceFactor, attenuationExp));

            // Smooth cutoff at range boundary
            attenuation *= smoothstep(1.0, 0.5, distanceFactor);
        } else if (lightType == 2) {
            // Spot light
            vec3 lightPos = light.positionOrDirection.xyz;
            vec3 lightDir = lightPos - v_WorldPos;
            float distance = length(lightDir);
            L = normalize(lightDir);

            vec3 spotDir = normalize(light.direction.xyz);
            float theta = dot(L, -spotDir);
            float innerCutoff = light.params.y; // Already cosine
            float outerCutoff = light.params.z; // Already cosine

            // Calculate spot intensity with smooth falloff
            // Use max to prevent division by zero when inner and outer cutoffs are too close
            float epsilon = max(innerCutoff - outerCutoff, 0.0001);
            float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
            // Apply smooth curve for better visual falloff
            spotIntensity = smoothstep(0.0, 1.0, spotIntensity);

            float range = light.params.x;
            float attenuationExp = light.params.w;

            // Distance-based attenuation with configurable exponent
            float distanceFactor = clamp(distance / range, 0.0, 1.0);
            attenuation = 1.0 / (1.0 + pow(distanceFactor, attenuationExp));

            // Smooth cutoff at range boundary
            attenuation *= smoothstep(1.0, 0.5, distanceFactor);

            // Apply spot intensity to attenuation
            attenuation *= spotIntensity;
        }

        vec3 H = normalize(V + L);

        // Cook-Torrance BRDF
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);

        // Calculate shadow
        float shadow = 1.0;
        if (castsShadows && shadowMapIndex >= 0) {
            // All lights with shadows use the standard shadow map path
            // (spot, omni, and directional lights all use non-cascaded shadows for now)
            vec3 lightPos = light.positionOrDirection.xyz;
            shadow = calculateShadow(shadowMapIndex, v_WorldPos, N, L, lightPos);
        }

        vec3 radiance = light.color.rgb * light.color.w * attenuation;
        Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL * shadow;
    }

    // Ambient lighting
    vec3 ambient = u_Lights.ambientLight.rgb * u_Lights.ambientLight.a * albedo.rgb * ao;

    // Apply tint color
    vec3 color = (ambient + Lo + emissive) * u_TintColor.rgb;

    // HDR tonemapping (simple Reinhard)
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, albedo.a);
}
