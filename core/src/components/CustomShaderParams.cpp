#include "lupine/components/CustomShaderParams.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace components {

using namespace math;

void CustomShaderParams::Clear() {
    m_TextureParams.clear();
}

void CustomShaderParams::BuildBlock(RenderContext& ctx, const std::string& parametersJson,
                                    MaterialPropertyBlock& outBlock) {
    if (parametersJson.empty()) {
        return;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(parametersJson);
    } catch (const std::exception& e) {
        LOG_WARN(LogCategory::Render, "CustomShaderParams: failed to parse shaderParameters: {}", e.what());
        return;
    }

    if (!root.is_object()) {
        return;
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        const std::string& name = it.key();
        const nlohmann::json& entry = it.value();
        if (!entry.is_object() || !entry.contains("type") || !entry.contains("value")) {
            continue;
        }

        const std::string type = entry["type"].get<std::string>();
        const nlohmann::json& value = entry["value"];

        if (type == "float" && value.is_number()) {
            outBlock.setFloat(name, value.get<float>());
        } else if (type == "int" && value.is_number()) {
            outBlock.setInt(name, value.get<int>());
        } else if (type == "bool" && value.is_boolean()) {
            outBlock.setBool(name, value.get<bool>());
        } else if (type == "vec2" && value.is_array() && value.size() >= 2) {
            outBlock.setVec2(name, Vec2(value[0].get<float>(), value[1].get<float>()));
        } else if (type == "vec3" && value.is_array() && value.size() >= 3) {
            outBlock.setVec3(name, Vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>()));
        } else if (type == "vec4" && value.is_array() && value.size() >= 4) {
            outBlock.setVec4(name, Vec4(value[0].get<float>(), value[1].get<float>(),
                                        value[2].get<float>(), value[3].get<float>()));
        } else if (type == "color" && value.is_array() && value.size() >= 4) {
            outBlock.setColor(name, Color(value[0].get<float>(), value[1].get<float>(),
                                          value[2].get<float>(), value[3].get<float>()));
        } else if (type == "texture" && value.is_string()) {
            const std::string texPath = value.get<std::string>();
            if (texPath.empty()) {
                continue;
            }

            TextureParam& slot = m_TextureParams[name];

            // (Re)load the image asset whenever the referenced path changes.
            if (slot.path != texPath || !slot.asset.IsValid()) {
                slot.path = texPath;
                slot.handle = TextureHandle();
                slot.asset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
                if (!slot.asset->LoadFromFile(texPath, true, asset::ImageColorSpace::sRGB)) {
                    slot.asset.Reset();
                }
            }

            // Upload to the GPU once the asset has valid pixel data.
            if (!slot.handle.isValid() && slot.asset.IsValid() && slot.asset->IsLoaded() &&
                slot.asset->GetWidth() > 0 && slot.asset->GetHeight() > 0 &&
                slot.asset->GetData() != nullptr) {
                IGfxDevice* device = ctx.getDevice();
                if (device) {
                    slot.handle = lupine::CreateTexture2DFromImage(device, *slot.asset, TextureFormat::RGBA8_UNORM);
                }
            }

            if (slot.handle.isValid()) {
                outBlock.setTexture(name, slot.handle);
            }
        }
    }
}

} // namespace components
} // namespace lupine
