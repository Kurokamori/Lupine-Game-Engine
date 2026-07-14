// Transparent - Metal Shading Language
// Glass/transparent shader with fresnel reflections, refraction, and chromatic aberration

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
    float3 v_ViewPos;
    float3 v_Normal;
    float2 v_TexCoord;
    float4 v_Color;
    float3 v_ViewDir;
    float4 v_ClipPos;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_View;
    float4x4 u_Model;
    float4x4 u_NormalMatrix;
    float3 u_CameraPosition;
    float4 u_TintColor;
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;
    float4 u_TextureFlags;
    float4 u_MaterialParams1;
    float4 u_TransparentParams;
    float4 u_TransparentParams2;
    float4 u_TransparentParams3;
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

    // Custom fresnel for glass with adjustable power
    float fresnelEffect(float cosTheta, float power) {
        return pow(1.0 - cosTheta, power);
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
    out.v_ViewPos = (uniforms.u_View * worldPos).xyz;
    out.v_Normal = (float3x3(uniforms.u_NormalMatrix[0].xyz, uniforms.u_NormalMatrix[1].xyz, uniforms.u_NormalMatrix[2].xyz) * in.a_Normal);
    out.v_TexCoord = in.a_TexCoord;
    out.v_Color = in.a_Color;
    out.v_ViewDir = normalize(uniforms.u_CameraPosition.xyz - worldPos.xyz);
    float4 clipPos = (uniforms.u_ViewProjection * worldPos);
    out.v_ClipPos = clipPos;
    out.position = clipPos;

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

float3 getNormalFromMap(VertexOut in, constant MaterialUniforms& uniforms,
                        texture2d<float> u_NormalTexture, sampler u_NormalTexture_sampler) {
    if (uniforms.u_TextureFlags.z < 0.5)
        return normalize(in.v_Normal);
    float3 tNorm = u_NormalTexture.sample(u_NormalTexture_sampler, in.v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= uniforms.u_MaterialParams1.z;
    float3 Q1 = dfdx(in.v_WorldPos);
    float3 Q2 = dfdy(in.v_WorldPos);
    float2 st1 = dfdx(in.v_TexCoord);
    float2 st2 = dfdy(in.v_TexCoord);
    float3 Nn = normalize(in.v_Normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(Nn, T));
    float3x3 TBN = float3x3(T, B, Nn);
    return normalize(TBN * tNorm);
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    constant LightUniformBuffer& lightData [[buffer(3)]],
    texture2d<float> u_AlbedoTexture [[texture(0)]],
    sampler u_AlbedoTexture_sampler [[sampler(0)]],
    texture2d<float> u_NormalTexture [[texture(1)]],
    sampler u_NormalTexture_sampler [[sampler(1)]],
    texture2d<float> u_EmissiveTexture [[texture(2)]],
    sampler u_EmissiveTexture_sampler [[sampler(2)]],
    texture2d<float> shadowMap0 [[texture(3)]],
    texture2d<float> shadowMap1 [[texture(4)]],
    texture2d<float> shadowMap2 [[texture(5)]],
    texture2d<float> shadowMap3 [[texture(6)]],
    texture2d<float> shadowMap4 [[texture(7)]],
    texture2d<float> shadowMap5 [[texture(8)]],
    texture2d<float> shadowMap6 [[texture(9)]],
    texture2d<float> shadowMap7 [[texture(10)]],
    texturecube<float> shadowCubeMap0 [[texture(11)]],
    texturecube<float> shadowCubeMap1 [[texture(12)]],
    texturecube<float> shadowCubeMap2 [[texture(13)]],
    texturecube<float> shadowCubeMap3 [[texture(14)]],
    texturecube<float> shadowCubeMap4 [[texture(15)]],
    texturecube<float> shadowCubeMap5 [[texture(16)]],
    texturecube<float> shadowCubeMap6 [[texture(17)]],
    texturecube<float> shadowCubeMap7 [[texture(18)]],
    sampler shadowSampler [[sampler(3)]]
) {
    float4 FragColor;

    // Unpack parameters
    // uniforms.u_TransparentParams: x=opacity, y=refractiveIndex, z=chromaticAberration, w=fresnelPower
    float opacity = uniforms.u_TransparentParams.x;
    float refractiveIndex = uniforms.u_TransparentParams.y;
    float fresnelPower = uniforms.u_TransparentParams.w;
    // uniforms.u_TransparentParams2: x=reflectivity, y=roughness, z=thickness, w=normalScale
    float reflectivity = uniforms.u_TransparentParams2.x;
    float roughness = uniforms.u_TransparentParams2.y;
    float normalScale = uniforms.u_TransparentParams2.w;
    // uniforms.u_TransparentParams3: x=emissiveStrength, y=alphaCutoff, z=unused, w=unused
    float emissiveStrength = uniforms.u_TransparentParams3.x;
    float alphaCutoff = uniforms.u_TransparentParams3.y;
    // Default values
    if (opacity <= 0.0) { opacity = 0.5; }
    if (refractiveIndex <= 0.0) { refractiveIndex = 1.5; }
    if (fresnelPower <= 0.0) { fresnelPower = 5.0; }
    // Sample base color (tint)
    float4 baseColor = uniforms.u_AlbedoColor;
    if (uniforms.u_TextureFlags.x > 0.5) {
    baseColor *= u_AlbedoTexture.sample(u_AlbedoTexture_sampler, in.v_TexCoord);
    }
    // Alpha cutoff
    if (baseColor.a < alphaCutoff) {
    discard;
    }
    // Normal mapping (screen-space TBN)
    float3 N = normalize(in.v_Normal);
    if (uniforms.u_TextureFlags.z > 0.5) {
    float3 tangentNormal = u_NormalTexture.sample(u_NormalTexture_sampler, in.v_TexCoord).rgb * 2.0 - 1.0;
    tangentNormal.xy *= normalScale;
    float3 Q1 = dfdx(in.v_WorldPos);
    float3 Q2 = dfdy(in.v_WorldPos);
    float2 st1 = dfdx(in.v_TexCoord);
    float2 st2 = dfdy(in.v_TexCoord);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    N = normalize((TBN * tangentNormal));
    }
    float3 V = normalize(in.v_ViewDir);
    // Calculate fresnel effect
    float NdotV = max(dot(N, V), 0.0);
    float fresnel = fresnelEffect(NdotV, fresnelPower);
    // Ambient from light UBO
    float3 ambient = lightData.ambientLight.rgb * 0.2;
    // Accumulate lighting from all dynamic lights
    float3 totalDiffuse = float3(0.0, 0.0, 0.0);
    float3 totalSpecular = float3(0.0, 0.0, 0.0);
    int numLights = int(lightData.lightCounts.x);
    for (int i = 0; i < 16; i++) {
    if (i >= numLights) break;
    float3 lightColor = lightData.lights[i].color.rgb;
    float lightIntensity = lightData.lights[i].color.w;
    int lightType = int(lightData.lights[i].flags.x);
    float3 lightPos = lightData.lights[i].positionOrDirection.xyz;
    // Compute light direction
    float3 L;
    float attenuation = 1.0;
    if (lightType == 0) {
    // Directional light
    L = normalize(-lightData.lights[i].direction.xyz);
    } else {
    // Point or spot light
    float3 toLight = lightPos - in.v_WorldPos;
    float dist = length(toLight);
    L = normalize(toLight);
    float lightRange = lightData.lights[i].params.x;
    float distFactor = clamp(dist / lightRange, 0.0, 1.0);
    attenuation = 1.0 / (1.0 + distFactor * distFactor);
    attenuation *= smoothstep(1.0, 0.5, distFactor);
    if (lightType == 2) {
    // Spot light cone attenuation
    float3 spotDir = normalize(-lightData.lights[i].direction.xyz);
    float cosAngle = dot(L, spotDir);
    float innerCone = lightData.lights[i].params.y;
    float outerCone = lightData.lights[i].params.z;
    float epsilon = max(innerCone - outerCone, 0.0001);
    attenuation *= clamp((cosAngle - outerCone) / epsilon, 0.0, 1.0);
    }
    }
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    // Specular (Blinn-Phong with roughness)
    float specPower = mix(128.0, 8.0, roughness);
    float spec = pow(NdotH, specPower);
    // Diffuse contribution (subtle for glass)
    float3 diffuse = lightColor * lightIntensity * NdotL * 0.1;
    // Specular (reflection highlights)
    float3 specular = lightColor * lightIntensity * spec * reflectivity;
    // Shadow
    float shadow = 1.0;
    if (uniforms.u_ReceiveShadow != 0 && lightData.lights[i].flags.y > 0.5) {
    shadow = calculateShadow(int(lightData.lights[i].flags.z), in.v_WorldPos, N, L, lightPos, lightData, shadowMap0, shadowMap1, shadowMap2, shadowMap3, shadowMap4, shadowMap5, shadowMap6, shadowMap7, shadowCubeMap0, shadowCubeMap1, shadowCubeMap2, shadowCubeMap3, shadowCubeMap4, shadowCubeMap5, shadowCubeMap6, shadowCubeMap7, shadowSampler);
    }
    float lit = attenuation * shadow;
    totalDiffuse += diffuse * lit;
    totalSpecular += specular * lit;
    }
    // Emissive
    float3 emissive = uniforms.u_EmissiveColor.rgb * emissiveStrength;
    if (uniforms.u_TextureFlags.w > 0.5) {
    emissive *= u_EmissiveTexture.sample(u_EmissiveTexture_sampler, in.v_TexCoord).rgb;
    }
    // Glass color composition
    float3 transmissionColor = baseColor.rgb;
    float3 reflectionColor = ambient;
    // Mix based on fresnel and reflectivity
    float fresnelReflection = fresnel * reflectivity;
    float3 finalColor = mix(transmissionColor, reflectionColor, fresnelReflection);
    // Add specular highlights
    finalColor += totalSpecular;
    // Add diffuse lighting (very subtle)
    finalColor += totalDiffuse * baseColor.rgb;
    // Add emissive
    finalColor += emissive;
    // Apply tint
    finalColor *= uniforms.u_TintColor.rgb;
    // Distance fog
    finalColor = applyFog(finalColor, in.v_WorldPos, uniforms.u_CameraPosition, lightData);
    // Final alpha: base opacity increased by fresnel effect
    float finalAlpha = opacity + (1.0 - opacity) * fresnel * reflectivity;
    finalAlpha = clamp(finalAlpha, 0.0, 1.0);
    // Gamma correction
    finalColor = pow(finalColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    FragColor = float4(finalColor, finalAlpha);

    return FragColor;
}
