
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec2 v_TexCoord;
out vec4 v_Color;
out vec2 v_LocalPos;  // Position in quad space (-0.5 to 0.5)

void main() {
    v_TexCoord = a_TexCoord;
    v_Color = a_Color;
    v_LocalPos = a_Position.xy;  // Store local position for distance calculations
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
