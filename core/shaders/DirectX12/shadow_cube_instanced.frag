// DirectX12 ShadowCubeInstanced Fragment Shader

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
    float3 u_LightPos;
    float u_LightRange;
};

Texture2D u_AlbedoTexture : register(t0);
SamplerState u_AlbedoTexture_sampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
    float3 v_WorldPos : TEXCOORD1;
};

float main(PS_INPUT input) : SV_TARGET
{
    float fragDepth;

    if (u_HasAlbedoTexture == 1) {
    float alpha = u_AlbedoTexture.Sample(u_AlbedoTexture_sampler, input.v_TexCoord).a;
    if (alpha < u_AlphaCutoff) {
    discard;
    }
    }
    float lightDistance = length(input.v_WorldPos - u_LightPos);
    float linearDepth = lightDistance / u_LightRange;
    linearDepth = clamp(linearDepth, 0.0, 1.0);
    fragDepth = linearDepth;

    return fragDepth;
}
