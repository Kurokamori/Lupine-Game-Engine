// DirectX11 Text3D Fragment Shader

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
};

Texture2D u_FontAtlas : register(t0);
SamplerState u_FontAtlas_sampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
    float4 v_Color : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    float alpha = u_FontAtlas.Sample(u_FontAtlas_sampler, input.v_TexCoord).r;
    if (alpha < 0.01) {
    discard;
    }
    float4 finalColor = input.v_Color * u_TintColor;
    FragColor = float4(finalColor.rgb, finalColor.a * alpha);

    return FragColor;
}
