// DirectX12 Skybox Vertex Shader

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
    int u_SkyboxType;
    float4 u_SkyboxColor;
    float4 u_SkyTopColor;
    float4 u_SkyHorizonColor;
    float4 u_SkyBottomColor;
};

struct VS_INPUT
{
    float3 a_Position : POSITION;
    float3 a_Normal : NORMAL;
    float2 a_TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 v_Position : TEXCOORD0;
    float3 v_TexCoord3D : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.v_TexCoord3D = input.a_Position;
    output.v_Position = input.a_Position;
    float4 pos = mul(u_ViewProjection, float4(input.a_Position, 1.0));
    output.position = float4(pos.xy, pos.w, pos.w);

    return output;
}
