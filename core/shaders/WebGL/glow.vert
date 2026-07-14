#version 300 es

precision highp float;
precision highp int;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;
uniform vec3 u_CameraPosition;
uniform vec4 u_TintColor;
uniform vec4 u_AlbedoColor;
uniform vec4 u_EmissiveColor;
uniform vec4 u_TextureFlags;
uniform vec4 u_GlowParams;
uniform vec4 u_GlowParams2;
uniform vec4 u_GlowParams3;

out vec3 v_WorldPos;
out vec3 v_ViewPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Color;
out vec3 v_ViewDir;

    // Fresnel-based rim glow effect
    float fresnelGlow(vec3 N, vec3 V, float power) {
        float fresnel = 1.0 - max(dot(N, V), 0.0);
        return pow(fresnel, power);
    }

    // Color temperature shift (warm to cool)
    vec3 temperatureShift(vec3 color, float shift) {
        vec3 warmColor = vec3(1.0, 0.8, 0.6);
        vec3 coolColor = vec3(0.6, 0.8, 1.0);
        if (shift > 0.0) {
            return mix(color, color * warmColor, shift);
        } else {
            return mix(color, color * coolColor, -shift);
        }
    }



    void main() {
        vec4 worldPos = (u_Model * vec4(a_Position, 1.0));
        v_WorldPos = worldPos.xyz;
        v_ViewPos = (u_View * worldPos).xyz;
        v_Normal = (mat3(u_NormalMatrix) * a_Normal);
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_ViewDir = normalize(u_CameraPosition.xyz - worldPos.xyz);
        gl_Position = (u_ViewProjection * worldPos);
    }
