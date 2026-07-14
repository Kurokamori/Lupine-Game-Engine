/**
 * @file lc_animation_tree.cpp
 * @brief Lupine Engine C API - AnimationTree implementation
 */

#include "components/lc_animation_tree.h"
#include "../core/lc_internal.h"

#include <lupine/components/AnimationTree.hpp>
#include <cstring>
#include <string>
#include <memory>

namespace {

std::shared_ptr<lupine::components::AnimationTree> GetTree(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    return std::dynamic_pointer_cast<lupine::components::AnimationTree>(comp);
}

void CopyString(const std::string& value, char* out, int size) {
    if (!out || size <= 0) return;
    CopyStringToBuffer(out, static_cast<size_t>(size), value.c_str());
}

} // anonymous namespace

LC_API LCResult lc_animation_tree_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        ::SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::AnimationTree>();
        comp->DefineProperties();
        if (!compName.empty()) comp->SetName(compName);
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        ::SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create AnimationTree");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_animation_tree_set_graph_path(LCComponentHandle component, const char* path) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->SetGraphPath(path ? path : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_set_animation_player(LCComponentHandle component, const char* node_path) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->SetAnimationPlayerPath(node_path ? node_path : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_set_root_node(LCComponentHandle component, const char* node_path) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->SetRootNodePath(node_path ? node_path : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_set_active(LCComponentHandle component, bool active) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->SetActive(active);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_is_active(LCComponentHandle component, bool* out_active) {
    if (!out_active) { ::SetError(LC_ERROR_NULL_POINTER, "out_active is NULL"); return LC_ERROR_NULL_POINTER; }
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    *out_active = tree->GetActive();
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_reload_graph(LCComponentHandle component) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->ReloadGraph();
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_set_float(LCComponentHandle component, const char* name, float value) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->SetFloat(name ? name : "", value);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_set_int(LCComponentHandle component, const char* name, int value) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->SetInt(name ? name : "", value);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_set_bool(LCComponentHandle component, const char* name, bool value) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->SetBool(name ? name : "", value);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_set_trigger(LCComponentHandle component, const char* name) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->SetTrigger(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_get_float(LCComponentHandle component, const char* name, float* out_value) {
    if (!out_value) { ::SetError(LC_ERROR_NULL_POINTER, "out_value is NULL"); return LC_ERROR_NULL_POINTER; }
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    *out_value = tree->GetFloat(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_get_int(LCComponentHandle component, const char* name, int* out_value) {
    if (!out_value) { ::SetError(LC_ERROR_NULL_POINTER, "out_value is NULL"); return LC_ERROR_NULL_POINTER; }
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    *out_value = tree->GetInt(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_get_bool(LCComponentHandle component, const char* name, bool* out_value) {
    if (!out_value) { ::SetError(LC_ERROR_NULL_POINTER, "out_value is NULL"); return LC_ERROR_NULL_POINTER; }
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    *out_value = tree->GetBool(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_travel(LCComponentHandle component, const char* state, int layer) {
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    tree->Travel(state ? state : "", layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_animation_tree_get_current_state(LCComponentHandle component, int layer, char* out_state, int state_size) {
    if (!out_state || state_size <= 0) { ::SetError(LC_ERROR_NULL_POINTER, "out_state is NULL"); return LC_ERROR_NULL_POINTER; }
    auto tree = GetTree(component);
    if (!tree) { ::SetError(LC_ERROR_INVALID_HANDLE, "Invalid AnimationTree handle"); return LC_ERROR_INVALID_HANDLE; }
    CopyString(tree->GetCurrentState(layer), out_state, state_size);
    return LC_SUCCESS;
}
