
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

layout(push_constant) uniform PushConstants {
    mat4 u_ViewProjection;
    mat4 u_Model;
    mat4 u_NormalMatrix;
    vec4 u_TintColor;
} pc;

layout(set = 0, binding = 2) uniform MaterialData {
    mat4 u_View;
    vec3 u_CameraPosition;
    vec4 u_AlbedoColor;
    vec4 u_EmissiveColor;
    vec4 u_TextureFlags;
    vec4 u_MaterialParams1;
    vec4 u_TransparentParams;
    vec4 u_TransparentParams2;
    vec4 u_TransparentParams3;
    int u_ReceiveShadow;
} material;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_ViewPos;
layout(location = 2) out vec3 v_Normal;
layout(location = 3) out vec2 v_TexCoord;
layout(location = 4) out vec4 v_Color;
layout(location = 5) out vec3 v_ViewDir;
layout(location = 6) out vec4 v_ClipPos;

    // Custom fresnel for glass with adjustable power
    float fresnelEffect(float cosTheta, float power) {
        return pow(1.0 - cosTheta, power);
    }



    void main() {
        vec4 worldPos = (pc.u_Model * vec4(a_Position, 1.0));
        v_WorldPos = worldPos.xyz;
        v_ViewPos = (material.u_View * worldPos).xyz;
        v_Normal = (mat3(pc.u_NormalMatrix) * a_Normal);
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_ViewDir = normalize(material.u_CameraPosition.xyz - worldPos.xyz);
        vec4 clipPos = (pc.u_ViewProjection * worldPos);
        v_ClipPos = clipPos;
        gl_Position = clipPos;
    }
