
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec4 a_Tangent;
layout(location = 5) in vec4 a_BoneIDs;
layout(location = 6) in vec4 a_BoneWeights;

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
    vec4 u_ToonMaterialParams;
    vec4 u_ToonMaterialParams2;
    vec4 u_ToonParams;
    int u_ReceiveShadow;
    bool u_UseSkinning;
} material;

layout(std140, set = 0, binding = 1) uniform BoneData {
    mat4 u_BoneTransforms[128];
} bones;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_ViewPos;
layout(location = 2) out vec3 v_Normal;
layout(location = 3) out vec2 v_TexCoord;
layout(location = 4) out vec4 v_Color;
layout(location = 5) out vec3 v_ViewDir;

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

        if (material.u_UseSkinning) {
            mat4 boneTransform = bones.u_BoneTransforms[int(a_BoneIDs.x)] * a_BoneWeights.x;
            boneTransform += bones.u_BoneTransforms[int(a_BoneIDs.y)] * a_BoneWeights.y;
            boneTransform += bones.u_BoneTransforms[int(a_BoneIDs.z)] * a_BoneWeights.z;
            boneTransform += bones.u_BoneTransforms[int(a_BoneIDs.w)] * a_BoneWeights.w;
            localPos = (boneTransform * localPos);
            localNormal = (mat3(boneTransform) * a_Normal);
        }

        vec4 worldPos = (pc.u_Model * localPos);
        v_WorldPos = worldPos.xyz;
        v_ViewPos = (material.u_View * worldPos).xyz;
        v_Normal = (mat3(pc.u_NormalMatrix) * localNormal);
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_ViewDir = normalize(material.u_CameraPosition.xyz - worldPos.xyz);
        gl_Position = (pc.u_ViewProjection * worldPos);
    }
