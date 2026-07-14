
#version 450

layout(location = 0) in vec2 v_TexCoord;

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    int u_HasAlbedoTexture;
    float u_AlphaCutoff;
} material;

layout(set = 0, binding = 4) uniform sampler2D u_AlbedoTexture;


    void main() {
        if (material.u_HasAlbedoTexture == 1) {
            float alpha = texture(u_AlbedoTexture, v_TexCoord).a;
            if (alpha < material.u_AlphaCutoff) {
                discard;
            }
        }
        FragColor = vec4(1.0);
    }
