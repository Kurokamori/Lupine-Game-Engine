
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Texture;
uniform vec4 u_TintColor;
uniform bool u_UseTexture;

out vec4 FragColor;

void main() {
    vec4 color = v_Color * u_TintColor;
    if (u_UseTexture) {
        color *= texture(u_Texture, v_TexCoord);
    }
    FragColor = color;
}
