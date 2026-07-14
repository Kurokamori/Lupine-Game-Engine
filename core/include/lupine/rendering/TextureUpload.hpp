#pragma once

#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/gfx/GfxTypes.hpp"

#include <cstdint>

namespace lupine {

class IGfxDevice;

namespace asset {
class ImageAsset;
}

/**
 * Global toggle for whether newly created 2D content textures (sprites, images,
 * UI textures, tilemaps, ...) are given a full mipmap chain and sampled
 * trilinearly.
 *
 * The engine renders the scene directly at the physical window resolution and
 * maps the design canvas onto it through the viewport, so when the window is
 * smaller than the design resolution every sprite is minified on screen. Without
 * a mip chain that minification is a single bilinear tap of the full-resolution
 * texture, which aliases ("crunchy"). With a mip chain the GPU samples the
 * appropriately downsampled level, matching the smooth result other engines give.
 *
 * Default is true (smooth). The runtime sets it from the project's texture-filter
 * setting at startup; pixel-art projects set it to false (and a Nearest default
 * filter) to keep crisp, mip-free sampling.
 */
void SetGenerateTextureMipmaps(bool enabled);
bool GetGenerateTextureMipmaps();

/**
 * Create a sampled RGBA Texture2D from an image asset.
 *
 * When mipmaps are enabled globally and the image is larger than 1x1 the texture
 * is created with the image's full mip chain (generating it in the asset on
 * demand). The mip data is uploaded backend-appropriately: OpenGL/WebGL generate
 * the chain on the GPU from the base level, while DirectX/Vulkan/Metal upload the
 * CPU-side chain level by level. When mipmaps are disabled (or the image is 1x1)
 * a single-level texture is created exactly as before.
 *
 * Returns an invalid handle if the device or image is unusable.
 */
TextureHandle CreateTexture2DFromImage(IGfxDevice* device, asset::ImageAsset& image,
                                       TextureFormat format = TextureFormat::RGBA8_UNORM);

/**
 * Create a sampled RGBA Texture2D directly from tightly-packed 4-channel (RGBA8)
 * pixels, applying the same mipmap policy as CreateTexture2DFromImage. Used by
 * components that build pixel buffers at runtime (dynamic atlases, video frames,
 * generated particle textures). When @p allowMipmaps is false the texture is
 * always single-level regardless of the global setting (e.g. for buffers that are
 * re-uploaded every frame, where a mip chain would be rebuilt needlessly).
 */
TextureHandle CreateTexture2DFromPixels(IGfxDevice* device, uint32_t width, uint32_t height,
                                        const void* rgbaPixels,
                                        TextureFormat format = TextureFormat::RGBA8_UNORM,
                                        bool allowMipmaps = true);

} // namespace lupine
