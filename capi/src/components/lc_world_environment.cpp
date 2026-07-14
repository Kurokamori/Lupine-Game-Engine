/**
 * @file lc_world_environment.cpp
 * @brief Implementation of WorldEnvironment C API
 */

#include "components/lc_world_environment.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/WorldEnvironment.hpp>

#include <cstring>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

int ToEngineSkyboxType(LCSkyboxType type) {
    return static_cast<int>(type);
}

LCSkyboxType FromEngineSkyboxType(int type) {
    return static_cast<LCSkyboxType>(type);
}

int ToEngineFogMode(LCFogMode mode) {
    return static_cast<int>(mode);
}

LCFogMode FromEngineFogMode(int mode) {
    return static_cast<LCFogMode>(mode);
}

WorldEnvironment* GetWorldEnvironment(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<WorldEnvironment*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * WorldEnvironment Creation
 * ============================================================================ */

LC_API LCResult lc_world_environment_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = std::make_shared<WorldEnvironment>();
        if (name) {
            comp->SetName(name);
        }
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create WorldEnvironment component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Skybox Settings
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_skybox_type(LCComponentHandle component, LCSkyboxType* out_type) {
    if (!out_type) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_type = FromEngineSkyboxType(env->GetSkyboxType());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_skybox_type(LCComponentHandle component, LCSkyboxType type) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSkyboxType(ToEngineSkyboxType(type));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_skybox_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetSkyboxColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_skybox_color(LCComponentHandle component, LCColor color) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSkyboxColor(ToEngineColor(color));
    return LC_SUCCESS;
}

/* ============================================================================
 * Procedural Sky Gradient
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_sky_top_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetSkyTopColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_sky_top_color(LCComponentHandle component, LCColor color) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSkyTopColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_sky_horizon_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetSkyHorizonColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_sky_horizon_color(LCComponentHandle component, LCColor color) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSkyHorizonColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_sky_bottom_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetSkyBottomColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_sky_bottom_color(LCComponentHandle component, LCColor color) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSkyBottomColor(ToEngineColor(color));
    return LC_SUCCESS;
}

/* ============================================================================
 * Cubemap Texture Paths
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_cubemap_pos_x(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    const std::string& path = env->GetCubemapPosX();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_cubemap_pos_x(LCComponentHandle component, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetCubemapPosX(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_cubemap_neg_x(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    const std::string& path = env->GetCubemapNegX();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_cubemap_neg_x(LCComponentHandle component, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetCubemapNegX(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_cubemap_pos_y(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    const std::string& path = env->GetCubemapPosY();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_cubemap_pos_y(LCComponentHandle component, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetCubemapPosY(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_cubemap_neg_y(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    const std::string& path = env->GetCubemapNegY();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_cubemap_neg_y(LCComponentHandle component, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetCubemapNegY(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_cubemap_pos_z(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    const std::string& path = env->GetCubemapPosZ();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_cubemap_pos_z(LCComponentHandle component, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetCubemapPosZ(path);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_cubemap_neg_z(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    const std::string& path = env->GetCubemapNegZ();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_cubemap_neg_z(LCComponentHandle component, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetCubemapNegZ(path);
    return LC_SUCCESS;
}

/* ============================================================================
 * Panoramic Texture
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_panoramic_texture(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    const std::string& path = env->GetPanoramicTexture();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_panoramic_texture(LCComponentHandle component, const char* path) {
    if (!path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetPanoramicTexture(path);
    return LC_SUCCESS;
}

/* ============================================================================
 * Fog Settings
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_fog_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_enabled = env->GetFogEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_fog_enabled(LCComponentHandle component, bool enabled) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFogEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_fog_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetFogColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_fog_color(LCComponentHandle component, LCColor color) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFogColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_fog_density(LCComponentHandle component, float* out_density) {
    if (!out_density) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_density = env->GetFogDensity();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_fog_density(LCComponentHandle component, float density) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFogDensity(density);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_fog_start(LCComponentHandle component, float* out_start) {
    if (!out_start) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_start = env->GetFogStart();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_fog_start(LCComponentHandle component, float start) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFogStart(start);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_fog_end(LCComponentHandle component, float* out_end) {
    if (!out_end) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_end = env->GetFogEnd();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_fog_end(LCComponentHandle component, float end) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFogEnd(end);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_fog_mode(LCComponentHandle component, LCFogMode* out_mode) {
    if (!out_mode) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_mode = FromEngineFogMode(env->GetFogMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_fog_mode(LCComponentHandle component, LCFogMode mode) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFogMode(ToEngineFogMode(mode));
    return LC_SUCCESS;
}

/* ============================================================================
 * Ambient Light Settings
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_ambient_light_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_enabled = env->GetAmbientLightEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ambient_light_enabled(LCComponentHandle component, bool enabled) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetAmbientLightEnabled(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_ambient_light_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetAmbientLightColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ambient_light_color(LCComponentHandle component, LCColor color) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetAmbientLightColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_ambient_light_intensity(LCComponentHandle component, float* out_intensity) {
    if (!out_intensity) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_intensity = env->GetAmbientLightIntensity();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ambient_light_intensity(LCComponentHandle component, float intensity) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetAmbientLightIntensity(intensity);
    return LC_SUCCESS;
}

/* ============================================================================
 * Volumetric Fog
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_volumetric_fog_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_enabled = env->GetVolumetricFogEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_volumetric_fog_enabled(LCComponentHandle component, bool enabled) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetVolumetricFogEnabled(enabled);
    return LC_SUCCESS;
}

/* ============================================================================
 * Post-Processing Settings
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_post_processing_enabled(LCComponentHandle component, bool* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetPostProcessingEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_post_processing_enabled(LCComponentHandle component, bool value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetPostProcessingEnabled(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_tonemap_mode(LCComponentHandle component, LCTonemapMode* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = static_cast<LCTonemapMode>(env->GetTonemapMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_tonemap_mode(LCComponentHandle component, LCTonemapMode value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetTonemapMode(static_cast<int>(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_exposure(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetExposure();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_exposure(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetExposure(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_white_point(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetWhitePoint();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_white_point(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetWhitePoint(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_post_flip_y(LCComponentHandle component, LCFlipYMode* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = static_cast<LCFlipYMode>(env->GetPostFlipYMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_post_flip_y(LCComponentHandle component, LCFlipYMode value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetPostFlipYMode(static_cast<int>(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_bloom_enabled(LCComponentHandle component, bool* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetBloomEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_bloom_enabled(LCComponentHandle component, bool value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetBloomEnabled(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_bloom_threshold(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetBloomThreshold();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_bloom_threshold(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetBloomThreshold(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_bloom_soft_knee(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetBloomSoftKnee();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_bloom_soft_knee(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetBloomSoftKnee(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_bloom_intensity(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetBloomIntensity();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_bloom_intensity(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetBloomIntensity(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_bloom_iterations(LCComponentHandle component, int* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetBloomIterations();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_bloom_iterations(LCComponentHandle component, int value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetBloomIterations(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_ssao_enabled(LCComponentHandle component, bool* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetSSAOEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ssao_enabled(LCComponentHandle component, bool value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSSAOEnabled(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_ssao_radius(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetSSAORadius();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ssao_radius(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSSAORadius(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_ssao_intensity(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetSSAOIntensity();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ssao_intensity(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSSAOIntensity(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_ssao_bias(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetSSAOBias();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ssao_bias(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSSAOBias(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_ssao_samples(LCComponentHandle component, int* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetSSAOSamples();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ssao_samples(LCComponentHandle component, int value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSSAOSamples(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_ssao_power(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetSSAOPower();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_ssao_power(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSSAOPower(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_color_grading_enabled(LCComponentHandle component, bool* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetColorGradingEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_color_grading_enabled(LCComponentHandle component, bool value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetColorGradingEnabled(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_contrast(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetContrast();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_contrast(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetContrast(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_saturation(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetSaturation();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_saturation(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetSaturation(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_brightness(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetBrightnessAdjust();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_brightness(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetBrightnessAdjust(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_temperature(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetTemperature();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_temperature(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetTemperature(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_tint(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetTintAdjust();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_tint(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetTintAdjust(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_color_filter(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetColorFilter());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_color_filter(LCComponentHandle component, LCColor value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetColorFilter(ToEngineColor(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_color_lift(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetColorLift());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_color_lift(LCComponentHandle component, LCColor value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetColorLift(ToEngineColor(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_color_gamma(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetColorGamma());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_color_gamma(LCComponentHandle component, LCColor value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetColorGamma(ToEngineColor(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_color_gain(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetColorGain());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_color_gain(LCComponentHandle component, LCColor value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetColorGain(ToEngineColor(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_vignette_enabled(LCComponentHandle component, bool* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetVignetteEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_vignette_enabled(LCComponentHandle component, bool value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetVignetteEnabled(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_vignette_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_color = FromEngineColor(env->GetVignetteColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_vignette_color(LCComponentHandle component, LCColor value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetVignetteColor(ToEngineColor(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_vignette_intensity(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetVignetteIntensity();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_vignette_intensity(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetVignetteIntensity(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_vignette_smoothness(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetVignetteSmoothness();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_vignette_smoothness(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetVignetteSmoothness(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_vignette_roundness(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetVignetteRoundness();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_vignette_roundness(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetVignetteRoundness(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_vignette_center_x(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetVignetteCenterX();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_vignette_center_x(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetVignetteCenterX(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_vignette_center_y(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetVignetteCenterY();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_vignette_center_y(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetVignetteCenterY(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_chromatic_aberration_enabled(LCComponentHandle component, bool* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetChromaticAberrationEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_chromatic_aberration_enabled(LCComponentHandle component, bool value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetChromaticAberrationEnabled(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_chromatic_aberration_amount(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetChromaticAberrationAmount();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_chromatic_aberration_amount(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetChromaticAberrationAmount(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_film_grain_enabled(LCComponentHandle component, bool* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetFilmGrainEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_film_grain_enabled(LCComponentHandle component, bool value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFilmGrainEnabled(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_film_grain_intensity(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetFilmGrainIntensity();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_film_grain_intensity(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFilmGrainIntensity(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_film_grain_size(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetFilmGrainSize();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_film_grain_size(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetFilmGrainSize(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_overlay_texture(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    const std::string path = env->GetOverlayTexturePath();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_overlay_texture(LCComponentHandle component, const char* path) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetOverlayTexturePath(path ? path : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_overlay_blend_mode(LCComponentHandle component, LCOverlayBlendMode* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = static_cast<LCOverlayBlendMode>(env->GetOverlayBlendMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_overlay_blend_mode(LCComponentHandle component, LCOverlayBlendMode value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetOverlayBlendMode(static_cast<int>(value));
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_get_overlay_opacity(LCComponentHandle component, float* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    *out_value = env->GetOverlayOpacity();
    return LC_SUCCESS;
}

LC_API LCResult lc_world_environment_set_overlay_opacity(LCComponentHandle component, float value) {
    auto env = GetWorldEnvironment(component);
    if (!env) return LC_ERROR_INVALID_HANDLE;
    env->SetOverlayOpacity(value);
    return LC_SUCCESS;
}

