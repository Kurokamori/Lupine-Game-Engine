/**
 * @file lc_video_player.cpp
 * @brief Lupine Engine C API - VideoPlayer implementation
 */

#include "components/lc_video_player.h"
#include "../core/lc_internal.h"

#include <lupine/components/VideoPlayer.hpp>
#include <cstring>

namespace {

using lupine::components::VideoPlayer;

// Resolve a handle to a VideoPlayer*, reporting the appropriate error otherwise.
VideoPlayer* ResolveVideo(LCComponentHandle component, LCResult* err) {
    auto comp = GetComponent(component);
    if (!comp) {
        ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        *err = LC_ERROR_INVALID_HANDLE;
        return nullptr;
    }
    auto video = std::dynamic_pointer_cast<VideoPlayer>(comp);
    if (!video) {
        ::SetError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a VideoPlayer");
        *err = LC_ERROR_COMPONENT_INVALID_TYPE;
        return nullptr;
    }
    return video.get();
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

LC_API LCResult lc_video_player_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto comp = std::make_shared<VideoPlayer>();
        comp->DefineProperties();
        if (name && name[0] != '\0') {
            comp->SetName(name);
        }
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create VideoPlayer");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Source / state
 * ============================================================================ */

LC_API LCResult lc_video_player_get_video_path(LCComponentHandle component, char* out_path, int path_size) {
    if (!out_path || path_size <= 0) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_path is NULL or path_size invalid");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        CopyString(v->GetVideoPath(), out_path, path_size);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_video_path failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_set_video_path(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        ::SetError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        v->SetVideoPath(std::string(filepath));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_set_video_path failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

// Boolean / scalar getters with a uniform body.
#define LC_VIDEO_GET_BOOL(fn_name, expr)                                      \
    LC_API LCResult fn_name(LCComponentHandle component, bool* out_value) {    \
        if (!out_value) {                                                     \
            ::SetError(LC_ERROR_NULL_POINTER, "out_value is NULL");           \
            return LC_ERROR_NULL_POINTER;                                     \
        }                                                                     \
        try {                                                                 \
            LCResult err;                                                     \
            VideoPlayer* v = ResolveVideo(component, &err);                   \
            if (!v) return err;                                               \
            *out_value = (expr);                                              \
            return LC_SUCCESS;                                                \
        } catch (...) {                                                       \
            ::SetError(LC_ERROR_INTERNAL_ERROR, #fn_name " failed");          \
            return LC_ERROR_INTERNAL_ERROR;                                   \
        }                                                                     \
    }

LC_VIDEO_GET_BOOL(lc_video_player_is_loaded, v->IsLoaded())
LC_VIDEO_GET_BOOL(lc_video_player_has_audio, v->HasAudio())
LC_VIDEO_GET_BOOL(lc_video_player_is_playing, v->IsPlaying())
LC_VIDEO_GET_BOOL(lc_video_player_get_loop, v->GetLoop())
LC_VIDEO_GET_BOOL(lc_video_player_get_auto_play, v->GetAutoPlay())
LC_VIDEO_GET_BOOL(lc_video_player_get_audio_enabled, v->GetAudioEnabled())
LC_VIDEO_GET_BOOL(lc_video_player_get_muted, v->GetMuted())
LC_VIDEO_GET_BOOL(lc_video_player_get_flip_h, v->GetFlipH())
LC_VIDEO_GET_BOOL(lc_video_player_get_flip_v, v->GetFlipV())
LC_VIDEO_GET_BOOL(lc_video_player_get_seek_enabled, v->GetSeekEnabled())
LC_VIDEO_GET_BOOL(lc_video_player_get_preview_in_editor, v->GetPreviewInEditor())

#undef LC_VIDEO_GET_BOOL

LC_API LCResult lc_video_player_get_duration(LCComponentHandle component, double* out_seconds) {
    if (!out_seconds) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_seconds is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        *out_seconds = v->GetDuration();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_duration failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_position(LCComponentHandle component, double* out_seconds) {
    if (!out_seconds) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_seconds is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        *out_seconds = v->GetPlaybackPosition();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_position failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_video_width(LCComponentHandle component, int* out_width) {
    if (!out_width) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_width is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        *out_width = v->GetVideoWidth();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_video_width failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_video_height(LCComponentHandle component, int* out_height) {
    if (!out_height) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_height is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        *out_height = v->GetVideoHeight();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_video_height failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Playback control (void)
 * ============================================================================ */

#define LC_VIDEO_VOID_CALL(fn_name, call)                                     \
    LC_API LCResult fn_name(LCComponentHandle component) {                    \
        try {                                                                 \
            LCResult err;                                                     \
            VideoPlayer* v = ResolveVideo(component, &err);                   \
            if (!v) return err;                                               \
            call;                                                             \
            return LC_SUCCESS;                                                \
        } catch (...) {                                                       \
            ::SetError(LC_ERROR_INTERNAL_ERROR, #fn_name " failed");          \
            return LC_ERROR_INTERNAL_ERROR;                                   \
        }                                                                     \
    }

LC_VIDEO_VOID_CALL(lc_video_player_play, v->Play())
LC_VIDEO_VOID_CALL(lc_video_player_stop, v->Stop())
LC_VIDEO_VOID_CALL(lc_video_player_pause, v->Pause())
LC_VIDEO_VOID_CALL(lc_video_player_resume, v->Resume())
LC_VIDEO_VOID_CALL(lc_video_player_restart, v->Restart())
LC_VIDEO_VOID_CALL(lc_video_player_replay, v->Replay())

#undef LC_VIDEO_VOID_CALL

LC_API LCResult lc_video_player_seek(LCComponentHandle component, double seconds) {
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        v->Seek(seconds);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_seek failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Playback settings (setters)
 * ============================================================================ */

#define LC_VIDEO_SET_BOOL(fn_name, setter)                                    \
    LC_API LCResult fn_name(LCComponentHandle component, bool value) {        \
        try {                                                                 \
            LCResult err;                                                     \
            VideoPlayer* v = ResolveVideo(component, &err);                   \
            if (!v) return err;                                               \
            v->setter(value);                                                 \
            return LC_SUCCESS;                                                \
        } catch (...) {                                                       \
            ::SetError(LC_ERROR_INTERNAL_ERROR, #fn_name " failed");          \
            return LC_ERROR_INTERNAL_ERROR;                                   \
        }                                                                     \
    }

LC_VIDEO_SET_BOOL(lc_video_player_set_loop, SetLoop)
LC_VIDEO_SET_BOOL(lc_video_player_set_auto_play, SetAutoPlay)
LC_VIDEO_SET_BOOL(lc_video_player_set_audio_enabled, SetAudioEnabled)
LC_VIDEO_SET_BOOL(lc_video_player_set_muted, SetMuted)
LC_VIDEO_SET_BOOL(lc_video_player_set_flip_h, SetFlipH)
LC_VIDEO_SET_BOOL(lc_video_player_set_flip_v, SetFlipV)
LC_VIDEO_SET_BOOL(lc_video_player_set_seek_enabled, SetSeekEnabled)
LC_VIDEO_SET_BOOL(lc_video_player_set_preview_in_editor, SetPreviewInEditor)

#undef LC_VIDEO_SET_BOOL

LC_API LCResult lc_video_player_set_speed(LCComponentHandle component, float speed) {
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        v->SetSpeed(speed);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_set_speed failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_speed(LCComponentHandle component, float* out_speed) {
    if (!out_speed) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_speed is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        *out_speed = v->GetSpeed();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_speed failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_set_volume(LCComponentHandle component, float volume) {
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        v->SetVolume(volume);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_set_volume failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_volume(LCComponentHandle component, float* out_volume) {
    if (!out_volume) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_volume is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        *out_volume = v->GetVolume();
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_volume failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_set_bus(LCComponentHandle component, const char* bus) {
    if (!bus) {
        ::SetError(LC_ERROR_NULL_POINTER, "bus is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        v->SetBus(std::string(bus));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_set_bus failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_bus(LCComponentHandle component, char* out_bus, int bus_size) {
    if (!out_bus || bus_size <= 0) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_bus is NULL or bus_size invalid");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        CopyString(v->GetBus(), out_bus, bus_size);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_bus failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Visual properties
 * ============================================================================ */

LC_API LCResult lc_video_player_set_offset(LCComponentHandle component, LCVec2 offset) {
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        v->SetOffset(lupine::math::Vec2(offset.x, offset.y));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_set_offset failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_offset(LCComponentHandle component, LCVec2* out_offset) {
    if (!out_offset) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        lupine::math::Vec2 val = v->GetOffset();
        *out_offset = LCVec2{val.x, val.y};
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_offset failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_set_size(LCComponentHandle component, LCVec2 size) {
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        v->SetSize(lupine::math::Vec2(size.x, size.y));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_set_size failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_size(LCComponentHandle component, LCVec2* out_size) {
    if (!out_size) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        lupine::math::Vec2 val = v->GetSize();
        *out_size = LCVec2{val.x, val.y};
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_size failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_set_modulate(LCComponentHandle component, LCColor color) {
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        v->SetModulate(lupine::math::Color(color.r, color.g, color.b, color.a));
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_set_modulate failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_video_player_get_modulate(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        LCResult err;
        VideoPlayer* v = ResolveVideo(component, &err);
        if (!v) return err;
        lupine::math::Color c = v->GetModulate();
        *out_color = LCColor{c.r, c.g, c.b, c.a};
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "lc_video_player_get_modulate failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* The flip_v getters are provided via the LC_VIDEO_GET_BOOL block above. */
