// DirectX11 Sprite3D Fragment Shader

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
    float4 u_Modulate;
};

Texture2D u_Texture : register(t0);
SamplerState u_Texture_sampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
    float4 v_Color : TEXCOORD1;
    float3 v_WorldPos : TEXCOORD2;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    float4 color = input.v_Color * u_Modulate;
    if (u_UseTexture) {
    float4 texColor = u_Texture.Sample(u_Texture_sampler, input.v_TexCoord);
    color *= texColor;
    if (u_AlphaCutoff > 0.0 && color.a < u_AlphaCutoff) {
    discard;
    }
    }
    FragColor = color;

    return FragColor;
}
