// DirectX12 RadialGradient Fragment Shader

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
    float4 u_InnerColor;
    float4 u_OuterColor;
    float u_Radius;
    float2 u_Center;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
    float4 v_Color : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    float dist = distance(input.v_TexCoord, u_Center);
    float t = clamp(dist / max(u_Radius, 0.0001), 0.0, 1.0);
    float4 gradient = lerp(u_InnerColor, u_OuterColor, t);
    FragColor = gradient * input.v_Color;

    return FragColor;
}
