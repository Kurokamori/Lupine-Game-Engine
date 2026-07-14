// ShadowMapSkeletal - Metal Shading Language
// Shadow map for skeletal meshes with bone transforms

#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 a_Position [[attribute(0)]];
    float3 a_Normal [[attribute(1)]];
    float2 a_TexCoord [[attribute(2)]];
    float4 a_Color [[attribute(3)]];
    float4 a_Tangent [[attribute(4)]];
    float4 a_BoneIDs [[attribute(5)]];
    float4 a_BoneWeights [[attribute(6)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 v_TexCoord;
};

struct MaterialUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    bool u_UseSkinning;
    float4x4 u_BoneTransforms[128];
};

vertex VertexOut vertex_main(
    VertexIn in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]],
    uint vid [[vertex_id]],
    uint iid [[instance_id]]
) {
    VertexOut out;

    float4 localPos = float4(in.a_Position, 1.0);
    if (uniforms.u_UseSkinning) {
    float4x4 boneTransform = uniforms.u_BoneTransforms[int(in.a_BoneIDs.x)] * in.a_BoneWeights.x;
    boneTransform += uniforms.u_BoneTransforms[int(in.a_BoneIDs.y)] * in.a_BoneWeights.y;
    boneTransform += uniforms.u_BoneTransforms[int(in.a_BoneIDs.z)] * in.a_BoneWeights.z;
    boneTransform += uniforms.u_BoneTransforms[int(in.a_BoneIDs.w)] * in.a_BoneWeights.w;
    localPos = (boneTransform * localPos);
    }
    out.v_TexCoord = in.a_TexCoord;
    out.position = (uniforms.u_ViewProjection * (uniforms.u_Model * localPos));

    return out;
}

fragment float4 fragment_main(
    VertexOut in [[stage_in]],
    constant MaterialUniforms& uniforms [[buffer(16)]]
) {
    float4 FragColor;

    FragColor = float4(1.0, 1.0, 1.0, 1.0);

    return FragColor;
}
