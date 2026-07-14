/**
 * @file lc_scene_instance.cpp
 * @brief Implementation of SceneInstance C API
 */

#include "core/lc_scene_instance.h"
#include "lc_internal.h"

#include <lupine/core/SceneInstance.hpp>
#include <lupine/core/Node.hpp>
#include <lupine/core/Scene.hpp>
#include <lupine/core/SceneManager.hpp>
#include <lupine/core/Prefab.hpp>
#include <lupine/asset/Asset.hpp>

#include <cstring>
#include <memory>
#include <string>

using namespace lupine;
using namespace lupine::core;

namespace {

SceneInstance* GetSceneInstance(LCNodeHandle handle) {
    auto node = GetNode(handle);
    if (!node) return nullptr;
    return dynamic_cast<SceneInstance*>(node.get());
}

std::shared_ptr<lupine::core::Node> ResolveSpawnParent(LCNodeHandle parent_or_null) {
    if (parent_or_null) {
        return GetNode(parent_or_null);
    }
    lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
    if (!sceneManager) {
        return nullptr;
    }
    lupine::core::Scene* scene = sceneManager->GetCurrentScene();
    if (!scene) {
        return nullptr;
    }
    return scene->GetRoot();
}

} // anonymous namespace

/* ============================================================================
 * SceneInstance Creation
 * ============================================================================ */

LC_API LCResult lc_scene_instance_create(const char* name, LCNodeHandle* out_node) {
    if (!out_node) {
        SetError(LC_ERROR_NULL_POINTER, "out_node is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto node = name ? std::make_shared<SceneInstance>(name) : std::make_shared<SceneInstance>();
        node->RegisterProperties();
        *out_node = CreateHandle(node);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create SceneInstance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Scene Reference
 * ============================================================================ */

LC_API LCResult lc_scene_instance_set_scene_reference(LCNodeHandle node, const char* filepath) {
    auto instance = GetSceneInstance(node);
    if (!instance) return LC_ERROR_INVALID_HANDLE;

    bool success = instance->SetSceneReference(filepath ? filepath : "");
    return success ? LC_SUCCESS : LC_ERROR_INTERNAL_ERROR;
}

LC_API LCResult lc_scene_instance_get_scene_reference(LCNodeHandle node, char* out_buffer, size_t buffer_size) {
    if (!out_buffer) return LC_ERROR_NULL_POINTER;
    auto instance = GetSceneInstance(node);
    if (!instance) return LC_ERROR_INVALID_HANDLE;

    std::string path = instance->GetSceneReference();
    CopyStringToBuffer(out_buffer, buffer_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_scene_instance_reload_scene(LCNodeHandle node) {
    auto instance = GetSceneInstance(node);
    if (!instance) return LC_ERROR_INVALID_HANDLE;

    bool success = instance->ReloadScene();
    return success ? LC_SUCCESS : LC_ERROR_INTERNAL_ERROR;
}

LC_API LCResult lc_scene_instance_clear(LCNodeHandle node) {
    auto instance = GetSceneInstance(node);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    instance->ClearInstance();
    return LC_SUCCESS;
}

/* ============================================================================
 * State Queries
 * ============================================================================ */

LC_API LCResult lc_scene_instance_has_valid_reference(LCNodeHandle node, bool* out_valid) {
    if (!out_valid) return LC_ERROR_NULL_POINTER;
    auto instance = GetSceneInstance(node);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    *out_valid = instance->HasValidReference();
    return LC_SUCCESS;
}

LC_API LCResult lc_scene_instance_get_instanced_root(LCNodeHandle node, LCNodeHandle* out_root) {
    if (!out_root) return LC_ERROR_NULL_POINTER;
    auto instance = GetSceneInstance(node);
    if (!instance) return LC_ERROR_INVALID_HANDLE;

    auto root = instance->GetInstancedRoot();
    if (root) {
        *out_root = CreateHandle(root);
    } else {
        *out_root = nullptr;
    }
    return LC_SUCCESS;
}

/* ============================================================================
 * One-call Instantiation
 * ============================================================================ */

LC_API LCResult lc_instantiate_scene(const char* scene_path, LCNodeHandle parent_or_null, LCNodeHandle* out_node) {
    if (!scene_path) {
        SetError(LC_ERROR_NULL_POINTER, "scene_path must not be NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_node) {
        SetError(LC_ERROR_NULL_POINTER, "out_node must not be NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::shared_ptr<lupine::core::Node> attachParent = ResolveSpawnParent(parent_or_null);
        if (!attachParent) {
            SetError(LC_ERROR_OPERATION_FAILED, "No parent or scene root to attach the scene instance to");
            return LC_ERROR_OPERATION_FAILED;
        }

        std::string resolvedPath = lupine::asset::Asset::ResolveAssetPath(scene_path);
        if (resolvedPath.empty()) {
            resolvedPath = scene_path;
        }

        std::shared_ptr<lupine::core::SceneInstance> sceneInstance =
            std::make_shared<lupine::core::SceneInstance>();
        sceneInstance->RegisterProperties();
        attachParent->AddChild(sceneInstance);

        if (!sceneInstance->SetSceneReference(resolvedPath)) {
            attachParent->RemoveChild(sceneInstance);
            SetError(LC_ERROR_OPERATION_FAILED, "Failed to load the referenced scene");
            return LC_ERROR_OPERATION_FAILED;
        }

        *out_node = CreateHandle(sceneInstance);
        return LC_SUCCESS;

    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_instantiate_scene");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_instantiate_prefab(const char* prefab_path, LCNodeHandle parent_or_null, LCNodeHandle* out_node) {
    if (!prefab_path) {
        SetError(LC_ERROR_NULL_POINTER, "prefab_path must not be NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_node) {
        SetError(LC_ERROR_NULL_POINTER, "out_node must not be NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string resolvedPath = lupine::asset::Asset::ResolveAssetPath(prefab_path);
        if (resolvedPath.empty()) {
            resolvedPath = prefab_path;
        }

        lupine::core::Prefab prefab;
        if (!prefab.Load(resolvedPath)) {
            SetError(LC_ERROR_OPERATION_FAILED, "Failed to load prefab");
            return LC_ERROR_OPERATION_FAILED;
        }

        std::shared_ptr<lupine::core::Node> instance = prefab.Instantiate();
        if (!instance) {
            SetError(LC_ERROR_OPERATION_FAILED, "Prefab produced no instance");
            return LC_ERROR_OPERATION_FAILED;
        }

        std::shared_ptr<lupine::core::Node> attachParent = ResolveSpawnParent(parent_or_null);
        if (!attachParent) {
            SetError(LC_ERROR_OPERATION_FAILED, "No parent or scene root to attach the prefab to");
            return LC_ERROR_OPERATION_FAILED;
        }

        attachParent->AddChild(instance);
        *out_node = CreateHandle(instance);
        return LC_SUCCESS;

    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_instantiate_prefab");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
