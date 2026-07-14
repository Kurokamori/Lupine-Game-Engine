
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec4 a_Tangent;
layout(location = 5) in vec4 a_BoneIDs;
layout(location = 6) in vec4 a_BoneWeights;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform bool u_UseSkinning;
uniform mat4 u_BoneTransforms[128];

out vec2 v_TexCoord;



    void main() {
        vec4 localPos = vec4(a_Position, 1.0);

        if (u_UseSkinning) {
            mat4 boneTransform = u_BoneTransforms[int(a_BoneIDs.x)] * a_BoneWeights.x;
            boneTransform += u_BoneTransforms[int(a_BoneIDs.y)] * a_BoneWeights.y;
            boneTransform += u_BoneTransforms[int(a_BoneIDs.z)] * a_BoneWeights.z;
            boneTransform += u_BoneTransforms[int(a_BoneIDs.w)] * a_BoneWeights.w;
            localPos = (boneTransform * localPos);
        }

        v_TexCoord = a_TexCoord;
        gl_Position = (u_ViewProjection * (u_Model * localPos));
    }
