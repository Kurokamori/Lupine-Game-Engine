
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;
uniform sampler2D u_FontAtlas;

out vec4 FragColor;


    void main() {
        float alpha = texture(u_FontAtlas, v_TexCoord).r;

        if (alpha < 0.01) {
            discard;
        }

        vec4 finalColor = v_Color * u_TintColor;
        FragColor = vec4(finalColor.rgb, finalColor.a * alpha);
    }
