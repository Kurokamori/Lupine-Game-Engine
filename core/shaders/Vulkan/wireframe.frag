
#version 450

layout(location = 0) in vec4 v_Color;

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
    vec4 u_TintColor;
} pc;


    void main() {
        FragColor = v_Color * pc.u_TintColor;
    }
