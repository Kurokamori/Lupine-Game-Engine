#version 300 es

precision highp float;
precision highp int;

in vec2 v_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform int u_HasAlbedoTexture;
uniform float u_AlphaCutoff;
uniform sampler2D u_AlbedoTexture;

out vec4 FragColor;


    void main() {
        if (u_HasAlbedoTexture == 1) {
            float alpha = texture(u_AlbedoTexture, v_TexCoord).a;
            if (alpha < u_AlphaCutoff) {
                discard;
            }
        }
        FragColor = vec4(1.0);
    }
