// DirectX12 ShadowMapSkeletal Fragment Shader

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

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    FragColor = float4(1.0, 1.0, 1.0, 1.0);

    return FragColor;
}
