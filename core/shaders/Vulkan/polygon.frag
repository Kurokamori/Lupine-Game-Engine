
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
    bool u_UseTexture;
    vec2 u_Size;
    vec4 u_PolygonParams;
} material;

layout(set = 0, binding = 4) uniform sampler2D u_Texture;


    #define PI 3.14159265359

    float sdNgon(vec2 p, float r, float n) {
        float an = PI / n;
        float he = r * cos(an);
        float a = atan(p.y, p.x);
        a = abs(mod(a + an, 2.0 * an) - an);
        return length(p) * cos(a) - he;
    }

    void main() {
        float sides = material.u_PolygonParams.x;
        float rotation = material.u_PolygonParams.y;

        sides = max(3.0, sides);

        float cosR = cos(rotation);
        float sinR = sin(rotation);
        vec2 rotatedPos = vec2(
            v_LocalPos.x * cosR - v_LocalPos.y * sinR,
            v_LocalPos.x * sinR + v_LocalPos.y * cosR
        );

        vec2 scaledPos = rotatedPos * material.u_Size;

        float radius = min(material.u_Size.x, material.u_Size.y) * 0.5;

        float dist = sdNgon(scaledPos, radius, sides);

        float edgeSoftness = 1.5;
        float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);

        vec4 color = v_Color * pc.u_TintColor;

        if (material.u_UseTexture) {
            color *= texture(u_Texture, v_TexCoord);
        }

        color.a *= alpha;

        if (color.a < 0.01) {
            discard;
        }

        FragColor = color;
    }
