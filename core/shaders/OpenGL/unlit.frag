
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Texture;
uniform vec4 u_TintColor;
uniform bool u_UseTexture;
uniform vec4 u_UVRect;  // (min_u, min_v, max_u, max_v)

out vec4 FragColor;

void main() {
    vec4 color = v_Color * u_TintColor;
    if (u_UseTexture) {
        // Apply UV rect transformation
        vec2 uv = v_TexCoord;
        uv = mix(u_UVRect.xy, u_UVRect.zw, uv);

        vec4 texColor = texture(u_Texture, uv);
        color *= texColor;
    }

    // Ensure we output something visible
    if (color.a < 0.01) {
        discard;
    }

    FragColor = color;
}
