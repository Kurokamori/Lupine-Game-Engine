/**
 * @file lc_collision2d.cpp
 * @brief Lupine Engine C API - CollisionBody2D implementation
 */

#include "physics/lc_collision2d.h"
#include "../core/lc_internal.h"

#include <lupine/components/CollisionBody2DComponent.hpp>

namespace {

void SetCollision2DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API Vec2 to engine Vec2
lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);
}

// Convert engine Vec2 to C API Vec2
LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

// Convert C API Color to engine Color
lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

// Convert engine Color to C API Color
LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

// Convert C API shape to engine shape
lupine::components::CollisionShape2DType ToEngineShape2D(LCCollisionShape2D shape) {
    switch (shape) {
        case LC_COLLISION2D_RECTANGLE: return lupine::components::CollisionShape2DType::Rectangle;
        case LC_COLLISION2D_CIRCLE:    return lupine::components::CollisionShape2DType::Circle;
        case LC_COLLISION2D_TRIANGLE:  return lupine::components::CollisionShape2DType::Triangle;
        case LC_COLLISION2D_PENTAGON:  return lupine::components::CollisionShape2DType::Pentagon;
        case LC_COLLISION2D_HEXAGON:   return lupine::components::CollisionShape2DType::Hexagon;
        case LC_COLLISION2D_POLYGON:   return lupine::components::CollisionShape2DType::Polygon;
        default:                       return lupine::components::CollisionShape2DType::Rectangle;
    }
}

// Convert engine shape to C API shape
LCCollisionShape2D FromEngineShape2D(lupine::components::CollisionShape2DType shape) {
    switch (shape) {
        case lupine::components::CollisionShape2DType::Rectangle: return LC_COLLISION2D_RECTANGLE;
        case lupine::components::CollisionShape2DType::Circle:    return LC_COLLISION2D_CIRCLE;
        case lupine::components::CollisionShape2DType::Triangle:  return LC_COLLISION2D_TRIANGLE;
        case lupine::components::CollisionShape2DType::Pentagon:  return LC_COLLISION2D_PENTAGON;
        case lupine::components::CollisionShape2DType::Hexagon:   return LC_COLLISION2D_HEXAGON;
        case lupine::components::CollisionShape2DType::Polygon:   return LC_COLLISION2D_POLYGON;
        default:                                                   return LC_COLLISION2D_RECTANGLE;
    }
}

} // anonymous namespace

/* ============================================================================
 * CollisionBody2D Creation
 * ============================================================================ */

LC_API LCResult lc_collision2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::CollisionBody2DComponent>(compName);
        comp->DefineProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to create CollisionBody2D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Shape Configuration
 * ============================================================================ */

LC_API LCResult lc_collision2d_get_shape_type(LCComponentHandle component, LCCollisionShape2D* out_shape) {
    if (!out_shape) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_shape is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_shape = FromEngineShape2D(collision->GetShapeType());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get shape type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_shape_type(LCComponentHandle component, LCCollisionShape2D shape) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetShapeType(ToEngineShape2D(shape));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set shape type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_get_size(LCComponentHandle component, LCVec2* out_size) {
    if (!out_size) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_size = FromEngineVec2(collision->GetSize());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_size(LCComponentHandle component, LCVec2 size) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetSize(ToEngineVec2(size));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_get_radius(LCComponentHandle component, float* out_radius) {
    if (!out_radius) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_radius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_radius = collision->GetRadius();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get radius");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_radius(LCComponentHandle component, float radius) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetRadius(radius);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set radius");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_get_offset(LCComponentHandle component, LCVec2* out_offset) {
    if (!out_offset) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_offset = FromEngineVec2(collision->GetOffset());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_offset(LCComponentHandle component, LCVec2 offset) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetOffset(ToEngineVec2(offset));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Polygon Vertices
 * ============================================================================ */

LC_API LCResult lc_collision2d_get_vertex_count(LCComponentHandle component, int* out_count) {
    if (!out_count) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_count = static_cast<int>(collision->GetVertices().size());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get vertex count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_get_vertex(LCComponentHandle component, int index, LCVec2* out_vertex) {
    if (!out_vertex) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_vertex is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        const auto& vertices = collision->GetVertices();
        if (index < 0 || index >= static_cast<int>(vertices.size())) {
            SetCollision2DError(LC_ERROR_INVALID_PARAMETER, "Vertex index out of bounds");
            return LC_ERROR_INVALID_PARAMETER;
        }

        *out_vertex = FromEngineVec2(vertices[index]);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get vertex");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_vertices(LCComponentHandle component, const LCVec2* vertices, int count) {
    if (!vertices && count > 0) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "vertices is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::vector<lupine::math::Vec2> engineVertices;
        engineVertices.reserve(count);
        for (int i = 0; i < count; ++i) {
            engineVertices.push_back(ToEngineVec2(vertices[i]));
        }

        collision->SetVertices(engineVertices);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set vertices");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_add_vertex(LCComponentHandle component, LCVec2 vertex) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->AddVertex(ToEngineVec2(vertex));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to add vertex");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_update_vertex(LCComponentHandle component, int index, LCVec2 vertex) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->UpdateVertex(index, ToEngineVec2(vertex));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to update vertex");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_remove_vertex(LCComponentHandle component, int index) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->RemoveVertex(index);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to remove vertex");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_clear_vertices(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->ClearVertices();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to clear vertices");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Physics Material Properties
 * ============================================================================ */

LC_API LCResult lc_collision2d_get_density(LCComponentHandle component, float* out_density) {
    if (!out_density) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_density is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_density = collision->GetDensity();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get density");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_density(LCComponentHandle component, float density) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetDensity(density);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set density");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_get_friction(LCComponentHandle component, float* out_friction) {
    if (!out_friction) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_friction is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_friction = collision->GetFriction();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get friction");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_friction(LCComponentHandle component, float friction) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetFriction(friction);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set friction");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_get_restitution(LCComponentHandle component, float* out_restitution) {
    if (!out_restitution) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_restitution is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_restitution = collision->GetRestitution();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get restitution");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_restitution(LCComponentHandle component, float restitution) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetRestitution(restitution);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set restitution");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_is_sensor(LCComponentHandle component, bool* out_is_sensor) {
    if (!out_is_sensor) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_is_sensor is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_is_sensor = collision->IsSensor();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get sensor state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_sensor(LCComponentHandle component, bool is_sensor) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetSensor(is_sensor);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set sensor state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_get_collision_layers(LCComponentHandle component, uint32_t* out_layers) {
    if (!out_layers) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_layers is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_layers = collision->GetCollisionLayers();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get collision layers");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_collision_layers(LCComponentHandle component, uint32_t layers) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetCollisionLayers(layers);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set collision layers");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_get_collision_mask(LCComponentHandle component, uint32_t* out_mask) {
    if (!out_mask) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_mask is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_mask = collision->GetCollisionMask();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get collision mask");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_collision_mask(LCComponentHandle component, uint32_t mask) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetCollisionMask(mask);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set collision mask");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Debug Visualization
 * ============================================================================ */

LC_API LCResult lc_collision2d_get_debug_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetCollision2DError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(collision->GetDebugColor());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get debug color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision2d_set_debug_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionBody2DComponent>(comp);
        if (!collision) {
            SetCollision2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetDebugColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set debug color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
