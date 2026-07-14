// RoundedRect - Metal Shading Language
// Rounded rectangle with per-corner radius and texture support

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
    bool u_UseTexture;
    float4 u_CornerRadius;
    float2 u_Size;
    float4 u_UVRect;
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


fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    texture2d<float> u_Texture [[texture(0)]],
    sampler u_Texture_sampler [[sampler(0)]]
) {
    float4 FragColor;

    float dist = sdRoundedBox(in.v_LocalPos, uniforms.u_Size, uniforms.u_CornerRadius);
    float edgeSoftness = 1.5 / length(uniforms.u_Size);
    float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);
    float4 color = in.v_Color * uniforms.u_TintColor;
    if (uniforms.u_UseTexture) {
    float2 uvMin = uniforms.u_UVRect.xy;
    float2 uvMax = uniforms.u_UVRect.zw;
    float2 remappedUV = uvMin + (in.v_TexCoord * (uvMax - uvMin));
    color *= u_Texture.sample(u_Texture_sampler, remappedUV);
    }
    color.a *= alpha;
    if (color.a < 0.01) {
    discard;
    }
    FragColor = color;

    return FragColor;
}
