
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

out vec2 v_TexCoord;

uniform mat4 u_LightSpaceMatrix;
uniform mat4 u_Model;

void main() {
    vec4 localPos = vec4(a_Position, 1.0);
    
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
        if (totalWeight > 0.0) {
            localPos = boneTransform * localPos;
        }
    }
    
    v_TexCoord = a_TexCoord;
    gl_Position = u_LightSpaceMatrix * u_Model * localPos;
}


