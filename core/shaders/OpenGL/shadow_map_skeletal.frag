
#version 330 core

in vec2 v_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform bool u_UseSkinning;
uniform mat4 u_BoneTransforms[128];

out vec4 FragColor;


    void main() {
        FragColor = vec4(1.0);
    }
