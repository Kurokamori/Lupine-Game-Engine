#pragma once

#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/Material.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include <string>
#include <unordered_map>

namespace lupine {

class RenderContext;

namespace components {

/**
 * Reusable helper for 2D UI components that attach a custom Lupine Shader (.lsh).
 *
 * Parses the component's serialized `shaderParameters` JSON (a map of uniform name to a
 * typed value) into a MaterialPropertyBlock that can be passed to the renderer, loading
 * and uploading texture-typed parameters into GPU textures on demand.
 *
 * The expected JSON shape is:
 *   {"u_Name": {"type": "float|int|bool|vec2|vec3|vec4|color|texture", "value": <...>}}
 *
 * Shared by ColorRect, Image2D, Panel and Shape2D so the parameter handling lives in one
 * place. Each component still issues its own draw call (with its own standard uniforms).
 */
class CustomShaderParams {
public:
    /**
     * Parse `parametersJson` into `outBlock`. Texture-typed parameters are loaded/uploaded
     * and cached per uniform name so they are only re-uploaded when their path changes.
     */
    void BuildBlock(RenderContext& ctx, const std::string& parametersJson, MaterialPropertyBlock& outBlock);

    /**
     * Drop all cached texture parameters (does not destroy GPU textures, which are owned by
     * the device and released on shutdown — matching the engine's existing texture lifetime).
     */
    void Clear();

private:
    /**
     * Per-instance state for a texture-typed shader parameter.
     */
    struct TextureParam {
        std::string path;
        asset::AssetRef<asset::ImageAsset> asset;
        TextureHandle handle;
    };

    // Texture parameters keyed by uniform name.
    std::unordered_map<std::string, TextureParam> m_TextureParams;
};

} // namespace components
} // namespace lupine
