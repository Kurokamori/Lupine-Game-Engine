// DirectX11 Stylized Vertex Shader

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
    float4 u_StylizedParams;
};

struct VS_INPUT
{
    float3 a_Position : POSITION;
    float3 a_Normal : NORMAL;
    float2 a_TexCoord : TEXCOORD0;
    float4 a_Color : COLOR;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 v_WorldPos : TEXCOORD0;
    float3 v_ViewPos : TEXCOORD1;
    float3 v_Normal : TEXCOORD2;
    float2 v_TexCoord : TEXCOORD3;
    float4 v_Color : TEXCOORD4;
    float3 v_ViewDir : TEXCOORD5;
};

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
    float calculateRimLighting(float3 N, float3 V, float rimPower, float rimIntensity) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        rim = pow(rim, rimPower) * rimIntensity;
        return rim;
    }

    // Apply shadow ramp (procedural warm-to-cool color shift)
    float3 applyShadowRamp(float lightAmount, float3 baseColor, float shadowBrightness, float shadowWarmth) {
        // Procedural shadow ramp with warmth control
        float3 warmShift = float3(1.0, 0.9, 0.8);
        float3 coolShift = float3(0.8, 0.85, 1.0);
        float3 colorShift = lerp(coolShift, warmShift, lightAmount);
        colorShift = lerp(float3(1.0, 1.0, 1.0), colorShift, shadowWarmth);
        float shadowFactor = lerp(shadowBrightness, 1.0, lightAmount);
        return baseColor * colorShift * shadowFactor;
    }

    // Soft specular highlight calculation
    float calculateSpecular(float3 N, float3 V, float3 L, float specularPower, float specularSoftness, float NdotL) {
        float3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);
        float specularRaw = pow(NdotH, specularPower);
        float specularThreshold = 0.5;
        float specular = softStep(specularThreshold - specularSoftness,
                                  specularThreshold + specularSoftness,
                                  specularRaw);
        return specular * smoothstep(0.0, 0.3, NdotL);
    }

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 worldPos = mul(u_Model, float4(input.a_Position, 1.0));
    output.v_WorldPos = worldPos.xyz;
    output.v_ViewPos = mul(u_View, worldPos).xyz;
    output.v_Normal = mul((float3x3)u_NormalMatrix, input.a_Normal);
    output.v_TexCoord = input.a_TexCoord;
    output.v_Color = input.a_Color;
    output.v_ViewDir = normalize(u_CameraPosition.xyz - worldPos.xyz);
    output.position = mul(u_ViewProjection, worldPos);

    return output;
}
