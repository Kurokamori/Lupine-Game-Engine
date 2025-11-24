

#include "core/lc_scene.h"
#include "core/lc_core.h"
#include "core/lc_node.h"

#include <lupine\core\Scene.hpp>
#include <lupine\core\Node.hpp>

#include <unordered_map>
#include <mutex>
#include <memory>

namespace {

std::unordered_map<LCSceneHandle, std::shared_ptr<lupine::core::Scene>> g_sceneHandles;
std::mutex g_sceneHandlesMutex;

extern std::shared_ptr<lupine::core::Node> GetNode(LCNodeHandle handle);
extern LCNodeHandle CreateHandle(std::shared_ptr<lupine::core::Node> node);

LCSceneHandle CreateSceneHandle(std::shared_ptr<lupine::core::Scene> scene) {
    if (!scene) return nullptr;

    std::lock_guard<std::mutex> lock(g_sceneHandlesMutex);
    LCSceneHandle handle = reinterpret_cast<LCSceneHandle>(scene.get());
    g_sceneHandles[handle] = scene;
    return handle;
}

std::shared_ptr<lupine::core::Scene> GetScene(LCSceneHandle handle) {
    if (!handle) return nullptr;

    std::lock_guard<std::mutex> lock(g_sceneHandlesMutex);
    auto it = g_sceneHandles.find(handle);
    return (it != g_sceneHandles.end()) ? it->second : nullptr;
}

void DestroySceneHandle(LCSceneHandle handle) {
    if (!handle) return;

    std::lock_guard<std::mutex> lock(g_sceneHandlesMutex);
    g_sceneHandles.erase(handle);
}

bool IsValidSceneHandle(LCSceneHandle handle) {
    if (!handle) return false;

    std::lock_guard<std::mutex> lock(g_sceneHandlesMutex);
    return g_sceneHandles.find(handle) != g_sceneHandles.end();
}

void SetSceneError(LCResult code, const char* message) {
    extern void SetError(LCResult code, const char* message);
    SetError(code, message);
}

}

extern "C" {

LC_API LCResult lc_scene_create(const char* name, LCSceneHandle* out_scene) {
    if (!out_scene) {
        SetSceneError(LC_ERROR_NULL_POINTER, "out_scene is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string sceneName = name ? name : "Scene";
        auto scene = std::make_shared<lupine::core::Scene>(sceneName);

        *out_scene = CreateSceneHandle(scene);
        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetSceneError(LC_ERROR_OUT_OF_MEMORY, e.what());
        return LC_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        SetSceneError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_scene_create");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_destroy(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) {
        SetSceneError(LC_ERROR_INVALID_HANDLE, "Invalid scene handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        if (scenePtr->IsInitialized()) {
            scenePtr->Shutdown();
        }

        DestroySceneHandle(scene);

        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetSceneError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    } catch (...) {
        SetSceneError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_scene_destroy");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Scene Loading and Saving
 * ============================================================================ */

LC_API LCResult lc_scene_load(const char* filepath, LCSceneHandle* out_scene) {
    if (!filepath) {
        SetSceneError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!out_scene) {
        SetSceneError(LC_ERROR_NULL_POINTER, "out_scene is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto scene = std::make_shared<lupine::core::Scene>();

        if (!scene->Load(filepath)) {
            SetSceneError(LC_ERROR_SCENE_LOAD_FAILED, "Failed to load scene file");
            return LC_ERROR_SCENE_LOAD_FAILED;
        }

        *out_scene = CreateSceneHandle(scene);
        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetSceneError(LC_ERROR_SCENE_LOAD_FAILED, e.what());
        return LC_ERROR_SCENE_LOAD_FAILED;
    } catch (...) {
        SetSceneError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_scene_load");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_save(LCSceneHandle scene, const char* filepath) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;
    if (!filepath) return LC_ERROR_NULL_POINTER;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        if (!scenePtr->Save(filepath)) {
            SetSceneError(LC_ERROR_SCENE_SAVE_FAILED, "Failed to save scene file");
            return LC_ERROR_SCENE_SAVE_FAILED;
        }

        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetSceneError(LC_ERROR_SCENE_SAVE_FAILED, e.what());
        return LC_ERROR_SCENE_SAVE_FAILED;
    } catch (...) {
        SetSceneError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_scene_save");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_save_current(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        if (scenePtr->GetFilePath().empty()) {
            SetSceneError(LC_ERROR_SCENE_SAVE_FAILED, "Scene has no filepath");
            return LC_ERROR_SCENE_SAVE_FAILED;
        }

        if (!scenePtr->Save()) {
            SetSceneError(LC_ERROR_SCENE_SAVE_FAILED, "Failed to save scene");
            return LC_ERROR_SCENE_SAVE_FAILED;
        }

        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetSceneError(LC_ERROR_SCENE_SAVE_FAILED, e.what());
        return LC_ERROR_SCENE_SAVE_FAILED;
    } catch (...) {
        SetSceneError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_scene_save_current");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Scene Properties
 * ============================================================================ */

LC_API const char* lc_scene_get_name(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return nullptr;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return nullptr;
        return scenePtr->GetName().c_str();
    } catch (...) {
        return nullptr;
    }
}

LC_API LCResult lc_scene_set_name(LCSceneHandle scene, const char* name) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;
    if (!name) return LC_ERROR_NULL_POINTER;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;
        scenePtr->SetName(name);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API const char* lc_scene_get_filepath(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return nullptr;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return nullptr;

        const std::string& filepath = scenePtr->GetFilePath();
        if (filepath.empty()) return nullptr;

        return filepath.c_str();
    } catch (...) {
        return nullptr;
    }
}

LC_API bool lc_scene_is_initialized(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return false;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return false;
        return scenePtr->IsInitialized();
    } catch (...) {
        return false;
    }
}

LC_API bool lc_scene_is_active(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return false;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return false;
        return scenePtr->IsActive();
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_scene_set_active(LCSceneHandle scene, bool active) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;
        scenePtr->SetActive(active);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_scene_is_in_editor(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return false;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return false;
        return scenePtr->IsInEditor();
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_scene_set_in_editor(LCSceneHandle scene, bool in_editor) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;
        scenePtr->SetInEditor(in_editor);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Scene Graph Management
 * ============================================================================ */

LC_API LCResult lc_scene_set_root(LCSceneHandle scene, LCNodeHandle root) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        auto nodePtr = GetNode(root);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->SetRoot(nodePtr);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_get_root(LCSceneHandle scene, LCNodeHandle* out_root) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;
    if (!out_root) return LC_ERROR_NULL_POINTER;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        auto rootPtr = scenePtr->GetRoot();
        if (rootPtr) {
            *out_root = CreateHandle(rootPtr);
            return LC_SUCCESS;
        } else {
            *out_root = nullptr;
            return LC_SUCCESS;
        }

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_add_node(LCSceneHandle scene, LCNodeHandle node) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->AddNode(nodePtr);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_remove_node(LCSceneHandle scene, LCNodeHandle node) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->RemoveNode(nodePtr);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_find_node(LCSceneHandle scene, const char* path, LCNodeHandle* out_node) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;
    if (!path) return LC_ERROR_NULL_POINTER;
    if (!out_node) return LC_ERROR_NULL_POINTER;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        auto foundPtr = scenePtr->FindNode(path);
        if (foundPtr) {
            *out_node = CreateHandle(foundPtr);
            return LC_SUCCESS;
        } else {
            *out_node = nullptr;
            return LC_ERROR_INVALID_PARAMETER;
        }

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_find_node_by_uuid(LCSceneHandle scene, const char* uuid, LCNodeHandle* out_node) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;
    if (!uuid) return LC_ERROR_NULL_POINTER;
    if (!out_node) return LC_ERROR_NULL_POINTER;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::UUID uuidObj = lupine::core::UUID::FromString(uuid);
        auto foundPtr = scenePtr->FindNodeByUUID(uuidObj);

        if (foundPtr) {
            *out_node = CreateHandle(foundPtr);
            return LC_SUCCESS;
        } else {
            *out_node = nullptr;
            return LC_ERROR_INVALID_PARAMETER;
        }

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Scene Lifecycle
 * ============================================================================ */

LC_API LCResult lc_scene_initialize(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->Initialize();
        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetSceneError(LC_ERROR_INITIALIZATION_FAILED, e.what());
        return LC_ERROR_INITIALIZATION_FAILED;
    } catch (...) {
        SetSceneError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_scene_initialize");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_shutdown(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->Shutdown();
        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetSceneError(LC_ERROR_SHUTDOWN_FAILED, e.what());
        return LC_ERROR_SHUTDOWN_FAILED;
    } catch (...) {
        SetSceneError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_scene_shutdown");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_update(LCSceneHandle scene, float delta_time) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->Update(delta_time);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_physics_update(LCSceneHandle scene, float delta_time) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->PhysicsUpdate(delta_time);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_render(LCSceneHandle scene) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->Render();
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_process_input(LCSceneHandle scene, float delta_time) {
    if (!IsValidSceneHandle(scene)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto scenePtr = GetScene(scene);
        if (!scenePtr) return LC_ERROR_INVALID_HANDLE;

        scenePtr->ProcessInput(delta_time);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

}
