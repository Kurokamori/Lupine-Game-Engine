
#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec3 v_WorldPos;

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    vec4 u_Modulate;
    float u_AlphaCutoff;
    bool u_UseTexture;
} material;

layout(set = 0, binding = 4) uniform sampler2D u_Texture;


    void main() {
        vec4 color = v_Color * material.u_Modulate;

        if (material.u_UseTexture) {
            vec4 texColor = texture(u_Texture, v_TexCoord);
            color *= texColor;

            if (material.u_AlphaCutoff > 0.0 && color.a < material.u_AlphaCutoff) {
                discard;
            }
        }

        FragColor = color;
    }
