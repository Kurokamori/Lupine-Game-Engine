// DirectX12 Standard3D Vertex Shader

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
    float3 v_Normal : TEXCOORD1;
    float2 v_TexCoord : TEXCOORD2;
    float4 v_Color : TEXCOORD3;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 worldPos = mul(u_Model, float4(input.a_Position, 1.0));
    output.v_WorldPos = worldPos.xyz;
    output.v_Normal = mul((float3x3)u_NormalMatrix, input.a_Normal);
    output.v_TexCoord = input.a_TexCoord;
    output.v_Color = input.a_Color;
    output.position = mul(u_ViewProjection, worldPos);

    return output;
}
