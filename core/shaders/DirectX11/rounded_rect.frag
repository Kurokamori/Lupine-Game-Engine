// DirectX11 RoundedRect Fragment Shader

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
    float4 u_CornerRadius;
    float2 u_Size;
    float2 _padSize;
    float4 u_MaterialParams2;
    float4 u_CameraPosition;
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;
    int u_ReceiveShadow;
    float _pad3;
    float _pad4;
    float _pad5;
};

Texture2D u_Texture : register(t0);
SamplerState u_Texture_sampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
    float4 v_Color : TEXCOORD1;
    float2 v_LocalPos : TEXCOORD2;
};


    float sdRoundedBox(float2 p, float2 size, float4 radius) {
        float2 pos = p * size;
        float2 halfSize = size * 0.5;

        float r;
        if (pos.x > 0.0) {
            r = (pos.y > 0.0) ? radius.z : radius.y;
        } else {
            r = (pos.y > 0.0) ? radius.w : radius.x;
        }

        r = min(r, min(halfSize.x, halfSize.y));

        float2 q = abs(pos) - halfSize + r;
        float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;

        return dist;
    }


float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    float dist = sdRoundedBox(input.v_LocalPos, u_Size, u_CornerRadius);
    float edgeSoftness = 1.5 / length(u_Size);
    float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);
    float4 color = input.v_Color * u_TintColor;
    if (u_UseTexture) {
    float2 uvMin = u_UVRect.xy;
    float2 uvMax = u_UVRect.zw;
    float2 remappedUV = uvMin + (input.v_TexCoord * (uvMax - uvMin));
    color *= u_Texture.Sample(u_Texture_sampler, remappedUV);
    }
    color.a *= alpha;
    if (color.a < 0.01) {
    discard;
    }
    FragColor = color;

    return FragColor;
}
