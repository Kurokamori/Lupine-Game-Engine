
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform int u_HasAlbedoTexture;
uniform float u_AlphaCutoff;
uniform vec3 u_LightPos;
uniform float u_LightRange;

out vec2 v_TexCoord;
out vec3 v_WorldPos;



    void main() {
        v_TexCoord = a_TexCoord;
        vec4 worldPos = (u_Model * vec4(a_Position, 1.0));
        v_WorldPos = worldPos.xyz;
        gl_Position = (u_ViewProjection * worldPos);
    }
