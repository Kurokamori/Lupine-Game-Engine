
#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
    vec4 u_TintColor;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    int u_UseTexture;
    float u_AlphaCutoff;
} material;

layout(set = 0, binding = 4) uniform sampler2D u_Texture;


    void main() {
        vec4 color = v_Color * pc.u_TintColor;
        if (material.u_UseTexture != 0) {
            color *= texture(u_Texture, v_TexCoord);
        }

        if (color.a < material.u_AlphaCutoff) {
            discard;
        }

        FragColor = color;
    }
