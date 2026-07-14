
#version 450

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_ViewPos;
layout(location = 2) in vec3 v_Normal;
layout(location = 3) in vec2 v_TexCoord;
layout(location = 4) in vec4 v_Color;
layout(location = 5) in vec3 v_ViewDir;

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
    mat4 u_NormalMatrix;
    vec4 u_TintColor;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    mat4 u_View;
    vec3 u_CameraPosition;
    vec4 u_AlbedoColor;
    vec4 u_EmissiveColor;
    vec4 u_TextureFlags;
    vec4 u_ToonMaterialParams;
    vec4 u_ToonMaterialParams2;
    vec4 u_ToonParams;
    int u_ReceiveShadow;
    bool u_UseSkinning;
} material;

layout(std140, set = 0, binding = 1) uniform BoneData {
    mat4 u_BoneTransforms[128];
} bones;

layout(set = 0, binding = 4) uniform sampler2D u_AlbedoTexture;
layout(set = 0, binding = 5) uniform sampler2D u_NormalTexture;
layout(set = 0, binding = 6) uniform sampler2D u_EmissiveTexture;

struct Light {
    vec4 positionOrDirection;
    vec4 direction;
    vec4 color;
    vec4 params;
    vec4 flags;
};
struct ShadowMap {
    mat4 lightSpaceMatrix;
    vec4 shadowParams;
    vec4 shadowParams2;
};
struct CascadedShadowMap {
    mat4 cascadeMatrices[8];
    vec4 cascadeSplits;
    vec4 cascadeSplits2;
    vec4 cascadeParams;
    vec4 cascadeParams2;
};
layout(std140, set = 0, binding = 3) uniform LightData {
    Light lights[16];
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    vec4 ambientLight;
    vec4 lightCounts;
    vec4 fogColor;
    vec4 fogParams;
} u_Lights;

layout(set = 0, binding = 8) uniform sampler2D u_ShadowMaps[8];
layout(set = 0, binding = 9) uniform samplerCube u_ShadowCubeMaps[8];

float sampleShadowMap(int index, vec2 uv) {
    return texture(u_ShadowMaps[index], uv).r;
}
float sampleShadowCubeMap(int index, vec3 dir) {
    return texture(u_ShadowCubeMaps[index], dir).r;
}

float calculateShadowPCF(mat4 lsMatrix, int smIndex, vec3 wPos, vec3 N, vec3 lDir,
                         float bias, float nBias, float blur, float opacity, float res) {
    vec4 lsPos = (lsMatrix * vec4(wPos, 1.0));
    vec3 proj = lsPos.xyz / lsPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float curDepth = proj.z;
    float NdL = max(dot(N, lDir), 0.0);
    float fBias = mix(bias * 2.0, bias, NdL);
    float shadow = 0.0;
    vec2 texelSz = 1.0 / vec2(res, res);
    int kr = clamp(int(blur), 1, 3);
    int sc = 0;
    for (int x = -kr; x <= kr; ++x) {
        for (int y = -kr; y <= kr; ++y) {
            vec2 off = vec2(float(x), float(y)) * texelSz * blur;
            float d = sampleShadowMap(smIndex, proj.xy + off);
            shadow += curDepth - fBias > d ? 1.0 : 0.0;
            sc++;
        }
    }
    shadow /= float(sc);
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadowCube(int cmIndex, vec3 lPos, vec3 wPos, vec3 N,
                          float bias, float blur, float opacity, float lRange) {
    vec3 frag2light = wPos - lPos;
    float curDepth = length(frag2light);
    float normDepth = curDepth / lRange;
    vec3 sDir = normalize(frag2light);
    float closest = sampleShadowCubeMap(cmIndex, sDir);
    vec3 lDir2 = -sDir;
    float NdL = max(dot(N, lDir2), 0.0);
    float nBias = bias / lRange;
    float fBias = mix(nBias * 3.0, nBias * 0.5, NdL);
    float shadow = (normDepth - fBias) > closest ? 1.0 : 0.0;
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadow(int smIndex, vec3 wPos, vec3 N, vec3 lDir, vec3 lPos) {
    if (smIndex < 0 || smIndex >= 8) return 1.0;
    ShadowMap sm = u_Lights.shadowMaps[smIndex];
    bool isCube = sm.shadowParams2.y > 0.5;
    if (isCube) {
        float lRange = sm.shadowParams2.z;
        return calculateShadowCube(smIndex, lPos, wPos, N,
            sm.shadowParams.x, sm.shadowParams.z, sm.shadowParams.w, lRange);
    } else {
        return calculateShadowPCF(sm.lightSpaceMatrix, smIndex, wPos, N, lDir,
            sm.shadowParams.x, sm.shadowParams.y, sm.shadowParams.z,
            sm.shadowParams.w, sm.shadowParams2.x);
    }
}

vec3 applyFog(vec3 color, vec3 wPos, vec3 camPos) {
    if (u_Lights.fogColor.w < 0.5) {
        return color;
    }
    float dist = length(wPos - camPos);
    float density = u_Lights.fogParams.x;
    float fogStart = u_Lights.fogParams.y;
    float fogEnd = u_Lights.fogParams.z;
    int mode = int(u_Lights.fogParams.w);
    float factor = 1.0;
    if (mode == 0) {
        factor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);
    } else if (mode == 1) {
        factor = exp(-density * max(dist - fogStart, 0.0));
    } else {
        float dd = density * max(dist - fogStart, 0.0);
        factor = exp(-dd * dd);
    }
    return mix(u_Lights.fogColor.rgb, color, clamp(factor, 0.0, 1.0));
}

    // Wrap NdotL from [-1, 1] to [0, 1] range for robust skeletal mesh lighting
    float wrapLighting(float NdotL) {
        return NdotL * 0.5 + 0.5;
    }

    // Cel shading: quantize value into discrete bands
    float celShade(float value, float bands, float threshold, float softness) {
        float adjustedValue = smoothstep(threshold - softness, threshold + softness, value);
        if (bands <= 1.0) {
            return adjustedValue;
        }
        float quantized = floor(adjustedValue * bands) / (bands - 1.0);
        return clamp(quantized, 0.0, 1.0);
    }

    // Improved cel shading with stacking bands
    float celShadeStacked(float value, float bands) {
        if (bands <= 1.0) {
            return step(0.5, value);
        }
        float quantized = floor(value * bands) / (bands - 1.0);
        return clamp(quantized, 0.0, 1.0);
    }

    // Calculate rim lighting (fresnel-based edge glow)
    float calculateRimLighting(vec3 N, vec3 V, float rimPower, float rimIntensity) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        rim = pow(rim, rimPower) * rimIntensity;
        return rim;
    }


    void main() {
        // Sample albedo texture
        vec4 albedo = material.u_AlbedoColor;
        if (material.u_TextureFlags.x > 0.5) {
            albedo *= texture(u_AlbedoTexture, v_TexCoord);
        }

        // Alpha test (material.u_ToonMaterialParams2.x = alphaCutoff)
        if (albedo.a < material.u_ToonMaterialParams2.x) {
            discard;
        }

        // Unpack toon parameters with defaults
        // material.u_ToonMaterialParams: x=shadowBands, y=specularBands, z=normalScale, w=emissiveStrength
        float shadowBands = max(material.u_ToonMaterialParams.x, 2.0);
        float specularBands = max(material.u_ToonMaterialParams.y, 2.0);
        // material.u_ToonParams: x=shadowThreshold, y=shadowSoftness, z=specularThreshold, w=specularSoftness
        float shadowThreshold = material.u_ToonParams.x > 0.0 ? material.u_ToonParams.x : 0.5;
        float shadowSoftness = material.u_ToonParams.y > 0.0 ? material.u_ToonParams.y : 0.02;
        float specularThreshold = material.u_ToonParams.z > 0.0 ? material.u_ToonParams.z : 0.3;
        float specularSoftness = material.u_ToonParams.w > 0.0 ? material.u_ToonParams.w : 0.05;
        // material.u_ToonMaterialParams2: x=alphaCutoff, y=rimPower, z=rimIntensity, w=specularPower
        float rimPower = material.u_ToonMaterialParams2.y > 0.0 ? material.u_ToonMaterialParams2.y : 3.0;
        float rimIntensity = material.u_ToonMaterialParams2.z;
        float specularPower = material.u_ToonMaterialParams2.w > 0.0 ? material.u_ToonMaterialParams2.w : 32.0;

        vec3 N = normalize(v_Normal);
        vec3 V = normalize(v_ViewDir);

        // Ambient from light UBO
        vec3 ambient = u_Lights.ambientLight.rgb;

        // Accumulate lighting from all dynamic lights
        vec3 totalDiffuse = vec3(0.0);
        vec3 totalSpecular = vec3(0.0);
        int numLights = int(u_Lights.lightCounts.x);

        for (int i = 0; i < 16; ++i) {
            if (i >= numLights) break;

            vec3 lightColor = u_Lights.lights[i].color.rgb;
            float lightIntensity = u_Lights.lights[i].color.w;
            int lightType = int(u_Lights.lights[i].flags.x);
            vec3 lightPos = u_Lights.lights[i].positionOrDirection.xyz;

            // Compute light direction
            vec3 L;
            float attenuation = 1.0;

            if (lightType == 0) {
                // Directional light
                L = normalize(-u_Lights.lights[i].direction.xyz);
            } else {
                // Point or spot light
                vec3 toLight = lightPos - v_WorldPos;
                float dist = length(toLight);
                L = normalize(toLight);
                float range = u_Lights.lights[i].params.x;
                float distFactor = clamp(dist / range, 0.0, 1.0);
                attenuation = 1.0 / (1.0 + distFactor * distFactor);
                attenuation *= smoothstep(1.0, 0.5, distFactor);

                if (lightType == 2) {
                    // Spot light cone attenuation
                    vec3 spotDir = normalize(-u_Lights.lights[i].direction.xyz);
                    float cosAngle = dot(L, spotDir);
                    float innerCone = u_Lights.lights[i].params.y;
                    float outerCone = u_Lights.lights[i].params.z;
                    float epsilon = max(innerCone - outerCone, 0.0001);
                    attenuation *= clamp((cosAngle - outerCone) / epsilon, 0.0, 1.0);
                }
            }

            vec3 H = normalize(V + L);

            // Cel-shaded diffuse using wrapped NdotL
            float rawNdotL = dot(N, L);
            float wrappedNdotL = wrapLighting(rawNdotL);
            float NdotL = max(rawNdotL, 0.0);

            float celDiffuse = celShade(wrappedNdotL, shadowBands, shadowThreshold, shadowSoftness);

            // Cel-shaded specular (Blinn-Phong based)
            float NdotH = max(dot(N, H), 0.0);
            float specularRaw = pow(NdotH, specularPower);
            float specularFactor = specularRaw * NdotL;
            float specularStep = smoothstep(specularThreshold - specularSoftness,
                                            specularThreshold + specularSoftness,
                                            specularFactor);
            float celSpecular = specularBands > 1.0 ?
                celShadeStacked(specularStep, specularBands) : specularStep;

            // Shadow
            float shadow = 1.0;
            if (material.u_ReceiveShadow != 0 && u_Lights.lights[i].flags.y > 0.5) {
                shadow = calculateShadow(int(u_Lights.lights[i].flags.z), v_WorldPos, N, L, lightPos);
            }

            vec3 litColor = lightColor * lightIntensity * attenuation * shadow;
            totalDiffuse += litColor * celDiffuse;
            totalSpecular += litColor * celSpecular;
        }

        // Calculate rim lighting
        float rim = calculateRimLighting(N, V, rimPower, rimIntensity);
        vec3 rimColor = ambient * rim;

        // Emissive (material.u_ToonMaterialParams.w = emissiveStrength)
        vec3 emissive = material.u_EmissiveColor.rgb * material.u_ToonMaterialParams.w;
        if (material.u_TextureFlags.w > 0.5) {
            emissive *= texture(u_EmissiveTexture, v_TexCoord).rgb;
        }

        // Final color composition
        vec3 diffuseColor = albedo.rgb * (ambient + totalDiffuse);
        vec3 specularColor = totalSpecular;
        vec3 color = diffuseColor + specularColor + rimColor + emissive;

        // Apply tint
        color *= pc.u_TintColor.rgb;

        // Distance fog
        color = applyFog(color, v_WorldPos, material.u_CameraPosition);

        // Gamma correction
        color = pow(color, vec3(1.0 / 2.2));

        FragColor = vec4(color, albedo.a);
    }
