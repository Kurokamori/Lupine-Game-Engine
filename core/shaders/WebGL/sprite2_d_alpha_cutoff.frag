#version 300 es

precision highp float;
precision highp int;

in vec2 v_TexCoord;
in vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;
uniform int u_UseTexture;
uniform float u_AlphaCutoff;
uniform sampler2D u_Texture;

out vec4 FragColor;


    void main() {
        vec4 color = v_Color * u_TintColor;
        if (u_UseTexture != 0) {
            color *= texture(u_Texture, v_TexCoord);
        }

        if (color.a < u_AlphaCutoff) {
            discard;
        }

        FragColor = color;
    }
