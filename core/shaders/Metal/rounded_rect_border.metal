// RoundedRectBorder - Metal Shading Language
// Rounded rectangle border with per-side width

#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 a_Position [[attribute(0)]];
    float3 a_Normal [[attribute(1)]];
    float2 a_TexCoord [[attribute(2)]];
    float4 a_Color [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 v_TexCoord;
    float4 v_Color;
    float2 v_LocalPos;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    float4 u_TintColor;
    float4 u_Color;
    float4 u_CornerRadius;
    float2 u_Size;
    float4 u_BorderWidth;
    bool u_EnableAntialiasing;
};

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    uint vid [[vertex_id]],
    uint iid [[instance_id]]
) {
    VertexOut out;

    out.v_TexCoord = in.a_TexCoord;
    out.v_Color = in.a_Color;
    out.v_LocalPos = in.a_Position.xy;
    out.position = (uniforms.u_ViewProjection * (uniforms.u_Model * float4(in.a_Position, 1.0)));

    return out;
}


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


fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]]
) {
    float4 FragColor;

    float2 pixelPos = in.v_LocalPos * uniforms.u_Size;
    float outerDist = sdRoundedBox(pixelPos, uniforms.u_Size, uniforms.u_CornerRadius);
    float edgeSoftness = uniforms.u_EnableAntialiasing ? 1.5 : 0.0;
    float outerAlpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, outerDist);
    float2 innerSize = uniforms.u_Size - float2(uniforms.u_BorderWidth.w + uniforms.u_BorderWidth.y, uniforms.u_BorderWidth.x + uniforms.u_BorderWidth.z);
    float innerAlpha = 1.0;
    if (innerSize.x > 0.0 && innerSize.y > 0.0) {
    float4 innerRadius = float4(
    max(0.0, uniforms.u_CornerRadius.x - min(uniforms.u_BorderWidth.x, uniforms.u_BorderWidth.w)),
    max(0.0, uniforms.u_CornerRadius.y - min(uniforms.u_BorderWidth.x, uniforms.u_BorderWidth.y)),
    max(0.0, uniforms.u_CornerRadius.z - min(uniforms.u_BorderWidth.z, uniforms.u_BorderWidth.y)),
    max(0.0, uniforms.u_CornerRadius.w - min(uniforms.u_BorderWidth.z, uniforms.u_BorderWidth.w))
    );
    float2 borderOffset = float2(
    (uniforms.u_BorderWidth.w - uniforms.u_BorderWidth.y) * 0.5,
    (uniforms.u_BorderWidth.x - uniforms.u_BorderWidth.z) * 0.5
    );
    float2 innerPos = pixelPos - borderOffset;
    float innerDist = sdRoundedBox(innerPos, innerSize, innerRadius);
    innerAlpha = smoothstep(-edgeSoftness, edgeSoftness, innerDist);
    }
    float alpha = outerAlpha * innerAlpha;
    float4 color;
    if (uniforms.u_Color.a > 0.0) {
    color = uniforms.u_Color;
    } else {
    color = in.v_Color * uniforms.u_TintColor;
    }
    color.a *= alpha;
    if (color.a < 0.01) {
    discard;
    }
    FragColor = color;

    return FragColor;
}
