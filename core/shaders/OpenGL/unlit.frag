
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;
uniform int u_UseTexture;
uniform vec4 u_UVRect;
uniform sampler2D u_Texture;

out vec4 FragColor;


    void main() {
        vec4 color = v_Color * u_TintColor;
        if (u_UseTexture != 0) {
            vec2 uv = mix(u_UVRect.xy, u_UVRect.zw, v_TexCoord);
            color *= texture(u_Texture, uv);
        }
        if (color.a < 0.01) {
            discard;
        }
        FragColor = color;
    }
