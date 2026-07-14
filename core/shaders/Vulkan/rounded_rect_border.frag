
#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec2 v_LocalPos;

layout(location = 0) out vec4 FragColor;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
    vec4 u_TintColor;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    vec4 u_Color;
    vec4 u_CornerRadius;
    vec2 u_Size;
    vec4 u_BorderWidth;
    bool u_EnableAntialiasing;
} material;


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
        vec2 pixelPos = v_LocalPos * material.u_Size;

        float outerDist = sdRoundedBox(pixelPos, material.u_Size, material.u_CornerRadius);

        float edgeSoftness = material.u_EnableAntialiasing ? 1.5 : 0.0;
        float outerAlpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, outerDist);

        vec2 innerSize = material.u_Size - vec2(material.u_BorderWidth.w + material.u_BorderWidth.y, material.u_BorderWidth.x + material.u_BorderWidth.z);

        float innerAlpha = 1.0;

        if (innerSize.x > 0.0 && innerSize.y > 0.0) {
            vec4 innerRadius = vec4(
                max(0.0, material.u_CornerRadius.x - min(material.u_BorderWidth.x, material.u_BorderWidth.w)),
                max(0.0, material.u_CornerRadius.y - min(material.u_BorderWidth.x, material.u_BorderWidth.y)),
                max(0.0, material.u_CornerRadius.z - min(material.u_BorderWidth.z, material.u_BorderWidth.y)),
                max(0.0, material.u_CornerRadius.w - min(material.u_BorderWidth.z, material.u_BorderWidth.w))
            );

            vec2 borderOffset = vec2(
                (material.u_BorderWidth.w - material.u_BorderWidth.y) * 0.5,
                (material.u_BorderWidth.x - material.u_BorderWidth.z) * 0.5
            );

            vec2 innerPos = pixelPos - borderOffset;
            float innerDist = sdRoundedBox(innerPos, innerSize, innerRadius);

            innerAlpha = smoothstep(-edgeSoftness, edgeSoftness, innerDist);
        }

        float alpha = outerAlpha * innerAlpha;

        vec4 color;
        if (material.u_Color.a > 0.0) {
            color = material.u_Color;
        } else {
            color = v_Color * pc.u_TintColor;
        }
        color.a *= alpha;

        if (color.a < 0.01) {
            discard;
        }

        FragColor = color;
    }
