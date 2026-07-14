// DirectX12 Skeletal Fragment Shader

cbuffer PushConstants : register(b0)
{
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    float4x4 u_NormalMatrix;
    float4 u_TintColor;
    int u_UseTexture;
    float u_AlphaCutoff;
    float _pad1;
    float _pad2;
    float4 u_UVRect;
    float4 u_TextureFlags;
    float4 u_MaterialParams1;
    float4 u_MaterialParams2;
    float4 u_CameraPosition;
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;
    int u_ReceiveShadow;
    float _pad3;
    float _pad4;
    float _pad5;
    // --- Shader-specific uniforms (set via setUniform*) ---
    float4x4 u_View;
    bool u_UseSkinning;
};

cbuffer BoneData : register(b2)
{
    float4x4 u_BoneTransforms[128];
};

Texture2D u_AlbedoTexture : register(t0);
SamplerState u_AlbedoTexture_sampler : register(s0);
Texture2D u_MetallicRoughnessTexture : register(t1);
SamplerState u_MetallicRoughnessTexture_sampler : register(s1);
Texture2D u_NormalTexture : register(t2);
SamplerState u_NormalTexture_sampler : register(s2);
Texture2D u_EmissiveTexture : register(t3);
SamplerState u_EmissiveTexture_sampler : register(s3);

struct Light {
    float4 positionOrDirection;
    float4 direction;
    float4 color;
    float4 params;
    float4 flags;
};
struct ShadowMap {
    float4x4 lightSpaceMatrix;
    float4 shadowParams;
    float4 shadowParams2;
};
struct CascadedShadowMap {
    float4x4 cascadeMatrices[8];
    float4 cascadeSplits;
    float4 cascadeSplits2;
    float4 cascadeParams;
    float4 cascadeParams2;
};

cbuffer LightData : register(b3)
{
    Light lights[16];
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    float4 ambientLight;
    float4 lightCounts;
    float4 fogColor;
    float4 fogParams;
};

Texture2D u_ShadowMaps[8] : register(t4);
TextureCube u_ShadowCubeMaps[8] : register(t12);
SamplerState u_ShadowSampler : register(s8);

float sampleShadowMap(int idx, float2 uv) {
    // SampleLevel with explicit LOD 0 — shadow maps are single-mip depth textures.
    // Using Sample() inside a dynamic branch is undefined behavior (broken gradients).
    float r = 1.0;
    if (idx == 0) r = u_ShadowMaps[0].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 1) r = u_ShadowMaps[1].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 2) r = u_ShadowMaps[2].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 3) r = u_ShadowMaps[3].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 4) r = u_ShadowMaps[4].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 5) r = u_ShadowMaps[5].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 6) r = u_ShadowMaps[6].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 7) r = u_ShadowMaps[7].SampleLevel(u_ShadowSampler, uv, 0).r;
    return r;
}
float sampleShadowCubeMap(int idx, float3 dir) {
    float r = 1.0;
    if (idx == 0) r = u_ShadowCubeMaps[0].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 1) r = u_ShadowCubeMaps[1].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 2) r = u_ShadowCubeMaps[2].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 3) r = u_ShadowCubeMaps[3].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 4) r = u_ShadowCubeMaps[4].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 5) r = u_ShadowCubeMaps[5].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 6) r = u_ShadowCubeMaps[6].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 7) r = u_ShadowCubeMaps[7].SampleLevel(u_ShadowSampler, dir, 0).r;
    return r;
}

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 v_WorldPos : TEXCOORD0;
    float3 v_ViewPos : TEXCOORD1;
    float3 v_Normal : TEXCOORD2;
    float2 v_TexCoord : TEXCOORD3;
    float4 v_Color : TEXCOORD4;
    float3 v_ViewDir : TEXCOORD5;
};

float calculateShadowPCF(float4x4 lsMatrix, int smIndex, float3 wPos, float3 N, float3 lDir,
                         float bias, float nBias, float blur, float opacity, float res) {
    float4 lsPos = mul(lsMatrix, float4(wPos, 1.0));
    float3 proj = lsPos.xyz / lsPos.w;
    proj.xy = proj.xy * 0.5 + 0.5;
    proj.y = 1.0 - proj.y;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float curDepth = proj.z;
    float NdL = max(dot(N, lDir), 0.0);
    float fBias = lerp(bias * 2.0, bias, NdL);
    float shadow = 0.0;
    float2 texelSz = 1.0 / float2(res, res);
    int kr = clamp(int(blur), 1, 3);
    int sc = 0;
    for (int x = -kr; x <= kr; ++x) {
        for (int y = -kr; y <= kr; ++y) {
            float2 off = float2(float(x), float(y)) * texelSz * blur;
            float d = sampleShadowMap(smIndex, proj.xy + off);
            shadow += curDepth - fBias > d ? 1.0 : 0.0;
            sc++;
        }
    }
    shadow /= float(sc);
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadowCube(int cmIndex, float3 lPos, float3 wPos, float3 N,
                          float bias, float blur, float opacity, float lRange) {
    float3 frag2light = wPos - lPos;
    float curDepth = length(frag2light);
    float normDepth = curDepth / lRange;
    float3 sDir = normalize(frag2light);
    float closest = sampleShadowCubeMap(cmIndex, sDir);
    float3 lDir2 = -sDir;
    float NdL = max(dot(N, lDir2), 0.0);
    float nBias = bias / lRange;
    float fBias = lerp(nBias * 3.0, nBias * 0.5, NdL);
    float shadow = (normDepth - fBias) > closest ? 1.0 : 0.0;
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadow(int smIndex, float3 wPos, float3 N, float3 lDir, float3 lPos) {
    if (smIndex < 0 || smIndex >= 8) return 1.0;
    ShadowMap sm = shadowMaps[smIndex];
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

float3 applyFog(float3 color, float3 wPos, float3 camPos) {
    if (fogColor.w < 0.5) {
        return color;
    }
    float dist = length(wPos - camPos);
    float density = fogParams.x;
    float fogStart = fogParams.y;
    float fogEnd = fogParams.z;
    int mode = int(fogParams.w);
    float factor = 1.0;
    if (mode == 0) {
        factor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);
    } else if (mode == 1) {
        factor = exp(-density * max(dist - fogStart, 0.0));
    } else {
        float dd = density * max(dist - fogStart, 0.0);
        factor = exp(-dd * dd);
    }
    return lerp(fogColor.rgb, color, clamp(factor, 0.0, 1.0));
}

float3 getNormalFromMap(PS_INPUT input) {
    if (u_TextureFlags.z < 0.5)
        return normalize(input.v_Normal);
    float3 tNorm = u_NormalTexture.Sample(u_NormalTexture_sampler, input.v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= u_MaterialParams1.z;
    float3 Q1 = ddx(input.v_WorldPos);
    float3 Q2 = ddy(input.v_WorldPos);
    float2 st1 = ddx(input.v_TexCoord);
    float2 st2 = ddy(input.v_TexCoord);
    float3 Nn = normalize(input.v_Normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(Nn, T));
    float3x3 TBN = float3x3(T, B, Nn);
    return normalize(mul(TBN, tNorm));
}

    static const float PI = 3.14159265359;

    float3 fresnelSchlick(float cosTheta, float3 F0) {
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }

    float distributionGGX(float3 N, float3 H, float roughness) {
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

    float geometrySmith(float3 N, float3 V, float3 L, float roughness) {
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
    }

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    // Material parameters
    float metallic = u_MaterialParams1.x;
    float roughness = u_MaterialParams1.y;
    float emissiveStrength = u_MaterialParams1.w;
    float alphaCutoff = u_MaterialParams2.x;
    float aoStrength = u_MaterialParams2.y;
    // Base albedo (don't multiply by input.v_Color — imported models may lack vertex colors)
    float4 albedoSample = u_AlbedoColor;
    if (u_TextureFlags.x > 0.5) {
    albedoSample *= u_AlbedoTexture.Sample(u_AlbedoTexture_sampler, input.v_TexCoord);
    }
    // Alpha test
    if (alphaCutoff > 0.0 && albedoSample.a < alphaCutoff) {
    discard;
    }
    float3 albedo = albedoSample.rgb;
    // Metallic / roughness from texture
    if (u_TextureFlags.y > 0.5) {
    float4 mrSample = u_MetallicRoughnessTexture.Sample(u_MetallicRoughnessTexture_sampler, input.v_TexCoord);
    metallic = mrSample.b;
    roughness = mrSample.g;
    }
    roughness = clamp(roughness, 0.04, 1.0);
    // Normal mapping via #feature normal_mapping
    float3 N = getNormalFromMap(input);
    float3 V = normalize(input.v_ViewDir);
    // Dielectric F0 = 0.04, metallic lerps toward albedo
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    // Accumulate lighting from all lights
    float3 Lo = float3(0.0, 0.0, 0.0);
    int numLights = int(lightCounts.x);
    for (int i = 0; i < 16; ++i) {
    if (i >= numLights) break;
    int lightType = int(lights[i].flags.x);
    float3 lightPos = lights[i].positionOrDirection.xyz;
    float3 lightColor = lights[i].color.rgb * lights[i].color.w;
    float3 L;
    float attenuation = 1.0;
    if (lightType == 0) {
    // Directional light
    L = normalize(-lights[i].direction.xyz);
    } else if (lightType == 1) {
    // Point light
    float3 toLight = lightPos - input.v_WorldPos;
    float dist = length(toLight);
    L = normalize(toLight);
    float range = lights[i].params.x;
    float distFactor = dist / range;
    attenuation = 1.0 / (1.0 + distFactor * distFactor);
    attenuation *= smoothstep(1.0, 0.5, distFactor);
    } else {
    // Spot light
    float3 toLight = lightPos - input.v_WorldPos;
    float dist = length(toLight);
    L = normalize(toLight);
    float3 spotDir = normalize(lights[i].direction.xyz);
    float theta = dot(L, -spotDir);
    float innerCutoff = lights[i].params.y;
    float outerCutoff = lights[i].params.z;
    float epsilon = max(innerCutoff - outerCutoff, 0.0001);
    float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    float range = lights[i].params.x;
    float distFactor = clamp(dist / range, 0.0, 1.0);
    attenuation = 1.0 / (1.0 + distFactor * distFactor);
    attenuation *= smoothstep(1.0, 0.5, distFactor);
    attenuation *= spotIntensity;
    }
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    // Shadow
    float shadow = 1.0;
    if (u_ReceiveShadow != 0 && lights[i].flags.y > 0.5) {
    shadow = calculateShadow(int(lights[i].flags.z), input.v_WorldPos, N, L, lightPos);
    }
    // Cook-Torrance BRDF
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    float3 specular = numerator / denominator;
    float3 kD = (float3(1.0, 1.0, 1.0) - F) * (1.0 - metallic);
    float3 radiance = lightColor * attenuation;
    Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
    }
    // Ambient lighting from scene
    float3 ambient = ambientLight.rgb * ambientLight.a * albedo * aoStrength;
    // Emissive
    float3 emissive = u_EmissiveColor.rgb * emissiveStrength;
    if (u_TextureFlags.w > 0.5) {
    emissive *= u_EmissiveTexture.Sample(u_EmissiveTexture_sampler, input.v_TexCoord).rgb;
    }
    float3 color = ambient + Lo + emissive;
    // Apply tint
    color *= u_TintColor.rgb;
    // Distance fog
    color = applyFog(color, input.v_WorldPos, u_CameraPosition);
    // HDR tonemap (Reinhard)
    color = color / (color + float3(1.0, 1.0, 1.0));
    // Gamma correction
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    FragColor = float4(color, albedoSample.a);

    return FragColor;
}
