#include "lupine/components/ParticleTextures.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>
#include <cmath>

namespace lupine {
namespace components {

namespace {

struct ShapeTextures {
    TextureHandle square;
    TextureHandle circle;
};

std::map<IGfxDevice*, ShapeTextures>& GetCache() {
    static std::map<IGfxDevice*, ShapeTextures> s_Cache;
    return s_Cache;
}

std::mutex& GetCacheMutex() {
    static std::mutex s_Mutex;
    return s_Mutex;
}

float SmoothStep(float edge0, float edge1, float x) {
    if (edge0 == edge1) {
        return x < edge0 ? 0.0f : 1.0f;
    }
    float t = (x - edge0) / (edge1 - edge0);
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

TextureHandle CreateSquareTexture(IGfxDevice* device) {
    const uint32_t size = 2;
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4, 255);

    return lupine::CreateTexture2DFromPixels(device, size, size, pixels.data(), TextureFormat::RGBA8_UNORM);
}

TextureHandle CreateCircleTexture(IGfxDevice* device) {
    const uint32_t size = 64;
    const float center = (static_cast<float>(size) - 1.0f) * 0.5f;
    const float radius = static_cast<float>(size) * 0.5f;

    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4, 0);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            float dx = static_cast<float>(x) - center;
            float dy = static_cast<float>(y) - center;
            float dist = std::sqrt(dx * dx + dy * dy) / radius;
            // Solid core with a soft 15% feathered edge.
            float alpha = 1.0f - SmoothStep(0.85f, 1.0f, dist);

            size_t idx = (static_cast<size_t>(y) * size + x) * 4;
            pixels[idx + 0] = 255;
            pixels[idx + 1] = 255;
            pixels[idx + 2] = 255;
            pixels[idx + 3] = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, alpha)) * 255.0f);
        }
    }

    return lupine::CreateTexture2DFromPixels(device, size, size, pixels.data(), TextureFormat::RGBA8_UNORM);
}

} // anonymous namespace

TextureHandle GetBuiltinParticleTexture(IGfxDevice* device, ParticleShape shape) {
    if (!device) {
        return TextureHandle();
    }

    std::lock_guard<std::mutex> lock(GetCacheMutex());
    ShapeTextures& entry = GetCache()[device];

    if (shape == ParticleShape::Circle) {
        if (!entry.circle.isValid()) {
            entry.circle = CreateCircleTexture(device);
        }
        return entry.circle;
    }

    if (!entry.square.isValid()) {
        entry.square = CreateSquareTexture(device);
    }
    return entry.square;
}

} // namespace components
} // namespace lupine
