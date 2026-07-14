/**
 * @file lc_skeletal_mesh3d.cpp
 * @brief Lupine Engine C API - SkeletalMesh3D implementation
 */

#include "components/lc_skeletal_mesh3d.h"
#include "../core/lc_internal.h"

#include <lupine/components/SkeletalMesh3D.hpp>
#include <cstring>

namespace {

void SetSkeletalMesh3DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API color to engine color
lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

// Convert engine color to C API color
LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

} // anonymous namespace

/* ============================================================================
 * SkeletalMesh3D Creation
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::SkeletalMesh3D>(compName);
        comp->DefineProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to create SkeletalMesh3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Model Loading
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_get_model_path(LCComponentHandle component, char* out_path, int path_size) {
    if (!out_path || path_size <= 0) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_path is NULL or path_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string path = mesh->GetModelPath();
        CopyStringToBuffer(out_path, path_size, path.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get model path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_model_path(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetModelPath(std::string(filepath));
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set model path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_load_model(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        bool success = mesh->LoadModel(std::string(filepath));
        if (!success) {
            SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to load model");
            return LC_ERROR_INTERNAL_ERROR;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to load model");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Animation Information
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_get_animation_count(LCComponentHandle component, int* out_count) {
    if (!out_count) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        auto names = mesh->GetAnimationNames();
        *out_count = static_cast<int>(names.size());
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get animation count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_animation_name_at(LCComponentHandle component, int index, char* out_name, int name_size) {
    if (!out_name || name_size <= 0) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_name is NULL or name_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        auto names = mesh->GetAnimationNames();
        if (index < 0 || index >= static_cast<int>(names.size())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        CopyStringToBuffer(out_name, name_size, names[index].c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get animation name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_current_animation(LCComponentHandle component, char* out_name, int name_size) {
    if (!out_name || name_size <= 0) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_name is NULL or name_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string name = mesh->GetCurrentAnimation();
        CopyStringToBuffer(out_name, name_size, name.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get current animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_current_animation(LCComponentHandle component, const char* name) {
    if (!name) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetCurrentAnimation(std::string(name));
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set current animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_default_animation(LCComponentHandle component, char* out_name, int name_size) {
    if (!out_name || name_size <= 0) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_name is NULL or name_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string name = mesh->GetDefaultAnimation();
        CopyStringToBuffer(out_name, name_size, name.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get default animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_default_animation(LCComponentHandle component, const char* name) {
    if (!name) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetDefaultAnimation(std::string(name));
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set default animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Playback Control
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_play_animation(LCComponentHandle component, const char* animation_name, bool loop) {
    if (!animation_name) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "animation_name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->PlayAnimation(std::string(animation_name), loop);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to play animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_stop_animation(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->StopAnimation();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to stop animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_is_playing(LCComponentHandle component, bool* out_playing) {
    if (!out_playing) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_playing is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_playing = mesh->IsPlaying();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get playing state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_playing(LCComponentHandle component, bool playing) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetPlaying(playing);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set playing state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_animation_time(LCComponentHandle component, float* out_time) {
    if (!out_time) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_time is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_time = mesh->GetAnimationTime();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get animation time");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_animation_time(LCComponentHandle component, float time) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetAnimationTime(time);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set animation time");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Playback Properties
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_get_playback_speed(LCComponentHandle component, float* out_speed) {
    if (!out_speed) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_speed is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_speed = mesh->GetPlaybackSpeed();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get playback speed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_playback_speed(LCComponentHandle component, float speed) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetPlaybackSpeed(speed);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set playback speed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_loop(LCComponentHandle component, bool* out_loop) {
    if (!out_loop) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_loop is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_loop = mesh->GetLoop();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get loop state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_loop(LCComponentHandle component, bool loop) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetLoop(loop);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set loop state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_auto_play(LCComponentHandle component, bool* out_auto_play) {
    if (!out_auto_play) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_auto_play is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_auto_play = mesh->GetAutoPlay();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get auto-play state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_auto_play(LCComponentHandle component, bool auto_play) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetAutoPlay(auto_play);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set auto-play state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * GPU Skinning
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_get_gpu_skinning(LCComponentHandle component, bool* out_gpu_skinning) {
    if (!out_gpu_skinning) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_gpu_skinning is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_gpu_skinning = mesh->GetGPUSkinning();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get GPU skinning state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_gpu_skinning(LCComponentHandle component, bool gpu_skinning) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetGPUSkinning(gpu_skinning);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set GPU skinning state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Root Motion
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_get_root_motion_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_enabled = mesh->GetRootMotionEnabled();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get root motion state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_root_motion_enabled(LCComponentHandle component, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetRootMotionEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set root motion state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_root_bone_name(LCComponentHandle component, char* out_name, int name_size) {
    if (!out_name || name_size <= 0) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_name is NULL or name_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string name = mesh->GetRootBoneName();
        CopyStringToBuffer(out_name, name_size, name.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get root bone name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_root_bone_name(LCComponentHandle component, const char* name) {
    if (!name) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetRootBoneName(std::string(name));
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set root bone name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Shadow Settings
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_get_cast_shadow(LCComponentHandle component, bool* out_cast) {
    if (!out_cast) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_cast is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_cast = mesh->GetCastShadow();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get cast shadow state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_cast_shadow(LCComponentHandle component, bool cast) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetCastShadow(cast);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set cast shadow state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_receive_shadow(LCComponentHandle component, bool* out_receive) {
    if (!out_receive) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_receive is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_receive = mesh->GetReceiveShadow();
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get receive shadow state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_receive_shadow(LCComponentHandle component, bool receive) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetReceiveShadow(receive);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set receive shadow state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Material Slots
 * ============================================================================ */

LC_API LCResult lc_skeletal_mesh3d_get_material_slot_count(LCComponentHandle component, int* out_count) {
    if (!out_count) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_count = static_cast<int>(mesh->GetMaterialSlotCount());
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_material_slot_name(LCComponentHandle component, int slot_index, char* out_name, int name_size) {
    if (!out_name || name_size <= 0) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_name is NULL or name_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        auto slot = mesh->GetMaterialSlot(static_cast<uint32_t>(slot_index));
        if (!slot) {
            SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot");
            return LC_ERROR_INTERNAL_ERROR;
        }

        CopyStringToBuffer(out_name, name_size, slot->name.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_material_slot_override_enabled(LCComponentHandle component, int slot_index, bool* out_enabled) {
    if (!out_enabled) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        auto slot = mesh->GetMaterialSlot(static_cast<uint32_t>(slot_index));
        if (!slot) {
            SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot");
            return LC_ERROR_INTERNAL_ERROR;
        }

        *out_enabled = slot->enableOverride;
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot override enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_material_slot_override_enabled(LCComponentHandle component, int slot_index, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        mesh->SetMaterialSlotOverrideEnabled(static_cast<uint32_t>(slot_index), enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set material slot override enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_material_slot_albedo_color(LCComponentHandle component, int slot_index, LCColor* out_color) {
    if (!out_color) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        auto slot = mesh->GetMaterialSlot(static_cast<uint32_t>(slot_index));
        if (!slot) {
            SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot");
            return LC_ERROR_INTERNAL_ERROR;
        }

        *out_color = FromEngineColor(slot->albedoColor);
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot albedo color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_material_slot_albedo_color(LCComponentHandle component, int slot_index, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        mesh->SetMaterialSlotAlbedoColor(static_cast<uint32_t>(slot_index), ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set material slot albedo color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_material_slot_albedo_texture(LCComponentHandle component, int slot_index, const char* texture_path) {
    if (!texture_path) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "texture_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        mesh->SetMaterialSlotAlbedoTexture(static_cast<uint32_t>(slot_index), std::string(texture_path));
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set material slot albedo texture");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_material_slot_metallic(LCComponentHandle component, int slot_index, float* out_metallic) {
    if (!out_metallic) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_metallic is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        auto slot = mesh->GetMaterialSlot(static_cast<uint32_t>(slot_index));
        if (!slot) {
            SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot");
            return LC_ERROR_INTERNAL_ERROR;
        }

        *out_metallic = slot->metallic;
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot metallic");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_material_slot_metallic(LCComponentHandle component, int slot_index, float metallic) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        auto slot = mesh->GetMaterialSlot(static_cast<uint32_t>(slot_index));
        if (!slot) {
            SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot");
            return LC_ERROR_INTERNAL_ERROR;
        }

        slot->metallic = metallic;
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set material slot metallic");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_get_material_slot_roughness(LCComponentHandle component, int slot_index, float* out_roughness) {
    if (!out_roughness) {
        SetSkeletalMesh3DError(LC_ERROR_NULL_POINTER, "out_roughness is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        auto slot = mesh->GetMaterialSlot(static_cast<uint32_t>(slot_index));
        if (!slot) {
            SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot");
            return LC_ERROR_INTERNAL_ERROR;
        }

        *out_roughness = slot->roughness;
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot roughness");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_skeletal_mesh3d_set_material_slot_roughness(LCComponentHandle component, int slot_index, float roughness) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::SkeletalMesh3D>(comp);
        if (!mesh) {
            SetSkeletalMesh3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SkeletalMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        if (slot_index < 0 || slot_index >= static_cast<int>(mesh->GetMaterialSlotCount())) {
            SetSkeletalMesh3DError(LC_ERROR_INVALID_PARAMETER, "Slot index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        auto slot = mesh->GetMaterialSlot(static_cast<uint32_t>(slot_index));
        if (!slot) {
            SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get material slot");
            return LC_ERROR_INTERNAL_ERROR;
        }

        slot->roughness = roughness;
        return LC_SUCCESS;
    } catch (...) {
        SetSkeletalMesh3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set material slot roughness");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
