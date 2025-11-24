#include "lupine/rendering/PBRMaterial.hpp"
#include <cstring>

namespace lupine {

GPUPBRMaterialData PBRMaterial::toGPUData() const {
    GPUPBRMaterialData data;

    data.albedoColor = Vec4(
        properties.albedoColor.r,
        properties.albedoColor.g,
        properties.albedoColor.b,
        properties.albedoColor.a
    );

    data.emissiveColor = Vec4(
        properties.emissiveColor.r,
        properties.emissiveColor.g,
        properties.emissiveColor.b,
        properties.emissiveStrength
    );

    data.params1 = Vec4(
        properties.metallic,
        properties.roughness,
        properties.normalScale,
        properties.emissiveStrength
    );

    data.params2 = Vec4(
        properties.alphaCutoff,
        properties.aoStrength,
        properties.heightScale,
        0.0f
    );

    data.textureFlags = Vec4(
        properties.useAlbedoTexture ? 1.0f : 0.0f,
        properties.useMetallicRoughnessTexture ? 1.0f : 0.0f,
        properties.useNormalTexture ? 1.0f : 0.0f,
        properties.useEmissiveTexture ? 1.0f : 0.0f
    );

    return data;
}

void PBRMaterial::getTextureBindings(TextureHandle* outTextures, uint32_t maxCount) const {
    if (!outTextures || maxCount < 6) {
        return;
    }

    outTextures[0] = properties.albedoTexture;
    outTextures[1] = properties.metallicRoughnessTexture;
    outTextures[2] = properties.normalTexture;
    outTextures[3] = properties.emissiveTexture;
    outTextures[4] = properties.aoTexture;
    outTextures[5] = properties.heightTexture;
}

}

