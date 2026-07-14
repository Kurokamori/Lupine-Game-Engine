
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 4) in vec4 a_InstanceModel0;
layout(location = 5) in vec4 a_InstanceModel1;
layout(location = 6) in vec4 a_InstanceModel2;
layout(location = 7) in vec4 a_InstanceModel3;
layout(location = 8) in vec4 a_InstanceColor;
layout(location = 9) in vec4 a_InstanceCustom;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    int u_HasAlbedoTexture;
    float u_AlphaCutoff;
} material;

layout(location = 0) out vec2 v_TexCoord;



    void main() {
        v_TexCoord = a_TexCoord;
        vec4 worldPos = a_InstanceModel0 * a_Position.x
                      + a_InstanceModel1 * a_Position.y
                      + a_InstanceModel2 * a_Position.z
                      + a_InstanceModel3;
        gl_Position = (pc.u_ViewProjection * worldPos);
    }
