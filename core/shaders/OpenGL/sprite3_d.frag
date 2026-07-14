
#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;
in vec3 v_WorldPos;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_Modulate;
uniform float u_AlphaCutoff;
uniform bool u_UseTexture;
uniform sampler2D u_Texture;

out vec4 FragColor;


    void main() {
        vec4 color = v_Color * u_Modulate;

        if (u_UseTexture) {
            vec4 texColor = texture(u_Texture, v_TexCoord);
            color *= texColor;

            if (u_AlphaCutoff > 0.0 && color.a < u_AlphaCutoff) {
                discard;
            }
        }

        FragColor = color;
    }
