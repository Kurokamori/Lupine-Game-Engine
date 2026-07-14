// DirectX11 Glow Vertex Shader

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
