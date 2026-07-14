// DirectX11 Text Vertex Shader

cbuffer PushConstants : register(b0)
{
    float4x4 u_ViewProjection;
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
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.v_TexCoord = input.a_TexCoord;
    output.v_Color = input.a_Color;
    output.position = mul(u_ViewProjection, float4(input.a_Position, 1.0));

    return output;
}
