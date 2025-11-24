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

    // Get GPU resources for rendering
    TextureHandle GetSkyboxTexture() const { return m_SkyboxTexture; }
    MaterialHandle GetSkyboxMaterial() const { return m_SkyboxMaterial; }
    MeshHandle GetSkyboxMesh() const { return m_SkyboxMesh; }

    // Ensure skybox resources are created (called by RenderWorld)
    void EnsureSkyboxResourcesCreated(IGfxDevice* device);

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

    // Update flags
    bool m_SkyboxNeedsUpdate = true;

    // Helper methods
    void LoadSkyboxTextures();
    void UploadSkyboxTextures(IGfxDevice* device);
    void CreateSkyboxMesh(IGfxDevice* device);
    void CreateSkyboxMaterial(IGfxDevice* device);
};

} // namespace components
} // namespace lupine

