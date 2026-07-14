#version 300 es

precision highp float;
precision highp int;

in vec2 v_TexCoord;
in vec4 v_Color;
in vec2 v_LocalPos;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;
uniform bool u_UseTexture;
uniform vec2 u_Size;
uniform vec4 u_PolygonParams;
uniform sampler2D u_Texture;

out vec4 FragColor;


    #define PI 3.14159265359

    float sdNgon(vec2 p, float r, float n) {
        float an = PI / n;
        float he = r * cos(an);
        float a = atan(p.y, p.x);
        a = abs(mod(a + an, 2.0 * an) - an);
        return length(p) * cos(a) - he;
    }

    void main() {
        float sides = u_PolygonParams.x;
        float rotation = u_PolygonParams.y;

        sides = max(3.0, sides);

        float cosR = cos(rotation);
        float sinR = sin(rotation);
        vec2 rotatedPos = vec2(
            v_LocalPos.x * cosR - v_LocalPos.y * sinR,
            v_LocalPos.x * sinR + v_LocalPos.y * cosR
        );

        vec2 scaledPos = rotatedPos * u_Size;

        float radius = min(u_Size.x, u_Size.y) * 0.5;

        float dist = sdNgon(scaledPos, radius, sides);

        float edgeSoftness = 1.5;
        float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);

        vec4 color = v_Color * u_TintColor;

        if (u_UseTexture) {
            color *= texture(u_Texture, v_TexCoord);
        }

        color.a *= alpha;

        if (color.a < 0.01) {
            discard;
        }

        FragColor = color;
    }
