// Sprite3D - Metal Shading Language
// 3D sprite rendering with alpha cutoff

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
    float3 v_WorldPos;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    float4 u_Modulate;
    float u_AlphaCutoff;
    bool u_UseTexture;
};

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    uint vid [[vertex_id]],
    uint iid [[instance_id]]
) {
    VertexOut out;

    float4 worldPos = (uniforms.u_Model * float4(in.a_Position, 1.0));
    out.v_WorldPos = worldPos.xyz;
    out.v_TexCoord = in.a_TexCoord;
    out.v_Color = in.a_Color;
    out.position = (uniforms.u_ViewProjection * worldPos);

    return out;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    texture2d<float> u_Texture [[texture(0)]],
    sampler u_Texture_sampler [[sampler(0)]]
) {
    float4 FragColor;

    float4 color = in.v_Color * uniforms.u_Modulate;
    if (uniforms.u_UseTexture) {
    float4 texColor = u_Texture.sample(u_Texture_sampler, in.v_TexCoord);
    color *= texColor;
    if (uniforms.u_AlphaCutoff > 0.0 && color.a < uniforms.u_AlphaCutoff) {
    discard;
    }
    }
    FragColor = color;

    return FragColor;
}
