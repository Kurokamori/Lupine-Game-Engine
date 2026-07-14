
#version 330 core

in vec2 v_TexCoord;
in vec3 v_WorldPos;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform int u_HasAlbedoTexture;
uniform float u_AlphaCutoff;
uniform vec3 u_LightPos;
uniform float u_LightRange;
uniform sampler2D u_AlbedoTexture;

out float fragDepth;


    void main() {
        if (u_HasAlbedoTexture == 1) {
            float alpha = texture(u_AlbedoTexture, v_TexCoord).a;
            if (alpha < u_AlphaCutoff) {
                discard;
            }
        }

        float lightDistance = length(v_WorldPos - u_LightPos);
        float linearDepth = lightDistance / u_LightRange;
        linearDepth = clamp(linearDepth, 0.0, 1.0);
        fragDepth = linearDepth;
    }
