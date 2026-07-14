#version 300 es

precision highp float;
precision highp int;

in vec3 v_WorldPos;
in vec3 v_ViewPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_Color;
in vec3 v_ViewDir;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;
uniform vec3 u_CameraPosition;
uniform vec4 u_TintColor;
uniform vec4 u_AlbedoColor;
uniform vec4 u_EmissiveColor;
uniform vec4 u_TextureFlags;
uniform vec4 u_GlowParams;
uniform vec4 u_GlowParams2;
uniform vec4 u_GlowParams3;
uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_EmissiveTexture;

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
layout(std140) uniform LightData {
    Light lights[16];
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    vec4 ambientLight;
    vec4 lightCounts;
    vec4 fogColor;
    vec4 fogParams;
} u_Lights;

out vec4 FragColor;

    // Fresnel-based rim glow effect
    float fresnelGlow(vec3 N, vec3 V, float power) {
        float fresnel = 1.0 - max(dot(N, V), 0.0);
        return pow(fresnel, power);
    }

    // Color temperature shift (warm to cool)
    vec3 temperatureShift(vec3 color, float shift) {
        vec3 warmColor = vec3(1.0, 0.8, 0.6);
        vec3 coolColor = vec3(0.6, 0.8, 1.0);
        if (shift > 0.0) {
            return mix(color, color * warmColor, shift);
        } else {
            return mix(color, color * coolColor, -shift);
        }
    }


    void main() {
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
        vec4 baseColor = u_AlbedoColor;
        if (u_TextureFlags.x > 0.5) {
            baseColor *= texture(u_AlbedoTexture, v_TexCoord);
        }

        // Alpha cutoff
        if (baseColor.a < alphaCutoff) {
            discard;
        }

        // Sample emissive - if emissive color is black/unset, use white so base color shows
        vec3 emissiveSample = u_EmissiveColor.rgb;
        float emissiveLength = dot(emissiveSample, emissiveSample);
        if (emissiveLength < 0.001) {
            emissiveSample = vec3(1.0);
        }
        if (u_TextureFlags.w > 0.5) {
            emissiveSample *= texture(u_EmissiveTexture, v_TexCoord).rgb;
        }

        // Calculate pulse effect
        float pulse = 1.0;
        if (pulseSpeed > 0.0) {
            pulse = 1.0 + sin(time * pulseSpeed) * pulseAmount;
        }

        // Fresnel rim effect
        vec3 N = normalize(v_Normal);
        vec3 V = normalize(v_ViewDir);
        float fresnel = fresnelGlow(N, V, fresnelPower) * fresnelIntensity;

        // Total glow multiplier
        float totalGlow = (coreBrightness + fresnel) * intensity * pulse;

        // Glow color: base color * emissive multiplier
        vec3 glowColor = baseColor.rgb * emissiveSample;

        // Apply color temperature shift
        if (abs(colorShift) > 0.001) {
            glowColor = temperatureShift(glowColor, colorShift);
        }

        // Ambient from scene lights (subtle environmental tint on glow objects)
        vec3 ambient = u_Lights.ambientLight.rgb;
        glowColor += baseColor.rgb * ambient * 0.15;

        // Apply tint
        glowColor *= u_TintColor.rgb;

        // Final color with glow
        vec3 finalColor = glowColor * totalGlow;

        // Use base alpha, boosted slightly by glow intensity
        float finalAlpha = min(baseColor.a * (0.5 + totalGlow * 0.5), 1.0);

        // Gamma correction
        finalColor = pow(finalColor, vec3(1.0 / 2.2));

        FragColor = vec4(finalColor, finalAlpha);
    }
