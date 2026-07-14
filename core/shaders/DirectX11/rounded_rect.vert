// DirectX11 RoundedRect Vertex Shader

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
    float4 u_CornerRadius;
    float2 u_Size;
    float2 _padSize;
    float4 u_MaterialParams2;
    float4 u_CameraPosition;
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;
    int u_ReceiveShadow;
    float _pad3;
    float _pad4;
    float _pad5;
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
    float2 v_TexCoord : TEXCOORD0;
    float4 v_Color : TEXCOORD1;
    float2 v_LocalPos : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.v_TexCoord = input.a_TexCoord;
    output.v_Color = input.a_Color;
    output.v_LocalPos = input.a_Position.xy;
    output.position = mul(u_ViewProjection, mul(u_Model, float4(input.a_Position, 1.0)));

    return output;
}
