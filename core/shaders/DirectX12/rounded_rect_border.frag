// DirectX12 RoundedRectBorder Fragment Shader

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
    // --- Shader-specific uniforms (set via setUniform*) ---
    float4 u_Color;
    float4 u_BorderWidth;
    bool u_EnableAntialiasing;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 v_TexCoord : TEXCOORD0;
    float4 v_Color : TEXCOORD1;
    float2 v_LocalPos : TEXCOORD2;
};


    float sdRoundedBox(float2 p, float2 size, float4 radius) {
        float2 halfSize = size * 0.5;

        float r;
        if (p.x > 0.0) {
            r = (p.y > 0.0) ? radius.z : radius.y;
        } else {
            r = (p.y > 0.0) ? radius.w : radius.x;
        }

        r = min(r, min(halfSize.x, halfSize.y));

        float2 q = abs(p) - halfSize + r;
        float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;

        return dist;
    }


float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    float2 pixelPos = input.v_LocalPos * u_Size;
    float outerDist = sdRoundedBox(pixelPos, u_Size, u_CornerRadius);
    float edgeSoftness = u_EnableAntialiasing ? 1.5 : 0.0;
    float outerAlpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, outerDist);
    float2 innerSize = u_Size - float2(u_BorderWidth.w + u_BorderWidth.y, u_BorderWidth.x + u_BorderWidth.z);
    float innerAlpha = 1.0;
    if (innerSize.x > 0.0 && innerSize.y > 0.0) {
    float4 innerRadius = float4(
    max(0.0, u_CornerRadius.x - min(u_BorderWidth.x, u_BorderWidth.w)),
    max(0.0, u_CornerRadius.y - min(u_BorderWidth.x, u_BorderWidth.y)),
    max(0.0, u_CornerRadius.z - min(u_BorderWidth.z, u_BorderWidth.y)),
    max(0.0, u_CornerRadius.w - min(u_BorderWidth.z, u_BorderWidth.w))
    );
    float2 borderOffset = float2(
    (u_BorderWidth.w - u_BorderWidth.y) * 0.5,
    (u_BorderWidth.x - u_BorderWidth.z) * 0.5
    );
    float2 innerPos = pixelPos - borderOffset;
    float innerDist = sdRoundedBox(innerPos, innerSize, innerRadius);
    innerAlpha = smoothstep(-edgeSoftness, edgeSoftness, innerDist);
    }
    float alpha = outerAlpha * innerAlpha;
    float4 color;
    if (u_Color.a > 0.0) {
    color = u_Color;
    } else {
    color = input.v_Color * u_TintColor;
    }
    color.a *= alpha;
    if (color.a < 0.01) {
    discard;
    }
    FragColor = color;

    return FragColor;
}
