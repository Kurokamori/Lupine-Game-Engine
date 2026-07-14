#version 300 es

precision highp float;
precision highp int;

layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 4) in vec4 a_InstanceModel0;
layout(location = 5) in vec4 a_InstanceModel1;
layout(location = 6) in vec4 a_InstanceModel2;
layout(location = 7) in vec4 a_InstanceModel3;
layout(location = 8) in vec4 a_InstanceColor;
layout(location = 9) in vec4 a_InstanceCustom;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform int u_HasAlbedoTexture;
uniform float u_AlphaCutoff;

out vec2 v_TexCoord;



    void main() {
        v_TexCoord = a_TexCoord;
        vec4 worldPos = a_InstanceModel0 * a_Position.x
                      + a_InstanceModel1 * a_Position.y
                      + a_InstanceModel2 * a_Position.z
                      + a_InstanceModel3;
        gl_Position = (u_ViewProjection * worldPos);
    }
