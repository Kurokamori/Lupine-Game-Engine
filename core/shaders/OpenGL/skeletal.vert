
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec3 a_Tangent;
layout(location = 5) in vec4 a_BoneIDs;
layout(location = 6) in vec4 a_BoneWeights;

const int MAX_BONES = 128;
uniform mat4 u_BoneTransforms[MAX_BONES];
uniform bool u_UseSkinning;

uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;
uniform vec3 u_CameraPosition;

out vec3 v_WorldPos;
out vec3 v_ViewPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Color;
out vec3 v_ViewDir;

void main() {
    vec4 localPos = vec4(a_Position, 1.0);
    vec3 localNormal = a_Normal;

    // Apply skeletal animation if enabled
    if (u_UseSkinning) {
        mat4 boneTransform = mat4(0.0);
        float totalWeight = 0.0;

        // Accumulate bone transformations weighted by bone weights
        for (int i = 0; i < 4; i++) {
            int boneID = int(a_BoneIDs[i]);
            float weight = a_BoneWeights[i];

            if (boneID >= 0 && boneID < MAX_BONES && weight > 0.0) {
                boneTransform += u_BoneTransforms[boneID] * weight;
                totalWeight += weight;
            }
        }

        // Only apply bone transformation if we have valid weights
        // If totalWeight is 0, the vertex has no bone influences and should use identity
        if (totalWeight > 0.0) {
            localPos = boneTransform * localPos;
            // Transform normals using inverse-transpose of bone transform to handle non-uniform scaling
            mat3 boneNormalMatrix = transpose(inverse(mat3(boneTransform)));
            localNormal = boneNormalMatrix * localNormal;
        }
    }

    // Transform to world space (applies to both skinned and non-skinned)
    vec4 worldPos = u_Model * localPos;
    v_WorldPos = worldPos.xyz;
    v_ViewPos = (u_View * worldPos).xyz;

    // Apply normal matrix to transform normals to world space
    // Don't normalize here - let the fragment shader handle it
    v_Normal = mat3(u_NormalMatrix) * localNormal;

    v_TexCoord = a_TexCoord;
    v_Color = a_Color;
    v_ViewDir = normalize(u_CameraPosition - v_WorldPos);

    gl_Position = u_ViewProjection * worldPos;
}

