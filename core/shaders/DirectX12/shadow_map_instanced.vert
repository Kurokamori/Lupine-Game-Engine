// DirectX12 ShadowMapInstanced Vertex Shader

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
    int u_HasAlbedoTexture;
};

struct VS_INPUT
{
    float3 a_Position : POSITION;
    float2 a_TexCoord : TEXCOORD0;
    float4 a_InstanceModel0 : TEXCOORD1;
    float4 a_InstanceModel1 : TEXCOORD2;
    float4 a_InstanceModel2 : TEXCOORD3;
    float4 a_InstanceModel3 : TEXCOORD4;
    float4 a_InstanceColor : TEXCOORD5;
    float4 a_InstanceCustom : TEXCOORD6;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.v_TexCoord = input.a_TexCoord;
    float4 worldPos = input.a_InstanceModel0 * input.a_Position.x
    + input.a_InstanceModel1 * input.a_Position.y
    + input.a_InstanceModel2 * input.a_Position.z
    + input.a_InstanceModel3;
    output.position = mul(u_ViewProjection, worldPos);

    return output;
}
