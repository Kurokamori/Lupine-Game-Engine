// DirectX11 BoundingBox Fragment Shader

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
    float4 u_BoxColor;
    float u_LineWidth;
    float2 u_ScreenSize;
    float4 u_BoxMin;
    float4 u_BoxMax;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 v_Color : COLOR;
    float2 v_LineCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    float dist = abs(input.v_LineCoord.y);
    float aa = 1.0 - smoothstep(0.7, 1.0, dist);
    float4 color = input.v_Color;
    color.a *= aa;
    if (color.a < 0.01) { discard; }
    FragColor = color;

    return FragColor;
}
