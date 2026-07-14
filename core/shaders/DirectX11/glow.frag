// DirectX11 Glow Fragment Shader

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
    float4 u_GlowParams;
    float4 u_GlowParams2;
    float4 u_GlowParams3;
};

Texture2D u_AlbedoTexture : register(t0);
SamplerState u_AlbedoTexture_sampler : register(s0);
Texture2D u_EmissiveTexture : register(t1);
SamplerState u_EmissiveTexture_sampler : register(s1);

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

    // Fresnel-based rim glow effect
    float fresnelGlow(float3 N, float3 V, float power) {
        float fresnel = 1.0 - max(dot(N, V), 0.0);
        return pow(fresnel, power);
    }

    // Color temperature shift (warm to cool)
    float3 temperatureShift(float3 color, float shift) {
        float3 warmColor = float3(1.0, 0.8, 0.6);
        float3 coolColor = float3(0.6, 0.8, 1.0);
        if (shift > 0.0) {
            return lerp(color, color * warmColor, shift);
        } else {
            return lerp(color, color * coolColor, -shift);
        }
    }

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    // Unpack parameters with defaults
    // u_GlowParams: x=intensity, y=falloff, z=pulseSpeed, w=pulseAmount
    float intensity = u_GlowParams.x > 0.0 ? u_GlowParams.x : 1.0;
    float pulseSpeed = u_GlowParams.z;
    float pulseAmount = u_GlowParams.w;
    // u_GlowParams2: x=coreSize, y=coreBrightness, z=outerGlow, w=alphaCutoff
    float coreBrightness = u_GlowParams2.y > 0.0 ? u_GlowParams2.y : 1.0;
    float alphaCutoff = u_GlowParams2.w;
    // u_GlowParams3: x=fresnelPower, y=fresnelIntensity, z=time, w=colorShift
    float fresnelPower = u_GlowParams3.x > 0.0 ? u_GlowParams3.x : 3.0;
    float fresnelIntensity = u_GlowParams3.y;
    float time = u_GlowParams3.z;
    float colorShift = u_GlowParams3.w;
    // Sample base color
    float4 baseColor = u_AlbedoColor;
    if (u_TextureFlags.x > 0.5) {
    baseColor *= u_AlbedoTexture.Sample(u_AlbedoTexture_sampler, input.v_TexCoord);
    }
    // Alpha cutoff
    if (baseColor.a < alphaCutoff) {
    discard;
    }
    // Sample emissive - if emissive color is black/unset, use white so base color shows
    float3 emissiveSample = u_EmissiveColor.rgb;
    float emissiveLength = dot(emissiveSample, emissiveSample);
    if (emissiveLength < 0.001) {
    emissiveSample = float3(1.0, 1.0, 1.0);
    }
    if (u_TextureFlags.w > 0.5) {
    emissiveSample *= u_EmissiveTexture.Sample(u_EmissiveTexture_sampler, input.v_TexCoord).rgb;
    }
    // Calculate pulse effect
    float pulse = 1.0;
    if (pulseSpeed > 0.0) {
    pulse = 1.0 + sin(time * pulseSpeed) * pulseAmount;
    }
    // Fresnel rim effect
    float3 N = normalize(input.v_Normal);
    float3 V = normalize(input.v_ViewDir);
    float fresnel = fresnelGlow(N, V, fresnelPower) * fresnelIntensity;
    // Total glow multiplier
    float totalGlow = (coreBrightness + fresnel) * intensity * pulse;
    // Glow color: base color * emissive multiplier
    float3 glowColor = baseColor.rgb * emissiveSample;
    // Apply color temperature shift
    if (abs(colorShift) > 0.001) {
    glowColor = temperatureShift(glowColor, colorShift);
    }
    // Ambient from scene lights (subtle environmental tint on glow objects)
    float3 ambient = ambientLight.rgb;
    glowColor += baseColor.rgb * ambient * 0.15;
    // Apply tint
    glowColor *= u_TintColor.rgb;
    // Final color with glow
    float3 finalColor = glowColor * totalGlow;
    // Use base alpha, boosted slightly by glow intensity
    float finalAlpha = min(baseColor.a * (0.5 + totalGlow * 0.5), 1.0);
    // Gamma correction
    finalColor = pow(finalColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    FragColor = float4(finalColor, finalAlpha);

    return FragColor;
}
