// DirectX11 Toon Fragment Shader

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
    float4 u_ToonMaterialParams;
    float4 u_ToonMaterialParams2;
    float4 u_ToonParams;
};

Texture2D u_AlbedoTexture : register(t0);
SamplerState u_AlbedoTexture_sampler : register(s0);
Texture2D u_NormalTexture : register(t1);
SamplerState u_NormalTexture_sampler : register(s1);
Texture2D u_EmissiveTexture : register(t2);
SamplerState u_EmissiveTexture_sampler : register(s2);

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
    float calculateRimLighting(float3 N, float3 V, float rimPower, float rimIntensity) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        rim = pow(rim, rimPower) * rimIntensity;
        return rim;
    }

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    // Sample albedo texture
    float4 albedo = u_AlbedoColor;
    if (u_TextureFlags.x > 0.5) {
    albedo *= u_AlbedoTexture.Sample(u_AlbedoTexture_sampler, input.v_TexCoord);
    }
    // Alpha test (u_ToonMaterialParams2.x = alphaCutoff)
    if (albedo.a < u_ToonMaterialParams2.x) {
    discard;
    }
    // Unpack toon parameters with defaults
    // u_ToonMaterialParams: x=shadowBands, y=specularBands, z=normalScale, w=emissiveStrength
    float shadowBands = max(u_ToonMaterialParams.x, 2.0);
    float specularBands = max(u_ToonMaterialParams.y, 2.0);
    float normalScale = u_ToonMaterialParams.z;
    // u_ToonParams: x=shadowThreshold, y=shadowSoftness, z=specularThreshold, w=specularSoftness
    float shadowThreshold = u_ToonParams.x > 0.0 ? u_ToonParams.x : 0.5;
    float shadowSoftness = u_ToonParams.y > 0.0 ? u_ToonParams.y : 0.02;
    float specularThreshold = u_ToonParams.z > 0.0 ? u_ToonParams.z : 0.3;
    float specularSoftness = u_ToonParams.w > 0.0 ? u_ToonParams.w : 0.05;
    // u_ToonMaterialParams2: x=alphaCutoff, y=rimPower, z=rimIntensity, w=specularPower
    float rimPower = u_ToonMaterialParams2.y > 0.0 ? u_ToonMaterialParams2.y : 3.0;
    float rimIntensity = u_ToonMaterialParams2.z;
    float specularPower = u_ToonMaterialParams2.w > 0.0 ? u_ToonMaterialParams2.w : 32.0;
    // Normal mapping (screen-space TBN)
    float3 N = normalize(input.v_Normal);
    if (u_TextureFlags.z > 0.5) {
    float3 tangentNormal = u_NormalTexture.Sample(u_NormalTexture_sampler, input.v_TexCoord).rgb * 2.0 - 1.0;
    tangentNormal.xy *= normalScale;
    float3 Q1 = ddx(input.v_WorldPos);
    float3 Q2 = ddy(input.v_WorldPos);
    float2 st1 = ddx(input.v_TexCoord);
    float2 st2 = ddy(input.v_TexCoord);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    N = normalize(mul(TBN, tangentNormal));
    }
    float3 V = normalize(input.v_ViewDir);
    // Ambient from light UBO
    float3 ambient = ambientLight.rgb;
    // Accumulate lighting from all dynamic lights
    float3 totalDiffuse = float3(0.0, 0.0, 0.0);
    float3 totalSpecular = float3(0.0, 0.0, 0.0);
    int numLights = int(lightCounts.x);
    for (int i = 0; i < 16; i++) {
    if (i >= numLights) break;
    float3 lightColor = lights[i].color.rgb;
    float lightIntensity = lights[i].color.w;
    int lightType = int(lights[i].flags.x);
    float3 lightPos = lights[i].positionOrDirection.xyz;
    // Compute light direction
    float3 L;
    float attenuation = 1.0;
    if (lightType == 0) {
    // Directional light
    L = normalize(-lights[i].direction.xyz);
    } else {
    // Point or spot light
    float3 toLight = lightPos - input.v_WorldPos;
    float dist = length(toLight);
    L = normalize(toLight);
    float lightRange = lights[i].params.x;
    float distFactor = clamp(dist / lightRange, 0.0, 1.0);
    attenuation = 1.0 / (1.0 + distFactor * distFactor);
    attenuation *= smoothstep(1.0, 0.5, distFactor);
    if (lightType == 2) {
    // Spot light cone attenuation
    float3 spotDir = normalize(-lights[i].direction.xyz);
    float cosAngle = dot(L, spotDir);
    float innerCone = lights[i].params.y;
    float outerCone = lights[i].params.z;
    float epsilon = max(innerCone - outerCone, 0.0001);
    attenuation *= clamp((cosAngle - outerCone) / epsilon, 0.0, 1.0);
    }
    }
    float3 H = normalize(V + L);
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
    if (u_ReceiveShadow != 0 && lights[i].flags.y > 0.5) {
    shadow = calculateShadow(int(lights[i].flags.z), input.v_WorldPos, N, L, lightPos);
    }
    float3 litColor = lightColor * lightIntensity * attenuation * shadow;
    totalDiffuse += litColor * celDiffuse;
    totalSpecular += litColor * celSpecular;
    }
    // Rim lighting
    float rim = calculateRimLighting(N, V, rimPower, rimIntensity);
    float3 rimColor = ambient * rim;
    // Emissive (u_ToonMaterialParams.w = emissiveStrength)
    float3 emissive = u_EmissiveColor.rgb * u_ToonMaterialParams.w;
    if (u_TextureFlags.w > 0.5) {
    emissive *= u_EmissiveTexture.Sample(u_EmissiveTexture_sampler, input.v_TexCoord).rgb;
    }
    // Final color composition
    float3 diffuseColor = albedo.rgb * (ambient + totalDiffuse);
    float3 specularColor = totalSpecular;
    float3 color = diffuseColor + specularColor + rimColor + emissive;
    // Apply tint
    color *= u_TintColor.rgb;
    // Distance fog
    color = applyFog(color, input.v_WorldPos, u_CameraPosition);
    // Gamma correction
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    FragColor = float4(color, albedo.a);

    return FragColor;
}
