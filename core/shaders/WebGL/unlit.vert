#version 300 es

precision highp float;
precision highp int;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;
uniform int u_UseTexture;
uniform vec4 u_UVRect;

out vec2 v_TexCoord;
out vec4 v_Color;



    void main() {
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        gl_Position = (u_ViewProjection * (u_Model * vec4(a_Position, 1.0)));
    }
