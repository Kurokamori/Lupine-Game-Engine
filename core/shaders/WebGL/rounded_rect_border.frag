#version 300 es

precision highp float;
precision highp int;

in vec2 v_TexCoord;
in vec4 v_Color;
in vec2 v_LocalPos;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;
uniform vec4 u_Color;
uniform vec4 u_CornerRadius;
uniform vec2 u_Size;
uniform vec4 u_BorderWidth;
uniform bool u_EnableAntialiasing;

out vec4 FragColor;


    float sdRoundedBox(vec2 p, vec2 size, vec4 radius) {
        vec2 halfSize = size * 0.5;

        float r;
        if (p.x > 0.0) {
            r = (p.y > 0.0) ? radius.z : radius.y;
        } else {
            r = (p.y > 0.0) ? radius.w : radius.x;
        }

        r = min(r, min(halfSize.x, halfSize.y));

        vec2 q = abs(p) - halfSize + r;
        float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;

        return dist;
    }

    void main() {
        vec2 pixelPos = v_LocalPos * u_Size;

        float outerDist = sdRoundedBox(pixelPos, u_Size, u_CornerRadius);

        float edgeSoftness = u_EnableAntialiasing ? 1.5 : 0.0;
        float outerAlpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, outerDist);

        vec2 innerSize = u_Size - vec2(u_BorderWidth.w + u_BorderWidth.y, u_BorderWidth.x + u_BorderWidth.z);

        float innerAlpha = 1.0;

        if (innerSize.x > 0.0 && innerSize.y > 0.0) {
            vec4 innerRadius = vec4(
                max(0.0, u_CornerRadius.x - min(u_BorderWidth.x, u_BorderWidth.w)),
                max(0.0, u_CornerRadius.y - min(u_BorderWidth.x, u_BorderWidth.y)),
                max(0.0, u_CornerRadius.z - min(u_BorderWidth.z, u_BorderWidth.y)),
                max(0.0, u_CornerRadius.w - min(u_BorderWidth.z, u_BorderWidth.w))
            );

            vec2 borderOffset = vec2(
                (u_BorderWidth.w - u_BorderWidth.y) * 0.5,
                (u_BorderWidth.x - u_BorderWidth.z) * 0.5
            );

            vec2 innerPos = pixelPos - borderOffset;
            float innerDist = sdRoundedBox(innerPos, innerSize, innerRadius);

            innerAlpha = smoothstep(-edgeSoftness, edgeSoftness, innerDist);
        }

        float alpha = outerAlpha * innerAlpha;

        vec4 color;
        if (u_Color.a > 0.0) {
            color = u_Color;
        } else {
            color = v_Color * u_TintColor;
        }
        color.a *= alpha;

        if (color.a < 0.01) {
            discard;
        }

        FragColor = color;
    }
