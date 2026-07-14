// ShadowCubeInstanced - Metal Shading Language
// GPU-instanced point light shadow cube map with linear depth (MultiMesh)

#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 a_Position [[attribute(0)]];
    float2 a_TexCoord [[attribute(2)]];
    float4 a_InstanceModel0 [[attribute(4)]];
    float4 a_InstanceModel1 [[attribute(5)]];
    float4 a_InstanceModel2 [[attribute(6)]];
    float4 a_InstanceModel3 [[attribute(7)]];
    float4 a_InstanceColor [[attribute(8)]];
    float4 a_InstanceCustom [[attribute(9)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 v_TexCoord;
    float3 v_WorldPos;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    int u_HasAlbedoTexture;
    float u_AlphaCutoff;
    float3 u_LightPos;
    float u_LightRange;
};

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    uint vid [[vertex_id]],
    uint iid [[instance_id]]
) {
    VertexOut out;

    out.v_TexCoord = in.a_TexCoord;
    float4 worldPos = in.a_InstanceModel0 * in.a_Position.x
    + in.a_InstanceModel1 * in.a_Position.y
    + in.a_InstanceModel2 * in.a_Position.z
    + in.a_InstanceModel3;
    out.v_WorldPos = worldPos.xyz;
    out.position = (uniforms.u_ViewProjection * worldPos);

    return out;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    texture2d<float> u_AlbedoTexture [[texture(0)]],
    sampler u_AlbedoTexture_sampler [[sampler(0)]]
) {
    float4 fragDepth;

    if (uniforms.u_HasAlbedoTexture == 1) {
    float alpha = u_AlbedoTexture.sample(u_AlbedoTexture_sampler, in.v_TexCoord).a;
    if (alpha < uniforms.u_AlphaCutoff) {
    discard;
    }
    }
    float lightDistance = length(in.v_WorldPos - uniforms.u_LightPos);
    float linearDepth = lightDistance / uniforms.u_LightRange;
    linearDepth = clamp(linearDepth, 0.0, 1.0);
    fragDepth = linearDepth;

    return fragDepth;
}
