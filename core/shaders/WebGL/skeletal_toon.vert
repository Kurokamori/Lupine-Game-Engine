#version 300 es

precision highp float;
precision highp int;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec4 a_Tangent;
layout(location = 5) in vec4 a_BoneIDs;
layout(location = 6) in vec4 a_BoneWeights;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;
uniform vec3 u_CameraPosition;
uniform vec4 u_TintColor;
uniform vec4 u_AlbedoColor;
uniform vec4 u_EmissiveColor;
uniform vec4 u_TextureFlags;
uniform vec4 u_ToonMaterialParams;
uniform vec4 u_ToonMaterialParams2;
uniform vec4 u_ToonParams;
uniform int u_ReceiveShadow;
uniform bool u_UseSkinning;
uniform mat4 u_BoneTransforms[128];

out vec3 v_WorldPos;
out vec3 v_ViewPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Color;
out vec3 v_ViewDir;

    // Wrap NdotL from [-1, 1] to [0, 1] range for robust skeletal mesh lighting
    float wrapLighting(float NdotL) {
        return NdotL * 0.5 + 0.5;
    }

    // Cel shading: quantize value into discrete bands
    float celShade(float value, float bands, float threshold, float softness) {
        float adjustedValue = smoothstep(threshold - softness, threshold + softness, value);
        if (bands <= 1.0) {
            return adjustedValue;
        }
        float quantized = floor(adjustedValue * bands) / (bands - 1.0);
        return clamp(quantized, 0.0, 1.0);
    }

    // Improved cel shading with stacking bands
    float celShadeStacked(float value, float bands) {
        if (bands <= 1.0) {
            return step(0.5, value);
        }
        float quantized = floor(value * bands) / (bands - 1.0);
        return clamp(quantized, 0.0, 1.0);
    }

    // Calculate rim lighting (fresnel-based edge glow)
    float calculateRimLighting(vec3 N, vec3 V, float rimPower, float rimIntensity) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        rim = pow(rim, rimPower) * rimIntensity;
        return rim;
    }



    void main() {
        vec4 localPos = vec4(a_Position, 1.0);
        vec3 localNormal = a_Normal;

        if (u_UseSkinning) {
            mat4 boneTransform = u_BoneTransforms[int(a_BoneIDs.x)] * a_BoneWeights.x;
            boneTransform += u_BoneTransforms[int(a_BoneIDs.y)] * a_BoneWeights.y;
            boneTransform += u_BoneTransforms[int(a_BoneIDs.z)] * a_BoneWeights.z;
            boneTransform += u_BoneTransforms[int(a_BoneIDs.w)] * a_BoneWeights.w;
            localPos = (boneTransform * localPos);
            localNormal = (mat3(boneTransform) * a_Normal);
        }

        vec4 worldPos = (u_Model * localPos);
        v_WorldPos = worldPos.xyz;
        v_ViewPos = (u_View * worldPos).xyz;
        v_Normal = (mat3(u_NormalMatrix) * localNormal);
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_ViewDir = normalize(u_CameraPosition.xyz - worldPos.xyz);
        gl_Position = (u_ViewProjection * worldPos);
    }
