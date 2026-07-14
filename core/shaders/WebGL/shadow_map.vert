#version 300 es

precision highp float;
precision highp int;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform int u_HasAlbedoTexture;
uniform float u_AlphaCutoff;

out vec2 v_TexCoord;



    void main() {
        v_TexCoord = a_TexCoord;
        gl_Position = (u_ViewProjection * (u_Model * vec4(a_Position, 1.0)));
    }
