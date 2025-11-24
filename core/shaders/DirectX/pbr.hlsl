// pbr fragment shader for Direct X 11


// Textures
Texture2D u_AlbedoTexture;
Texture2D u_MetallicRoughnessTexture;
Texture2D u_NormalTexture;
Texture2D u_EmissiveTexture;
Texture2D u_AOTexture;

// Shadow map textures
Texture2D u_ShadowMap0;
Texture2D u_ShadowMap1;
Texture2D u_ShadowMap2;
Texture2D u_ShadowMap3;
Texture2D u_ShadowMap4;
Texture2D u_ShadowMap5;
Texture2D u_ShadowMap6;
Texture2D u_ShadowMap7;
Texture2D u_ShadowMap8;
Texture2D u_ShadowMap9;
Texture2D u_ShadowMap10;
Texture2D u_ShadowMap11;
Texture2D u_ShadowMap12;
Texture2D u_ShadowMap13;
Texture2D u_ShadowMap14;
Texture2D u_ShadowMap15;
Texture2D u_ShadowMap16;
Texture2D u_ShadowMap17;
Texture2D u_ShadowMap18;
Texture2D u_ShadowMap19;
Texture2D u_ShadowMap20;
Texture2D u_ShadowMap21;
Texture2D u_ShadowMap22;
Texture2D u_ShadowMap23;
Texture2D u_ShadowMap24;
Texture2D u_ShadowMap25;
Texture2D u_ShadowMap26;
Texture2D u_ShadowMap27;
Texture2D u_ShadowMap28;
Texture2D u_ShadowMap29;
Texture2D u_ShadowMap30;
Texture2D u_ShadowMap31;

// Common Sampler
SamplerState u_Sampler;

// Material properties
cbuffer MaterialData
{
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;
    // x: Metallic, y: Roughness, z: normalScale, w = emissiveStrength
    float4 u_MaterialParams1;
    // x = alphaCutoff, y = aoStrength, z = heightScale, w = unused
    float4 u_MaterialParams2;
}