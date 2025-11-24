
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;

out vec3 v_Position;
out vec3 v_TexCoord;

void main() {
    // Use position as texture coordinates for cubemap sampling
    v_TexCoord = a_Position;
    v_Position = a_Position;
    
    // Remove translation from view matrix to keep skybox centered on camera
    mat4 viewNoTranslation = u_View;
    viewNoTranslation[3][0] = 0.0;
    viewNoTranslation[3][1] = 0.0;
    viewNoTranslation[3][2] = 0.0;
    
    // Calculate position
    vec4 pos = u_ViewProjection * vec4(a_Position, 1.0);
    
    // Set z = w to ensure skybox is always at maximum depth
    gl_Position = pos.xyww;
}

