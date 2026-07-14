#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include <string>

namespace lupine {

// Forward declarations
class IGfxDevice;

namespace components {

/**
 * WorldEnvironment Component
 * 
 * Manages global environment settings for a scene including:
 * - Skybox (color, procedural, cubemap, panoramic)
 * - Fog (linear, exponential)
 * - Ambient lighting
 * - Volumetric fog (disabled for OpenGL backend)
 */
class WorldEnvironment : public core::Component {
public:
    WorldEnvironment();
    virtual ~WorldEnvironment();

    // Component interface
    virtual std::string GetTypeName() const override { return "WorldEnvironment"; }
    virtual void DefineProperties() override;
    virtual void OnAwake() override;
    virtual void OnReady() override;
    virtual void OnRender() override;

    // Serialization
    virtual nlohmann::json Serialize() const override;
    virtual void Deserialize(const nlohmann::json& json) override;

    // Property change notification - handle side effects
    virtual void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ===== Skybox Settings =====
    
    enum class SkyboxType {
        None = 0,
        Color = 1,
        Procedural = 2,
        Cubemap = 3,
        Panoramic = 4
    };

    // Skybox type
    void SetSkyboxType(int type) { SetPropertyValue<int>("skyboxType", type); m_SkyboxNeedsUpdate = true; }
    int GetSkyboxType() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("skyboxType");
        return prop ? prop->GetValue<int>() : 0;
    }

    // Skybox color (for Color type)
    void SetSkyboxColor(const math::Color& color) { SetPropertyValue<math::Color>("skyboxColor", color); }
    math::Color GetSkyboxColor() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("skyboxColor");
        return prop ? prop->GetValue<math::Color>() : math::Color(0.5f, 0.7f, 1.0f, 1.0f);
    }

    // Skybox gradient colors (for Procedural type)
    void SetSkyTopColor(const math::Color& color) { SetPropertyValue<math::Color>("skyTopColor", color); }
    math::Color GetSkyTopColor() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("skyTopColor");
        return prop ? prop->GetValue<math::Color>() : math::Color(0.1f, 0.3f, 0.8f, 1.0f);
    }
    void SetSkyHorizonColor(const math::Color& color) { SetPropertyValue<math::Color>("skyHorizonColor", color); }
    math::Color GetSkyHorizonColor() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("skyHorizonColor");
        return prop ? prop->GetValue<math::Color>() : math::Color(0.6f, 0.7f, 0.9f, 1.0f);
    }
    void SetSkyBottomColor(const math::Color& color) { SetPropertyValue<math::Color>("skyBottomColor", color); }
    math::Color GetSkyBottomColor() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("skyBottomColor");
        return prop ? prop->GetValue<math::Color>() : math::Color(0.8f, 0.8f, 0.8f, 1.0f);
    }

    // Cubemap texture paths (for Cubemap type)
    void SetCubemapPosX(const std::string& path) { SetPropertyValue<std::string>("cubemapPosX", path); m_SkyboxNeedsUpdate = true; }
    std::string GetCubemapPosX() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("cubemapPosX");
        return prop ? prop->GetValue<std::string>() : "";
    }
    void SetCubemapNegX(const std::string& path) { SetPropertyValue<std::string>("cubemapNegX", path); m_SkyboxNeedsUpdate = true; }
    std::string GetCubemapNegX() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("cubemapNegX");
        return prop ? prop->GetValue<std::string>() : "";
    }
    void SetCubemapPosY(const std::string& path) { SetPropertyValue<std::string>("cubemapPosY", path); m_SkyboxNeedsUpdate = true; }
    std::string GetCubemapPosY() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("cubemapPosY");
        return prop ? prop->GetValue<std::string>() : "";
    }
    void SetCubemapNegY(const std::string& path) { SetPropertyValue<std::string>("cubemapNegY", path); m_SkyboxNeedsUpdate = true; }
    std::string GetCubemapNegY() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("cubemapNegY");
        return prop ? prop->GetValue<std::string>() : "";
    }
    void SetCubemapPosZ(const std::string& path) { SetPropertyValue<std::string>("cubemapPosZ", path); m_SkyboxNeedsUpdate = true; }
    std::string GetCubemapPosZ() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("cubemapPosZ");
        return prop ? prop->GetValue<std::string>() : "";
    }
    void SetCubemapNegZ(const std::string& path) { SetPropertyValue<std::string>("cubemapNegZ", path); m_SkyboxNeedsUpdate = true; }
    std::string GetCubemapNegZ() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("cubemapNegZ");
        return prop ? prop->GetValue<std::string>() : "";
    }

    // Panoramic texture path (for Panoramic type)
    void SetPanoramicTexture(const std::string& path) { SetPropertyValue<std::string>("panoramicTexture", path); m_SkyboxNeedsUpdate = true; }
    std::string GetPanoramicTexture() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("panoramicTexture");
        return prop ? prop->GetValue<std::string>() : "";
    }

    // ===== Fog Settings =====

    void SetFogEnabled(bool enabled) { SetPropertyValue<bool>("fogEnabled", enabled); }
    bool GetFogEnabled() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("fogEnabled");
        return prop ? prop->GetValue<bool>() : false;
    }

    void SetFogColor(const math::Color& color) { SetPropertyValue<math::Color>("fogColor", color); }
    math::Color GetFogColor() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("fogColor");
        return prop ? prop->GetValue<math::Color>() : math::Color(0.5f, 0.5f, 0.5f, 1.0f);
    }

    void SetFogDensity(float density) { SetPropertyValue<float>("fogDensity", density); }
    float GetFogDensity() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("fogDensity");
        return prop ? prop->GetValue<float>() : 0.01f;
    }

    void SetFogStart(float start) { SetPropertyValue<float>("fogStart", start); }
    float GetFogStart() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("fogStart");
        return prop ? prop->GetValue<float>() : 10.0f;
    }

    void SetFogEnd(float end) { SetPropertyValue<float>("fogEnd", end); }
    float GetFogEnd() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("fogEnd");
        return prop ? prop->GetValue<float>() : 100.0f;
    }

    enum class FogMode {
        Linear = 0,
        Exponential = 1,
        ExponentialSquared = 2
    };

    void SetFogMode(int mode) { SetPropertyValue<int>("fogMode", mode); }
    int GetFogMode() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("fogMode");
        return prop ? prop->GetValue<int>() : 0;
    }

    // ===== Ambient Light Settings =====

    void SetAmbientLightEnabled(bool enabled) { SetPropertyValue<bool>("ambientLightEnabled", enabled); }
    bool GetAmbientLightEnabled() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("ambientLightEnabled");
        return prop ? prop->GetValue<bool>() : true;
    }

    void SetAmbientLightColor(const math::Color& color) { SetPropertyValue<math::Color>("ambientLightColor", color); }
    math::Color GetAmbientLightColor() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("ambientLightColor");
        return prop ? prop->GetValue<math::Color>() : math::Color(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void SetAmbientLightIntensity(float intensity) { SetPropertyValue<float>("ambientLightIntensity", intensity); }
    float GetAmbientLightIntensity() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("ambientLightIntensity");
        return prop ? prop->GetValue<float>() : 0.2f;
    }

    // ===== Volumetric Fog Settings (disabled for OpenGL) =====

    void SetVolumetricFogEnabled(bool enabled) { SetPropertyValue<bool>("volumetricFogEnabled", enabled); }
    bool GetVolumetricFogEnabled() const {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty("volumetricFogEnabled");
        return prop ? prop->GetValue<bool>() : false;
    }

    // ===== Post-Processing Settings =====
    // A composable, WorldEnvironment-driven post-process chain. Because each view
    // (including SubViewport subtrees) gathers its own WorldEnvironment, these settings
    // give per-viewport post-processing automatically.

    enum class TonemapMode {
        Linear = 0,
        Reinhard = 1,
        ReinhardExtended = 2,
        ACES = 3,
        Filmic = 4,
        AGX = 5
    };

    enum class OverlayBlendMode {
        Normal = 0,
        Additive = 1,
        Multiply = 2,
        Screen = 3,
        Overlay = 4,
        SoftLight = 5
    };

    enum class FlipYMode {
        Auto = 0,
        Off = 1,
        On = 2
    };

    // Master toggle + tonemapping / exposure
    void SetPostProcessingEnabled(bool enabled) { SetPropertyValue<bool>("postProcessingEnabled", enabled); }
    bool GetPostProcessingEnabled() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("postProcessingEnabled"); return p ? p->GetValue<bool>() : false; }

    void SetTonemapMode(int mode) { SetPropertyValue<int>("tonemapMode", mode); }
    int GetTonemapMode() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("tonemapMode"); return p ? p->GetValue<int>() : 0; }

    void SetExposure(float v) { SetPropertyValue<float>("exposure", v); }
    float GetExposure() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("exposure"); return p ? p->GetValue<float>() : 1.0f; }

    void SetWhitePoint(float v) { SetPropertyValue<float>("whitePoint", v); }
    float GetWhitePoint() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("whitePoint"); return p ? p->GetValue<float>() : 4.0f; }

    // Bloom
    void SetBloomEnabled(bool v) { SetPropertyValue<bool>("bloomEnabled", v); }
    bool GetBloomEnabled() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("bloomEnabled"); return p ? p->GetValue<bool>() : false; }
    void SetBloomThreshold(float v) { SetPropertyValue<float>("bloomThreshold", v); }
    float GetBloomThreshold() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("bloomThreshold"); return p ? p->GetValue<float>() : 1.0f; }
    void SetBloomSoftKnee(float v) { SetPropertyValue<float>("bloomSoftKnee", v); }
    float GetBloomSoftKnee() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("bloomSoftKnee"); return p ? p->GetValue<float>() : 0.5f; }
    void SetBloomIntensity(float v) { SetPropertyValue<float>("bloomIntensity", v); }
    float GetBloomIntensity() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("bloomIntensity"); return p ? p->GetValue<float>() : 0.6f; }
    void SetBloomIterations(int v) { SetPropertyValue<int>("bloomIterations", v); }
    int GetBloomIterations() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("bloomIterations"); return p ? p->GetValue<int>() : 6; }

    // SSAO
    void SetSSAOEnabled(bool v) { SetPropertyValue<bool>("ssaoEnabled", v); }
    bool GetSSAOEnabled() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("ssaoEnabled"); return p ? p->GetValue<bool>() : false; }
    void SetSSAORadius(float v) { SetPropertyValue<float>("ssaoRadius", v); }
    float GetSSAORadius() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("ssaoRadius"); return p ? p->GetValue<float>() : 0.5f; }
    void SetSSAOIntensity(float v) { SetPropertyValue<float>("ssaoIntensity", v); }
    float GetSSAOIntensity() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("ssaoIntensity"); return p ? p->GetValue<float>() : 1.0f; }
    void SetSSAOBias(float v) { SetPropertyValue<float>("ssaoBias", v); }
    float GetSSAOBias() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("ssaoBias"); return p ? p->GetValue<float>() : 0.025f; }
    void SetSSAOSamples(int v) { SetPropertyValue<int>("ssaoSamples", v); }
    int GetSSAOSamples() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("ssaoSamples"); return p ? p->GetValue<int>() : 24; }
    void SetSSAOPower(float v) { SetPropertyValue<float>("ssaoPower", v); }
    float GetSSAOPower() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("ssaoPower"); return p ? p->GetValue<float>() : 1.5f; }

    // Color grading
    void SetColorGradingEnabled(bool v) { SetPropertyValue<bool>("colorGradingEnabled", v); }
    bool GetColorGradingEnabled() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("colorGradingEnabled"); return p ? p->GetValue<bool>() : false; }
    void SetContrast(float v) { SetPropertyValue<float>("contrast", v); }
    float GetContrast() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("contrast"); return p ? p->GetValue<float>() : 1.0f; }
    void SetSaturation(float v) { SetPropertyValue<float>("saturation", v); }
    float GetSaturation() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("saturation"); return p ? p->GetValue<float>() : 1.0f; }
    void SetBrightnessAdjust(float v) { SetPropertyValue<float>("brightness", v); }
    float GetBrightnessAdjust() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("brightness"); return p ? p->GetValue<float>() : 0.0f; }
    void SetTemperature(float v) { SetPropertyValue<float>("temperature", v); }
    float GetTemperature() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("temperature"); return p ? p->GetValue<float>() : 0.0f; }
    void SetTintAdjust(float v) { SetPropertyValue<float>("tint", v); }
    float GetTintAdjust() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("tint"); return p ? p->GetValue<float>() : 0.0f; }
    void SetColorFilter(const math::Color& c) { SetPropertyValue<math::Color>("colorFilter", c); }
    math::Color GetColorFilter() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("colorFilter"); return p ? p->GetValue<math::Color>() : math::Color(1.0f, 1.0f, 1.0f, 0.0f); }
    void SetColorLift(const math::Color& c) { SetPropertyValue<math::Color>("colorLift", c); }
    math::Color GetColorLift() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("colorLift"); return p ? p->GetValue<math::Color>() : math::Color(0.0f, 0.0f, 0.0f, 1.0f); }
    void SetColorGamma(const math::Color& c) { SetPropertyValue<math::Color>("colorGamma", c); }
    math::Color GetColorGamma() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("colorGamma"); return p ? p->GetValue<math::Color>() : math::Color(1.0f, 1.0f, 1.0f, 1.0f); }
    void SetColorGain(const math::Color& c) { SetPropertyValue<math::Color>("colorGain", c); }
    math::Color GetColorGain() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("colorGain"); return p ? p->GetValue<math::Color>() : math::Color(1.0f, 1.0f, 1.0f, 1.0f); }

    // Vignette
    void SetVignetteEnabled(bool v) { SetPropertyValue<bool>("vignetteEnabled", v); }
    bool GetVignetteEnabled() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("vignetteEnabled"); return p ? p->GetValue<bool>() : false; }
    void SetVignetteColor(const math::Color& c) { SetPropertyValue<math::Color>("vignetteColor", c); }
    math::Color GetVignetteColor() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("vignetteColor"); return p ? p->GetValue<math::Color>() : math::Color(0.0f, 0.0f, 0.0f, 1.0f); }
    void SetVignetteIntensity(float v) { SetPropertyValue<float>("vignetteIntensity", v); }
    float GetVignetteIntensity() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("vignetteIntensity"); return p ? p->GetValue<float>() : 0.4f; }
    void SetVignetteSmoothness(float v) { SetPropertyValue<float>("vignetteSmoothness", v); }
    float GetVignetteSmoothness() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("vignetteSmoothness"); return p ? p->GetValue<float>() : 0.5f; }
    void SetVignetteRoundness(float v) { SetPropertyValue<float>("vignetteRoundness", v); }
    float GetVignetteRoundness() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("vignetteRoundness"); return p ? p->GetValue<float>() : 1.0f; }
    void SetVignetteCenterX(float v) { SetPropertyValue<float>("vignetteCenterX", v); }
    float GetVignetteCenterX() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("vignetteCenterX"); return p ? p->GetValue<float>() : 0.5f; }
    void SetVignetteCenterY(float v) { SetPropertyValue<float>("vignetteCenterY", v); }
    float GetVignetteCenterY() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("vignetteCenterY"); return p ? p->GetValue<float>() : 0.5f; }

    // Chromatic aberration
    void SetChromaticAberrationEnabled(bool v) { SetPropertyValue<bool>("chromaticAberrationEnabled", v); }
    bool GetChromaticAberrationEnabled() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("chromaticAberrationEnabled"); return p ? p->GetValue<bool>() : false; }
    void SetChromaticAberrationAmount(float v) { SetPropertyValue<float>("chromaticAberrationAmount", v); }
    float GetChromaticAberrationAmount() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("chromaticAberrationAmount"); return p ? p->GetValue<float>() : 0.004f; }

    // Film grain
    void SetFilmGrainEnabled(bool v) { SetPropertyValue<bool>("filmGrainEnabled", v); }
    bool GetFilmGrainEnabled() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("filmGrainEnabled"); return p ? p->GetValue<bool>() : false; }
    void SetFilmGrainIntensity(float v) { SetPropertyValue<float>("filmGrainIntensity", v); }
    float GetFilmGrainIntensity() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("filmGrainIntensity"); return p ? p->GetValue<float>() : 0.08f; }
    void SetFilmGrainSize(float v) { SetPropertyValue<float>("filmGrainSize", v); }
    float GetFilmGrainSize() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("filmGrainSize"); return p ? p->GetValue<float>() : 1.0f; }

    // Overlay texture
    void SetOverlayTexturePath(const std::string& path) { SetPropertyValue<std::string>("overlayTexture", path); m_PostOverlayNeedsUpdate = true; }
    std::string GetOverlayTexturePath() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("overlayTexture"); return p ? p->GetValue<std::string>() : ""; }
    void SetOverlayBlendMode(int mode) { SetPropertyValue<int>("overlayBlendMode", mode); }
    int GetOverlayBlendMode() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("overlayBlendMode"); return p ? p->GetValue<int>() : 0; }
    void SetOverlayOpacity(float v) { SetPropertyValue<float>("overlayOpacity", v); }
    float GetOverlayOpacity() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("overlayOpacity"); return p ? p->GetValue<float>() : 1.0f; }

    // Backend orientation override (Auto/Off/On)
    void SetPostFlipYMode(int mode) { SetPropertyValue<int>("postFlipY", mode); }
    int GetPostFlipYMode() const { const core::ComponentProperty* p = m_CustomProperties.GetProperty("postFlipY"); return p ? p->GetValue<int>() : 0; }

    // Get GPU resources for rendering
    TextureHandle GetSkyboxTexture() const { return m_SkyboxTexture; }
    MaterialHandle GetSkyboxMaterial() const { return m_SkyboxMaterial; }
    MeshHandle GetSkyboxMesh() const { return m_SkyboxMesh; }

    // Ensure skybox resources are created (called by RenderWorld)
    void EnsureSkyboxResourcesCreated(IGfxDevice* device);

    // Ensure post-process resources (the overlay texture) are loaded/uploaded.
    // Called by RenderWorld when gathering post-process settings.
    void EnsurePostProcessResourcesCreated(IGfxDevice* device);

    // The uploaded overlay texture handle (invalid if no overlay is set).
    TextureHandle GetOverlayTextureHandle() const { return m_PostOverlayTexture; }

private:
    // Skybox state
    SkyboxType m_SkyboxType = SkyboxType::None;
    math::Color m_SkyboxColor = math::Color(0.5f, 0.7f, 1.0f, 1.0f);
    
    // Procedural skybox
    math::Color m_SkyTopColor = math::Color(0.1f, 0.3f, 0.8f, 1.0f);
    math::Color m_SkyHorizonColor = math::Color(0.6f, 0.7f, 0.9f, 1.0f);
    math::Color m_SkyBottomColor = math::Color(0.8f, 0.8f, 0.8f, 1.0f);
    
    // Cubemap paths
    std::string m_CubemapPosX;
    std::string m_CubemapNegX;
    std::string m_CubemapPosY;
    std::string m_CubemapNegY;
    std::string m_CubemapPosZ;
    std::string m_CubemapNegZ;
    
    // Panoramic texture
    std::string m_PanoramicTexture;

    // Fog state
    bool m_FogEnabled = false;
    math::Color m_FogColor = math::Color(0.5f, 0.5f, 0.5f, 1.0f);
    float m_FogDensity = 0.01f;
    float m_FogStart = 10.0f;
    float m_FogEnd = 100.0f;
    FogMode m_FogMode = FogMode::Linear;

    // Ambient light state
    bool m_AmbientLightEnabled = true;
    math::Color m_AmbientLightColor = math::Color(1.0f, 1.0f, 1.0f, 1.0f);
    float m_AmbientLightIntensity = 0.2f;

    // Volumetric fog (disabled for OpenGL)
    bool m_VolumetricFogEnabled = false;

    // GPU resources
    TextureHandle m_SkyboxTexture;
    MaterialHandle m_SkyboxMaterial;
    MeshHandle m_SkyboxMesh;

    // Asset references for cubemap/panoramic textures
    asset::AssetRef<asset::ImageAsset> m_CubemapAssets[6];  // +X, -X, +Y, -Y, +Z, -Z
    asset::AssetRef<asset::ImageAsset> m_PanoramicAsset;

    // Post-process overlay texture
    asset::AssetRef<asset::ImageAsset> m_PostOverlayAsset;
    TextureHandle m_PostOverlayTexture;
    std::string m_PostOverlayLoadedPath;  // path of the currently-uploaded overlay
    bool m_PostOverlayNeedsUpdate = true;

    // Update flags
    bool m_SkyboxNeedsUpdate = true;

    // Helper methods
    void LoadSkyboxTextures();
    void UploadSkyboxTextures(IGfxDevice* device);
    void CreateSkyboxMesh(IGfxDevice* device);
    void CreateSkyboxMaterial(IGfxDevice* device);
    void LoadOverlayTexture();
    void UploadOverlayTexture(IGfxDevice* device);
};

} // namespace components
} // namespace lupine

