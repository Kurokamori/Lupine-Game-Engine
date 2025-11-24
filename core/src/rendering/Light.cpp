#include "lupine/rendering/Light.hpp"
#include <cmath>

namespace lupine {

GPULightData LightDescriptor::toGPUData(int shadowMapIndex) const {
    GPULightData data;

    if (type == LightType::Directional) {
        data.positionOrDirection = Vec4(direction.x, direction.y, direction.z, 0.0f);
        data.direction = Vec4(direction.x, direction.y, direction.z, 0.0f);
    } else {
        data.positionOrDirection = Vec4(position.x, position.y, position.z, 1.0f);

        if (type == LightType::Spot) {
            data.direction = Vec4(direction.x, direction.y, direction.z, 0.0f);
        }
    }

    data.color = Vec4(color.r, color.g, color.b, intensity);

    if (type == LightType::Point) {
        data.params = Vec4(range, 0.0f, 0.0f, attenuation);
    } else if (type == LightType::Spot) {

        float innerCos = std::cos(innerConeAngle);
        float outerCos = std::cos(outerConeAngle);
        data.params = Vec4(range, innerCos, outerCos, attenuation);
    } else {

        data.params = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    data.flags = Vec4(
        static_cast<float>(static_cast<uint32_t>(type)),
        castShadows ? 1.0f : 0.0f,
        static_cast<float>(shadowMapIndex),
        0.0f
    );

    return data;
}

}

