// DirectX11 Polygon Fragment Shader

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
    float4 u_PolygonParams;
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


    #define PI 3.14159265359

    float sdNgon(float2 p, float r, float n) {
        float an = PI / n;
        float he = r * cos(an);
        float a = atan2(p.y, p.x);
        a = abs(((a + an) - (2.0 * an) * floor((a + an) / (2.0 * an))) - an);
        return length(p) * cos(a) - he;
    }


float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    float sides = u_PolygonParams.x;
    float rotation = u_PolygonParams.y;
    sides = max(3.0, sides);
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    float2 rotatedPos = float2(
    input.v_LocalPos.x * cosR - input.v_LocalPos.y * sinR,
    input.v_LocalPos.x * sinR + input.v_LocalPos.y * cosR
    );
    float2 scaledPos = rotatedPos * u_Size;
    float radius = min(u_Size.x, u_Size.y) * 0.5;
    float dist = sdNgon(scaledPos, radius, sides);
    float edgeSoftness = 1.5;
    float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);
    float4 color = input.v_Color * u_TintColor;
    if (u_UseTexture) {
    color *= u_Texture.Sample(u_Texture_sampler, input.v_TexCoord);
    }
    color.a *= alpha;
    if (color.a < 0.01) {
    discard;
    }
    FragColor = color;

    return FragColor;
}
