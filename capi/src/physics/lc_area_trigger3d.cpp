/**
 * @file lc_area_trigger3d.cpp
 * @brief Lupine Engine C API - AreaTrigger3D implementation
 */

#include "physics/lc_area_trigger3d.h"
#include "../core/lc_internal.h"

#include <lupine/components/AreaTrigger3DComponent.hpp>
#include <cstring>

namespace {

void SetAreaTrigger3DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

} // anonymous namespace

/* ============================================================================
 * AreaTrigger3D Creation
 * ============================================================================ */

LC_API LCResult lc_area_trigger3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetAreaTrigger3DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::AreaTrigger3DComponent>(compName);
        comp->DefineProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to create AreaTrigger3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Monitoring Configuration
 * ============================================================================ */

LC_API LCResult lc_area_trigger3d_get_monitoring(LCComponentHandle component, bool* out_monitoring) {
    if (!out_monitoring) {
        SetAreaTrigger3DError(LC_ERROR_NULL_POINTER, "out_monitoring is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_monitoring = trigger->GetMonitoring();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get monitoring state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger3d_set_monitoring(LCComponentHandle component, bool monitoring) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        trigger->SetMonitoring(monitoring);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set monitoring state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger3d_get_monitorable(LCComponentHandle component, bool* out_monitorable) {
    if (!out_monitorable) {
        SetAreaTrigger3DError(LC_ERROR_NULL_POINTER, "out_monitorable is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_monitorable = trigger->GetMonitorable();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get monitorable state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger3d_set_monitorable(LCComponentHandle component, bool monitorable) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        trigger->SetMonitorable(monitorable);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set monitorable state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger3d_get_priority(LCComponentHandle component, int* out_priority) {
    if (!out_priority) {
        SetAreaTrigger3DError(LC_ERROR_NULL_POINTER, "out_priority is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_priority = trigger->GetPriority();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get priority");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger3d_set_priority(LCComponentHandle component, int priority) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        trigger->SetPriority(priority);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set priority");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Overlap Detection
 * ============================================================================ */

LC_API LCResult lc_area_trigger3d_get_overlapping_count(LCComponentHandle component, int* out_count) {
    if (!out_count) {
        SetAreaTrigger3DError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_count = static_cast<int>(trigger->GetOverlappingBodies().size());
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get overlapping count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger3d_get_overlapping_body(LCComponentHandle component, int index, char* out_uuid, int buffer_size) {
    if (!out_uuid || buffer_size <= 0) {
        SetAreaTrigger3DError(LC_ERROR_NULL_POINTER, "out_uuid is NULL or buffer_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        const auto& bodies = trigger->GetOverlappingBodies();
        if (index < 0 || index >= static_cast<int>(bodies.size())) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_PARAMETER, "Index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        // unordered_set doesn't have index access, so iterate
        auto it = bodies.begin();
        std::advance(it, index);
        std::string uuidStr = it->ToString();
        CopyStringToBuffer(out_uuid, buffer_size, uuidStr.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get overlapping body");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger3d_is_overlapping(LCComponentHandle component, const char* uuid, bool* out_is_overlapping) {
    if (!uuid || !out_is_overlapping) {
        SetAreaTrigger3DError(LC_ERROR_NULL_POINTER, "uuid or out_is_overlapping is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger3DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        lupine::core::UUID bodyId = lupine::core::UUID::FromString(uuid);
        *out_is_overlapping = trigger->IsOverlapping(bodyId);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger3DError(LC_ERROR_INTERNAL_ERROR, "Failed to check overlap");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
