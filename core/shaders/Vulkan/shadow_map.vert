
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    int u_HasAlbedoTexture;
    float u_AlphaCutoff;
} material;

layout(location = 0) out vec2 v_TexCoord;



    void main() {
        v_TexCoord = a_TexCoord;
        gl_Position = (pc.u_ViewProjection * (pc.u_Model * vec4(a_Position, 1.0)));
    }
