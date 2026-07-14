
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

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
    vec4 u_MaterialParams1;
    vec4 u_MaterialParams2;
    vec4 u_StylizedParams;
    int u_ReceiveShadow;
} material;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_ViewPos;
layout(location = 2) out vec3 v_Normal;
layout(location = 3) out vec2 v_TexCoord;
layout(location = 4) out vec4 v_Color;
layout(location = 5) out vec3 v_ViewDir;

    // Half-Lambert diffuse for softer shading
    float halfLambert(float NdotL, float power) {
        float wrapped = NdotL * 0.5 + 0.5;
        return pow(wrapped, power);
    }

    // Soft step for smooth transitions
    float softStep(float edge0, float edge1, float x) {
        float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

    // Calculate rim lighting (fresnel-based edge glow)
    float calculateRimLighting(vec3 N, vec3 V, float rimPower, float rimIntensity) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        rim = pow(rim, rimPower) * rimIntensity;
        return rim;
    }

    // Apply shadow ramp (procedural warm-to-cool color shift)
    vec3 applyShadowRamp(float lightAmount, vec3 baseColor, float shadowBrightness, float shadowWarmth) {
        // Procedural shadow ramp with warmth control
        vec3 warmShift = vec3(1.0, 0.9, 0.8);
        vec3 coolShift = vec3(0.8, 0.85, 1.0);
        vec3 colorShift = mix(coolShift, warmShift, lightAmount);
        colorShift = mix(vec3(1.0), colorShift, shadowWarmth);
        float shadowFactor = mix(shadowBrightness, 1.0, lightAmount);
        return baseColor * colorShift * shadowFactor;
    }

    // Soft specular highlight calculation
    float calculateSpecular(vec3 N, vec3 V, vec3 L, float specularPower, float specularSoftness, float NdotL) {
        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);
        float specularRaw = pow(NdotH, specularPower);
        float specularThreshold = 0.5;
        float specular = softStep(specularThreshold - specularSoftness,
                                  specularThreshold + specularSoftness,
                                  specularRaw);
        return specular * smoothstep(0.0, 0.3, NdotL);
    }



    void main() {
        vec4 worldPos = (pc.u_Model * vec4(a_Position, 1.0));
        v_WorldPos = worldPos.xyz;
        v_ViewPos = (material.u_View * worldPos).xyz;
        v_Normal = (mat3(pc.u_NormalMatrix) * a_Normal);
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_ViewDir = normalize(material.u_CameraPosition.xyz - worldPos.xyz);
        gl_Position = (pc.u_ViewProjection * worldPos);
    }
