// DirectX11 ShadowMapSkeletal Vertex Shader

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
    bool u_UseSkinning;
};

cbuffer BoneData : register(b2)
{
    float4x4 u_BoneTransforms[128];
};

struct VS_INPUT
{
    float3 a_Position : POSITION;
    float3 a_Normal : NORMAL;
    float2 a_TexCoord : TEXCOORD0;
    float4 a_Color : COLOR;
    float4 a_Tangent : TANGENT;
    float4 a_BoneIDs : BLENDINDICES;
    float4 a_BoneWeights : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 localPos = float4(input.a_Position, 1.0);
    if (u_UseSkinning) {
    float4x4 boneTransform = u_BoneTransforms[int(input.a_BoneIDs.x)] * input.a_BoneWeights.x;
    boneTransform += u_BoneTransforms[int(input.a_BoneIDs.y)] * input.a_BoneWeights.y;
    boneTransform += u_BoneTransforms[int(input.a_BoneIDs.z)] * input.a_BoneWeights.z;
    boneTransform += u_BoneTransforms[int(input.a_BoneIDs.w)] * input.a_BoneWeights.w;
    localPos = mul(boneTransform, localPos);
    }
    output.v_TexCoord = input.a_TexCoord;
    output.position = mul(u_ViewProjection, mul(u_Model, localPos));

    return output;
}
