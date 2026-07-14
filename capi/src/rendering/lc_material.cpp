/**
 * @file lc_material.cpp
 * @brief Implementation of Lupine Engine C API - Material System
 */

#include "rendering/lc_material.h"
#include "../core/lc_internal.h"

#include <lupine/rendering/Material.hpp>
#include <lupine/rendering/PBRMaterial.hpp>
#include <lupine/math/Vec2.hpp>
#include <lupine/math/Vec3.hpp>
#include <lupine/math/Vec4.hpp>
#include <lupine/math/Color.hpp>

#include <unordered_map>
#include <mutex>
#include <memory>
#include <cstring>

using namespace lupine;
using namespace lupine::math;

namespace {

// MaterialPropertyBlock storage
std::unordered_map<LCMaterialPropertyBlockHandle, std::unique_ptr<MaterialPropertyBlock>> g_PropertyBlocks;
std::mutex g_PropertyBlocksMutex;
LCMaterialPropertyBlockHandle g_NextPropertyBlockHandle = reinterpret_cast<LCMaterialPropertyBlockHandle>(1);

// Material storage (for custom created materials)
std::unordered_map<LCMaterialHandle, std::unique_ptr<Material>> g_Materials;
std::mutex g_MaterialsMutex;
LCMaterialHandle g_NextMaterialHandle = reinterpret_cast<LCMaterialHandle>(1);

// Helper functions for type conversion
Color ToEngineColor(LCColor c) {
    return Color(c.r, c.g, c.b, c.a);
}

LCColor FromEngineColor(const Color& c) {
    return LCColor{c.r, c.g, c.b, c.a};
}

Vec2 ToEngineVec2(LCVec2 v) {
    return Vec2(v.x, v.y);
}

LCVec2 FromEngineVec2(const Vec2& v) {
    return LCVec2{v.x, v.y};
}

Vec3 ToEngineVec3(LCVec3 v) {
    return Vec3(v.x, v.y, v.z);
}

LCVec3 FromEngineVec3(const Vec3& v) {
    return LCVec3{v.x, v.y, v.z};
}

Vec4 ToEngineVec4(LCVec4 v) {
    return Vec4(v.x, v.y, v.z, v.w);
}

LCVec4 FromEngineVec4(const Vec4& v) {
    return LCVec4{v.x, v.y, v.z, v.w};
}

MaterialPropertyBlock* GetPropertyBlock(LCMaterialPropertyBlockHandle handle) {
    std::lock_guard<std::mutex> lock(g_PropertyBlocksMutex);
    auto it = g_PropertyBlocks.find(handle);
    if (it != g_PropertyBlocks.end()) {
        return it->second.get();
    }
    return nullptr;
}

Material* GetMaterial(LCMaterialHandle handle) {
    std::lock_guard<std::mutex> lock(g_MaterialsMutex);
    auto it = g_Materials.find(handle);
    if (it != g_Materials.end()) {
        return it->second.get();
    }
    return nullptr;
}

RenderLayer ToEngineRenderLayer(LCRenderLayer layer) {
    switch (layer) {
        case LC_RENDER_LAYER_DEFAULT: return RenderLayer::Default;
        case LC_RENDER_LAYER_BACKGROUND: return RenderLayer::Background;
        case LC_RENDER_LAYER_OPAQUE: return RenderLayer::Opaque;
        case LC_RENDER_LAYER_TRANSPARENT: return RenderLayer::Transparent;
        case LC_RENDER_LAYER_UI: return RenderLayer::UI;
        case LC_RENDER_LAYER_OVERLAY: return RenderLayer::Overlay;
        case LC_RENDER_LAYER_DEBUG: return RenderLayer::Debug;
        default: return RenderLayer::Default;
    }
}

LCRenderLayer FromEngineRenderLayer(RenderLayer layer) {
    switch (layer) {
        case RenderLayer::Default: return LC_RENDER_LAYER_DEFAULT;
        case RenderLayer::Background: return LC_RENDER_LAYER_BACKGROUND;
        case RenderLayer::Opaque: return LC_RENDER_LAYER_OPAQUE;
        case RenderLayer::Transparent: return LC_RENDER_LAYER_TRANSPARENT;
        case RenderLayer::UI: return LC_RENDER_LAYER_UI;
        case RenderLayer::Overlay: return LC_RENDER_LAYER_OVERLAY;
        case RenderLayer::Debug: return LC_RENDER_LAYER_DEBUG;
        default: return LC_RENDER_LAYER_DEFAULT;
    }
}

} // anonymous namespace

/* ============================================================================
 * PBR Material Properties Functions
 * ============================================================================ */

LC_API LCPBRMaterialProperties lc_pbr_material_properties_default(void) {
    LCPBRMaterialProperties props = {};

    // Albedo/Base Color
    props.albedoColor = LCColor{1.0f, 1.0f, 1.0f, 1.0f};
    props.useAlbedoTexture = false;

    // Metallic-Roughness
    props.metallic = 0.0f;
    props.roughness = 0.5f;
    props.useMetallicRoughnessTexture = false;

    // Normal Mapping
    props.useNormalTexture = false;
    props.normalScale = 1.0f;

    // Emissive
    props.emissiveColor = LCColor{0.0f, 0.0f, 0.0f, 1.0f};
    props.useEmissiveTexture = false;
    props.emissiveStrength = 1.0f;

    // Alpha/Transparency
    props.alphaCutoff = 0.5f;
    props.alphaBlend = false;

    // Ambient Occlusion
    props.useAOTexture = false;
    props.aoStrength = 1.0f;

    // Height/Displacement
    props.useHeightTexture = false;
    props.heightScale = 0.05f;

    // Shader Selection
    props.shaderType = LC_SHADER_PBR;

    // Toon Shader Parameters
    props.toonShadowBands = 3.0f;
    props.toonShadowThreshold = 0.5f;
    props.toonShadowSoftness = 0.02f;
    props.toonSpecularBands = 2.0f;
    props.toonSpecularPower = 32.0f;
    props.toonRimIntensity = 0.0f;
    props.toonRimPower = 3.0f;

    // Stylized Shader Parameters
    props.stylizedShadowSoftness = 0.3f;
    props.stylizedSpecularSoftness = 0.15f;
    props.stylizedShadowBrightness = 0.4f;
    props.stylizedShadowWarmth = 0.5f;
    props.stylizedSpecularIntensity = 0.5f;
    props.stylizedHalfLambertPower = 1.5f;

    // Transparent Shader Parameters
    props.transparentOpacity = 0.5f;
    props.transparentRefractiveIndex = 1.5f;
    props.transparentChromaticAberration = 0.0f;
    props.transparentFresnelPower = 5.0f;
    props.transparentReflectivity = 0.5f;
    props.transparentRoughness = 0.0f;
    props.transparentThickness = 1.0f;

    // Glow Shader Parameters
    props.glowIntensity = 1.0f;
    props.glowFalloff = 2.0f;
    props.glowPulseSpeed = 0.0f;
    props.glowPulseAmount = 0.0f;
    props.glowCoreSize = 0.3f;
    props.glowCoreBrightness = 2.0f;
    props.glowOuterGlow = 1.0f;
    props.glowFresnelPower = 3.0f;
    props.glowFresnelIntensity = 0.5f;
    props.glowColorShift = 0.0f;

    // Rendering Options
    props.doubleSided = false;
    props.castShadows = true;
    props.receiveShadows = true;

    return props;
}

/* ============================================================================
 * Material Property Block Functions
 * ============================================================================ */

LC_API LCResult lc_material_property_block_create(LCMaterialPropertyBlockHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::lock_guard<std::mutex> lock(g_PropertyBlocksMutex);

        auto block = std::make_unique<MaterialPropertyBlock>();
        LCMaterialPropertyBlockHandle handle = g_NextPropertyBlockHandle;
        g_NextPropertyBlockHandle = reinterpret_cast<LCMaterialPropertyBlockHandle>(
            reinterpret_cast<uintptr_t>(g_NextPropertyBlockHandle) + 1
        );

        g_PropertyBlocks[handle] = std::move(block);
        *outHandle = handle;

        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create material property block");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_material_property_block_destroy(LCMaterialPropertyBlockHandle handle) {
    if (!handle) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    std::lock_guard<std::mutex> lock(g_PropertyBlocksMutex);
    auto it = g_PropertyBlocks.find(handle);
    if (it == g_PropertyBlocks.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Property block handle not found");
        return LC_ERROR_INVALID_HANDLE;
    }

    g_PropertyBlocks.erase(it);
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_clear(LCMaterialPropertyBlockHandle handle) {
    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    block->clear();
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_is_empty(LCMaterialPropertyBlockHandle handle, bool* outEmpty) {
    if (!outEmpty) {
        SetError(LC_ERROR_NULL_POINTER, "outEmpty is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *outEmpty = block->isEmpty();
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_has_property(LCMaterialPropertyBlockHandle handle, const char* name, bool* outHas) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outHas) {
        SetError(LC_ERROR_NULL_POINTER, "outHas is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *outHas = block->hasProperty(name);
    return LC_SUCCESS;
}

/* Property Setters */

LC_API LCResult lc_material_property_block_set_float(LCMaterialPropertyBlockHandle handle, const char* name, float value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    block->setFloat(name, value);
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_set_int(LCMaterialPropertyBlockHandle handle, const char* name, int value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    block->setInt(name, value);
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_set_bool(LCMaterialPropertyBlockHandle handle, const char* name, bool value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    block->setBool(name, value);
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_set_vec2(LCMaterialPropertyBlockHandle handle, const char* name, LCVec2 value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    block->setVec2(name, ToEngineVec2(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_set_vec3(LCMaterialPropertyBlockHandle handle, const char* name, LCVec3 value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    block->setVec3(name, ToEngineVec3(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_set_vec4(LCMaterialPropertyBlockHandle handle, const char* name, LCVec4 value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    block->setVec4(name, ToEngineVec4(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_material_property_block_set_color(LCMaterialPropertyBlockHandle handle, const char* name, LCColor value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    block->setColor(name, ToEngineColor(value));
    return LC_SUCCESS;
}

/* Property Getters */

LC_API LCResult lc_material_property_block_get_float(LCMaterialPropertyBlockHandle handle, const char* name, float* outValue) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outValue) {
        SetError(LC_ERROR_NULL_POINTER, "outValue is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const MaterialPropertyValue* prop = block->getProperty(name);
    if (!prop) {
        SetError(LC_ERROR_NOT_FOUND, "Property not found");
        return LC_ERROR_NOT_FOUND;
    }

    if (auto* val = std::get_if<float>(prop)) {
        *outValue = *val;
        return LC_SUCCESS;
    }

    SetError(LC_ERROR_INVALID_PARAMETER, "Property is not a float");
    return LC_ERROR_INVALID_PARAMETER;
}

LC_API LCResult lc_material_property_block_get_int(LCMaterialPropertyBlockHandle handle, const char* name, int* outValue) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outValue) {
        SetError(LC_ERROR_NULL_POINTER, "outValue is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const MaterialPropertyValue* prop = block->getProperty(name);
    if (!prop) {
        SetError(LC_ERROR_NOT_FOUND, "Property not found");
        return LC_ERROR_NOT_FOUND;
    }

    if (auto* val = std::get_if<int>(prop)) {
        *outValue = *val;
        return LC_SUCCESS;
    }

    SetError(LC_ERROR_INVALID_PARAMETER, "Property is not an int");
    return LC_ERROR_INVALID_PARAMETER;
}

LC_API LCResult lc_material_property_block_get_bool(LCMaterialPropertyBlockHandle handle, const char* name, bool* outValue) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outValue) {
        SetError(LC_ERROR_NULL_POINTER, "outValue is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const MaterialPropertyValue* prop = block->getProperty(name);
    if (!prop) {
        SetError(LC_ERROR_NOT_FOUND, "Property not found");
        return LC_ERROR_NOT_FOUND;
    }

    if (auto* val = std::get_if<bool>(prop)) {
        *outValue = *val;
        return LC_SUCCESS;
    }

    SetError(LC_ERROR_INVALID_PARAMETER, "Property is not a bool");
    return LC_ERROR_INVALID_PARAMETER;
}

LC_API LCResult lc_material_property_block_get_vec2(LCMaterialPropertyBlockHandle handle, const char* name, LCVec2* outValue) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outValue) {
        SetError(LC_ERROR_NULL_POINTER, "outValue is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const MaterialPropertyValue* prop = block->getProperty(name);
    if (!prop) {
        SetError(LC_ERROR_NOT_FOUND, "Property not found");
        return LC_ERROR_NOT_FOUND;
    }

    if (auto* val = std::get_if<Vec2>(prop)) {
        *outValue = FromEngineVec2(*val);
        return LC_SUCCESS;
    }

    SetError(LC_ERROR_INVALID_PARAMETER, "Property is not a Vec2");
    return LC_ERROR_INVALID_PARAMETER;
}

LC_API LCResult lc_material_property_block_get_vec3(LCMaterialPropertyBlockHandle handle, const char* name, LCVec3* outValue) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outValue) {
        SetError(LC_ERROR_NULL_POINTER, "outValue is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const MaterialPropertyValue* prop = block->getProperty(name);
    if (!prop) {
        SetError(LC_ERROR_NOT_FOUND, "Property not found");
        return LC_ERROR_NOT_FOUND;
    }

    if (auto* val = std::get_if<Vec3>(prop)) {
        *outValue = FromEngineVec3(*val);
        return LC_SUCCESS;
    }

    SetError(LC_ERROR_INVALID_PARAMETER, "Property is not a Vec3");
    return LC_ERROR_INVALID_PARAMETER;
}

LC_API LCResult lc_material_property_block_get_vec4(LCMaterialPropertyBlockHandle handle, const char* name, LCVec4* outValue) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outValue) {
        SetError(LC_ERROR_NULL_POINTER, "outValue is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const MaterialPropertyValue* prop = block->getProperty(name);
    if (!prop) {
        SetError(LC_ERROR_NOT_FOUND, "Property not found");
        return LC_ERROR_NOT_FOUND;
    }

    if (auto* val = std::get_if<Vec4>(prop)) {
        *outValue = FromEngineVec4(*val);
        return LC_SUCCESS;
    }

    SetError(LC_ERROR_INVALID_PARAMETER, "Property is not a Vec4");
    return LC_ERROR_INVALID_PARAMETER;
}

LC_API LCResult lc_material_property_block_get_color(LCMaterialPropertyBlockHandle handle, const char* name, LCColor* outValue) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outValue) {
        SetError(LC_ERROR_NULL_POINTER, "outValue is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* block = GetPropertyBlock(handle);
    if (!block) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid property block handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const MaterialPropertyValue* prop = block->getProperty(name);
    if (!prop) {
        SetError(LC_ERROR_NOT_FOUND, "Property not found");
        return LC_ERROR_NOT_FOUND;
    }

    if (auto* val = std::get_if<Color>(prop)) {
        *outValue = FromEngineColor(*val);
        return LC_SUCCESS;
    }

    SetError(LC_ERROR_INVALID_PARAMETER, "Property is not a Color");
    return LC_ERROR_INVALID_PARAMETER;
}

/* ============================================================================
 * Material Management Functions
 * ============================================================================ */

LC_API LCResult lc_material_create_pbr(const char* name, const LCPBRMaterialProperties* properties, LCMaterialHandle* outHandle) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!properties) {
        SetError(LC_ERROR_NULL_POINTER, "properties is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::lock_guard<std::mutex> lock(g_MaterialsMutex);

        auto material = std::make_unique<Material>();
        material->name = name;
        material->castsShadows = properties->castShadows;
        material->receivesShadows = properties->receiveShadows;
        material->isTransparent = properties->alphaBlend;
        material->alphaClipThreshold = properties->alphaCutoff;

        // Set material properties
        material->properties["albedoColor"] = ToEngineColor(properties->albedoColor);
        material->properties["metallic"] = properties->metallic;
        material->properties["roughness"] = properties->roughness;
        material->properties["normalScale"] = properties->normalScale;
        material->properties["emissiveColor"] = ToEngineColor(properties->emissiveColor);
        material->properties["emissiveStrength"] = properties->emissiveStrength;
        material->properties["alphaCutoff"] = properties->alphaCutoff;
        material->properties["aoStrength"] = properties->aoStrength;
        material->properties["heightScale"] = properties->heightScale;

        LCMaterialHandle handle = g_NextMaterialHandle;
        g_NextMaterialHandle = reinterpret_cast<LCMaterialHandle>(
            reinterpret_cast<uintptr_t>(g_NextMaterialHandle) + 1
        );

        g_Materials[handle] = std::move(material);
        *outHandle = handle;

        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create PBR material");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_material_destroy(LCMaterialHandle handle) {
    if (!handle) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    std::lock_guard<std::mutex> lock(g_MaterialsMutex);
    auto it = g_Materials.find(handle);
    if (it == g_Materials.end()) {
        SetError(LC_ERROR_INVALID_HANDLE, "Material handle not found");
        return LC_ERROR_INVALID_HANDLE;
    }

    g_Materials.erase(it);
    return LC_SUCCESS;
}

LC_API LCResult lc_material_is_valid(LCMaterialHandle handle, bool* outValid) {
    if (!outValid) {
        SetError(LC_ERROR_NULL_POINTER, "outValid is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    std::lock_guard<std::mutex> lock(g_MaterialsMutex);
    *outValid = (handle != nullptr && g_Materials.find(handle) != g_Materials.end());
    return LC_SUCCESS;
}

LC_API LCResult lc_material_get_name(LCMaterialHandle handle, char* outName, uint32_t maxLen) {
    if (!outName) {
        SetError(LC_ERROR_NULL_POINTER, "outName is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    CopyStringToBuffer(outName, maxLen, material->name.c_str());
    return LC_SUCCESS;
}

/* ============================================================================
 * Material Property Functions
 * ============================================================================ */

LC_API LCResult lc_material_set_render_layer(LCMaterialHandle handle, LCRenderLayer layer) {
    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    material->renderLayer = ToEngineRenderLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_material_get_render_layer(LCMaterialHandle handle, LCRenderLayer* outLayer) {
    if (!outLayer) {
        SetError(LC_ERROR_NULL_POINTER, "outLayer is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *outLayer = FromEngineRenderLayer(material->renderLayer);
    return LC_SUCCESS;
}

LC_API LCResult lc_material_set_cast_shadows(LCMaterialHandle handle, bool castShadows) {
    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    material->castsShadows = castShadows;
    return LC_SUCCESS;
}

LC_API LCResult lc_material_get_cast_shadows(LCMaterialHandle handle, bool* outCastShadows) {
    if (!outCastShadows) {
        SetError(LC_ERROR_NULL_POINTER, "outCastShadows is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *outCastShadows = material->castsShadows;
    return LC_SUCCESS;
}

LC_API LCResult lc_material_set_receive_shadows(LCMaterialHandle handle, bool receiveShadows) {
    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    material->receivesShadows = receiveShadows;
    return LC_SUCCESS;
}

LC_API LCResult lc_material_get_receive_shadows(LCMaterialHandle handle, bool* outReceiveShadows) {
    if (!outReceiveShadows) {
        SetError(LC_ERROR_NULL_POINTER, "outReceiveShadows is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *outReceiveShadows = material->receivesShadows;
    return LC_SUCCESS;
}

LC_API LCResult lc_material_set_transparent(LCMaterialHandle handle, bool transparent) {
    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    material->isTransparent = transparent;
    return LC_SUCCESS;
}

LC_API LCResult lc_material_get_transparent(LCMaterialHandle handle, bool* outTransparent) {
    if (!outTransparent) {
        SetError(LC_ERROR_NULL_POINTER, "outTransparent is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *outTransparent = material->isTransparent;
    return LC_SUCCESS;
}

LC_API LCResult lc_material_set_alpha_clip_threshold(LCMaterialHandle handle, float threshold) {
    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    material->alphaClipThreshold = threshold;
    return LC_SUCCESS;
}

LC_API LCResult lc_material_get_alpha_clip_threshold(LCMaterialHandle handle, float* outThreshold) {
    if (!outThreshold) {
        SetError(LC_ERROR_NULL_POINTER, "outThreshold is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* material = GetMaterial(handle);
    if (!material) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid material handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    *outThreshold = material->alphaClipThreshold;
    return LC_SUCCESS;
}

/* ============================================================================
 * Default Material Library
 * Note: These require RenderWorld access. Currently return NOT_SUPPORTED
 * as RenderWorld is not accessible from CAPI initialization.
 * ============================================================================ */

LC_API LCResult lc_material_get_default_pbr(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_toon(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_stylized(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_transparent(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_glow(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_skeletal(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_skeletal_toon(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_skeletal_stylized(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_text3d(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_get_default_colored_double_sided(LCMaterialHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    SetError(LC_ERROR_NOT_SUPPORTED, "Default materials require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Custom Shader Support
 * ============================================================================ */

LC_API LCResult lc_material_get_or_create_custom(
    const char* vertexShaderPath,
    const char* fragmentShaderPath,
    bool,
    LCMaterialHandle* outHandle
) {
    if (!vertexShaderPath) {
        SetError(LC_ERROR_NULL_POINTER, "vertexShaderPath is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!fragmentShaderPath) {
        SetError(LC_ERROR_NULL_POINTER, "fragmentShaderPath is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    SetError(LC_ERROR_NOT_SUPPORTED, "Custom shaders require rendering system initialization");
    return LC_ERROR_NOT_SUPPORTED;
}

LC_API LCResult lc_material_clear_custom_shader_cache(void) {
    // No-op since we don't have RenderWorld access
    return LC_SUCCESS;
}
