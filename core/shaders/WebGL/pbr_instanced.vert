#version 300 es

precision highp float;
precision highp int;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec4 a_InstanceModel0;
layout(location = 5) in vec4 a_InstanceModel1;
layout(location = 6) in vec4 a_InstanceModel2;
layout(location = 7) in vec4 a_InstanceModel3;
layout(location = 8) in vec4 a_InstanceColor;
layout(location = 9) in vec4 a_InstanceCustom;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;
uniform vec3 u_CameraPosition;
uniform vec4 u_TintColor;
uniform vec4 u_AlbedoColor;
uniform vec4 u_EmissiveColor;
uniform vec4 u_MaterialParams1;
uniform vec4 u_MaterialParams2;
uniform vec4 u_TextureFlags;
uniform int u_ReceiveShadow;

out vec3 v_WorldPos;
out vec3 v_ViewPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Color;
out vec3 v_ViewDir;

    const float PI = 3.14159265359;

    vec3 fresnelSchlick(float cosTheta, vec3 F0) {
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }

    float distributionGGX(vec3 N, vec3 H, float roughness) {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH * NdotH;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        return a2 / denom;
    }

    float geometrySchlickGGX(float NdotV, float roughness) {
        float r = (roughness + 1.0);
        float k = (r * r) / 8.0;
        return NdotV / (NdotV * (1.0 - k) + k);
    }

    float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
    }



    void main() {
        // Reconstruct the per-instance world transform from its four columns by
        // column-scaled addition. This is backend-agnostic (no mat4 constructor
        // whose column/row order differs between GLSL and HLSL).
        vec4 worldPos = a_InstanceModel0 * a_Position.x
                      + a_InstanceModel1 * a_Position.y
                      + a_InstanceModel2 * a_Position.z
                      + a_InstanceModel3;
        v_WorldPos = worldPos.xyz;
        v_ViewPos = (u_View * worldPos).xyz;

        // Normal in world space using the instance basis (upper-left 3x3).
        vec3 worldNormal = a_InstanceModel0.xyz * a_Normal.x
                         + a_InstanceModel1.xyz * a_Normal.y
                         + a_InstanceModel2.xyz * a_Normal.z;
        v_Normal = normalize(worldNormal);

        v_TexCoord = a_TexCoord;
        v_Color = a_InstanceColor;
        v_ViewDir = normalize(u_CameraPosition.xyz - worldPos.xyz);
        gl_Position = (u_ViewProjection * worldPos);
    }
