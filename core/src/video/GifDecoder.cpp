#include "lupine/video/GifDecoder.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace lupine {
namespace video {

namespace {

// Read the raw bytes of an asset, mirroring how ImageAsset/AudioAsset resolve
// res:// paths and transparently fall back to a mounted pack file.
bool ReadAssetBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    auto& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        std::string packPath = packFS.resolveAsset(normalized);
        if (packFS.exists(packPath)) {
            out = packFS.readAsset(normalized);
            return !out.empty();
        }
    }

    std::string resolved = normalized;
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        std::string r = assetDb.ResolveAsset(normalized);
        if (!r.empty()) {
            resolved = r;
        }
    }

    std::ifstream file(resolved, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR(LogCategory::Core, "GifDecoder: Failed to open {} (resolved: {})", path, resolved);
        return false;
    }

    std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(out.data()), size)) {
        return false;
    }
    return true;
}

// stb_image's GIF loader does not honor stbi_set_flip_vertically_on_load, so we
// flip each frame manually to match the rest of the engine's textures (ImageAsset
// loads with vertical flip enabled).
void FlipRowsInPlace(uint8_t* pixels, int width, int height) {
    const size_t rowBytes = static_cast<size_t>(width) * 4u;
    std::vector<uint8_t> tmp(rowBytes);
    for (int y = 0; y < height / 2; ++y) {
        uint8_t* top = pixels + static_cast<size_t>(y) * rowBytes;
        uint8_t* bottom = pixels + static_cast<size_t>(height - 1 - y) * rowBytes;
        std::copy(top, top + rowBytes, tmp.data());
        std::copy(bottom, bottom + rowBytes, top);
        std::copy(tmp.begin(), tmp.end(), bottom);
    }
}

} // namespace

bool GifDecoder::LoadFromMemory(const uint8_t* data, size_t size, GifData& out) {
    out = GifData();

    if (!data || size == 0) {
        return false;
    }

    int* delays = nullptr;
    int width = 0;
    int height = 0;
    int frameCount = 0;
    int channels = 0;

    // stbi_set_flip_vertically_on_load is a GLOBAL stb flag. ImageAsset sets it
    // true and never resets it, and stb's GIF loader honors it in some builds.
    // Force it false here so our orientation is deterministic regardless of prior
    // load order (otherwise the GIF gets double-flipped, e.g. upside-down only in
    // the editor); we then apply our own single flip below to match ImageAsset.
    stbi_set_flip_vertically_on_load(false);

    // Returns a contiguous block of `frameCount` frames, each width*height*4 bytes.
    stbi_uc* pixels = stbi_load_gif_from_memory(
        data, static_cast<int>(size),
        &delays, &width, &height, &frameCount, &channels, 4);

    if (!pixels) {
        const char* reason = stbi_failure_reason();
        LOG_ERROR(LogCategory::Core, "GifDecoder: stbi_load_gif_from_memory failed: {}",
                  reason ? reason : "unknown");
        return false;
    }

    if (width <= 0 || height <= 0 || frameCount <= 0) {
        stbi_image_free(pixels);
        if (delays) stbi_image_free(delays);
        return false;
    }

    out.width = width;
    out.height = height;
    out.frames.reserve(static_cast<size_t>(frameCount));
    out.frameDelays.reserve(static_cast<size_t>(frameCount));

    const size_t frameBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;

    for (int i = 0; i < frameCount; ++i) {
        std::vector<uint8_t> frame(frameBytes);
        const uint8_t* src = pixels + static_cast<size_t>(i) * frameBytes;
        std::copy(src, src + frameBytes, frame.data());
        FlipRowsInPlace(frame.data(), width, height);
        out.frames.push_back(std::move(frame));

        // stb reports per-frame delay in milliseconds. GIFs frequently encode 0
        // (meaning "as fast as possible"); follow common viewer behavior and
        // substitute a sane default so playback never stalls or runs away.
        float seconds = delays ? (static_cast<float>(delays[i]) / 1000.0f) : 0.1f;
        if (seconds <= 0.0f) {
            seconds = 0.1f;
        }
        out.frameDelays.push_back(seconds);
    }

    stbi_image_free(pixels);
    if (delays) {
        stbi_image_free(delays);
    }

    return out.IsValid();
}

bool GifDecoder::LoadFromFile(const std::string& path, GifData& out) {
    out = GifData();

    std::vector<uint8_t> bytes;
    if (!ReadAssetBytes(path, bytes)) {
        return false;
    }
    return LoadFromMemory(bytes.data(), bytes.size(), out);
}

} // namespace video
} // namespace lupine
