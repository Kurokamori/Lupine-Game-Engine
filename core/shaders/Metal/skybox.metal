// Skybox - Metal Shading Language
// Skybox with multiple modes: solid color, gradient, cubemap, panoramic

#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 a_Position [[attribute(0)]];
    float3 a_Normal [[attribute(1)]];
    float2 a_TexCoord [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 v_Position;
    float3 v_TexCoord3D;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_View;
    int u_SkyboxType;
    float4 u_SkyboxColor;
    float4 u_SkyTopColor;
    float4 u_SkyHorizonColor;
    float4 u_SkyBottomColor;
};

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    uint vid [[vertex_id]],
    uint iid [[instance_id]]
) {
    VertexOut out;

    out.v_TexCoord3D = in.a_Position;
    out.v_Position = in.a_Position;
    float4 pos = (uniforms.u_ViewProjection * float4(in.a_Position, 1.0));
    out.position = float4(pos.xy, pos.w, pos.w);

    return out;
}


    float2 cartesianToSpherical(float3 dir) {
        float3 n = normalize(dir);
        float u = 0.5 + atan(n.z, n.x) / (2.0 * 3.14159265359);
        float v = 0.5 + asin(n.y) / 3.14159265359;
        return float2(u, v);
    }


fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    texturecube<float> u_CubemapTexture [[texture(0)]],
    sampler u_CubemapTexture_sampler [[sampler(0)]],
    texture2d<float> u_PanoramicTexture [[texture(1)]],
    sampler u_PanoramicTexture_sampler [[sampler(1)]]
) {
    float4 FragColor;

    if (uniforms.u_SkyboxType == 0) {
    FragColor = float4(0.0, 0.0, 0.0, 0.0);
    }
    else if (uniforms.u_SkyboxType == 1) {
    FragColor = uniforms.u_SkyboxColor;
    }
    else if (uniforms.u_SkyboxType == 2) {
    float3 dir = normalize(in.v_Position);
    float height = dir.y;
    float4 color;
    if (height > 0.0) {
    color = mix(uniforms.u_SkyHorizonColor, uniforms.u_SkyTopColor, height);
    } else {
    color = mix(uniforms.u_SkyBottomColor, uniforms.u_SkyHorizonColor, height + 1.0);
    }
    FragColor = color;
    }
    else if (uniforms.u_SkyboxType == 3) {
    FragColor = u_CubemapTexture.sample(u_CubemapTexture_sampler, in.v_TexCoord3D);
    }
    else if (uniforms.u_SkyboxType == 4) {
    float2 uv = cartesianToSpherical(in.v_Position);
    FragColor = u_PanoramicTexture.sample(u_PanoramicTexture_sampler, uv);
    }
    else {
    FragColor = float4(1.0, 0.0, 1.0, 1.0);
    }

    return FragColor;
}
