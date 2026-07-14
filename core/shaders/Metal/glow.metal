// Glow - Metal Shading Language
// Emissive glow shader with pulse animation, fresnel rim glow, and color temperature shift

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
    float4 u_GlowParams;
    float4 u_GlowParams2;
    float4 u_GlowParams3;
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
            return mix(color, color * warmColor, shift);
        } else {
            return mix(color, color * coolColor, -shift);
        }
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
    out.position = (uniforms.u_ViewProjection * worldPos);

    return out;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    constant LightUniformBuffer& lightData [[buffer(3)]],
    texture2d<float> u_AlbedoTexture [[texture(0)]],
    sampler u_AlbedoTexture_sampler [[sampler(0)]],
    texture2d<float> u_EmissiveTexture [[texture(1)]],
    sampler u_EmissiveTexture_sampler [[sampler(1)]]
) {
    float4 FragColor;

    // Unpack parameters with defaults
    // uniforms.u_GlowParams: x=intensity, y=falloff, z=pulseSpeed, w=pulseAmount
    float intensity = uniforms.u_GlowParams.x > 0.0 ? uniforms.u_GlowParams.x : 1.0;
    float pulseSpeed = uniforms.u_GlowParams.z;
    float pulseAmount = uniforms.u_GlowParams.w;
    // uniforms.u_GlowParams2: x=coreSize, y=coreBrightness, z=outerGlow, w=alphaCutoff
    float coreBrightness = uniforms.u_GlowParams2.y > 0.0 ? uniforms.u_GlowParams2.y : 1.0;
    float alphaCutoff = uniforms.u_GlowParams2.w;
    // uniforms.u_GlowParams3: x=fresnelPower, y=fresnelIntensity, z=time, w=colorShift
    float fresnelPower = uniforms.u_GlowParams3.x > 0.0 ? uniforms.u_GlowParams3.x : 3.0;
    float fresnelIntensity = uniforms.u_GlowParams3.y;
    float time = uniforms.u_GlowParams3.z;
    float colorShift = uniforms.u_GlowParams3.w;
    // Sample base color
    float4 baseColor = uniforms.u_AlbedoColor;
    if (uniforms.u_TextureFlags.x > 0.5) {
    baseColor *= u_AlbedoTexture.sample(u_AlbedoTexture_sampler, in.v_TexCoord);
    }
    // Alpha cutoff
    if (baseColor.a < alphaCutoff) {
    discard;
    }
    // Sample emissive - if emissive color is black/unset, use white so base color shows
    float3 emissiveSample = uniforms.u_EmissiveColor.rgb;
    float emissiveLength = dot(emissiveSample, emissiveSample);
    if (emissiveLength < 0.001) {
    emissiveSample = float3(1.0, 1.0, 1.0);
    }
    if (uniforms.u_TextureFlags.w > 0.5) {
    emissiveSample *= u_EmissiveTexture.sample(u_EmissiveTexture_sampler, in.v_TexCoord).rgb;
    }
    // Calculate pulse effect
    float pulse = 1.0;
    if (pulseSpeed > 0.0) {
    pulse = 1.0 + sin(time * pulseSpeed) * pulseAmount;
    }
    // Fresnel rim effect
    float3 N = normalize(in.v_Normal);
    float3 V = normalize(in.v_ViewDir);
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
    float3 ambient = lightData.ambientLight.rgb;
    glowColor += baseColor.rgb * ambient * 0.15;
    // Apply tint
    glowColor *= uniforms.u_TintColor.rgb;
    // Final color with glow
    float3 finalColor = glowColor * totalGlow;
    // Use base alpha, boosted slightly by glow intensity
    float finalAlpha = min(baseColor.a * (0.5 + totalGlow * 0.5), 1.0);
    // Gamma correction
    finalColor = pow(finalColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    FragColor = float4(finalColor, finalAlpha);

    return FragColor;
}
