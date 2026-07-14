
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform int u_SkyboxType;
uniform vec4 u_SkyboxColor;
uniform vec4 u_SkyTopColor;
uniform vec4 u_SkyHorizonColor;
uniform vec4 u_SkyBottomColor;

out vec3 v_Position;
out vec3 v_TexCoord3D;



    void main() {
        v_TexCoord3D = a_Position;
        v_Position = a_Position;
        vec4 pos = (u_ViewProjection * vec4(a_Position, 1.0));
        gl_Position = vec4(pos.xy, pos.w, pos.w);
    }
