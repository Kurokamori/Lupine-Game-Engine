/**
 * @file lc_animation_player.cpp
 * @brief Lupine Engine C API - AnimationPlayer implementation
 */

#include "components/lc_animation_player.h"
#include "../core/lc_internal.h"

#include <lupine/components/AnimationPlayer.hpp>
#include <cstring>
#include <string>
#include <memory>

namespace {

std::shared_ptr<lupine::components::AnimationPlayer> GetPlayer(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    return std::dynamic_pointer_cast<lupine::components::AnimationPlayer>(comp);
}

void CopyString(const std::string& value, char* out, int size) {
    if (!out || size <= 0) return;
    CopyStringToBuffer(out, static_cast<size_t>(size), value.c_str());
}

} // anonymous namespace

LC_API LCResult lc_animation_player_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::AnimationPlayer>();
        comp->DefineProperties();
        if (!compName.empty()) comp->SetName(compName);
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create AnimationPlayer");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animation_player_add_clip(LCComponentHandle component, const char* name, const char* path) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->AddClip(name ? name : "", path ? path : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_remove_clip(LCComponentHandle component, const char* name) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->RemoveClip(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_has_animation(LCComponentHandle component, const char* name, bool* out_has) {
    if (!out_has) { ::SetError(LC_ERROR_NULL_POINTER, "out_has is NULL"); return LC_ERROR_NULL_POINTER; }
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    *out_has = player->HasClip(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_reload_clips(LCComponentHandle component) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->ReloadClips();
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_play(LCComponentHandle component, const char* name) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->Play(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_play_blend(LCComponentHandle component, const char* name, float blend_time) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->Play(name ? name : "", blend_time);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_play_backwards(LCComponentHandle component, const char* name) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->PlayBackwards(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_stop(LCComponentHandle component, bool reset) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->Stop(reset);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_pause(LCComponentHandle component) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->Pause();
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_resume(LCComponentHandle component) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->Resume();
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_seek(LCComponentHandle component, float time, bool update) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->Seek(time, update);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_queue(LCComponentHandle component, const char* name) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->Queue(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_clear_queue(LCComponentHandle component) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->ClearQueue();
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_is_playing(LCComponentHandle component, bool* out_playing) {
    if (!out_playing) { ::SetError(LC_ERROR_NULL_POINTER, "out_playing is NULL"); return LC_ERROR_NULL_POINTER; }
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    *out_playing = player->IsPlaying();
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_get_current_animation(LCComponentHandle component, char* out_name, int name_size) {
    if (!out_name || name_size <= 0) { ::SetError(LC_ERROR_NULL_POINTER, "out_name is NULL"); return LC_ERROR_NULL_POINTER; }
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    CopyString(player->GetCurrentAnimation(), out_name, name_size);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_sample_at(LCComponentHandle component, const char* name, float time) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->SampleAt(name ? name : "", time);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_set_speed(LCComponentHandle component, float speed) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->SetSpeed(speed);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_get_speed(LCComponentHandle component, float* out_speed) {
    if (!out_speed) { ::SetError(LC_ERROR_NULL_POINTER, "out_speed is NULL"); return LC_ERROR_NULL_POINTER; }
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    *out_speed = player->GetSpeed();
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_set_auto_play(LCComponentHandle component, const char* name) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->SetAutoPlay(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_set_root_node(LCComponentHandle component, const char* path) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->SetRootNodePath(path ? path : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_player_set_default_blend_time(LCComponentHandle component, float blend_time) {
    auto player = GetPlayer(component);
    if (!player) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationPlayer handle"); return LC_ERROR_INVALID_HANDLE; }
    player->SetDefaultBlendTime(blend_time);
    return LC_SUCCESS;
}
