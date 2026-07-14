
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_InnerColor;
uniform vec4 u_OuterColor;
uniform float u_Radius;
uniform vec2 u_Center;

out vec4 FragColor;


    void main() {
        float dist = distance(v_TexCoord, u_Center);
        float t = clamp(dist / max(u_Radius, 0.0001), 0.0, 1.0);
        vec4 gradient = mix(u_InnerColor, u_OuterColor, t);
        FragColor = gradient * v_Color;
    }
