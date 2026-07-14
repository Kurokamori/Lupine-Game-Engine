#include "lupine/rendering/TextureUpload.hpp"

#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/asset/ImageAsset.hpp"

#include <atomic>

namespace lupine {

namespace {

std::atomic<bool> g_generateMipmaps{true};

// OpenGL and WebGL build the mip chain on the GPU from the uploaded base level
// (glGenerateMipmap inside createTexture when mipLevels > 1). Every other backend
// has the CPU-side chain uploaded level by level via updateTexture.
bool BackendAutoGeneratesMips(GraphicsBackend backend) {
    return backend == GraphicsBackend::OpenGL || backend == GraphicsBackend::WebGL;
}

TextureHandle UploadImageLevels(IGfxDevice* device, asset::ImageAsset& image,
                                TextureFormat format, uint32_t mipCount) {
    const uint32_t width = image.GetWidth();
    const uint32_t height = image.GetHeight();

    TextureDesc desc;
    desc.type = TextureType::Texture2D;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.mipLevels = mipCount;
    desc.arrayLayers = 1;
    desc.format = format;
    desc.usage = TextureUsage::Sampled;

    if (mipCount <= 1) {
        desc.initialData = image.GetData();
        return device->createTexture(desc);
    }

    if (BackendAutoGeneratesMips(device->getBackend())) {
        // The base level is enough; createTexture generates the rest on the GPU.
        desc.initialData = image.GetData();
        return device->createTexture(desc);
    }

    // Allocate all levels empty, then upload the CPU mip chain level by level so
    // the texture minifies smoothly without relying on a GPU mip generator (which
    // not every backend has).
    desc.initialData = nullptr;
    TextureHandle handle = device->createTexture(desc);
    if (!handle.isValid()) {
        return handle;
    }

    const std::vector<asset::MipLevel>& levels = image.GetAllMipLevels();
    const uint32_t uploadCount = (mipCount < static_cast<uint32_t>(levels.size()))
                                     ? mipCount
                                     : static_cast<uint32_t>(levels.size());
    for (uint32_t level = 0; level < uploadCount; ++level) {
        device->updateTexture(handle, levels[level].data.data(), level, 0);
    }

    return handle;
}

} // namespace

void SetGenerateTextureMipmaps(bool enabled) {
    g_generateMipmaps.store(enabled, std::memory_order_relaxed);
}

bool GetGenerateTextureMipmaps() {
    return g_generateMipmaps.load(std::memory_order_relaxed);
}

TextureHandle CreateTexture2DFromImage(IGfxDevice* device, asset::ImageAsset& image,
                                       TextureFormat format) {
    if (!device) {
        return TextureHandle();
    }

    const uint32_t width = image.GetWidth();
    const uint32_t height = image.GetHeight();
    if (width == 0 || height == 0 || image.GetData() == nullptr) {
        return TextureHandle();
    }

    const bool wantMips = GetGenerateTextureMipmaps() && (width > 1 || height > 1);
    if (wantMips) {
        image.EnsureMipmaps();
    }

    uint32_t mipCount = wantMips ? image.GetMipLevels() : 1u;
    if (mipCount < 1) {
        mipCount = 1;
    }

    return UploadImageLevels(device, image, format, mipCount);
}

TextureHandle CreateTexture2DFromPixels(IGfxDevice* device, uint32_t width, uint32_t height,
                                        const void* rgbaPixels, TextureFormat format,
                                        bool allowMipmaps) {
    if (!device || width == 0 || height == 0 || rgbaPixels == nullptr) {
        return TextureHandle();
    }

    const bool wantMips = allowMipmaps && GetGenerateTextureMipmaps() && (width > 1 || height > 1);

    if (!wantMips) {
        TextureDesc desc;
        desc.type = TextureType::Texture2D;
        desc.width = width;
        desc.height = height;
        desc.depth = 1;
        desc.mipLevels = 1;
        desc.arrayLayers = 1;
        desc.format = format;
        desc.usage = TextureUsage::Sampled;
        desc.initialData = rgbaPixels;
        return device->createTexture(desc);
    }

    // Reuse the asset mip generator (box filter) so the chain matches image-loaded
    // textures and the per-backend upload path is shared.
    asset::ImageAsset staging;
    if (!staging.LoadFromRawData(static_cast<const uint8_t*>(rgbaPixels), width, height, 4,
                                 true, asset::ImageColorSpace::sRGB)) {
        return TextureHandle();
    }

    return CreateTexture2DFromImage(device, staging, format);
}

} // namespace lupine
