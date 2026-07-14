#version 300 es

precision highp float;
precision highp int;

in vec2 v_TexCoord;
in vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform vec4 u_TintColor;
uniform bool u_UseTexture;
uniform vec4 u_GradientParams;
uniform vec2 u_Size;
uniform vec4 u_UVRect;
uniform sampler2D u_Texture;

out vec4 FragColor;


    void main() {
        // u_GradientParams.x = falloff exponent, .y = inner radius (0..1 fraction)
        float falloff = max(u_GradientParams.x, 0.0001);
        float innerRadius = clamp(u_GradientParams.y, 0.0, 0.999);

        // Distance from the quad centre (TexCoord 0.5), normalised so the quad
        // edge midpoint maps to 1.0 and the centre to 0.0.
        float dist = clamp(length(v_TexCoord - vec2(0.5)) * 2.0, 0.0, 1.0);

        // Remap past the inner radius, then apply the falloff curve.
        float t = clamp((dist - innerRadius) / max(1.0 - innerRadius, 0.0001), 0.0, 1.0);
        float atten = pow(1.0 - t, falloff);

        vec4 color = v_Color * u_TintColor;

        if (u_UseTexture) {
            vec2 uvMin = u_UVRect.xy;
            vec2 uvMax = u_UVRect.zw;
            vec2 remappedUV = uvMin + (v_TexCoord * (uvMax - uvMin));
            color *= texture(u_Texture, remappedUV);
        }

        color.a *= atten;

        if (color.a < 0.001) {
            discard;
        }

        FragColor = color;
    }
