/**
 * @file lc_project.cpp
 * @brief Implementation of the Project / ProjectSettings C API.
 *
 * Wraps lupine::core::Project behind opaque handles backed by a thread-safe
 * registry (mirrors the Prefab handle pattern). ProjectSettings fields are
 * exposed as typed getters/setters operating on the handle's settings object.
 */

#include "core/lc_project.h"
#include "lc_internal.h"

#include <lupine/core/Project.hpp>
#include <lupine/math/Math.hpp>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using lupine::core::Project;
using lupine::core::ProjectSettings;
using lupine::core::SplashScreenEntry;

namespace {

std::unordered_map<LCProjectHandle, std::shared_ptr<Project>> g_ProjectHandles;
std::mutex g_ProjectMutex;
uint64_t g_NextProjectHandle = 1;

LCProjectHandle CreateProjectHandle(std::shared_ptr<Project> project) {
    std::lock_guard<std::mutex> lock(g_ProjectMutex);
    LCProjectHandle handle = reinterpret_cast<LCProjectHandle>(g_NextProjectHandle++);
    g_ProjectHandles[handle] = project;
    return handle;
}

std::shared_ptr<Project> GetProject(LCProjectHandle handle) {
    std::lock_guard<std::mutex> lock(g_ProjectMutex);
    auto it = g_ProjectHandles.find(handle);
    return it != g_ProjectHandles.end() ? it->second : nullptr;
}

bool RemoveProjectHandle(LCProjectHandle handle) {
    std::lock_guard<std::mutex> lock(g_ProjectMutex);
    return g_ProjectHandles.erase(handle) > 0;
}

LCResult WriteAllocatedString(const std::string& value, char** out_str) {
    if (!out_str) {
        SetError(LC_ERROR_NULL_POINTER, "output string pointer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    char* buffer = DuplicateString(value);
    if (!buffer) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate string result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    *out_str = buffer;
    return LC_SUCCESS;
}

std::shared_ptr<Project> RequireProject(LCProjectHandle handle) {
    auto project = GetProject(handle);
    if (!project) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid project handle");
    }
    return project;
}

} // anonymous namespace

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

LC_API LCResult lc_project_create(LCProjectHandle* out_project) {
    if (!out_project) {
        SetError(LC_ERROR_NULL_POINTER, "out_project is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_project = CreateProjectHandle(std::make_shared<Project>());
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create project");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_project_create_from_path(const char* project_path, LCProjectHandle* out_project) {
    if (!project_path || !out_project) {
        SetError(LC_ERROR_NULL_POINTER, "project_path or out_project is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto project = std::make_shared<Project>();
        if (!project->Load(project_path)) {
            SetError(LC_ERROR_FILE_NOT_FOUND, "Failed to load project file");
            return LC_ERROR_FILE_NOT_FOUND;
        }
        *out_project = CreateProjectHandle(project);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception loading project");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_project_destroy(LCProjectHandle project) {
    if (!RemoveProjectHandle(project)) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid project handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_project_load(LCProjectHandle project, const char* project_path) {
    if (!project_path) {
        SetError(LC_ERROR_NULL_POINTER, "project_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    try {
        if (!proj->Load(project_path)) {
            SetError(LC_ERROR_FILE_NOT_FOUND, "Failed to load project file");
            return LC_ERROR_FILE_NOT_FOUND;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception loading project");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_project_save(LCProjectHandle project) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    try {
        if (!proj->Save()) {
            SetError(LC_ERROR_OPERATION_FAILED, "Failed to save project");
            return LC_ERROR_OPERATION_FAILED;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception saving project");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_project_save_as(LCProjectHandle project, const char* project_path) {
    if (!project_path) {
        SetError(LC_ERROR_NULL_POINTER, "project_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    try {
        if (!proj->SaveAs(project_path)) {
            SetError(LC_ERROR_OPERATION_FAILED, "Failed to save project");
            return LC_ERROR_OPERATION_FAILED;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception saving project");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_project_is_loaded(LCProjectHandle project, bool* out_loaded) {
    if (!out_loaded) {
        SetError(LC_ERROR_NULL_POINTER, "out_loaded is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    *out_loaded = proj->IsLoaded();
    return LC_SUCCESS;
}

/* ============================================================================
 * Paths
 * ============================================================================ */

LC_API LCResult lc_project_get_path(LCProjectHandle project, char** out_path) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    return WriteAllocatedString(proj->GetProjectPath(), out_path);
}

LC_API LCResult lc_project_get_directory(LCProjectHandle project, char** out_path) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    return WriteAllocatedString(proj->GetProjectDirectory(), out_path);
}

LC_API LCResult lc_project_get_scenes_directory(LCProjectHandle project, char** out_path) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    return WriteAllocatedString(proj->GetScenesDirectory(), out_path);
}

LC_API LCResult lc_project_get_assets_directory(LCProjectHandle project, char** out_path) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    return WriteAllocatedString(proj->GetAssetsDirectory(), out_path);
}

LC_API LCResult lc_project_get_scripts_directory(LCProjectHandle project, char** out_path) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    return WriteAllocatedString(proj->GetScriptsDirectory(), out_path);
}

/* ============================================================================
 * String settings accessor macros
 * ============================================================================ */

#define LC_PROJECT_STRING_GETTER(fn_name, field)                              \
    LC_API LCResult fn_name(LCProjectHandle project, char** out_value) {      \
        auto proj = RequireProject(project);                                  \
        if (!proj) return LC_ERROR_INVALID_HANDLE;                            \
        return WriteAllocatedString(proj->GetSettings().field, out_value);    \
    }

#define LC_PROJECT_STRING_SETTER(fn_name, field)                              \
    LC_API LCResult fn_name(LCProjectHandle project, const char* value) {     \
        if (!value) {                                                         \
            SetError(LC_ERROR_NULL_POINTER, "value is NULL");                 \
            return LC_ERROR_NULL_POINTER;                                     \
        }                                                                     \
        auto proj = RequireProject(project);                                  \
        if (!proj) return LC_ERROR_INVALID_HANDLE;                            \
        proj->GetSettings().field = value;                                    \
        return LC_SUCCESS;                                                    \
    }

LC_PROJECT_STRING_GETTER(lc_project_get_name, projectName)
LC_PROJECT_STRING_SETTER(lc_project_set_name, projectName)
LC_PROJECT_STRING_GETTER(lc_project_get_creator_name, creatorName)
LC_PROJECT_STRING_SETTER(lc_project_set_creator_name, creatorName)
LC_PROJECT_STRING_GETTER(lc_project_get_version, version)
LC_PROJECT_STRING_SETTER(lc_project_set_version, version)
LC_PROJECT_STRING_GETTER(lc_project_get_main_scene, mainScene)
LC_PROJECT_STRING_SETTER(lc_project_set_main_scene, mainScene)
LC_PROJECT_STRING_GETTER(lc_project_get_default_font, defaultFont)
LC_PROJECT_STRING_SETTER(lc_project_set_default_font, defaultFont)
LC_PROJECT_STRING_GETTER(lc_project_get_default_theme, defaultTheme)
LC_PROJECT_STRING_SETTER(lc_project_set_default_theme, defaultTheme)

/* ============================================================================
 * Scalar settings accessor macros
 * ============================================================================ */

#define LC_PROJECT_SCALAR_GETTER(fn_name, c_type, field)                      \
    LC_API LCResult fn_name(LCProjectHandle project, c_type* out_value) {     \
        if (!out_value) {                                                     \
            SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");             \
            return LC_ERROR_NULL_POINTER;                                     \
        }                                                                     \
        auto proj = RequireProject(project);                                  \
        if (!proj) return LC_ERROR_INVALID_HANDLE;                            \
        *out_value = static_cast<c_type>(proj->GetSettings().field);          \
        return LC_SUCCESS;                                                    \
    }

#define LC_PROJECT_SCALAR_SETTER(fn_name, c_type, field, field_type)          \
    LC_API LCResult fn_name(LCProjectHandle project, c_type value) {          \
        auto proj = RequireProject(project);                                  \
        if (!proj) return LC_ERROR_INVALID_HANDLE;                            \
        proj->GetSettings().field = static_cast<field_type>(value);           \
        return LC_SUCCESS;                                                    \
    }

LC_PROJECT_SCALAR_GETTER(lc_project_get_window_width, int, windowWidth)
LC_PROJECT_SCALAR_SETTER(lc_project_set_window_width, int, windowWidth, int)
LC_PROJECT_SCALAR_GETTER(lc_project_get_window_height, int, windowHeight)
LC_PROJECT_SCALAR_SETTER(lc_project_set_window_height, int, windowHeight, int)
LC_PROJECT_SCALAR_GETTER(lc_project_get_window_size_override, bool, windowSizeOverride)
LC_PROJECT_SCALAR_SETTER(lc_project_set_window_size_override, bool, windowSizeOverride, bool)
LC_PROJECT_SCALAR_GETTER(lc_project_get_window_size_override_width, int, windowSizeOverrideWidth)
LC_PROJECT_SCALAR_SETTER(lc_project_set_window_size_override_width, int, windowSizeOverrideWidth, int)
LC_PROJECT_SCALAR_GETTER(lc_project_get_window_size_override_height, int, windowSizeOverrideHeight)
LC_PROJECT_SCALAR_SETTER(lc_project_set_window_size_override_height, int, windowSizeOverrideHeight, int)
LC_PROJECT_SCALAR_GETTER(lc_project_get_fullscreen, bool, fullscreen)
LC_PROJECT_SCALAR_SETTER(lc_project_set_fullscreen, bool, fullscreen, bool)
LC_PROJECT_SCALAR_GETTER(lc_project_get_vsync, bool, vsync)
LC_PROJECT_SCALAR_SETTER(lc_project_set_vsync, bool, vsync, bool)
LC_PROJECT_SCALAR_GETTER(lc_project_get_target_frame_rate, int, targetFrameRate)
LC_PROJECT_SCALAR_SETTER(lc_project_set_target_frame_rate, int, targetFrameRate, int)
LC_PROJECT_SCALAR_GETTER(lc_project_get_max_undo_steps, int, maxUndoSteps)
LC_PROJECT_SCALAR_SETTER(lc_project_set_max_undo_steps, int, maxUndoSteps, int)
LC_PROJECT_SCALAR_GETTER(lc_project_get_physics_tick_rate, float, physicsTickRate)
LC_PROJECT_SCALAR_SETTER(lc_project_set_physics_tick_rate, float, physicsTickRate, float)

/* ============================================================================
 * Vector / color settings
 * ============================================================================ */

LC_API LCResult lc_project_get_clear_color(LCProjectHandle project, LCColor* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    const lupine::math::Color& c = proj->GetSettings().clearColor;
    *out_value = LCColor{c.r, c.g, c.b, c.a};
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_clear_color(LCProjectHandle project, LCColor value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().clearColor = lupine::math::Color(value.r, value.g, value.b, value.a);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_get_reference_resolution(LCProjectHandle project, LCVec2* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    const lupine::math::Vec2& v = proj->GetSettings().referenceResolution;
    *out_value = LCVec2{v.x, v.y};
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_reference_resolution(LCProjectHandle project, LCVec2 value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().referenceResolution = lupine::math::Vec2(value.x, value.y);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_get_gravity_2d(LCProjectHandle project, LCVec2* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    const lupine::math::Vec2& v = proj->GetSettings().gravity2D;
    *out_value = LCVec2{v.x, v.y};
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_gravity_2d(LCProjectHandle project, LCVec2 value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().gravity2D = lupine::math::Vec2(value.x, value.y);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_get_gravity_3d(LCProjectHandle project, LCVec3* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    const lupine::math::Vec3& v = proj->GetSettings().gravity3D;
    *out_value = LCVec3{v.x, v.y, v.z};
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_gravity_3d(LCProjectHandle project, LCVec3 value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().gravity3D = lupine::math::Vec3(value.x, value.y, value.z);
    return LC_SUCCESS;
}

/* ============================================================================
 * Enum settings
 * ============================================================================ */

LC_API LCResult lc_project_get_scale_mode(LCProjectHandle project, LCScaleMode* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    *out_value = static_cast<LCScaleMode>(proj->GetSettings().scaleMode);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_scale_mode(LCProjectHandle project, LCScaleMode value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().scaleMode = static_cast<ProjectSettings::ScaleMode>(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_get_viewport_scale_mode(LCProjectHandle project, LCViewportScaleMode* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    *out_value = static_cast<LCViewportScaleMode>(proj->GetSettings().viewportScaleMode);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_viewport_scale_mode(LCProjectHandle project, LCViewportScaleMode value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().viewportScaleMode = static_cast<ProjectSettings::ViewportScaleMode>(value);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_get_texture_filtering(LCProjectHandle project, LCTextureFiltering* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    *out_value = static_cast<LCTextureFiltering>(proj->GetSettings().textureFiltering);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_texture_filtering(LCProjectHandle project, LCTextureFiltering value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().textureFiltering = static_cast<ProjectSettings::TextureFiltering>(value);
    return LC_SUCCESS;
}

/* ============================================================================
 * Splash screens
 * ============================================================================ */

LC_API LCResult lc_project_get_splash_enabled(LCProjectHandle project, bool* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    *out_value = proj->GetSettings().splashScreenSettings.enabled;
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_splash_enabled(LCProjectHandle project, bool value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().splashScreenSettings.enabled = value;
    return LC_SUCCESS;
}

LC_API LCResult lc_project_get_splash_allow_skip(LCProjectHandle project, bool* out_value) {
    if (!out_value) {
        SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    *out_value = proj->GetSettings().splashScreenSettings.allowSkip;
    return LC_SUCCESS;
}

LC_API LCResult lc_project_set_splash_allow_skip(LCProjectHandle project, bool value) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().splashScreenSettings.allowSkip = value;
    return LC_SUCCESS;
}

LC_API LCResult lc_project_get_splash_entry_count(LCProjectHandle project, int* out_count) {
    if (!out_count) {
        SetError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    *out_count = static_cast<int>(proj->GetSettings().splashScreenSettings.entries.size());
    return LC_SUCCESS;
}

LC_API LCResult lc_project_get_splash_entry(LCProjectHandle project, int index,
                                            char** out_image_path, float* out_fade_in,
                                            float* out_hold, float* out_fade_out) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    auto& entries = proj->GetSettings().splashScreenSettings.entries;
    if (index < 0 || static_cast<size_t>(index) >= entries.size()) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Splash entry index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }
    const SplashScreenEntry& entry = entries[static_cast<size_t>(index)];
    if (out_fade_in) *out_fade_in = entry.fadeInDuration;
    if (out_hold) *out_hold = entry.holdDuration;
    if (out_fade_out) *out_fade_out = entry.fadeOutDuration;
    if (out_image_path) {
        return WriteAllocatedString(entry.imagePath, out_image_path);
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_project_add_splash_entry(LCProjectHandle project, const char* image_path,
                                            float fade_in, float hold, float fade_out) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().splashScreenSettings.entries.emplace_back(
        image_path ? image_path : "", fade_in, hold, fade_out);
    return LC_SUCCESS;
}

LC_API LCResult lc_project_clear_splash_entries(LCProjectHandle project) {
    auto proj = RequireProject(project);
    if (!proj) return LC_ERROR_INVALID_HANDLE;
    proj->GetSettings().splashScreenSettings.entries.clear();
    return LC_SUCCESS;
}
