
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_FontAtlas;
uniform vec4 u_TextColor;

out vec4 FragColor;

void main() {
    // Sample the font atlas (single channel for alpha)
    float alpha = texture(u_FontAtlas, v_TexCoord).r;

    // Discard fully transparent pixels to avoid depth buffer issues
    if (alpha < 0.01) {
        discard;
    }

    // Combine text color with vertex color and font alpha
    vec4 finalColor = v_Color * u_TextColor;
    FragColor = vec4(finalColor.rgb, finalColor.a * alpha);
}
