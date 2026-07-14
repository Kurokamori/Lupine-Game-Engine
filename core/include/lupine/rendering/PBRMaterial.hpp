#pragma once

#include "ResourceHandles.hpp"
#include "lupine/math/Vec4.hpp"
#include "lupine/math/Color.hpp"
#include <string>

namespace lupine {

using math::Vec4;
using math::Color;

/**
 * Shader types for material rendering
 */
enum class ShaderType {
    PBR,            // Physically-based rendering (default)
    Toon,           // Cel/toon shading
    Stylized,       // Fantasy-style soft shading with shadow ramps and rim lighting
    Unlit,          // No lighting calculations
    Standard3D,     // Basic lit shader
    Transparent,    // Transparent/Glass with refraction and fresnel effects
    Glow,           // Emissive glow for stars, lights, magic effects
    Custom          // User-provided custom shader
};

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

    // Shader selection
    ShaderType shaderType = ShaderType::PBR;
    std::string customVertShaderPath;
    std::string customFragShaderPath;

    // Toon shader specific parameters
    float shadowBands = 3.0f;
    float shadowThreshold = 0.5f;
    float shadowSoftness = 0.02f;
    float specularBands = 2.0f;
    float specularPower = 32.0f;
    float rimIntensity = 0.0f;
    float rimPower = 3.0f;

    // Stylized shader specific parameters
    float stylizedShadowSoftness = 0.3f;     // How soft the shadow transitions are
    float stylizedSpecularSoftness = 0.15f;  // How soft the specular edge is
    float stylizedShadowBrightness = 0.4f;   // How bright shadows are (0-1)
    float stylizedShadowWarmth = 0.5f;       // Warm/cool color shift in shadows (0-1)
    float stylizedSpecularIntensity = 0.5f;  // Specular highlight intensity
    float stylizedHalfLambertPower = 1.5f;   // Controls wrap-around lighting softness

    // Transparent/Glass shader specific parameters
    float transparentOpacity = 0.5f;         // Base opacity (0 = fully transparent, 1 = opaque)
    float transparentRefractiveIndex = 1.5f; // Index of refraction (1.0 = air, 1.5 = glass, 2.4 = diamond)
    float transparentChromaticAberration = 0.0f; // Color separation amount (0 = none)
    float transparentFresnelPower = 5.0f;    // Fresnel effect strength at grazing angles
    float transparentReflectivity = 0.5f;    // How much environment is reflected
    float transparentRoughness = 0.0f;       // Surface roughness (0 = smooth glass)
    float transparentThickness = 1.0f;       // Glass thickness (affects refraction offset)

    // Glow/Emissive shader specific parameters
    float glowIntensity = 1.0f;              // Overall glow brightness
    float glowFalloff = 2.0f;                // How quickly glow fades from center
    float glowPulseSpeed = 0.0f;             // Animation speed (0 = no pulse)
    float glowPulseAmount = 0.0f;            // Pulse intensity variation
    float glowCoreSize = 0.3f;               // Size of bright core (0-1)
    float glowCoreBrightness = 2.0f;         // Core brightness multiplier
    float glowOuterGlow = 1.0f;              // Outer glow intensity
    float glowFresnelPower = 3.0f;           // Rim glow effect power
    float glowFresnelIntensity = 0.5f;       // Rim glow intensity
    float glowColorShift = 0.0f;             // Color temperature shift (-1 = cool, +1 = warm)
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

