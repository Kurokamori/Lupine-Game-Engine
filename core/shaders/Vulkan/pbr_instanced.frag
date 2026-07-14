
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
    vec4 u_MaterialParams1;
    vec4 u_MaterialParams2;
    vec4 u_TextureFlags;
    int u_ReceiveShadow;
} material;

layout(set = 0, binding = 4) uniform sampler2D u_AlbedoTexture;
layout(set = 0, binding = 5) uniform sampler2D u_MetallicRoughnessTexture;
layout(set = 0, binding = 6) uniform sampler2D u_NormalTexture;
layout(set = 0, binding = 7) uniform sampler2D u_EmissiveTexture;

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

vec3 getNormalFromMap() {
    if (material.u_TextureFlags.z < 0.5)
        return normalize(v_Normal);
    vec3 tNorm = texture(u_NormalTexture, v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= material.u_MaterialParams1.z;
    vec3 Q1 = dFdx(v_WorldPos);
    vec3 Q2 = dFdy(v_WorldPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);
    vec3 Nn = normalize(v_Normal);
    vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    vec3 B = -normalize(cross(Nn, T));
    mat3 TBN = mat3(T, B, Nn);
    return normalize(TBN * tNorm);
}

    const float PI = 3.14159265359;

    vec3 fresnelSchlick(float cosTheta, vec3 F0) {
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }

    float distributionGGX(vec3 N, vec3 H, float roughness) {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH * NdotH;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        return a2 / denom;
    }

    float geometrySchlickGGX(float NdotV, float roughness) {
        float r = (roughness + 1.0);
        float k = (r * r) / 8.0;
        return NdotV / (NdotV * (1.0 - k) + k);
    }

    float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
    }


    void main() {
        // Material parameters
        float metallic = material.u_MaterialParams1.x;
        float roughness = material.u_MaterialParams1.y;
        float emissiveStrength = material.u_MaterialParams1.w;
        float alphaCutoff = material.u_MaterialParams2.x;
        float aoStrength = material.u_MaterialParams2.y;

        // Base albedo, tinted by the per-instance color carried in v_Color.
        vec4 albedoSample = material.u_AlbedoColor * v_Color;
        if (material.u_TextureFlags.x > 0.5) {
            albedoSample *= texture(u_AlbedoTexture, v_TexCoord);
        }

        // Alpha test
        if (alphaCutoff > 0.0 && albedoSample.a < alphaCutoff) {
            discard;
        }

        vec3 albedo = albedoSample.rgb;

        // Metallic / roughness from texture
        if (material.u_TextureFlags.y > 0.5) {
            vec4 mrSample = texture(u_MetallicRoughnessTexture, v_TexCoord);
            metallic = mrSample.b;
            roughness = mrSample.g;
        }
        roughness = clamp(roughness, 0.04, 1.0);

        // Normal mapping via #feature normal_mapping
        vec3 N = getNormalFromMap();

        vec3 V = normalize(v_ViewDir);

        // Dielectric F0 = 0.04, metallic lerps toward albedo
        vec3 F0 = mix(vec3(0.04), albedo, metallic);

        // Accumulate lighting from all lights
        vec3 Lo = vec3(0.0);
        int numLights = int(u_Lights.lightCounts.x);

        for (int i = 0; i < 16; ++i) {
            if (i >= numLights) break;

            int lightType = int(u_Lights.lights[i].flags.x);
            vec3 lightPos = u_Lights.lights[i].positionOrDirection.xyz;
            vec3 lightColor = u_Lights.lights[i].color.rgb * u_Lights.lights[i].color.w;

            vec3 L;
            float attenuation = 1.0;

            if (lightType == 0) {
                // Directional light
                L = normalize(-u_Lights.lights[i].direction.xyz);
            } else if (lightType == 1) {
                // Point light
                vec3 toLight = lightPos - v_WorldPos;
                float dist = length(toLight);
                L = normalize(toLight);
                float range = u_Lights.lights[i].params.x;
                float distFactor = dist / range;
                attenuation = 1.0 / (1.0 + distFactor * distFactor);
                attenuation *= smoothstep(1.0, 0.5, distFactor);
            } else {
                // Spot light
                vec3 toLight = lightPos - v_WorldPos;
                float dist = length(toLight);
                L = normalize(toLight);
                vec3 spotDir = normalize(u_Lights.lights[i].direction.xyz);
                float theta = dot(L, -spotDir);
                float innerCutoff = u_Lights.lights[i].params.y;
                float outerCutoff = u_Lights.lights[i].params.z;
                float epsilon = max(innerCutoff - outerCutoff, 0.0001);
                float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
                float range = u_Lights.lights[i].params.x;
                float distFactor = clamp(dist / range, 0.0, 1.0);
                attenuation = 1.0 / (1.0 + distFactor * distFactor);
                attenuation *= smoothstep(1.0, 0.5, distFactor);
                attenuation *= spotIntensity;
            }

            vec3 H = normalize(V + L);
            float NdotL = max(dot(N, L), 0.0);

            // Shadow
            float shadow = 1.0;
            if (material.u_ReceiveShadow != 0 && u_Lights.lights[i].flags.y > 0.5) {
                shadow = calculateShadow(int(u_Lights.lights[i].flags.z), v_WorldPos, N, L, lightPos);
            }

            // Cook-Torrance BRDF
            float D = distributionGGX(N, H, roughness);
            float G = geometrySmith(N, V, L, roughness);
            vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3 numerator = D * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
            vec3 specular = numerator / denominator;

            vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

            vec3 radiance = lightColor * attenuation;
            Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
        }

        // Ambient lighting from scene
        vec3 ambient = u_Lights.ambientLight.rgb * u_Lights.ambientLight.a * albedo * aoStrength;

        // Emissive
        vec3 emissive = material.u_EmissiveColor.rgb * emissiveStrength;
        if (material.u_TextureFlags.w > 0.5) {
            emissive *= texture(u_EmissiveTexture, v_TexCoord).rgb;
        }

        vec3 color = ambient + Lo + emissive;

        // Apply tint
        color *= pc.u_TintColor.rgb;

        // Distance fog
        color = applyFog(color, v_WorldPos, material.u_CameraPosition);

        // HDR tonemap (Reinhard)
        color = color / (color + vec3(1.0));

        // Gamma correction
        color = pow(color, vec3(1.0 / 2.2));

        FragColor = vec4(color, albedoSample.a);
    }
