
#version 450

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in vec4 v_Color;

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
    int u_UseTexture;
    float u_AlphaCutoff;
    int u_ReceiveShadow;
} material;

layout(set = 0, binding = 4) uniform sampler2D u_Texture;

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


    void main() {
        vec3 normal = normalize(v_Normal);

        // Accumulate lighting from all lights
        vec3 totalLight = vec3(0.0);
        int numLights = int(u_Lights.lightCounts.x);

        for (int i = 0; i < 16; ++i) {
            if (i >= numLights) break;

            int lightType = int(u_Lights.lights[i].flags.x);
            vec3 lightPos = u_Lights.lights[i].positionOrDirection.xyz;

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

            // Shadow
            float shadow = 1.0;
            if (material.u_ReceiveShadow != 0 && u_Lights.lights[i].flags.y > 0.5) {
                shadow = calculateShadow(int(u_Lights.lights[i].flags.z), v_WorldPos, normal, L, lightPos);
            }

            // Diffuse lighting
            float diff = max(dot(normal, L), 0.0);
            vec3 lightColor = u_Lights.lights[i].color.rgb * u_Lights.lights[i].color.w * attenuation;
            totalLight += lightColor * diff * shadow;
        }

        // Ambient lighting from scene
        vec3 ambient = u_Lights.ambientLight.rgb * u_Lights.ambientLight.a;

        vec4 baseColor = v_Color * pc.u_TintColor * material.u_AlbedoColor;
        if (material.u_UseTexture != 0) {
            baseColor *= texture(u_Texture, v_TexCoord);
        }

        // Alpha cutoff
        if (material.u_AlphaCutoff > 0.0 && baseColor.a < material.u_AlphaCutoff) {
            discard;
        }

        vec3 lighting = ambient + totalLight;
        vec3 litColor = applyFog(baseColor.rgb * lighting, v_WorldPos, material.u_CameraPosition);
        FragColor = vec4(litColor, baseColor.a);
    }
