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
uniform vec4 u_MaterialParams1;
uniform vec4 u_TransparentParams;
uniform vec4 u_TransparentParams2;
uniform vec4 u_TransparentParams3;
uniform int u_ReceiveShadow;

out vec3 v_WorldPos;
out vec3 v_ViewPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Color;
out vec3 v_ViewDir;
out vec4 v_ClipPos;

    // Custom fresnel for glass with adjustable power
    float fresnelEffect(float cosTheta, float power) {
        return pow(1.0 - cosTheta, power);
    }



    void main() {
        vec4 worldPos = (u_Model * vec4(a_Position, 1.0));
        v_WorldPos = worldPos.xyz;
        v_ViewPos = (u_View * worldPos).xyz;
        v_Normal = (mat3(u_NormalMatrix) * a_Normal);
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_ViewDir = normalize(u_CameraPosition.xyz - worldPos.xyz);
        vec4 clipPos = (u_ViewProjection * worldPos);
        v_ClipPos = clipPos;
        gl_Position = clipPos;
    }
