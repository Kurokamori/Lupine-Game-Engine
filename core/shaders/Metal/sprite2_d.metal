// Sprite2D - Metal Shading Language
// 2D sprite rendering with texture and tint color

#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 a_Position [[attribute(0)]];
    float2 a_TexCoord [[attribute(1)]];
    float4 a_Color [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 v_TexCoord;
    float4 v_Color;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    float4 u_TintColor;
    int u_UseTexture;
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
    constant MaterialUniforms& uniforms [[buffer(16)]],
    texture2d<float> u_Texture [[texture(0)]],
    sampler u_Texture_sampler [[sampler(0)]]
) {
    float4 FragColor;

    float4 color = in.v_Color * uniforms.u_TintColor;
    if (uniforms.u_UseTexture != 0) {
    color *= u_Texture.sample(u_Texture_sampler, in.v_TexCoord);
    }
    FragColor = color;

    return FragColor;
}
