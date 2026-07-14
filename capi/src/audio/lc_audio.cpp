
#include "audio/lc_audio.h"
#include "../core/lc_internal.h"

#include <lupine/components/AudioPlayer.hpp>
#include <lupine/components/AudioListener.hpp>
#include <lupine/audio/AudioManager.hpp>
#include <lupine/asset/AudioAsset.hpp>
#include <lupine/core/UUID.hpp>
#include <lupine/math/Vec3.hpp>

#include <cstring>
#include <string>
#include <mutex>
#include <unordered_map>

namespace {

void SetAudioError(LCResult code, const char* message) {
    ::SetError(code, message);

}

std::mutex g_AudioAssetCacheMutex;
std::unordered_map<std::string, lupine::asset::AssetRef<lupine::asset::AudioAsset>> g_AudioAssetCache;

lupine::asset::AssetRef<lupine::asset::AudioAsset> AcquireAudioAsset(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_AudioAssetCacheMutex);

    std::unordered_map<std::string, lupine::asset::AssetRef<lupine::asset::AudioAsset>>::iterator it =
        g_AudioAssetCache.find(path);
    if (it != g_AudioAssetCache.end()) {
        return it->second;
    }

    lupine::asset::AssetRef<lupine::asset::AudioAsset> audio(new lupine::asset::AudioAsset());
    if (!audio->LoadFromFile(path, lupine::asset::AudioLoadMode::Preload)) {
        return lupine::asset::AssetRef<lupine::asset::AudioAsset>();
    }

    g_AudioAssetCache[path] = audio;
    return audio;
}

LCResult WriteUuidString(const lupine::core::UUID& uuid, char* out_uuid, size_t uuid_size) {
    std::string text = uuid.ToString();
    if (uuid_size <= text.size()) {
        SetAudioError(LC_ERROR_INVALID_PARAMETER, "out_uuid buffer is too small");
        return LC_ERROR_INVALID_PARAMETER;
    }
    std::memcpy(out_uuid, text.c_str(), text.size() + 1);
    return LC_SUCCESS;
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
        player->DefineProperties();  // Initialize properties before use
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        const std::string& bus = player->GetBus();
        CopyStringToBuffer(out_bus, bus_size, bus.c_str());
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioPlayer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        player->SetRolloffFactor(factor);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set rolloff factor");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ============================================================================
 * AudioListener Functions
 * ============================================================================ */

}
LC_API LCResult lc_audio_listener_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string listenerName = name ? name : "";
        auto listener = std::make_shared<lupine::components::AudioListener>();
        listener->DefineProperties();  // Initialize properties before use
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioListener");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
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
            SetAudioError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AudioListener");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        listener->SetActive(active);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set active state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Global Audio Control (AudioManager)
 * ============================================================================ */

LC_API void lc_audio_set_source_volume(const char* uuid, float volume) {
    if (!uuid) return;
    try {
        lupine::audio::AudioManager::GetInstance().SetSourceVolume(
            lupine::core::UUID::FromString(uuid), volume);
    } catch (...) {
    }
}

LC_API void lc_audio_set_source_pitch(const char* uuid, float pitch) {
    if (!uuid) return;
    try {
        lupine::audio::AudioManager::GetInstance().SetSourcePitch(
            lupine::core::UUID::FromString(uuid), pitch);
    } catch (...) {
    }
}

LC_API void lc_audio_set_source_pan(const char* uuid, float pan) {
    if (!uuid) return;
    try {
        lupine::audio::AudioManager::GetInstance().SetSourcePan(
            lupine::core::UUID::FromString(uuid), pan);
    } catch (...) {
    }
}

LC_API void lc_audio_set_master_volume(float volume) {
    try {
        lupine::audio::AudioManager::GetInstance().SetMasterVolume(volume);
    } catch (...) {
    }
}

LC_API float lc_audio_get_master_volume(void) {
    try {
        return lupine::audio::AudioManager::GetInstance().GetMasterVolume();
    } catch (...) {
        return 0.0f;
    }
}

LC_API void lc_audio_set_master_muted(bool muted) {
    try {
        lupine::audio::AudioManager::GetInstance().SetMasterMuted(muted);
    } catch (...) {
    }
}

LC_API bool lc_audio_is_master_muted(void) {
    try {
        return lupine::audio::AudioManager::GetInstance().IsMasterMuted();
    } catch (...) {
        return false;
    }
}

LC_API void lc_audio_set_listener_position(LCVec3 position) {
    try {
        lupine::audio::AudioManager::GetInstance().SetListenerPosition(
            lupine::math::Vec3(position.x, position.y, position.z));
    } catch (...) {
    }
}

LC_API void lc_audio_set_listener_orientation(LCVec3 forward, LCVec3 up) {
    try {
        lupine::audio::AudioManager::GetInstance().SetListenerOrientation(
            lupine::math::Vec3(forward.x, forward.y, forward.z),
            lupine::math::Vec3(up.x, up.y, up.z));
    } catch (...) {
    }
}

LC_API void lc_audio_set_listener_velocity(LCVec3 velocity) {
    try {
        lupine::audio::AudioManager::GetInstance().SetListenerVelocity(
            lupine::math::Vec3(velocity.x, velocity.y, velocity.z));
    } catch (...) {
    }
}

LC_API void lc_audio_create_bus(const char* name, const char* parent) {
    if (!name) return;
    try {
        lupine::audio::AudioManager::GetInstance().CreateBus(
            std::string(name), parent ? std::string(parent) : std::string());
    } catch (...) {
    }
}

LC_API void lc_audio_destroy_bus(const char* name) {
    if (!name) return;
    try {
        lupine::audio::AudioManager::GetInstance().DestroyBus(std::string(name));
    } catch (...) {
    }
}

LC_API bool lc_audio_has_bus(const char* name) {
    if (!name) return false;
    try {
        return lupine::audio::AudioManager::GetInstance().HasBus(std::string(name));
    } catch (...) {
        return false;
    }
}

LC_API void lc_audio_set_bus_solo(const char* name, bool solo) {
    if (!name) return;
    try {
        lupine::audio::AudioManager::GetInstance().SetBusSolo(std::string(name), solo);
    } catch (...) {
    }
}

LC_API bool lc_audio_is_bus_solo(const char* name) {
    if (!name) return false;
    try {
        return lupine::audio::AudioManager::GetInstance().IsBusSolo(std::string(name));
    } catch (...) {
        return false;
    }
}

LC_API int lc_audio_bus_add_effect(const char* bus, const char* effect_type) {
    if (!bus || !effect_type) return -1;
    try {
        lupine::audio::AudioEffectType type;
        if (!lupine::audio::AudioEffectTypeFromString(std::string(effect_type), type)) {
            return -1;
        }
        return lupine::audio::AudioManager::GetInstance().AddBusEffect(std::string(bus), type);
    } catch (...) {
        return -1;
    }
}

LC_API void lc_audio_bus_remove_effect(const char* bus, int index) {
    if (!bus) return;
    try {
        lupine::audio::AudioManager::GetInstance().RemoveBusEffect(std::string(bus), index);
    } catch (...) {
    }
}

LC_API void lc_audio_bus_move_effect(const char* bus, int from_index, int to_index) {
    if (!bus) return;
    try {
        lupine::audio::AudioManager::GetInstance().MoveBusEffect(std::string(bus), from_index, to_index);
    } catch (...) {
    }
}

LC_API void lc_audio_bus_clear_effects(const char* bus) {
    if (!bus) return;
    try {
        lupine::audio::AudioManager::GetInstance().ClearBusEffects(std::string(bus));
    } catch (...) {
    }
}

LC_API void lc_audio_bus_set_effect_enabled(const char* bus, int index, bool enabled) {
    if (!bus) return;
    try {
        lupine::audio::AudioManager::GetInstance().SetBusEffectEnabled(std::string(bus), index, enabled);
    } catch (...) {
    }
}

LC_API void lc_audio_bus_set_effect_parameter(const char* bus, int index,
                                              const char* parameter, float value) {
    if (!bus || !parameter) return;
    try {
        lupine::audio::AudioManager::GetInstance().SetBusEffectParameter(
            std::string(bus), index, std::string(parameter), value);
    } catch (...) {
    }
}

LC_API int lc_audio_bus_get_effect_count(const char* bus) {
    if (!bus) return 0;
    try {
        return lupine::audio::AudioManager::GetInstance().GetBusEffectCount(std::string(bus));
    } catch (...) {
        return 0;
    }
}

LC_API float lc_audio_bus_get_effect_parameter(const char* bus, int index, const char* parameter) {
    if (!bus || !parameter) return 0.0f;
    try {
        return lupine::audio::AudioManager::GetInstance().GetBusEffectParameter(
            std::string(bus), index, std::string(parameter));
    } catch (...) {
        return 0.0f;
    }
}

LC_API bool lc_audio_bus_is_effect_enabled(const char* bus, int index) {
    if (!bus) return false;
    try {
        return lupine::audio::AudioManager::GetInstance().IsBusEffectEnabled(std::string(bus), index);
    } catch (...) {
        return false;
    }
}

LC_API float lc_audio_bus_get_level(const char* bus) {
    if (!bus) return 0.0f;
    try {
        return lupine::audio::AudioManager::GetInstance().GetBusPeak(std::string(bus));
    } catch (...) {
        return 0.0f;
    }
}

LC_API void lc_audio_bus_get_levels(const char* bus, float* out_left, float* out_right) {
    float left = 0.0f;
    float right = 0.0f;
    if (bus) {
        try {
            lupine::audio::AudioManager::GetInstance().GetBusLevels(std::string(bus), left, right);
        } catch (...) {
        }
    }
    if (out_left) *out_left = left;
    if (out_right) *out_right = right;
}

/* ============================================================================
 * Fire-and-Forget Mixer Playback (AudioManager, by UUID)
 * ============================================================================ */

LC_API LCResult lc_audio_play(const char* audio_path, const char* bus_name, bool loop,
                              float volume, char* out_uuid, size_t uuid_size) {
    if (!audio_path) {
        SetAudioError(LC_ERROR_NULL_POINTER, "audio_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::asset::AssetRef<lupine::asset::AudioAsset> audio = AcquireAudioAsset(std::string(audio_path));
        if (!audio) {
            SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to load audio asset");
            return LC_ERROR_INTERNAL_ERROR;
        }

        std::string bus = bus_name ? std::string(bus_name) : std::string("Master");
        lupine::audio::PlaybackMode mode =
            loop ? lupine::audio::PlaybackMode::Loop : lupine::audio::PlaybackMode::OneShot;

        lupine::core::UUID source = lupine::audio::AudioManager::GetInstance().Play(audio, bus, mode, volume);
        return WriteUuidString(source, out_uuid, uuid_size);
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to play audio");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_play_3d(const char* audio_path, LCVec3 position, const char* bus_name,
                                 bool loop, float volume, char* out_uuid, size_t uuid_size) {
    if (!audio_path) {
        SetAudioError(LC_ERROR_NULL_POINTER, "audio_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::asset::AssetRef<lupine::asset::AudioAsset> audio = AcquireAudioAsset(std::string(audio_path));
        if (!audio) {
            SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to load audio asset");
            return LC_ERROR_INTERNAL_ERROR;
        }

        std::string bus = bus_name ? std::string(bus_name) : std::string("Master");
        lupine::audio::PlaybackMode mode =
            loop ? lupine::audio::PlaybackMode::Loop : lupine::audio::PlaybackMode::OneShot;

        lupine::core::UUID source = lupine::audio::AudioManager::GetInstance().Play3D(
            audio, lupine::math::Vec3(position.x, position.y, position.z), bus, mode, volume);
        return WriteUuidString(source, out_uuid, uuid_size);
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to play 3D audio");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_play_scheduled(const char* audio_path, float delay_seconds,
                                        const char* bus_name, bool loop, float volume,
                                        char* out_uuid, size_t uuid_size) {
    if (!audio_path) {
        SetAudioError(LC_ERROR_NULL_POINTER, "audio_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::asset::AssetRef<lupine::asset::AudioAsset> audio = AcquireAudioAsset(std::string(audio_path));
        if (!audio) {
            SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to load audio asset");
            return LC_ERROR_INTERNAL_ERROR;
        }

        std::string bus = bus_name ? std::string(bus_name) : std::string("Master");
        lupine::audio::PlaybackMode mode =
            loop ? lupine::audio::PlaybackMode::Loop : lupine::audio::PlaybackMode::OneShot;

        lupine::core::UUID source = lupine::audio::AudioManager::GetInstance().Play(
            audio, bus, mode, volume, static_cast<double>(delay_seconds));
        return WriteUuidString(source, out_uuid, uuid_size);
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to play scheduled audio");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_play_scheduled_3d(const char* audio_path, LCVec3 position,
                                           float delay_seconds, const char* bus_name, bool loop,
                                           float volume, char* out_uuid, size_t uuid_size) {
    if (!audio_path) {
        SetAudioError(LC_ERROR_NULL_POINTER, "audio_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::asset::AssetRef<lupine::asset::AudioAsset> audio = AcquireAudioAsset(std::string(audio_path));
        if (!audio) {
            SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to load audio asset");
            return LC_ERROR_INTERNAL_ERROR;
        }

        std::string bus = bus_name ? std::string(bus_name) : std::string("Master");
        lupine::audio::PlaybackMode mode =
            loop ? lupine::audio::PlaybackMode::Loop : lupine::audio::PlaybackMode::OneShot;

        lupine::core::UUID source = lupine::audio::AudioManager::GetInstance().Play3D(
            audio, lupine::math::Vec3(position.x, position.y, position.z), bus, mode, volume,
            static_cast<double>(delay_seconds));
        return WriteUuidString(source, out_uuid, uuid_size);
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to play scheduled 3D audio");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_stop(const char* uuid) {
    if (!uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::audio::AudioManager::GetInstance().Stop(lupine::core::UUID::FromString(uuid));
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to stop audio source");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_pause(const char* uuid) {
    if (!uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::audio::AudioManager::GetInstance().Pause(lupine::core::UUID::FromString(uuid));
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to pause audio source");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_resume(const char* uuid) {
    if (!uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::audio::AudioManager::GetInstance().Resume(lupine::core::UUID::FromString(uuid));
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to resume audio source");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_source_is_playing(const char* uuid, bool* out_playing) {
    if (!uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_playing) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_playing is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_playing = lupine::audio::AudioManager::GetInstance().IsSourcePlaying(
            lupine::core::UUID::FromString(uuid));
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to query audio source playing state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_source_is_finished(const char* uuid, bool* out_finished) {
    if (!uuid) {
        SetAudioError(LC_ERROR_NULL_POINTER, "uuid is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_finished) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_finished is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_finished = lupine::audio::AudioManager::GetInstance().IsSourceFinished(
            lupine::core::UUID::FromString(uuid));
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to query audio source finished state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_set_bus_volume(const char* bus, float volume) {
    if (!bus) {
        SetAudioError(LC_ERROR_NULL_POINTER, "bus is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::audio::AudioManager::GetInstance().SetBusVolume(std::string(bus), volume);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set bus volume");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_get_bus_volume(const char* bus, float* out_volume) {
    if (!bus) {
        SetAudioError(LC_ERROR_NULL_POINTER, "bus is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_volume) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_volume is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_volume = lupine::audio::AudioManager::GetInstance().GetBusVolume(std::string(bus));
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to get bus volume");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_set_bus_muted(const char* bus, bool muted) {
    if (!bus) {
        SetAudioError(LC_ERROR_NULL_POINTER, "bus is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::audio::AudioManager::GetInstance().SetBusMuted(std::string(bus), muted);
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to set bus muted");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_audio_is_bus_muted(const char* bus, bool* out_muted) {
    if (!bus) {
        SetAudioError(LC_ERROR_NULL_POINTER, "bus is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_muted) {
        SetAudioError(LC_ERROR_NULL_POINTER, "out_muted is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_muted = lupine::audio::AudioManager::GetInstance().IsBusMuted(std::string(bus));
        return LC_SUCCESS;
    } catch (...) {
        SetAudioError(LC_ERROR_INTERNAL_ERROR, "Failed to query bus muted state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
