
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
    mat4 u_NormalMatrix;
    vec4 u_TintColor;
} pc;

layout(location = 0) out vec4 v_Color;



    void main() {
        v_Color = a_Color;
        gl_Position = (pc.u_ViewProjection * (pc.u_Model * vec4(a_Position, 1.0)));
    }
