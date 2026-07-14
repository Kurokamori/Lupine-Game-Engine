/**
 * @file lc_animated_sprite3d.cpp
 * @brief Lupine Engine C API - AnimatedSprite3D implementation
 */

#include "components/lc_animated_sprite3d.h"
#include "../core/lc_internal.h"

#include <lupine/components/AnimatedSprite3D.hpp>
#include <cstring>

namespace {

void SetAnimatedSprite3DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API Vec3 to engine Vec3
lupine::math::Vec3 ToEngineVec3(LCVec3 vec) {
    return lupine::math::Vec3(vec.x, vec.y, vec.z);
}

// Convert engine Vec3 to C API Vec3
LCVec3 FromEngineVec3(const lupine::math::Vec3& vec) {
    return LCVec3{vec.x, vec.y, vec.z};
}

// Convert C API color to engine color
lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

// Convert engine color to C API color
LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

// Convert billboard mode
lupine::components::BillboardMode ToBillboardMode(LCBillboardMode mode) {
    switch (mode) {
        case LC_BILLBOARD_DISABLED: return lupine::components::BillboardMode::Disabled;
        case LC_BILLBOARD_ENABLED: return lupine::components::BillboardMode::Enabled;
        case LC_BILLBOARD_Y_AXIS_ONLY: return lupine::components::BillboardMode::YAxisOnly;
        default: return lupine::components::BillboardMode::Disabled;
    }
}

LCBillboardMode FromBillboardMode(lupine::components::BillboardMode mode) {
    switch (mode) {
        case lupine::components::BillboardMode::Disabled: return LC_BILLBOARD_DISABLED;
        case lupine::components::BillboardMode::Enabled: return LC_BILLBOARD_ENABLED;
        case lupine::components::BillboardMode::YAxisOnly: return LC_BILLBOARD_Y_AXIS_ONLY;
        default: return LC_BILLBOARD_DISABLED;
    }
}

} // anonymous namespace

/* ============================================================================
 * AnimatedSprite3D Creation
 * ============================================================================ */

LC_API LCResult lc_animated_sprite3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::AnimatedSprite3D>();
        comp->DefineProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to create AnimatedSprite3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Animation File
 * ============================================================================ */

LC_API LCResult lc_animated_sprite3d_get_animation_file(LCComponentHandle component, char* out_path, int path_size) {
    if (!out_path || path_size <= 0) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_path is NULL or path_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string path = sprite->GetAnimationFilePath();
        CopyStringToBuffer(out_path, path_size, path.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get animation file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_animation_file(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetAnimationFilePath(std::string(filepath));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set animation file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Animation Name
 * ============================================================================ */

LC_API LCResult lc_animated_sprite3d_get_animation_name(LCComponentHandle component, char* out_name, int name_size) {
    if (!out_name || name_size <= 0) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_name is NULL or name_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string name = sprite->GetAnimationName();
        CopyStringToBuffer(out_name, name_size, name.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get animation name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_animation_name(LCComponentHandle component, const char* name) {
    if (!name) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetAnimationName(std::string(name));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set animation name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Playback State
 * ============================================================================ */

LC_API LCResult lc_animated_sprite3d_is_playing(LCComponentHandle component, bool* out_playing) {
    if (!out_playing) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_playing is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_playing = sprite->IsPlaying();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get playing state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_playing(LCComponentHandle component, bool playing) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetPlaying(playing);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set playing state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_loop(LCComponentHandle component, bool* out_loop) {
    if (!out_loop) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_loop is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_loop = sprite->GetLoop();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get loop state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_loop(LCComponentHandle component, bool loop) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetLoop(loop);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set loop state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_auto_play(LCComponentHandle component, bool* out_auto_play) {
    if (!out_auto_play) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_auto_play is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_auto_play = sprite->GetAutoPlay();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get auto-play state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_auto_play(LCComponentHandle component, bool auto_play) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetAutoPlay(auto_play);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set auto-play state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Animation Control
 * ============================================================================ */

LC_API LCResult lc_animated_sprite3d_play(LCComponentHandle component, const char* animation_name) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string name = animation_name ? animation_name : "";
        sprite->Play(name);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to play animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_stop(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->Stop();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to stop animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_pause(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->Pause();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to pause animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_resume(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->Resume();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to resume animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_frame(LCComponentHandle component, int* out_frame) {
    if (!out_frame) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_frame is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_frame = sprite->GetCurrentFrame();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get current frame");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_frame(LCComponentHandle component, int frame) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetFrame(frame);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set frame");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Visual Properties
 * ============================================================================ */

LC_API LCResult lc_animated_sprite3d_get_offset(LCComponentHandle component, LCVec3* out_offset) {
    if (!out_offset) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_offset = FromEngineVec3(sprite->GetOffset());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_offset(LCComponentHandle component, LCVec3 offset) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetOffset(ToEngineVec3(offset));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_flip_h(LCComponentHandle component, bool* out_flip_h) {
    if (!out_flip_h) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_flip_h is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_flip_h = sprite->GetFlipH();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get flip_h");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_flip_h(LCComponentHandle component, bool flip_h) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetFlipH(flip_h);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set flip_h");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_flip_v(LCComponentHandle component, bool* out_flip_v) {
    if (!out_flip_v) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_flip_v is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_flip_v = sprite->GetFlipV();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get flip_v");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_flip_v(LCComponentHandle component, bool flip_v) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetFlipV(flip_v);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set flip_v");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_modulate(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(sprite->GetModulate());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get modulate");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_modulate(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetModulate(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set modulate");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * 3D-Specific Properties
 * ============================================================================ */

LC_API LCResult lc_animated_sprite3d_get_billboard(LCComponentHandle component, LCBillboardMode* out_mode) {
    if (!out_mode) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_mode is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_mode = FromBillboardMode(sprite->GetBillboard());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get billboard mode");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_billboard(LCComponentHandle component, LCBillboardMode mode) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetBillboard(ToBillboardMode(mode));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set billboard mode");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_cast_shadows(LCComponentHandle component, bool* out_cast) {
    if (!out_cast) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_cast is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_cast = sprite->GetCastShadows();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get cast shadows");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_cast_shadows(LCComponentHandle component, bool cast) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetCastShadows(cast);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set cast shadows");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_receive_shadows(LCComponentHandle component, bool* out_receive) {
    if (!out_receive) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_receive is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_receive = sprite->GetReceiveShadows();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get receive shadows");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_receive_shadows(LCComponentHandle component, bool receive) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetReceiveShadows(receive);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set receive shadows");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_get_double_sided(LCComponentHandle component, bool* out_double_sided) {
    if (!out_double_sided) {
        SetAnimatedSprite3DError(LC_ERROR_NULL_POINTER, "out_double_sided is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_double_sided = sprite->GetDoubleSided();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get double sided");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite3d_set_double_sided(LCComponentHandle component, bool double_sided) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite3D>(comp);
        if (!sprite) {
            SetAnimatedSprite3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetDoubleSided(double_sided);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set double sided");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
