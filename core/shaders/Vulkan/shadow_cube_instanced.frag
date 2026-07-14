
#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec3 v_WorldPos;

layout(location = 0) out float fragDepth;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    int u_HasAlbedoTexture;
    float u_AlphaCutoff;
    vec3 u_LightPos;
    float u_LightRange;
} material;

layout(set = 0, binding = 4) uniform sampler2D u_AlbedoTexture;


    void main() {
        if (material.u_HasAlbedoTexture == 1) {
            float alpha = texture(u_AlbedoTexture, v_TexCoord).a;
            if (alpha < material.u_AlphaCutoff) {
                discard;
            }
        }

        float lightDistance = length(v_WorldPos - material.u_LightPos);
        float linearDepth = lightDistance / material.u_LightRange;
        linearDepth = clamp(linearDepth, 0.0, 1.0);
        fragDepth = linearDepth;
    }
