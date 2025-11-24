#pragma once

#include "ResourceHandles.hpp"
#include "lupine/math/Vec4.hpp"
#include "lupine/math/Color.hpp"
#include <string>

namespace lupine {

using math::Vec4;
using math::Color;

/**
 * PBR Material properties.
 * 
 * Supports physically-based rendering with:
 * - Albedo (base color) with optional texture
 * - Metallic workflow
 * - Roughness
 * - Normal mapping
 * - Emissive
 * 
 * This is designed to be extensible for future features like:
 * - Ambient occlusion
 * - Height/displacement mapping
 * - Subsurface scattering
 */
struct PBRMaterialProperties {
    // Albedo / Base Color
    Color albedoColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
    TextureHandle albedoTexture;
    bool useAlbedoTexture = false;
    
    // Metallic-Roughness
    float metallic = 0.0f;      // 0 = dielectric, 1 = metal
    float roughness = 0.5f;     // 0 = smooth, 1 = rough
    TextureHandle metallicRoughnessTexture; // R=unused, G=roughness, B=metallic (glTF 2.0 convention)
    bool useMetallicRoughnessTexture = false;
    
    // Normal Mapping
    TextureHandle normalTexture;
    bool useNormalTexture = false;
    float normalScale = 1.0f;
    
    // Emissive
    Color emissiveColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    TextureHandle emissiveTexture;
    bool useEmissiveTexture = false;
    float emissiveStrength = 1.0f;
    
    // Alpha
    float alphaCutoff = 0.5f;
    bool alphaBlend = false;
    
    // Future expansion slots
    TextureHandle aoTexture;           // Ambient occlusion
    bool useAOTexture = false;
    float aoStrength = 1.0f;
    
    TextureHandle heightTexture;       // Height/displacement
    bool useHeightTexture = false;
    float heightScale = 0.05f;
};

/**
 * GPU-side PBR material data.
 * Packed for efficient uniform buffer or push constant upload.
 * 
 * Memory layout (std140):
 * - Vec4 albedoColor (16 bytes)
 * - Vec4 emissiveColor (16 bytes)
 * - Vec4 params1 (16 bytes): x=metallic, y=roughness, z=normalScale, w=emissiveStrength
 * - Vec4 params2 (16 bytes): x=alphaCutoff, y=aoStrength, z=heightScale, w=unused
 * - Vec4 textureFlags (16 bytes): bitfield for texture usage
 * Total: 80 bytes
 */
struct alignas(16) GPUPBRMaterialData {
    Vec4 albedoColor;
    Vec4 emissiveColor;
    Vec4 params1; // metallic, roughness, normalScale, emissiveStrength
    Vec4 params2; // alphaCutoff, aoStrength, heightScale, unused
    Vec4 textureFlags; // x=albedo, y=metallicRoughness, z=normal, w=emissive (packed as floats for std140)
    
    GPUPBRMaterialData()
        : albedoColor(1.0f, 1.0f, 1.0f, 1.0f)
        , emissiveColor(0.0f, 0.0f, 0.0f, 1.0f)
        , params1(0.0f, 0.5f, 1.0f, 1.0f) // metallic=0, roughness=0.5, normalScale=1, emissiveStrength=1
        , params2(0.5f, 1.0f, 0.05f, 0.0f) // alphaCutoff=0.5, aoStrength=1, heightScale=0.05
        , textureFlags(0.0f, 0.0f, 0.0f, 0.0f)
    {}
};

/**
 * PBR Material descriptor.
 * High-level material definition for the rendering system.
 */
struct PBRMaterial {
    std::string name;
    
    // Material properties
    PBRMaterialProperties properties;
    
    // Shader handles (set by RenderWorld during material creation)
    ShaderHandle vertexShader;
    ShaderHandle fragmentShader;
    PipelineHandle pipeline;
    
    // Render state
    bool doubleSided = false;
    bool castShadows = true;
    bool receiveShadows = true;
    
    /**
     * Convert to GPU material data format
     */
    GPUPBRMaterialData toGPUData() const;
    
    /**
     * Get texture binding array for shader
     * Returns array of texture handles in binding order:
     * [0] = albedo, [1] = metallicRoughness, [2] = normal, [3] = emissive, [4] = ao, [5] = height
     */
    void getTextureBindings(TextureHandle* outTextures, uint32_t maxCount) const;
};

} // namespace lupine

