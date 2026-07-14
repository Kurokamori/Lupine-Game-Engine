// RadialGradient - Metal Shading Language
// Radial gradient for UI effects with configurable inner/outer colors and radius

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
    float4x4 u_Model;
    float4 u_InnerColor;
    float4 u_OuterColor;
    float u_Radius;
    float2 u_Center;
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
    out.position = (uniforms.u_ViewProjection * (uniforms.u_Model * float4(in.a_Position, 1.0)));

    return out;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]]
) {
    float4 FragColor;

    float dist = distance(in.v_TexCoord, uniforms.u_Center);
    float t = clamp(dist / max(uniforms.u_Radius, 0.0001), 0.0, 1.0);
    float4 gradient = mix(uniforms.u_InnerColor, uniforms.u_OuterColor, t);
    FragColor = gradient * in.v_Color;

    return FragColor;
}
