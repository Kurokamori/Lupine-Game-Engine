// Polygon - Metal Shading Language
// N-sided polygon with SDF, rotation, and anti-aliasing

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
    float2 u_Size;
    float4 u_PolygonParams;
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


    #define PI 3.14159265359

    float sdNgon(float2 p, float r, float n) {
        float an = PI / n;
        float he = r * cos(an);
        float a = atan(p.y, p.x);
        a = abs(((a + an) - (2.0 * an) * floor((a + an) / (2.0 * an))) - an);
        return length(p) * cos(a) - he;
    }


fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    texture2d<float> u_Texture [[texture(0)]],
    sampler u_Texture_sampler [[sampler(0)]]
) {
    float4 FragColor;

    float sides = uniforms.u_PolygonParams.x;
    float rotation = uniforms.u_PolygonParams.y;
    sides = max(3.0, sides);
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    float2 rotatedPos = float2(
    in.v_LocalPos.x * cosR - in.v_LocalPos.y * sinR,
    in.v_LocalPos.x * sinR + in.v_LocalPos.y * cosR
    );
    float2 scaledPos = rotatedPos * uniforms.u_Size;
    float radius = min(uniforms.u_Size.x, uniforms.u_Size.y) * 0.5;
    float dist = sdNgon(scaledPos, radius, sides);
    float edgeSoftness = 1.5;
    float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);
    float4 color = in.v_Color * uniforms.u_TintColor;
    if (uniforms.u_UseTexture) {
    color *= u_Texture.sample(u_Texture_sampler, in.v_TexCoord);
    }
    color.a *= alpha;
    if (color.a < 0.01) {
    discard;
    }
    FragColor = color;

    return FragColor;
}
