/**
 * @file lc_async_asset.cpp
 * @brief Implementation of the C API for the async priority-streaming loader.
 *
 * A thin handle-based wrapper over asset::AsyncAssetLoader (a singleton), so the
 * C surface stays in lockstep with the script-facing async archetype API.
 */

#include "asset/lc_async_asset.h"
#include "core/lc_core.h"
#include "../core/lc_internal.h"

#include <lupine/asset/AsyncAssetLoader.hpp>
#include <lupine/asset/ArchetypeInstance.hpp>
#include <lupine/scripting/ScriptAPI.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

lupine::asset::AsyncAssetLoader& Loader() {
    return lupine::asset::AsyncAssetLoader::GetInstance();
}

LCAsyncStatus ToCStatus(lupine::asset::AsyncLoadStatus status) {
    switch (status) {
        case lupine::asset::AsyncLoadStatus::Unknown:   return LC_ASYNC_STATUS_UNKNOWN;
        case lupine::asset::AsyncLoadStatus::Pending:   return LC_ASYNC_STATUS_PENDING;
        case lupine::asset::AsyncLoadStatus::Loaded:    return LC_ASYNC_STATUS_LOADED;
        case lupine::asset::AsyncLoadStatus::Failed:    return LC_ASYNC_STATUS_FAILED;
        case lupine::asset::AsyncLoadStatus::Cancelled: return LC_ASYNC_STATUS_CANCELLED;
    }
    return LC_ASYNC_STATUS_UNKNOWN;
}

LCResult WriteStringBuffer(const std::string& value, char* buffer, uint32_t max_length) {
    if (!buffer) {
        SetError(LC_ERROR_NULL_POINTER, "buffer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (max_length == 0) {
        SetError(LC_ERROR_INVALID_PARAMETER, "max_length is 0");
        return LC_ERROR_INVALID_PARAMETER;
    }
    std::size_t copyLength = std::min(static_cast<std::size_t>(max_length - 1), value.size());
    std::memcpy(buffer, value.c_str(), copyLength);
    buffer[copyLength] = '\0';
    return LC_SUCCESS;
}

} // namespace

LCResult lc_async_load_archetype_instance(const char* res_path, int priority,
                                          LCAsyncHandle* out_handle) {
    if (!out_handle) {
        SetError(LC_ERROR_NULL_POINTER, "out_handle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!res_path || res_path[0] == '\0') {
        *out_handle = 0;
        SetError(LC_ERROR_INVALID_PARAMETER, "res_path is NULL or empty");
        return LC_ERROR_INVALID_PARAMETER;
    }
    *out_handle = Loader().LoadArchetypeInstanceAsync(res_path, nullptr, priority);
    return LC_SUCCESS;
}

LCResult lc_async_load_archetype_definition(const char* file_path, int priority,
                                            LCAsyncHandle* out_handle) {
    if (!out_handle) {
        SetError(LC_ERROR_NULL_POINTER, "out_handle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!file_path || file_path[0] == '\0') {
        *out_handle = 0;
        SetError(LC_ERROR_INVALID_PARAMETER, "file_path is NULL or empty");
        return LC_ERROR_INVALID_PARAMETER;
    }
    *out_handle = Loader().LoadArchetypeDefinitionAsync(file_path, nullptr, priority);
    return LC_SUCCESS;
}

LCResult lc_async_pump(void) {
    Loader().Pump();
    return LC_SUCCESS;
}

LCResult lc_async_get_status(LCAsyncHandle handle, LCAsyncStatus* out_status) {
    if (!out_status) {
        SetError(LC_ERROR_NULL_POINTER, "out_status is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_status = ToCStatus(Loader().GetStatus(handle));
    return LC_SUCCESS;
}

LCResult lc_async_is_complete(LCAsyncHandle handle, bool* out_complete) {
    if (!out_complete) {
        SetError(LC_ERROR_NULL_POINTER, "out_complete is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_complete = Loader().IsComplete(handle);
    return LC_SUCCESS;
}

LCResult lc_async_get_class_name(LCAsyncHandle handle, char* buffer, uint32_t max_length) {
    return WriteStringBuffer(Loader().GetClassName(handle), buffer, max_length);
}

LCResult lc_async_get_resolved_fields_json(LCAsyncHandle handle, bool forget, char** out_json) {
    if (!out_json) {
        SetError(LC_ERROR_NULL_POINTER, "out_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    lupine::asset::AssetRef<lupine::asset::ArchetypeInstance> instance =
        Loader().GetArchetypeInstance(handle);
    if (!instance) {
        SetError(LC_ERROR_NOT_FOUND, "handle is not a loaded archetype instance");
        return LC_ERROR_NOT_FOUND;
    }
    std::string dumped = instance->GetResolvedFields().dump();
    if (forget) {
        Loader().Forget(handle);
    }
    char* allocated = DuplicateString(dumped);
    if (!allocated) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate JSON result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    *out_json = allocated;
    return LC_SUCCESS;
}

LCResult lc_async_get_instance_path(LCAsyncHandle handle, char* buffer, uint32_t max_length) {
    lupine::asset::AssetRef<lupine::asset::ArchetypeInstance> instance =
        Loader().GetArchetypeInstance(handle);
    if (!instance) {
        SetError(LC_ERROR_NOT_FOUND, "handle is not a loaded archetype instance");
        return LC_ERROR_NOT_FOUND;
    }
    return WriteStringBuffer(instance->GetPath(), buffer, max_length);
}

LCResult lc_async_set_priority(LCAsyncHandle handle, int priority) {
    Loader().SetPriority(handle, priority);
    return LC_SUCCESS;
}

LCResult lc_async_get_priority(LCAsyncHandle handle, int* out_priority) {
    if (!out_priority) {
        SetError(LC_ERROR_NULL_POINTER, "out_priority is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_priority = Loader().GetPriority(handle);
    return LC_SUCCESS;
}

LCResult lc_async_set_max_concurrent(int max_concurrent) {
    std::size_t budget = max_concurrent > 0 ? static_cast<std::size_t>(max_concurrent) : 0;
    Loader().SetMaxConcurrentLoads(budget);
    return LC_SUCCESS;
}

LCResult lc_async_get_max_concurrent(int* out_max) {
    if (!out_max) {
        SetError(LC_ERROR_NULL_POINTER, "out_max is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_max = static_cast<int>(Loader().GetMaxConcurrentLoads());
    return LC_SUCCESS;
}

LCResult lc_async_get_inflight_count(int* out_count) {
    if (!out_count) {
        SetError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_count = static_cast<int>(Loader().GetInFlightCount());
    return LC_SUCCESS;
}

LCResult lc_async_get_queued_count(int* out_count) {
    if (!out_count) {
        SetError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_count = static_cast<int>(Loader().GetQueuedCount());
    return LC_SUCCESS;
}

LCResult lc_async_get_pending_count(int* out_count) {
    if (!out_count) {
        SetError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_count = static_cast<int>(Loader().GetPendingCount());
    return LC_SUCCESS;
}

LCResult lc_async_cancel(LCAsyncHandle handle) {
    Loader().Cancel(handle);
    return LC_SUCCESS;
}

LCResult lc_async_forget(LCAsyncHandle handle) {
    Loader().Forget(handle);
    return LC_SUCCESS;
}

LCResult lc_async_reset(void) {
    Loader().Reset();
    return LC_SUCCESS;
}

LCResult lc_archetype_load_instance(const char* res_path, char** out_fields_json) {
    if (!out_fields_json) {
        SetError(LC_ERROR_NULL_POINTER, "out_fields_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!res_path || res_path[0] == '\0') {
        SetError(LC_ERROR_INVALID_PARAMETER, "res_path is NULL or empty");
        return LC_ERROR_INVALID_PARAMETER;
    }
    lupine::asset::AssetRef<lupine::asset::ArchetypeInstance> instance(
        new lupine::asset::ArchetypeInstance());
    if (!instance->LoadFromFile(res_path)) {
        SetError(LC_ERROR_OPERATION_FAILED, "Failed to load archetype instance");
        return LC_ERROR_OPERATION_FAILED;
    }
    std::string dumped = instance->GetResolvedFields().dump();
    char* allocated = DuplicateString(dumped);
    if (!allocated) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate JSON result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    *out_fields_json = allocated;
    return LC_SUCCESS;
}

LCResult lc_archetype_invalidate_caches(void) {
    lupine::scripting::ScriptAPI::InvalidateArchetypeCaches();
    return LC_SUCCESS;
}
