
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    vec4 u_InnerColor;
    vec4 u_OuterColor;
    float u_Radius;
    vec2 u_Center;
} material;

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec4 v_Color;



    void main() {
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        gl_Position = (pc.u_ViewProjection * (pc.u_Model * vec4(a_Position, 1.0)));
    }
