// DirectX11 SkeletalToon Vertex Shader

cbuffer PushConstants : register(b0)
{
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    float4x4 u_NormalMatrix;
    float4 u_TintColor;
    int u_UseTexture;
    float u_AlphaCutoff;
    float _pad1;
    float _pad2;
    float4 u_UVRect;
    float4 u_TextureFlags;
    float4 u_MaterialParams1;
    float4 u_MaterialParams2;
    float4 u_CameraPosition;
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;
    int u_ReceiveShadow;
    float _pad3;
    float _pad4;
    float _pad5;
    // --- Shader-specific uniforms (set via setUniform*) ---
    float4x4 u_View;
    float4 u_ToonMaterialParams;
    float4 u_ToonMaterialParams2;
    float4 u_ToonParams;
    bool u_UseSkinning;
};

cbuffer BoneData : register(b2)
{
    float4x4 u_BoneTransforms[128];
};

struct VS_INPUT
{
    float3 a_Position : POSITION;
    float3 a_Normal : NORMAL;
    float2 a_TexCoord : TEXCOORD0;
    float4 a_Color : COLOR;
    float4 a_Tangent : TANGENT;
    float4 a_BoneIDs : BLENDINDICES;
    float4 a_BoneWeights : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 v_WorldPos : TEXCOORD0;
    float3 v_ViewPos : TEXCOORD1;
    float3 v_Normal : TEXCOORD2;
    float2 v_TexCoord : TEXCOORD3;
    float4 v_Color : TEXCOORD4;
    float3 v_ViewDir : TEXCOORD5;
};

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
    float calculateRimLighting(float3 N, float3 V, float rimPower, float rimIntensity) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        rim = pow(rim, rimPower) * rimIntensity;
        return rim;
    }

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 localPos = float4(input.a_Position, 1.0);
    float3 localNormal = input.a_Normal;
    if (u_UseSkinning) {
    float4x4 boneTransform = u_BoneTransforms[int(input.a_BoneIDs.x)] * input.a_BoneWeights.x;
    boneTransform += u_BoneTransforms[int(input.a_BoneIDs.y)] * input.a_BoneWeights.y;
    boneTransform += u_BoneTransforms[int(input.a_BoneIDs.z)] * input.a_BoneWeights.z;
    boneTransform += u_BoneTransforms[int(input.a_BoneIDs.w)] * input.a_BoneWeights.w;
    localPos = mul(boneTransform, localPos);
    localNormal = mul((float3x3)boneTransform, input.a_Normal);
    }
    float4 worldPos = mul(u_Model, localPos);
    output.v_WorldPos = worldPos.xyz;
    output.v_ViewPos = mul(u_View, worldPos).xyz;
    output.v_Normal = mul((float3x3)u_NormalMatrix, localNormal);
    output.v_TexCoord = input.a_TexCoord;
    output.v_Color = input.a_Color;
    output.v_ViewDir = normalize(u_CameraPosition.xyz - worldPos.xyz);
    output.position = mul(u_ViewProjection, worldPos);

    return output;
}
