
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;
uniform vec4 u_Color;
uniform vec4 u_CornerRadius;
uniform vec2 u_Size;
uniform vec4 u_BorderWidth;
uniform bool u_EnableAntialiasing;

out vec2 v_TexCoord;
out vec4 v_Color;
out vec2 v_LocalPos;



    void main() {
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_LocalPos = a_Position.xy;
        gl_Position = (u_ViewProjection * (u_Model * vec4(a_Position, 1.0)));
    }
