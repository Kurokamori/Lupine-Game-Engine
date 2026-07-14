
#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 FragColor;

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


    void main() {
        float dist = distance(v_TexCoord, material.u_Center);
        float t = clamp(dist / max(material.u_Radius, 0.0001), 0.0, 1.0);
        vec4 gradient = mix(material.u_InnerColor, material.u_OuterColor, t);
        FragColor = gradient * v_Color;
    }
