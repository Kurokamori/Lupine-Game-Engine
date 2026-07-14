/**
 * @file lc_gif_player.cpp
 * @brief Lupine Engine C API - GifPlayer implementation
 */

#include "components/lc_gif_player.h"
#include "../core/lc_internal.h"

#include <lupine/components/GifPlayer.hpp>
#include <cstring>

namespace {

using lupine::components::GifPlayer;

// Resolve a handle to a GifPlayer*, reporting the appropriate error otherwise.
// The component registry keeps the shared_ptr alive while the handle is valid.
GifPlayer* ResolveGif(LCComponentHandle component, LCResult* err) {
    auto comp = GetComponent(component);
    if (!comp) {
        ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        *err = LC_ERROR_INVALID_HANDLE;
        return nullptr;
    }
    auto gif = std::dynamic_pointer_cast<GifPlayer>(comp);
    if (!gif) {
        ::SetError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a GifPlayer");
        *err = LC_ERROR_COMPONENT_INVALID_TYPE;
        return nullptr;
    }
    return gif.get();
}

void CopyString(const std::string& src, char* out, int size) {
    if (!out || size <= 0) {
        return;
    }
    CopyStringToBuffer(out, static_cast<size_t>(size), src.c_str());
}

} // namespace

/* ============================================================================
 * Creation
 * ============================================================================ */

LC_API LCResult lc_gif_player_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto comp = std::make_shared<GifPlayer>();
        comp->DefineProperties();
        if (name && name[0] != '\0') {
            comp->SetName(name);
        }
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create GifPlayer");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Source
 * ============================================================================ */

LC_API LCResult lc_gif_player_get_gif_path(LCComponentHandle component, char* out_path, int path_size) {
    if (!out_path || path_size <= 0) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_path is NULL or path_size invalid");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        CopyString(gif->GetGifPath(), out_path, path_size);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_gif_path failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_gif_path(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        ::SetError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetGifPath(std::string(filepath));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_gif_path failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_is_loaded(LCComponentHandle component, bool* out_loaded) {
    if (!out_loaded) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_loaded is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_loaded = gif->IsLoaded();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_is_loaded failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Playback control
 * ============================================================================ */

#define LC_GIF_VOID_CALL(fn_name, call)                                       \
    LC_API LCResult fn_name(LCComponentHandle component) {                    \
        try {                                                                 \
            LCResult err;                                                     \
            GifPlayer* gif = ResolveGif(component, &err);                     \
            if (!gif) return err;                                             \
            call;                                                             \
            return LC_SUCCESS;                                                \
        } catch (...) {                                                       \
            ::SetError(LC_ERROR_INTERNAL_ERROR, #fn_name " failed");          \
            return LC_ERROR_INTERNAL_ERROR;                                   \
        }                                                                     \
    }

LC_GIF_VOID_CALL(lc_gif_player_play, gif->Play())
LC_GIF_VOID_CALL(lc_gif_player_stop, gif->Stop())
LC_GIF_VOID_CALL(lc_gif_player_pause, gif->Pause())
LC_GIF_VOID_CALL(lc_gif_player_resume, gif->Resume())
LC_GIF_VOID_CALL(lc_gif_player_replay, gif->Replay())

#undef LC_GIF_VOID_CALL

LC_API LCResult lc_gif_player_is_playing(LCComponentHandle component, bool* out_playing) {
    if (!out_playing) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_playing is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_playing = gif->IsPlaying();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_is_playing failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_frame(LCComponentHandle component, int frame_index) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetFrame(frame_index);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_frame failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_current_frame(LCComponentHandle component, int* out_frame) {
    if (!out_frame) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_frame is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_frame = gif->GetCurrentFrame();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_current_frame failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_frame_count(LCComponentHandle component, int* out_count) {
    if (!out_count) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_count = gif->GetFrameCount();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_frame_count failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Playback settings
 * ============================================================================ */

LC_API LCResult lc_gif_player_set_loop_mode(LCComponentHandle component, LCGifLoopMode mode) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetLoopMode(static_cast<GifPlayer::LoopMode>(mode));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_loop_mode failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_loop_mode(LCComponentHandle component, LCGifLoopMode* out_mode) {
    if (!out_mode) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_mode is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_mode = static_cast<LCGifLoopMode>(gif->GetLoopMode());
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_loop_mode failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_auto_play(LCComponentHandle component, bool auto_play) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetAutoPlay(auto_play);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_auto_play failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_auto_play(LCComponentHandle component, bool* out_auto_play) {
    if (!out_auto_play) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_auto_play is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_auto_play = gif->GetAutoPlay();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_auto_play failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_preview_in_editor(LCComponentHandle component, bool enabled) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetPreviewInEditor(enabled);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_preview_in_editor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_preview_in_editor(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_enabled = gif->GetPreviewInEditor();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_preview_in_editor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_speed(LCComponentHandle component, float speed) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetSpeed(speed);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_speed failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_speed(LCComponentHandle component, float* out_speed) {
    if (!out_speed) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_speed is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_speed = gif->GetSpeed();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_speed failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_fps_override(LCComponentHandle component, float fps) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetFpsOverride(fps);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_fps_override failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_fps_override(LCComponentHandle component, float* out_fps) {
    if (!out_fps) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_fps is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_fps = gif->GetFpsOverride();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_fps_override failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Visual properties
 * ============================================================================ */

LC_API LCResult lc_gif_player_set_offset(LCComponentHandle component, LCVec2 offset) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetOffset(lupine::math::Vec2(offset.x, offset.y));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_offset failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_offset(LCComponentHandle component, LCVec2* out_offset) {
    if (!out_offset) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        lupine::math::Vec2 v = gif->GetOffset();
        *out_offset = LCVec2{v.x, v.y};
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_offset failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_size(LCComponentHandle component, LCVec2 size) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetSize(lupine::math::Vec2(size.x, size.y));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_size failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_size(LCComponentHandle component, LCVec2* out_size) {
    if (!out_size) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        lupine::math::Vec2 v = gif->GetSize();
        *out_size = LCVec2{v.x, v.y};
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_size failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_flip_h(LCComponentHandle component, bool flip) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetFlipH(flip);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_flip_h failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_flip_h(LCComponentHandle component, bool* out_flip) {
    if (!out_flip) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_flip is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_flip = gif->GetFlipH();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_flip_h failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_flip_v(LCComponentHandle component, bool flip) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetFlipV(flip);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_flip_v failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_flip_v(LCComponentHandle component, bool* out_flip) {
    if (!out_flip) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_flip is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_flip = gif->GetFlipV();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_flip_v failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_modulate(LCComponentHandle component, LCColor color) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetModulate(lupine::math::Color(color.r, color.g, color.b, color.a));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_modulate failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_modulate(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        lupine::math::Color c = gif->GetModulate();
        *out_color = LCColor{c.r, c.g, c.b, c.a};
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_modulate failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_set_pixel_snap(LCComponentHandle component, bool snap) {
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        gif->SetPixelSnap(snap);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_set_pixel_snap failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_gif_player_get_pixel_snap(LCComponentHandle component, bool* out_snap) {
    if (!out_snap) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_snap is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        GifPlayer* gif = ResolveGif(component, &err);
        if (!gif) return err;
        *out_snap = gif->GetPixelSnap();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_gif_player_get_pixel_snap failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
