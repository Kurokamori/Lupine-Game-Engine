/**
 * @file lc_area_trigger2d.cpp
 * @brief Lupine Engine C API - AreaTrigger2D implementation
 */

#include "physics/lc_area_trigger2d.h"
#include "../core/lc_internal.h"

#include <lupine/components/AreaTrigger2DComponent.hpp>
#include <cstring>
#include <vector>

namespace {

void SetAreaTrigger2DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);
}

LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

// Resolve a component handle to an AreaTrigger2DComponent, setting the matching
// error and returning nullptr on failure.
std::shared_ptr<lupine::components::AreaTrigger2DComponent> GetAreaTrigger2D(LCComponentHandle component) {
    auto comp = GetComponent(component);
    if (!comp) {
        SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        return nullptr;
    }
    auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
    if (!trigger) {
        SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
    }
    return trigger;
}

} // anonymous namespace

/* ============================================================================
 * AreaTrigger2D Creation
 * ============================================================================ */

LC_API LCResult lc_area_trigger2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::AreaTrigger2DComponent>(compName);
        comp->DefineProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to create AreaTrigger2D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Monitoring Configuration
 * ============================================================================ */

LC_API LCResult lc_area_trigger2d_get_monitoring(LCComponentHandle component, bool* out_monitoring) {
    if (!out_monitoring) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_monitoring is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_monitoring = trigger->GetMonitoring();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get monitoring state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_monitoring(LCComponentHandle component, bool monitoring) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        trigger->SetMonitoring(monitoring);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set monitoring state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_monitorable(LCComponentHandle component, bool* out_monitorable) {
    if (!out_monitorable) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_monitorable is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_monitorable = trigger->GetMonitorable();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get monitorable state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_monitorable(LCComponentHandle component, bool monitorable) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        trigger->SetMonitorable(monitorable);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set monitorable state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_priority(LCComponentHandle component, int* out_priority) {
    if (!out_priority) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_priority is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_priority = trigger->GetPriority();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get priority");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_priority(LCComponentHandle component, int priority) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        trigger->SetPriority(priority);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set priority");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Overlap Detection
 * ============================================================================ */

LC_API LCResult lc_area_trigger2d_get_overlapping_count(LCComponentHandle component, int* out_count) {
    if (!out_count) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_count = static_cast<int>(trigger->GetOverlappingBodies().size());
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get overlapping count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_overlapping_body(LCComponentHandle component, int index, char* out_uuid, int buffer_size) {
    if (!out_uuid || buffer_size <= 0) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_uuid is NULL or buffer_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        const auto& bodies = trigger->GetOverlappingBodies();
        if (index < 0 || index >= static_cast<int>(bodies.size())) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_PARAMETER, "Index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        std::string uuidStr = bodies[index].ToString();
        CopyStringToBuffer(out_uuid, buffer_size, uuidStr.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get overlapping body");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_is_overlapping(LCComponentHandle component, const char* uuid, bool* out_is_overlapping) {
    if (!uuid || !out_is_overlapping) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "uuid or out_is_overlapping is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto trigger = std::dynamic_pointer_cast<lupine::components::AreaTrigger2DComponent>(comp);
        if (!trigger) {
            SetAreaTrigger2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an AreaTrigger2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        lupine::core::UUID bodyId = lupine::core::UUID::FromString(uuid);
        *out_is_overlapping = trigger->IsOverlapping(bodyId);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to check overlap");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Sensor Shape Configuration
 * ============================================================================ */

LC_API LCResult lc_area_trigger2d_get_shape_type(LCComponentHandle component, LCCollisionShape2D* out_shape) {
    if (!out_shape) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_shape is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        *out_shape = static_cast<LCCollisionShape2D>(trigger->GetShapeType());
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get shape type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_shape_type(LCComponentHandle component, LCCollisionShape2D shape) {
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        trigger->SetShapeType(static_cast<lupine::components::CollisionShape2DType>(shape));
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set shape type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_size(LCComponentHandle component, LCVec2* out_size) {
    if (!out_size) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        *out_size = FromEngineVec2(trigger->GetSize());
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_size(LCComponentHandle component, LCVec2 size) {
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        trigger->SetSize(ToEngineVec2(size));
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_radius(LCComponentHandle component, float* out_radius) {
    if (!out_radius) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_radius is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        *out_radius = trigger->GetRadius();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get radius");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_radius(LCComponentHandle component, float radius) {
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        trigger->SetRadius(radius);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set radius");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_offset(LCComponentHandle component, LCVec2* out_offset) {
    if (!out_offset) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        *out_offset = FromEngineVec2(trigger->GetOffset());
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_offset(LCComponentHandle component, LCVec2 offset) {
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        trigger->SetOffset(ToEngineVec2(offset));
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_collision_layers(LCComponentHandle component, uint32_t* out_layers) {
    if (!out_layers) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_layers is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        *out_layers = trigger->GetCollisionLayers();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get collision layers");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_collision_layers(LCComponentHandle component, uint32_t layers) {
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        trigger->SetCollisionLayers(layers);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set collision layers");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_collision_mask(LCComponentHandle component, uint32_t* out_mask) {
    if (!out_mask) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_mask is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        *out_mask = trigger->GetCollisionMask();
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get collision mask");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_collision_mask(LCComponentHandle component, uint32_t mask) {
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        trigger->SetCollisionMask(mask);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set collision mask");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_vertex_count(LCComponentHandle component, int* out_count) {
    if (!out_count) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        *out_count = static_cast<int>(trigger->GetVertices().size());
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get vertex count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_get_vertex(LCComponentHandle component, int index, LCVec2* out_vertex) {
    if (!out_vertex) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "out_vertex is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        const auto& vertices = trigger->GetVertices();
        if (index < 0 || index >= static_cast<int>(vertices.size())) {
            SetAreaTrigger2DError(LC_ERROR_INVALID_PARAMETER, "Vertex index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }
        *out_vertex = FromEngineVec2(vertices[index]);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get vertex");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_area_trigger2d_set_vertices(LCComponentHandle component, const LCVec2* vertices, int count) {
    if (!vertices && count > 0) {
        SetAreaTrigger2DError(LC_ERROR_NULL_POINTER, "vertices is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (count < 0) {
        SetAreaTrigger2DError(LC_ERROR_INVALID_PARAMETER, "count cannot be negative");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        auto trigger = GetAreaTrigger2D(component);
        if (!trigger) return LC_ERROR_COMPONENT_INVALID_TYPE;
        std::vector<lupine::math::Vec2> engineVertices;
        engineVertices.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            engineVertices.push_back(ToEngineVec2(vertices[i]));
        }
        trigger->SetVertices(engineVertices);
        return LC_SUCCESS;
    } catch (...) {
        SetAreaTrigger2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set vertices");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
