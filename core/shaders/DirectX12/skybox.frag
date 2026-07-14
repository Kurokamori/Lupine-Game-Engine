// DirectX12 Skybox Fragment Shader

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
    int u_SkyboxType;
    float4 u_SkyboxColor;
    float4 u_SkyTopColor;
    float4 u_SkyHorizonColor;
    float4 u_SkyBottomColor;
};

TextureCube u_CubemapTexture : register(t0);
SamplerState u_CubemapTexture_sampler : register(s0);
Texture2D u_PanoramicTexture : register(t1);
SamplerState u_PanoramicTexture_sampler : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 v_Position : TEXCOORD0;
    float3 v_TexCoord3D : TEXCOORD1;
};


    float2 cartesianToSpherical(float3 dir) {
        float3 n = normalize(dir);
        float u = 0.5 + atan2(n.z, n.x) / (2.0 * 3.14159265359);
        float v = 0.5 + asin(n.y) / 3.14159265359;
        return float2(u, v);
    }


float4 main(PS_INPUT input) : SV_TARGET
{
    float4 FragColor;

    if (u_SkyboxType == 0) {
    FragColor = float4(0.0, 0.0, 0.0, 0.0);
    }
    else if (u_SkyboxType == 1) {
    FragColor = u_SkyboxColor;
    }
    else if (u_SkyboxType == 2) {
    float3 dir = normalize(input.v_Position);
    float height = dir.y;
    float4 color;
    if (height > 0.0) {
    color = lerp(u_SkyHorizonColor, u_SkyTopColor, height);
    } else {
    color = lerp(u_SkyBottomColor, u_SkyHorizonColor, height + 1.0);
    }
    FragColor = color;
    }
    else if (u_SkyboxType == 3) {
    FragColor = u_CubemapTexture.Sample(u_CubemapTexture_sampler, input.v_TexCoord3D);
    }
    else if (u_SkyboxType == 4) {
    float2 uv = cartesianToSpherical(input.v_Position);
    FragColor = u_PanoramicTexture.Sample(u_PanoramicTexture_sampler, uv);
    }
    else {
    FragColor = float4(1.0, 0.0, 1.0, 1.0);
    }

    return FragColor;
}
