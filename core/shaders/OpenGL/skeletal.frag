
#version 330 core

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
uniform bool u_ReceiveShadow;

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
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    vec4 ambientLight;       // RGB + intensity
    vec4 lightCounts;        // x=numLights, y=numShadowMaps, z=numCascadedShadowMaps, w=unused
} u_Lights;

out vec4 FragColor;

const float PI = 3.14159265359;

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

// PBR functions (same as p_b_r.frag)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Sample shadow cube map based on index
float sampleShadowCubeMap(int cubeMapIndex, vec3 direction) {
    if (cubeMapIndex == 0) return texture(u_ShadowCubeMap0, direction).r;
    if (cubeMapIndex == 1) return texture(u_ShadowCubeMap1, direction).r;
    if (cubeMapIndex == 2) return texture(u_ShadowCubeMap2, direction).r;
    if (cubeMapIndex == 3) return texture(u_ShadowCubeMap3, direction).r;
    if (cubeMapIndex == 4) return texture(u_ShadowCubeMap4, direction).r;
    if (cubeMapIndex == 5) return texture(u_ShadowCubeMap5, direction).r;
    if (cubeMapIndex == 6) return texture(u_ShadowCubeMap6, direction).r;
    if (cubeMapIndex == 7) return texture(u_ShadowCubeMap7, direction).r;
    return 1.0;
}

// Sample shadow map based on index
float sampleShadowMap(int shadowMapIndex, vec2 uv) {
    if (shadowMapIndex == 0) return texture(u_ShadowMap0, uv).r;
    if (shadowMapIndex == 1) return texture(u_ShadowMap1, uv).r;
    if (shadowMapIndex == 2) return texture(u_ShadowMap2, uv).r;
    if (shadowMapIndex == 3) return texture(u_ShadowMap3, uv).r;
    if (shadowMapIndex == 4) return texture(u_ShadowMap4, uv).r;
    if (shadowMapIndex == 5) return texture(u_ShadowMap5, uv).r;
    if (shadowMapIndex == 6) return texture(u_ShadowMap6, uv).r;
    if (shadowMapIndex == 7) return texture(u_ShadowMap7, uv).r;
    if (shadowMapIndex == 8) return texture(u_ShadowMap8, uv).r;
    if (shadowMapIndex == 9) return texture(u_ShadowMap9, uv).r;
    if (shadowMapIndex == 10) return texture(u_ShadowMap10, uv).r;
    if (shadowMapIndex == 11) return texture(u_ShadowMap11, uv).r;
    if (shadowMapIndex == 12) return texture(u_ShadowMap12, uv).r;
    if (shadowMapIndex == 13) return texture(u_ShadowMap13, uv).r;
    if (shadowMapIndex == 14) return texture(u_ShadowMap14, uv).r;
    if (shadowMapIndex == 15) return texture(u_ShadowMap15, uv).r;
    if (shadowMapIndex == 16) return texture(u_ShadowMap16, uv).r;
    if (shadowMapIndex == 17) return texture(u_ShadowMap17, uv).r;
    if (shadowMapIndex == 18) return texture(u_ShadowMap18, uv).r;
    if (shadowMapIndex == 19) return texture(u_ShadowMap19, uv).r;
    if (shadowMapIndex == 20) return texture(u_ShadowMap20, uv).r;
    if (shadowMapIndex == 21) return texture(u_ShadowMap21, uv).r;
    if (shadowMapIndex == 22) return texture(u_ShadowMap22, uv).r;
    if (shadowMapIndex == 23) return texture(u_ShadowMap23, uv).r;
    if (shadowMapIndex == 24) return texture(u_ShadowMap24, uv).r;
    if (shadowMapIndex == 25) return texture(u_ShadowMap25, uv).r;
    if (shadowMapIndex == 26) return texture(u_ShadowMap26, uv).r;
    if (shadowMapIndex == 27) return texture(u_ShadowMap27, uv).r;
    if (shadowMapIndex == 28) return texture(u_ShadowMap28, uv).r;
    if (shadowMapIndex == 29) return texture(u_ShadowMap29, uv).r;
    if (shadowMapIndex == 30) return texture(u_ShadowMap30, uv).r;
    if (shadowMapIndex == 31) return texture(u_ShadowMap31, uv).r;
    return 1.0;
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

    // Compare depths (both in [0,1] range)
    float shadow = (normalizedCurrentDepth - bias > closestDepth) ? 0.0 : 1.0;

    // Apply shadow opacity
    shadow = mix(1.0, shadow, shadowOpacity);

    return shadow;
}

// Calculate shadow with PCF
float calculateShadowPCF(mat4 lightSpaceMatrix, int shadowMapIndex, vec3 worldPos, vec3 normal, vec3 lightDir,
                        float bias, float normalBias, float shadowBlur, float shadowOpacity, float shadowResolution) {
    // Transform world position to light space
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(worldPos, 1.0);

    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Check if fragment is outside light frustum
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0; // No shadow
    }

    // Get current depth
    float currentDepth = projCoords.z;

    // Apply normal bias
    float NdotL = max(dot(normal, lightDir), 0.0);
    float biasAdjusted = bias + normalBias * sqrt(1.0 - NdotL * NdotL);

    // PCF (Percentage Closer Filtering)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(shadowResolution, shadowResolution);
    int pcfRadius = int(shadowBlur);
    int pcfSamples = 0;

    for (int x = -pcfRadius; x <= pcfRadius; ++x) {
        for (int y = -pcfRadius; y <= pcfRadius; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            float pcfDepth = sampleShadowMap(shadowMapIndex, projCoords.xy + offset);
            shadow += (currentDepth - biasAdjusted > pcfDepth) ? 0.0 : 1.0;
            pcfSamples++;
        }
    }
    shadow /= float(pcfSamples);

    // Apply shadow opacity
    shadow = mix(1.0, shadow, shadowOpacity);

    return shadow;
}

// Calculate shadow (dispatches to appropriate shadow calculation method)
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

void main() {
    // Sample textures
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
        vec4 mr = texture(u_MetallicRoughnessTexture, v_TexCoord);
        metallic *= mr.b;
        roughness *= mr.g;
    }

    vec3 N = getNormalFromMap();
    vec3 V = normalize(v_ViewDir);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

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

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
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
        if (u_ReceiveShadow && castsShadows && shadowMapIndex >= 0) {
            // All lights with shadows use the standard shadow map path
            // (spot, omni, and directional lights all use non-cascaded shadows for now)
            vec3 lightPos = light.positionOrDirection.xyz;
            shadow = calculateShadow(shadowMapIndex, v_WorldPos, N, L, lightPos);
        }

        vec3 radiance = light.color.rgb * light.color.w * attenuation;
        Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL * shadow;
    }

    vec3 emissive = u_EmissiveColor.rgb * u_MaterialParams1.w;
    if (u_TextureFlags.w > 0.5) {
        emissive *= texture(u_EmissiveTexture, v_TexCoord).rgb;
    }

    float ao = 1.0;
    vec3 ambient = u_Lights.ambientLight.rgb * u_Lights.ambientLight.a * albedo.rgb * ao;
    vec3 color = (ambient + Lo + emissive) * u_TintColor.rgb;

    // HDR tonemapping (simple Reinhard)
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, albedo.a);
}

