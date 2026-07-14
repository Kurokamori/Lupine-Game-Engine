#include "lupine/components/WorldEnvironment.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/DefaultShaders.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include <algorithm>
#include <cstring>

namespace lupine {
namespace components {

WorldEnvironment::WorldEnvironment()
    : Component() {
}

WorldEnvironment::~WorldEnvironment() {
}

void WorldEnvironment::DefineProperties() {

    DefineProperty(PROPERTY_ENUM_GROUP(skyboxType, 0, "Skybox", None, Color, Procedural, Cubemap, Panoramic));
    DefineProperty(PROPERTY_DEFAULT_GROUP(skyboxColor, Color, math::Color(0.5f, 0.7f, 1.0f, 1.0f), "Skybox"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(skyTopColor, Color, math::Color(0.1f, 0.3f, 0.8f, 1.0f), "Skybox"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(skyHorizonColor, Color, math::Color(0.6f, 0.7f, 0.9f, 1.0f), "Skybox"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(skyBottomColor, Color, math::Color(0.8f, 0.8f, 0.8f, 1.0f), "Skybox"));

    DefineProperty(PROPERTY_FILE_GROUP(cubemapPosX, std::string(""), "*.png,*.jpg,*.jpeg,*.hdr,*.exr", "Skybox"));
    DefineProperty(PROPERTY_FILE_GROUP(cubemapNegX, std::string(""), "*.png,*.jpg,*.jpeg,*.hdr,*.exr", "Skybox"));
    DefineProperty(PROPERTY_FILE_GROUP(cubemapPosY, std::string(""), "*.png,*.jpg,*.jpeg,*.hdr,*.exr", "Skybox"));
    DefineProperty(PROPERTY_FILE_GROUP(cubemapNegY, std::string(""), "*.png,*.jpg,*.jpeg,*.hdr,*.exr", "Skybox"));
    DefineProperty(PROPERTY_FILE_GROUP(cubemapPosZ, std::string(""), "*.png,*.jpg,*.jpeg,*.hdr,*.exr", "Skybox"));
    DefineProperty(PROPERTY_FILE_GROUP(cubemapNegZ, std::string(""), "*.png,*.jpg,*.jpeg,*.hdr,*.exr", "Skybox"));

    DefineProperty(PROPERTY_FILE_GROUP(panoramicTexture, std::string(""), "*.png,*.jpg,*.jpeg,*.hdr,*.exr", "Skybox"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(fogEnabled, Bool, false, "Fog"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fogColor, Color, math::Color(0.5f, 0.5f, 0.5f, 1.0f), "Fog"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fogDensity, 0.01f, 0.0f, 1.0f, 0.001f, "Fog"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fogStart, 10.0f, 0.0f, 1000.0f, 1.0f, "Fog"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fogEnd, 100.0f, 0.0f, 10000.0f, 1.0f, "Fog"));
    DefineProperty(PROPERTY_ENUM_GROUP(fogMode, 0, "Fog", Linear, Exponential, ExponentialSquared));

    DefineProperty(PROPERTY_DEFAULT_GROUP(ambientLightEnabled, Bool, true, "Ambient Light"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(ambientLightColor, Color, math::Color(1.0f, 1.0f, 1.0f, 1.0f), "Ambient Light"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(ambientLightIntensity, 0.2f, 0.0f, 10.0f, 0.01f, "Ambient Light"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(volumetricFogEnabled, Bool, false, "Volumetric Fog"));

    // ===== Post-Processing =====
    DefineProperty(PROPERTY_DEFAULT_GROUP(postProcessingEnabled, Bool, false, "Post Processing"));
    DefineProperty(PROPERTY_ENUM_GROUP(tonemapMode, 0, "Post Processing", Linear, Reinhard, ReinhardExtended, ACES, Filmic, AGX));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(exposure, 1.0f, 0.0f, 8.0f, 0.01f, "Post Processing"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(whitePoint, 4.0f, 0.1f, 16.0f, 0.1f, "Post Processing"));
    DefineProperty(PROPERTY_ENUM_GROUP(postFlipY, 0, "Advanced", Auto, Off, On));

    // Bloom
    DefineProperty(PROPERTY_DEFAULT_GROUP(bloomEnabled, Bool, false, "Bloom"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(bloomThreshold, 1.0f, 0.0f, 10.0f, 0.01f, "Bloom"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(bloomSoftKnee, 0.5f, 0.0f, 1.0f, 0.01f, "Bloom"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(bloomIntensity, 0.6f, 0.0f, 5.0f, 0.01f, "Bloom"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(bloomIterations, 6, 1, 8, 1, "Bloom"));

    // SSAO
    DefineProperty(PROPERTY_DEFAULT_GROUP(ssaoEnabled, Bool, false, "SSAO"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(ssaoRadius, 0.5f, 0.01f, 5.0f, 0.01f, "SSAO"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(ssaoIntensity, 1.0f, 0.0f, 4.0f, 0.01f, "SSAO"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(ssaoBias, 0.025f, 0.0f, 0.5f, 0.001f, "SSAO"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(ssaoSamples, 24, 1, 64, 1, "SSAO"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(ssaoPower, 1.5f, 0.1f, 8.0f, 0.1f, "SSAO"));

    // Color grading
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorGradingEnabled, Bool, false, "Color Grading"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(contrast, 1.0f, 0.0f, 2.0f, 0.01f, "Color Grading"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(saturation, 1.0f, 0.0f, 2.0f, 0.01f, "Color Grading"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(brightness, 0.0f, -1.0f, 1.0f, 0.01f, "Color Grading"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(temperature, 0.0f, -1.0f, 1.0f, 0.01f, "Color Grading"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(tint, 0.0f, -1.0f, 1.0f, 0.01f, "Color Grading"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorFilter, Color, math::Color(1.0f, 1.0f, 1.0f, 0.0f), "Color Grading"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorLift, Color, math::Color(0.0f, 0.0f, 0.0f, 1.0f), "Color Grading"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorGamma, Color, math::Color(1.0f, 1.0f, 1.0f, 1.0f), "Color Grading"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(colorGain, Color, math::Color(1.0f, 1.0f, 1.0f, 1.0f), "Color Grading"));

    // Vignette
    DefineProperty(PROPERTY_DEFAULT_GROUP(vignetteEnabled, Bool, false, "Vignette"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(vignetteColor, Color, math::Color(0.0f, 0.0f, 0.0f, 1.0f), "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(vignetteIntensity, 0.4f, 0.0f, 2.0f, 0.01f, "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(vignetteSmoothness, 0.5f, 0.0f, 1.0f, 0.01f, "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(vignetteRoundness, 1.0f, 0.0f, 1.0f, 0.01f, "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(vignetteCenterX, 0.5f, 0.0f, 1.0f, 0.01f, "Vignette"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(vignetteCenterY, 0.5f, 0.0f, 1.0f, 0.01f, "Vignette"));

    // Chromatic aberration
    DefineProperty(PROPERTY_DEFAULT_GROUP(chromaticAberrationEnabled, Bool, false, "Chromatic Aberration"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(chromaticAberrationAmount, 0.004f, 0.0f, 0.05f, 0.0005f, "Chromatic Aberration"));

    // Film grain
    DefineProperty(PROPERTY_DEFAULT_GROUP(filmGrainEnabled, Bool, false, "Film Grain"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(filmGrainIntensity, 0.08f, 0.0f, 1.0f, 0.01f, "Film Grain"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(filmGrainSize, 1.0f, 0.5f, 8.0f, 0.1f, "Film Grain"));

    // Overlay
    DefineProperty(PROPERTY_FILE_GROUP(overlayTexture, std::string(""), "*.png,*.jpg,*.jpeg,*.tga", "Overlay"));
    DefineProperty(PROPERTY_ENUM_GROUP(overlayBlendMode, 0, "Overlay", Normal, Additive, Multiply, Screen, Overlay, SoftLight));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(overlayOpacity, 1.0f, 0.0f, 1.0f, 0.01f, "Overlay"));
}

void WorldEnvironment::OnAwake() {

    if (m_SkyboxNeedsUpdate) {
        LoadSkyboxTextures();
    }
}

void WorldEnvironment::OnReady() {

}

void WorldEnvironment::OnRender() {

}

nlohmann::json WorldEnvironment::Serialize() const {

    return Component::Serialize();
}

void WorldEnvironment::Deserialize(const nlohmann::json& json) {

    Component::Deserialize(json);

    m_SkyboxNeedsUpdate = true;
}

void WorldEnvironment::OnPropertyChanged(const std::string& propertyName, const nlohmann::json&) {

    if (propertyName.find("skybox") != std::string::npos ||
        propertyName.find("cubemap") != std::string::npos ||
        propertyName.find("panoramic") != std::string::npos) {
        m_SkyboxNeedsUpdate = true;
        LoadSkyboxTextures();
    }

    if (propertyName == "overlayTexture") {
        m_PostOverlayNeedsUpdate = true;
        LoadOverlayTexture();
    }
}

void WorldEnvironment::EnsurePostProcessResourcesCreated(IGfxDevice* device) {
    if (!device) {
        return;
    }

    const std::string path = GetOverlayTexturePath();
    if (path.empty()) {
        if (m_PostOverlayTexture.isValid()) {
            device->destroyTexture(m_PostOverlayTexture);
            m_PostOverlayTexture = TextureHandle();
        }
        m_PostOverlayAsset.Reset();
        m_PostOverlayLoadedPath.clear();
        m_PostOverlayNeedsUpdate = false;
        return;
    }

    if (m_PostOverlayNeedsUpdate || m_PostOverlayLoadedPath != path) {
        if (!m_PostOverlayAsset.IsValid() || m_PostOverlayLoadedPath != path) {
            LoadOverlayTexture();
        }
        UploadOverlayTexture(device);
        m_PostOverlayNeedsUpdate = false;
    }
}

void WorldEnvironment::LoadOverlayTexture() {
    const std::string path = GetOverlayTexturePath();
    if (path.empty()) {
        m_PostOverlayAsset.Reset();
        return;
    }
    // Overlay art is sampled in display space (after tonemapping), so load it linearly
    // to avoid an extra gamma decode.
    m_PostOverlayAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_PostOverlayAsset->LoadFromFile(path, true, asset::ImageColorSpace::Linear);
    if (!loaded) {
        m_PostOverlayAsset.Reset();
    }
}

void WorldEnvironment::UploadOverlayTexture(IGfxDevice* device) {
    if (!device || !m_PostOverlayAsset.IsValid() || !m_PostOverlayAsset->IsLoaded()) {
        return;
    }

    if (m_PostOverlayTexture.isValid()) {
        device->destroyTexture(m_PostOverlayTexture);
        m_PostOverlayTexture = TextureHandle();
    }

    TextureDesc desc;
    desc.type = TextureType::Texture2D;
    desc.width = m_PostOverlayAsset->GetWidth();
    desc.height = m_PostOverlayAsset->GetHeight();
    desc.format = TextureFormat::RGBA8_UNORM;
    desc.usage = TextureUsage::Sampled;
    desc.initialData = m_PostOverlayAsset->GetData();
    m_PostOverlayTexture = device->createTexture(desc);
    m_PostOverlayLoadedPath = GetOverlayTexturePath();
}

void WorldEnvironment::EnsureSkyboxResourcesCreated(IGfxDevice* device) {
    if (!device) {
        return;
    }

    if (!m_SkyboxMesh.isValid()) {
        CreateSkyboxMesh(device);

    }

    SkyboxType skyboxType = static_cast<SkyboxType>(GetSkyboxType());
    if (m_SkyboxNeedsUpdate && (skyboxType == SkyboxType::Cubemap || skyboxType == SkyboxType::Panoramic)) {
        UploadSkyboxTextures(device);
        m_SkyboxNeedsUpdate = false;
    }
}

void WorldEnvironment::LoadSkyboxTextures() {
    SkyboxType skyboxType = static_cast<SkyboxType>(GetSkyboxType());
    switch (skyboxType) {
        case SkyboxType::Cubemap: {

            std::string cubemapPosX = GetCubemapPosX();
            std::string cubemapNegX = GetCubemapNegX();
            std::string cubemapPosY = GetCubemapPosY();
            std::string cubemapNegY = GetCubemapNegY();
            std::string cubemapPosZ = GetCubemapPosZ();
            std::string cubemapNegZ = GetCubemapNegZ();

            const std::string* paths[6] = {
                &cubemapPosX, &cubemapNegX,
                &cubemapPosY, &cubemapNegY,
                &cubemapPosZ, &cubemapNegZ
            };

            for (int i = 0; i < 6; ++i) {
                if (!paths[i]->empty()) {
                    m_CubemapAssets[i] = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
                    bool loaded = m_CubemapAssets[i]->LoadFromFile(*paths[i], true, asset::ImageColorSpace::sRGB);
                    if (!loaded) {

                        m_CubemapAssets[i].Reset();
                    }
                }
            }
            break;
        }

        case SkyboxType::Panoramic: {
            std::string panoramicTexture = GetPanoramicTexture();
            if (!panoramicTexture.empty()) {
                m_PanoramicAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
                bool loaded = m_PanoramicAsset->LoadFromFile(panoramicTexture, true, asset::ImageColorSpace::sRGB);
                if (!loaded) {

                    m_PanoramicAsset.Reset();
                }
            }
            break;
        }

        default:

            break;
    }
}

void WorldEnvironment::UploadSkyboxTextures(IGfxDevice* device) {
    if (!device) {
        return;
    }

    SkyboxType skyboxType = static_cast<SkyboxType>(GetSkyboxType());
    switch (skyboxType) {
        case SkyboxType::Cubemap: {

            bool allLoaded = true;
            for (int i = 0; i < 6; ++i) {
                if (!m_CubemapAssets[i].IsValid() || !m_CubemapAssets[i]->IsLoaded()) {
                    allLoaded = false;
                    break;
                }
            }

            if (!allLoaded) {

                return;
            }

            TextureDesc desc;
            desc.type = TextureType::TextureCube;
            desc.width = m_CubemapAssets[0]->GetWidth();
            desc.height = m_CubemapAssets[0]->GetHeight();
            desc.format = TextureFormat::RGBA8_SRGB;
            desc.usage = TextureUsage::Sampled;
            desc.arrayLayers = 6;

            desc.initialData = m_CubemapAssets[0]->GetData();

            m_SkyboxTexture = device->createTexture(desc);

            if (!m_SkyboxTexture.isValid()) {

            } else {

            }
            break;
        }

        case SkyboxType::Panoramic: {
            if (!m_PanoramicAsset.IsValid() || !m_PanoramicAsset->IsLoaded()) {

                return;
            }

            TextureDesc desc;
            desc.type = TextureType::Texture2D;
            desc.width = m_PanoramicAsset->GetWidth();
            desc.height = m_PanoramicAsset->GetHeight();
            desc.format = TextureFormat::RGBA8_SRGB;
            desc.usage = TextureUsage::Sampled;
            desc.initialData = m_PanoramicAsset->GetData();

            m_SkyboxTexture = device->createTexture(desc);

            if (!m_SkyboxTexture.isValid()) {

            } else {

            }
            break;
        }

        default:

            break;
    }
}

void WorldEnvironment::CreateSkyboxMesh(IGfxDevice* device) {
    if (!device) {
        return;
    }

    MeshData meshData;

    float size = 1.0f;

    math::Vec3 corners[8] = {
        math::Vec3(-size, -size, -size),
        math::Vec3( size, -size, -size),
        math::Vec3( size,  size, -size),
        math::Vec3(-size,  size, -size),
        math::Vec3(-size, -size,  size),
        math::Vec3( size, -size,  size),
        math::Vec3( size,  size,  size),
        math::Vec3(-size,  size,  size)
    };

    meshData.vertices.push_back(Vertex{corners[4], math::Vec3(0, 0, -1), math::Vec2(0, 0)});
    meshData.vertices.push_back(Vertex{corners[5], math::Vec3(0, 0, -1), math::Vec2(1, 0)});
    meshData.vertices.push_back(Vertex{corners[6], math::Vec3(0, 0, -1), math::Vec2(1, 1)});
    meshData.vertices.push_back(Vertex{corners[7], math::Vec3(0, 0, -1), math::Vec2(0, 1)});

    meshData.vertices.push_back(Vertex{corners[1], math::Vec3(0, 0, 1), math::Vec2(0, 0)});
    meshData.vertices.push_back(Vertex{corners[0], math::Vec3(0, 0, 1), math::Vec2(1, 0)});
    meshData.vertices.push_back(Vertex{corners[3], math::Vec3(0, 0, 1), math::Vec2(1, 1)});
    meshData.vertices.push_back(Vertex{corners[2], math::Vec3(0, 0, 1), math::Vec2(0, 1)});

    meshData.vertices.push_back(Vertex{corners[5], math::Vec3(-1, 0, 0), math::Vec2(0, 0)});
    meshData.vertices.push_back(Vertex{corners[1], math::Vec3(-1, 0, 0), math::Vec2(1, 0)});
    meshData.vertices.push_back(Vertex{corners[2], math::Vec3(-1, 0, 0), math::Vec2(1, 1)});
    meshData.vertices.push_back(Vertex{corners[6], math::Vec3(-1, 0, 0), math::Vec2(0, 1)});

    meshData.vertices.push_back(Vertex{corners[0], math::Vec3(1, 0, 0), math::Vec2(0, 0)});
    meshData.vertices.push_back(Vertex{corners[4], math::Vec3(1, 0, 0), math::Vec2(1, 0)});
    meshData.vertices.push_back(Vertex{corners[7], math::Vec3(1, 0, 0), math::Vec2(1, 1)});
    meshData.vertices.push_back(Vertex{corners[3], math::Vec3(1, 0, 0), math::Vec2(0, 1)});

    meshData.vertices.push_back(Vertex{corners[7], math::Vec3(0, -1, 0), math::Vec2(0, 0)});
    meshData.vertices.push_back(Vertex{corners[6], math::Vec3(0, -1, 0), math::Vec2(1, 0)});
    meshData.vertices.push_back(Vertex{corners[2], math::Vec3(0, -1, 0), math::Vec2(1, 1)});
    meshData.vertices.push_back(Vertex{corners[3], math::Vec3(0, -1, 0), math::Vec2(0, 1)});

    meshData.vertices.push_back(Vertex{corners[0], math::Vec3(0, 1, 0), math::Vec2(0, 0)});
    meshData.vertices.push_back(Vertex{corners[1], math::Vec3(0, 1, 0), math::Vec2(1, 0)});
    meshData.vertices.push_back(Vertex{corners[5], math::Vec3(0, 1, 0), math::Vec2(1, 1)});
    meshData.vertices.push_back(Vertex{corners[4], math::Vec3(0, 1, 0), math::Vec2(0, 1)});

    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t base = i * 4;

        meshData.indices.push_back(base + 0);
        meshData.indices.push_back(base + 1);
        meshData.indices.push_back(base + 2);

        meshData.indices.push_back(base + 0);
        meshData.indices.push_back(base + 2);
        meshData.indices.push_back(base + 3);
    }

    meshData.calculateBounds();

    m_SkyboxMesh = device->createMesh(meshData);

    if (!m_SkyboxMesh.isValid()) {

    }
}

void WorldEnvironment::CreateSkyboxMaterial(IGfxDevice* device) {
    if (!device) {
        return;
    }

    // Get the current graphics backend
    GraphicsBackend backend = device->getBackend();

    // Use backend-agnostic shader loading
    DefaultShaders::ShaderDataPair shaderData;
    if (!DefaultShaders::getShaderData("Skybox", backend, &shaderData)) {
        LOG_ERROR(LogCategory::Render, "WorldEnvironment: Failed to get Skybox shader data");
        return;
    }

    // Debug: Log shader data info

    // Verify shader source looks valid (should start with # for GLSL)
    if (shaderData.vertex.data && shaderData.vertex.size > 0 && !shaderData.vertex.isPrecompiled) {
        const char* src = static_cast<const char*>(shaderData.vertex.data);
        std::string preview(src, std::min(size_t(50), shaderData.vertex.size));
        
    }

    ShaderDesc vertexShaderDesc;
    vertexShaderDesc.stage = ShaderStage::Vertex;
    vertexShaderDesc.bytecode = shaderData.vertex.data;
    vertexShaderDesc.bytecodeSize = shaderData.vertex.size;
    vertexShaderDesc.entryPoint = DefaultShaders::getVertexEntryPoint(backend);

    ShaderDesc fragmentShaderDesc;
    fragmentShaderDesc.stage = ShaderStage::Fragment;
    fragmentShaderDesc.bytecode = shaderData.fragment.data;
    fragmentShaderDesc.bytecodeSize = shaderData.fragment.size;
    fragmentShaderDesc.entryPoint = DefaultShaders::getFragmentEntryPoint(backend);

    ShaderHandle vertexShader = device->createShader(vertexShaderDesc);
    ShaderHandle fragmentShader = device->createShader(fragmentShaderDesc);

    if (!vertexShader.isValid() || !fragmentShader.isValid()) {

        if (vertexShader.isValid()) device->destroyShader(vertexShader);
        if (fragmentShader.isValid()) device->destroyShader(fragmentShader);
        return;
    }

    device->destroyShader(vertexShader);
    device->destroyShader(fragmentShader);
}

}
}

