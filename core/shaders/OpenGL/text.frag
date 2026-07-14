
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform sampler2D u_FontAtlas;

out vec4 FragColor;


    void main() {
        float alpha = texture(u_FontAtlas, v_TexCoord).r;
        FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
    }
