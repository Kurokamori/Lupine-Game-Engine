// Text - Metal Shading Language
// 2D text rendering from font atlas

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
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
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
    out.position = (uniforms.u_ViewProjection * float4(in.a_Position, 1.0));

    return out;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    texture2d<float> u_FontAtlas [[texture(0)]],
    sampler u_FontAtlas_sampler [[sampler(0)]]
) {
    float4 FragColor;

    float alpha = u_FontAtlas.sample(u_FontAtlas_sampler, in.v_TexCoord).r;
    FragColor = float4(in.v_Color.rgb, in.v_Color.a * alpha);

    return FragColor;
}
