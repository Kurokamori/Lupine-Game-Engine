
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    mat4 u_View;
    int u_SkyboxType;
    vec4 u_SkyboxColor;
    vec4 u_SkyTopColor;
    vec4 u_SkyHorizonColor;
    vec4 u_SkyBottomColor;
} material;

layout(location = 0) out vec3 v_Position;
layout(location = 1) out vec3 v_TexCoord3D;



    void main() {
        v_TexCoord3D = a_Position;
        v_Position = a_Position;
        vec4 pos = (pc.u_ViewProjection * vec4(a_Position, 1.0));
        gl_Position = vec4(pos.xy, pos.w, pos.w);
    }
