
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec4 a_Tangent;
layout(location = 5) in vec4 a_BoneIDs;
layout(location = 6) in vec4 a_BoneWeights;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    bool u_UseSkinning;
} material;

layout(std140, set = 0, binding = 1) uniform BoneData {
    mat4 u_BoneTransforms[128];
} bones;

layout(location = 0) out vec2 v_TexCoord;



    void main() {
        vec4 localPos = vec4(a_Position, 1.0);

        if (material.u_UseSkinning) {
            mat4 boneTransform = bones.u_BoneTransforms[int(a_BoneIDs.x)] * a_BoneWeights.x;
            boneTransform += bones.u_BoneTransforms[int(a_BoneIDs.y)] * a_BoneWeights.y;
            boneTransform += bones.u_BoneTransforms[int(a_BoneIDs.z)] * a_BoneWeights.z;
            boneTransform += bones.u_BoneTransforms[int(a_BoneIDs.w)] * a_BoneWeights.w;
            localPos = (boneTransform * localPos);
        }

        v_TexCoord = a_TexCoord;
        gl_Position = (pc.u_ViewProjection * (pc.u_Model * localPos));
    }
