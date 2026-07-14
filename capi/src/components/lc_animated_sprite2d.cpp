/**
 * @file lc_animated_sprite2d.cpp
 * @brief Lupine Engine C API - AnimatedSprite2D implementation
 */

#include "components/lc_animated_sprite2d.h"
#include "../core/lc_internal.h"

#include <lupine/components/AnimatedSprite2D.hpp>
#include <cstring>

namespace {

void SetAnimatedSprite2DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API Vec2 to engine Vec2
lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);
}

// Convert engine Vec2 to C API Vec2
LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};
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
 * AnimatedSprite2D Creation
 * ============================================================================ */

LC_API LCResult lc_animated_sprite2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::AnimatedSprite2D>();
        comp->DefineProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to create AnimatedSprite2D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Animation File
 * ============================================================================ */

LC_API LCResult lc_animated_sprite2d_get_animation_file(LCComponentHandle component, char* out_path, int path_size) {
    if (!out_path || path_size <= 0) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_path is NULL or path_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string path = sprite->GetAnimationFilePath();
        CopyStringToBuffer(out_path, path_size, path.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get animation file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_animation_file(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetAnimationFilePath(std::string(filepath));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set animation file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Animation Name
 * ============================================================================ */

LC_API LCResult lc_animated_sprite2d_get_animation_name(LCComponentHandle component, char* out_name, int name_size) {
    if (!out_name || name_size <= 0) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_name is NULL or name_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string name = sprite->GetAnimationName();
        CopyStringToBuffer(out_name, name_size, name.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get animation name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_animation_name(LCComponentHandle component, const char* name) {
    if (!name) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetAnimationName(std::string(name));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set animation name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Playback State
 * ============================================================================ */

LC_API LCResult lc_animated_sprite2d_is_playing(LCComponentHandle component, bool* out_playing) {
    if (!out_playing) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_playing is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_playing = sprite->IsPlaying();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get playing state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_playing(LCComponentHandle component, bool playing) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetPlaying(playing);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set playing state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_get_loop(LCComponentHandle component, bool* out_loop) {
    if (!out_loop) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_loop is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_loop = sprite->GetLoop();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get loop state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_loop(LCComponentHandle component, bool loop) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetLoop(loop);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set loop state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_get_auto_play(LCComponentHandle component, bool* out_auto_play) {
    if (!out_auto_play) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_auto_play is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_auto_play = sprite->GetAutoPlay();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get auto-play state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_auto_play(LCComponentHandle component, bool auto_play) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetAutoPlay(auto_play);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set auto-play state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Animation Control
 * ============================================================================ */

LC_API LCResult lc_animated_sprite2d_play(LCComponentHandle component, const char* animation_name) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string name = animation_name ? animation_name : "";
        sprite->Play(name);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to play animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_stop(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->Stop();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to stop animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_pause(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->Pause();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to pause animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_resume(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->Resume();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to resume animation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_get_frame(LCComponentHandle component, int* out_frame) {
    if (!out_frame) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_frame is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_frame = sprite->GetCurrentFrame();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get current frame");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_frame(LCComponentHandle component, int frame) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetFrame(frame);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set frame");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Visual Properties
 * ============================================================================ */

LC_API LCResult lc_animated_sprite2d_get_offset(LCComponentHandle component, LCVec2* out_offset) {
    if (!out_offset) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_offset = FromEngineVec2(sprite->GetOffset());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_offset(LCComponentHandle component, LCVec2 offset) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetOffset(ToEngineVec2(offset));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_get_flip_h(LCComponentHandle component, bool* out_flip_h) {
    if (!out_flip_h) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_flip_h is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_flip_h = sprite->GetFlipH();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get flip_h");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_flip_h(LCComponentHandle component, bool flip_h) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetFlipH(flip_h);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set flip_h");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_get_flip_v(LCComponentHandle component, bool* out_flip_v) {
    if (!out_flip_v) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_flip_v is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_flip_v = sprite->GetFlipV();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get flip_v");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_flip_v(LCComponentHandle component, bool flip_v) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetFlipV(flip_v);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set flip_v");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_get_modulate(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(sprite->GetModulate());
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get modulate");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_modulate(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetModulate(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set modulate");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_get_pixel_snap(LCComponentHandle component, bool* out_pixel_snap) {
    if (!out_pixel_snap) {
        SetAnimatedSprite2DError(LC_ERROR_NULL_POINTER, "out_pixel_snap is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_pixel_snap = sprite->GetPixelSnap();
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get pixel_snap");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animated_sprite2d_set_pixel_snap(LCComponentHandle component, bool pixel_snap) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAnimatedSprite2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::AnimatedSprite2D>(comp);
        if (!sprite) {
            SetAnimatedSprite2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AnimatedSprite2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        sprite->SetPixelSnap(pixel_snap);
        return LC_SUCCESS;
    } catch (...) {
        SetAnimatedSprite2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set pixel_snap");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
