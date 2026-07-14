#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lupine {
namespace video {

/**
 * Decoded animated GIF data.
 *
 * Every frame shares the same dimensions. Each frame is stored as tightly
 * packed RGBA8 pixels and is pre-flipped vertically to match the engine's
 * texture convention (the same convention ImageAsset produces, so decoded
 * frames render correctly through drawSprite on every backend). Each frame
 * carries its own display delay in seconds.
 */
struct GifData {
    int width{0};
    int height{0};
    std::vector<std::vector<uint8_t>> frames;  // each width*height*4 bytes, RGBA8
    std::vector<float> frameDelays;            // seconds per frame, parallel to frames

    bool IsValid() const {
        return width > 0 && height > 0 && !frames.empty() &&
               frames.size() == frameDelays.size();
    }

    int FrameCount() const { return static_cast<int>(frames.size()); }

    // Total animation duration in seconds (sum of all per-frame delays).
    float TotalDuration() const {
        float total = 0.0f;
        for (float d : frameDelays) {
            total += d;
        }
        return total;
    }
};

/**
 * Animated GIF decoder backed by stb_image (stbi_load_gif_from_memory).
 *
 * Handles both animated and single-frame GIFs. File loading is pack-file and
 * res:// aware, mirroring how ImageAsset resolves and reads assets.
 */
class GifDecoder {
public:
    /**
     * Decode a GIF from a res:// path, physical path, or pack asset.
     * @return true and fills `out` on success; false on any failure.
     */
    static bool LoadFromFile(const std::string& path, GifData& out);

    /**
     * Decode a GIF from an in-memory byte buffer.
     */
    static bool LoadFromMemory(const uint8_t* data, size_t size, GifData& out);
};

} // namespace video
} // namespace lupine
