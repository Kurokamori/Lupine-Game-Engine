/**
 * @file lc_prefab.cpp
 * @brief Implementation of Prefab C API
 */

#include "core/lc_prefab.h"
#include "lc_internal.h"

#include <lupine/core/Prefab.hpp>
#include <cstring>
#include <unordered_map>
#include <mutex>

using namespace lupine;
using namespace lupine::core;

namespace {

// Thread-safe storage for prefab handles
std::unordered_map<LCPrefabHandle, std::shared_ptr<Prefab>> g_PrefabHandles;
std::mutex g_PrefabMutex;
uint64_t g_NextPrefabHandle = 1;

LCPrefabHandle CreatePrefabHandle(std::shared_ptr<Prefab> prefab) {
    std::lock_guard<std::mutex> lock(g_PrefabMutex);
    LCPrefabHandle handle = reinterpret_cast<LCPrefabHandle>(g_NextPrefabHandle++);
    g_PrefabHandles[handle] = prefab;
    return handle;
}

std::shared_ptr<Prefab> GetPrefab(LCPrefabHandle handle) {
    std::lock_guard<std::mutex> lock(g_PrefabMutex);
    auto it = g_PrefabHandles.find(handle);
    if (it != g_PrefabHandles.end()) {
        return it->second;
    }
    return nullptr;
}

void RemovePrefabHandle(LCPrefabHandle handle) {
    std::lock_guard<std::mutex> lock(g_PrefabMutex);
    g_PrefabHandles.erase(handle);
}

} // anonymous namespace

/* ============================================================================
 * Prefab Creation and Destruction
 * ============================================================================ */

LC_API LCResult lc_prefab_create(const char* name, LCPrefabHandle* out_prefab) {
    if (!out_prefab) {
        SetError(LC_ERROR_NULL_POINTER, "out_prefab is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto prefab = name ? std::make_shared<Prefab>(name) : std::make_shared<Prefab>();
        *out_prefab = CreatePrefabHandle(prefab);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create Prefab");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_prefab_create_from_node(LCNodeHandle root_node, LCPrefabHandle* out_prefab) {
    if (!out_prefab) {
        SetError(LC_ERROR_NULL_POINTER, "out_prefab is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto node = GetNode(root_node);
    if (!node) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto prefab = std::make_shared<Prefab>();
        prefab->CreateFromNode(node);
        *out_prefab = CreatePrefabHandle(prefab);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create Prefab from node");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_prefab_destroy(LCPrefabHandle prefab) {
    if (!GetPrefab(prefab)) {
        return LC_ERROR_INVALID_HANDLE;
    }
    RemovePrefabHandle(prefab);
    return LC_SUCCESS;
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

LC_API LCResult lc_prefab_load(const char* filepath, LCPrefabHandle* out_prefab) {
    if (!filepath || !out_prefab) {
        SetError(LC_ERROR_NULL_POINTER, "filepath or out_prefab is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto prefab = std::make_shared<Prefab>();
        if (prefab->Load(filepath)) {
            *out_prefab = CreatePrefabHandle(prefab);
            return LC_SUCCESS;
        } else {
            SetError(LC_ERROR_INTERNAL_ERROR, "Failed to load prefab from file");
            return LC_ERROR_INTERNAL_ERROR;
        }
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to load Prefab");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_prefab_save(LCPrefabHandle prefab, const char* filepath) {
    auto p = GetPrefab(prefab);
    if (!p) return LC_ERROR_INVALID_HANDLE;

    bool success;
    if (filepath) {
        success = p->Save(filepath);
    } else {
        success = p->Save();
    }
    return success ? LC_SUCCESS : LC_ERROR_INTERNAL_ERROR;
}

/* ============================================================================
 * Properties
 * ============================================================================ */

LC_API LCResult lc_prefab_get_name(LCPrefabHandle prefab, char* out_buffer, size_t buffer_size) {
    if (!out_buffer) return LC_ERROR_NULL_POINTER;
    auto p = GetPrefab(prefab);
    if (!p) return LC_ERROR_INVALID_HANDLE;

    std::string name = p->GetName();
    CopyStringToBuffer(out_buffer, buffer_size, name.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_prefab_set_name(LCPrefabHandle prefab, const char* name) {
    auto p = GetPrefab(prefab);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetName(name ? name : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_prefab_get_filepath(LCPrefabHandle prefab, char* out_buffer, size_t buffer_size) {
    if (!out_buffer) return LC_ERROR_NULL_POINTER;
    auto p = GetPrefab(prefab);
    if (!p) return LC_ERROR_INVALID_HANDLE;

    std::string path = p->GetFilePath();
    CopyStringToBuffer(out_buffer, buffer_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_prefab_is_valid(LCPrefabHandle prefab, bool* out_valid) {
    if (!out_valid) return LC_ERROR_NULL_POINTER;
    auto p = GetPrefab(prefab);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *out_valid = p->IsValid();
    return LC_SUCCESS;
}

/* ============================================================================
 * Instantiation
 * ============================================================================ */

LC_API LCResult lc_prefab_instantiate(LCPrefabHandle prefab, LCNodeHandle* out_root) {
    if (!out_root) return LC_ERROR_NULL_POINTER;
    auto p = GetPrefab(prefab);
    if (!p) return LC_ERROR_INVALID_HANDLE;

    auto root = p->Instantiate();
    if (root) {
        *out_root = CreateHandle(root);
        return LC_SUCCESS;
    } else {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to instantiate prefab");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_prefab_instantiate_as_child(LCPrefabHandle prefab, LCNodeHandle parent, LCNodeHandle* out_root) {
    if (!out_root) return LC_ERROR_NULL_POINTER;
    auto p = GetPrefab(prefab);
    if (!p) return LC_ERROR_INVALID_HANDLE;

    auto parentNode = GetNode(parent);
    if (!parentNode) return LC_ERROR_INVALID_HANDLE;

    auto root = p->InstantiateAsChild(parentNode);
    if (root) {
        *out_root = CreateHandle(root);
        return LC_SUCCESS;
    } else {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to instantiate prefab as child");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Utility
 * ============================================================================ */

LC_API LCResult lc_prefab_clear(LCPrefabHandle prefab) {
    auto p = GetPrefab(prefab);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->Clear();
    return LC_SUCCESS;
}
