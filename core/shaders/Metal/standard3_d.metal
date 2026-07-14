// Standard3D - Metal Shading Language
// Standard 3D rendering with multi-light support, shadows, and texture support

#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 a_Position [[attribute(0)]];
    float3 a_Normal [[attribute(1)]];
    float2 a_TexCoord [[attribute(2)]];
    float4 a_Color [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 v_WorldPos;
    float3 v_Normal;
    float2 v_TexCoord;
    float4 v_Color;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_View;
    float4x4 u_Model;
    float4x4 u_NormalMatrix;
    float3 u_CameraPosition;
    float4 u_TintColor;
    float4 u_AlbedoColor;
    int u_UseTexture;
    float u_AlphaCutoff;
    int u_ReceiveShadow;
};

constant int MAX_LIGHTS = 16;
constant int MAX_SHADOW_MAPS = 8;
constant int MAX_CASCADES = 8;

struct Light {
    float4 positionOrDirection;
    float4 direction;
    float4 color;
    float4 params;
    float4 flags;
};
struct ShadowMapData {
    float4x4 lightSpaceMatrix;
    float4 shadowParams;
    float4 shadowParams2;
};
struct CascadedShadowMapData {
    float4x4 cascadeMatrices[MAX_CASCADES];
    float4 cascadeSplits;
    float4 cascadeSplits2;
    float4 cascadeParams;
    float4 cascadeParams2;
};
struct LightUniformBuffer {
    Light lights[MAX_LIGHTS];
    ShadowMapData shadowMaps[MAX_SHADOW_MAPS];
    CascadedShadowMapData cascadedShadowMaps[MAX_SHADOW_MAPS];
    float4 ambientLight;
    float4 lightCounts;
    float4 fogColor;
    float4 fogParams;
};

float sampleShadowMap2D(int idx,
    float2 uv,
    texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
    texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
    sampler s) {
    float r = 1.0;
    switch(idx) {
        case 0: r = sm0.sample(s, uv).r; break;
        case 1: r = sm1.sample(s, uv).r; break;
        case 2: r = sm2.sample(s, uv).r; break;
        case 3: r = sm3.sample(s, uv).r; break;
        case 4: r = sm4.sample(s, uv).r; break;
        case 5: r = sm5.sample(s, uv).r; break;
        case 6: r = sm6.sample(s, uv).r; break;
        case 7: r = sm7.sample(s, uv).r; break;
    }
    return r;
}
float sampleShadowCubeM(int idx,
    float3 dir,
    texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
    texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
    sampler s) {
    float r = 1.0;
    switch(idx) {
        case 0: r = cm0.sample(s, dir).r; break;
        case 1: r = cm1.sample(s, dir).r; break;
        case 2: r = cm2.sample(s, dir).r; break;
        case 3: r = cm3.sample(s, dir).r; break;
        case 4: r = cm4.sample(s, dir).r; break;
        case 5: r = cm5.sample(s, dir).r; break;
        case 6: r = cm6.sample(s, dir).r; break;
        case 7: r = cm7.sample(s, dir).r; break;
    }
    return r;
}

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    uint vid [[vertex_id]],
    uint iid [[instance_id]]
) {
    VertexOut out;

    float4 worldPos = (uniforms.u_Model * float4(in.a_Position, 1.0));
    out.v_WorldPos = worldPos.xyz;
    out.v_Normal = (float3x3(uniforms.u_NormalMatrix[0].xyz, uniforms.u_NormalMatrix[1].xyz, uniforms.u_NormalMatrix[2].xyz) * in.a_Normal);
    out.v_TexCoord = in.a_TexCoord;
    out.v_Color = in.a_Color;
    out.position = (uniforms.u_ViewProjection * worldPos);

    return out;
}

float calculateShadowPCF(float4x4 lsMatrix, int smIndex, float3 wPos, float3 N, float3 lDir,
                         float bias, float nBias, float blur, float opacity, float res,
                         texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
                         texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
                         sampler shadowSampler) {
    float4 lsPos = (lsMatrix * float4(wPos, 1.0));
    float3 proj = lsPos.xyz / lsPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    proj.y = 1.0 - proj.y;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float curDepth = proj.z;
    float NdL = max(dot(N, lDir), 0.0);
    float fBias = mix(bias * 2.0, bias, NdL);
    float shadow = 0.0;
    float2 texelSz = 1.0 / float2(res, res);
    int kr = clamp(int(blur), 1, 3);
    int sc = 0;
    for (int x = -kr; x <= kr; ++x) {
        for (int y = -kr; y <= kr; ++y) {
            float2 off = float2(float(x), float(y)) * texelSz * blur;
            float d = sampleShadowMap2D(smIndex, proj.xy + off, sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7, shadowSampler);
            shadow += curDepth - fBias > d ? 1.0 : 0.0;
            sc++;
        }
    }
    shadow /= float(sc);
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadowCube(int cmIndex, float3 lPos, float3 wPos, float3 N,
                          float bias, float blur, float opacity, float lRange,
                          texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
                          texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
                          sampler shadowSampler) {
    float3 frag2light = wPos - lPos;
    float curDepth = length(frag2light);
    float normDepth = curDepth / lRange;
    float3 sDir = normalize(frag2light);
    float closest = sampleShadowCubeM(cmIndex, sDir, cm0, cm1, cm2, cm3, cm4, cm5, cm6, cm7, shadowSampler);
    float3 lDir2 = -sDir;
    float NdL = max(dot(N, lDir2), 0.0);
    float nBias = bias / lRange;
    float fBias = mix(nBias * 3.0, nBias * 0.5, NdL);
    float shadow = (normDepth - fBias) > closest ? 1.0 : 0.0;
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadow(int smIndex, float3 wPos, float3 N, float3 lDir, float3 lPos,
                      constant LightUniformBuffer& lightData,
                      texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
                      texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
                      texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
                      texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
                      sampler shadowSampler) {
    if (smIndex < 0 || smIndex >= 8) return 1.0;
    ShadowMapData sm = lightData.shadowMaps[smIndex];
    bool isCube = sm.shadowParams2.y > 0.5;
    if (isCube) {
        float lRange = sm.shadowParams2.z;
        return calculateShadowCube(smIndex, lPos, wPos, N,
            sm.shadowParams.x, sm.shadowParams.z, sm.shadowParams.w, lRange,
            cm0, cm1, cm2, cm3, cm4, cm5, cm6, cm7, shadowSampler);
    } else {
        return calculateShadowPCF(sm.lightSpaceMatrix, smIndex, wPos, N, lDir,
            sm.shadowParams.x, sm.shadowParams.y, sm.shadowParams.z,
            sm.shadowParams.w, sm.shadowParams2.x,
            sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7, shadowSampler);
    }
}

float3 applyFog(float3 color, float3 wPos, float3 camPos,
                constant LightUniformBuffer& lightData) {
    if (lightData.fogColor.w < 0.5) {
        return color;
    }
    float dist = length(wPos - camPos);
    float density = lightData.fogParams.x;
    float fogStart = lightData.fogParams.y;
    float fogEnd = lightData.fogParams.z;
    int mode = int(lightData.fogParams.w);
    float factor = 1.0;
    if (mode == 0) {
        factor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);
    } else if (mode == 1) {
        factor = exp(-density * max(dist - fogStart, 0.0));
    } else {
        float dd = density * max(dist - fogStart, 0.0);
        factor = exp(-dd * dd);
    }
    return mix(lightData.fogColor.rgb, color, clamp(factor, 0.0, 1.0));
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    constant LightUniformBuffer& lightData [[buffer(3)]],
    texture2d<float> u_Texture [[texture(0)]],
    sampler u_Texture_sampler [[sampler(0)]],
    texture2d<float> shadowMap0 [[texture(1)]],
    texture2d<float> shadowMap1 [[texture(2)]],
    texture2d<float> shadowMap2 [[texture(3)]],
    texture2d<float> shadowMap3 [[texture(4)]],
    texture2d<float> shadowMap4 [[texture(5)]],
    texture2d<float> shadowMap5 [[texture(6)]],
    texture2d<float> shadowMap6 [[texture(7)]],
    texture2d<float> shadowMap7 [[texture(8)]],
    texturecube<float> shadowCubeMap0 [[texture(9)]],
    texturecube<float> shadowCubeMap1 [[texture(10)]],
    texturecube<float> shadowCubeMap2 [[texture(11)]],
    texturecube<float> shadowCubeMap3 [[texture(12)]],
    texturecube<float> shadowCubeMap4 [[texture(13)]],
    texturecube<float> shadowCubeMap5 [[texture(14)]],
    texturecube<float> shadowCubeMap6 [[texture(15)]],
    texturecube<float> shadowCubeMap7 [[texture(16)]],
    sampler shadowSampler [[sampler(1)]]
) {
    float4 FragColor;

    float3 normal = normalize(in.v_Normal);
    // Accumulate lighting from all lights
    float3 totalLight = float3(0.0, 0.0, 0.0);
    int numLights = int(lightData.lightCounts.x);
    for (int i = 0; i < 16; ++i) {
    if (i >= numLights) break;
    int lightType = int(lightData.lights[i].flags.x);
    float3 lightPos = lightData.lights[i].positionOrDirection.xyz;
    float3 L;
    float attenuation = 1.0;
    if (lightType == 0) {
    // Directional light
    L = normalize(-lightData.lights[i].direction.xyz);
    } else if (lightType == 1) {
    // Point light
    float3 toLight = lightPos - in.v_WorldPos;
    float dist = length(toLight);
    L = normalize(toLight);
    float range = lightData.lights[i].params.x;
    float distFactor = dist / range;
    attenuation = 1.0 / (1.0 + distFactor * distFactor);
    attenuation *= smoothstep(1.0, 0.5, distFactor);
    } else {
    // Spot light
    float3 toLight = lightPos - in.v_WorldPos;
    float dist = length(toLight);
    L = normalize(toLight);
    float3 spotDir = normalize(lightData.lights[i].direction.xyz);
    float theta = dot(L, -spotDir);
    float innerCutoff = lightData.lights[i].params.y;
    float outerCutoff = lightData.lights[i].params.z;
    float epsilon = max(innerCutoff - outerCutoff, 0.0001);
    float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    float range = lightData.lights[i].params.x;
    float distFactor = clamp(dist / range, 0.0, 1.0);
    attenuation = 1.0 / (1.0 + distFactor * distFactor);
    attenuation *= smoothstep(1.0, 0.5, distFactor);
    attenuation *= spotIntensity;
    }
    // Shadow
    float shadow = 1.0;
    if (uniforms.u_ReceiveShadow != 0 && lightData.lights[i].flags.y > 0.5) {
    shadow = calculateShadow(int(lightData.lights[i].flags.z), in.v_WorldPos, normal, L, lightPos, lightData, shadowMap0, shadowMap1, shadowMap2, shadowMap3, shadowMap4, shadowMap5, shadowMap6, shadowMap7, shadowCubeMap0, shadowCubeMap1, shadowCubeMap2, shadowCubeMap3, shadowCubeMap4, shadowCubeMap5, shadowCubeMap6, shadowCubeMap7, shadowSampler);
    }
    // Diffuse lighting
    float diff = max(dot(normal, L), 0.0);
    float3 lightColor = lightData.lights[i].color.rgb * lightData.lights[i].color.w * attenuation;
    totalLight += lightColor * diff * shadow;
    }
    // Ambient lighting from scene
    float3 ambient = lightData.ambientLight.rgb * lightData.ambientLight.a;
    float4 baseColor = in.v_Color * uniforms.u_TintColor * uniforms.u_AlbedoColor;
    if (uniforms.u_UseTexture != 0) {
    baseColor *= u_Texture.sample(u_Texture_sampler, in.v_TexCoord);
    }
    // Alpha cutoff
    if (uniforms.u_AlphaCutoff > 0.0 && baseColor.a < uniforms.u_AlphaCutoff) {
    discard;
    }
    float3 lighting = ambient + totalLight;
    float3 litColor = applyFog(baseColor.rgb * lighting, in.v_WorldPos, uniforms.u_CameraPosition, lightData);
    FragColor = float4(litColor, baseColor.a);

    return FragColor;
}
