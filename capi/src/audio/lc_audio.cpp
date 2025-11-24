
#include "audio/lc_audio.h"
#include "../core/lc_internal.h"

#include <lupine\components\AudioPlayer.hpp>
#include <lupine\components\AudioListener.hpp>

#include <cstring>

namespace {

void SetAudioError(LCResult code, const char* message) {
    ::SetError(code, message);
}

} // anonymous namespace

/* ============================================================================
 * AudioPlayer Functions
 * ============================================================================ */

LC_API LCResult lc_audio_player_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string playerName = name ? name : "";
        auto player = std::make_shared<lupine::components::AudioPlayer>();
        *out_component = CreateComponentHandle(player);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to create AudioPlayer");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_play(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->Play();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to play audio");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_stop(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->Stop();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to stop audio");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_pause(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->Pause();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to pause audio");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_resume(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->Resume();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to resume audio");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_is_playing(LCComponentHandle component, bool* out_playing) {
    if (!out_playing) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_playing is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_playing = player->IsPlaying();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get playing state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_autoplay(LCComponentHandle component, bool* out_autoplay) {
    if (!out_autoplay) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_autoplay is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_autoplay = player->GetAutoplay();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get autoplay");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_autoplay(LCComponentHandle component, bool autoplay) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetAutoplay(autoplay);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set autoplay");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_loop(LCComponentHandle component, bool* out_loop) {
    if (!out_loop) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_loop is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_loop = player->GetLoop();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get loop");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_loop(LCComponentHandle component, bool loop) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetLoop(loop);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set loop");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_volume(LCComponentHandle component, float* out_volume) {
    if (!out_volume) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_volume is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_volume = player->GetVolume();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get volume");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_volume(LCComponentHandle component, float volume) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetVolume(volume);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set volume");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_pitch(LCComponentHandle component, float* out_pitch) {
    if (!out_pitch) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_pitch is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_pitch = player->GetPitch();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get pitch");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_pitch(LCComponentHandle component, float pitch) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetPitch(pitch);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set pitch");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_pan(LCComponentHandle component, float* out_pan) {
    if (!out_pan) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_pan is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_pan = player->GetPan();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get pan");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_pan(LCComponentHandle component, float pan) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetPan(pan);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set pan");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_bus(LCComponentHandle component, char* out_bus, size_t bus_size) {
    if (!out_bus) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_bus is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        const std::string& bus = player->GetBus();
        strncpy(out_bus, bus.c_str(), bus_size - 1);
        out_bus[bus_size - 1] = '\0';
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get bus");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_bus(LCComponentHandle component, const char* bus) {
    if (!bus) {
        SetAudioError(LC_ERROR_NULL_POINTER, "bus is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetBus(std::string(bus));
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set bus");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_is3d(LCComponentHandle component, bool* out_is3d) {
    if (!out_is3d) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_is3d is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_is3d = player->GetIs3D();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get is3d");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_is3d(LCComponentHandle component, bool is3d) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetIs3D(is3d);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set is3d");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_min_distance(LCComponentHandle component, float* out_distance) {
    if (!out_distance) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_distance is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_distance = player->GetMinDistance();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get min distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_min_distance(LCComponentHandle component, float distance) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetMinDistance(distance);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set min distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_max_distance(LCComponentHandle component, float* out_distance) {
    if (!out_distance) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_distance is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_distance = player->GetMaxDistance();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get max distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_max_distance(LCComponentHandle component, float distance) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetMaxDistance(distance);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set max distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_get_rolloff_factor(LCComponentHandle component, float* out_factor) {
    if (!out_factor) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_factor is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_factor = player->GetRolloffFactor();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get rolloff factor");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_player_set_rolloff_factor(LCComponentHandle component, float factor) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto player = std::dynamic_pointer_cast<lupine::components::AudioPlayer>(comp);
        if (!player) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioPlayer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        player->SetRolloffFactor(factor);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set rolloff factor");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * AudioListener Functions
 * ============================================================================ */

LC_API LCResult lc_audio_listener_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string listenerName = name ? name : "";
        auto listener = std::make_shared<lupine::components::AudioListener>();
        *out_component = CreateComponentHandle(listener);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to create AudioListener");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_listener_is_active(LCComponentHandle component, bool* out_active) {
    if (!out_active) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_active is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto listener = std::dynamic_pointer_cast<lupine::components::AudioListener>(comp);
        if (!listener) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioListener");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_active = listener->IsActive();
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get active state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_listener_set_active(LCComponentHandle component, bool active) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAudioError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto listener = std::dynamic_pointer_cast<lupine::components::AudioListener>(comp);
        if (!listener) {
            SetAudioError(LC_ERROR_TYPE_MISMATCH, "Component is not an AudioListener");
            return LC_ERROR_TYPE_MISMATCH;
        }

        listener->SetActive(active);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set active state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
