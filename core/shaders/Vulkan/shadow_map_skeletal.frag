
#version 450

layout(location = 0) in vec2 v_TexCoord;

layout(location = 0) out vec4 FragColor;

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


    void main() {
        FragColor = vec4(1.0);
    }
