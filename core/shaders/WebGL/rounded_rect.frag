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
uniform vec4 u_CornerRadius;
uniform vec2 u_Size;
uniform vec4 u_UVRect;
uniform sampler2D u_Texture;

out vec4 FragColor;


    float sdRoundedBox(vec2 p, vec2 size, vec4 radius) {
        vec2 pos = p * size;
        vec2 halfSize = size * 0.5;

        float r;
        if (pos.x > 0.0) {
            r = (pos.y > 0.0) ? radius.z : radius.y;
        } else {
            r = (pos.y > 0.0) ? radius.w : radius.x;
        }

        r = min(r, min(halfSize.x, halfSize.y));

        vec2 q = abs(pos) - halfSize + r;
        float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;

        return dist;
    }

    void main() {
        float dist = sdRoundedBox(v_LocalPos, u_Size, u_CornerRadius);

        float edgeSoftness = 1.5 / length(u_Size);
        float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);

        vec4 color = v_Color * u_TintColor;

        if (u_UseTexture) {
            vec2 uvMin = u_UVRect.xy;
            vec2 uvMax = u_UVRect.zw;
            vec2 remappedUV = uvMin + (v_TexCoord * (uvMax - uvMin));
            color *= texture(u_Texture, remappedUV);
        }

        color.a *= alpha;

        if (color.a < 0.01) {
            discard;
        }

        FragColor = color;
    }
