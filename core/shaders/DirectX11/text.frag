// DirectX11 Text Fragment Shader

cbuffer PushConstants : register(b0)
{
    float4x4 u_ViewProjection;
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
    FragColor = float4(input.v_Color.rgb, input.v_Color.a * alpha);

    return FragColor;
}
