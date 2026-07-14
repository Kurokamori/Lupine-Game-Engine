// Wireframe - Metal Shading Language
// Simple wireframe rendering

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
    float4 v_Color;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    float4 u_TintColor;
};

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    uint vid [[vertex_id]],
    uint iid [[instance_id]]
) {
    VertexOut out;

    out.v_Color = in.a_Color;
    out.position = (uniforms.u_ViewProjection * (uniforms.u_Model * float4(in.a_Position, 1.0)));

    return out;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]]
) {
    float4 FragColor;

    FragColor = in.v_Color * uniforms.u_TintColor;

    return FragColor;
}
