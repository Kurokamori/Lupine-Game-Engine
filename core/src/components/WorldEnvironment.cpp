#include "lupine/components/WorldEnvironment.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/DefaultShaders.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
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

void WorldEnvironment::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {

    if (propertyName.find("skybox") != std::string::npos ||
        propertyName.find("cubemap") != std::string::npos ||
        propertyName.find("panoramic") != std::string::npos) {
        m_SkyboxNeedsUpdate = true;
        LoadSkyboxTextures();
    }
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

    const char* vertexShaderSource = DefaultShaders::Skybox_Vertex();
    const char* fragmentShaderSource = DefaultShaders::Skybox_Fragment();

    ShaderDesc vertexShaderDesc;
    vertexShaderDesc.stage = ShaderStage::Vertex;
    vertexShaderDesc.bytecode = vertexShaderSource;
    vertexShaderDesc.bytecodeSize = std::strlen(vertexShaderSource);
    vertexShaderDesc.entryPoint = "main";

    ShaderDesc fragmentShaderDesc;
    fragmentShaderDesc.stage = ShaderStage::Fragment;
    fragmentShaderDesc.bytecode = fragmentShaderSource;
    fragmentShaderDesc.bytecodeSize = std::strlen(fragmentShaderSource);
    fragmentShaderDesc.entryPoint = "main";

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

