
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
    vec4 u_CornerRadius;
    vec2 u_Size;
    vec4 u_UVRect;
} material;

layout(set = 0, binding = 4) uniform sampler2D u_Texture;


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
        float dist = sdRoundedBox(v_LocalPos, material.u_Size, material.u_CornerRadius);

        float edgeSoftness = 1.5 / length(material.u_Size);
        float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);

        vec4 color = v_Color * pc.u_TintColor;

        if (material.u_UseTexture) {
            vec2 uvMin = material.u_UVRect.xy;
            vec2 uvMax = material.u_UVRect.zw;
            vec2 remappedUV = uvMin + (v_TexCoord * (uvMax - uvMin));
            color *= texture(u_Texture, remappedUV);
        }

        color.a *= alpha;

        if (color.a < 0.01) {
            discard;
        }

        FragColor = color;
    }
