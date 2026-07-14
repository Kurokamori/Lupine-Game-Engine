/**
 * @file lc_collision3d.cpp
 * @brief Lupine Engine C API - CollisionMesh3D implementation
 */

#include "physics/lc_collision3d.h"
#include "../core/lc_internal.h"

#include <lupine/components/CollisionMesh3DComponent.hpp>
#include <cstring>

namespace {

void SetCollision3DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API Vec3 to engine Vec3
lupine::math::Vec3 ToEngineVec3(LCVec3 vec) {
    return lupine::math::Vec3(vec.x, vec.y, vec.z);
}

// Convert engine Vec3 to C API Vec3
LCVec3 FromEngineVec3(const lupine::math::Vec3& vec) {
    return LCVec3{vec.x, vec.y, vec.z};
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
lupine::components::CollisionShape3DType ToEngineShape3D(LCCollisionShape3D shape) {
    switch (shape) {
        case LC_COLLISION3D_BOX:      return lupine::components::CollisionShape3DType::Box;
        case LC_COLLISION3D_SPHERE:   return lupine::components::CollisionShape3DType::Sphere;
        case LC_COLLISION3D_CAPSULE:  return lupine::components::CollisionShape3DType::Capsule;
        case LC_COLLISION3D_CYLINDER: return lupine::components::CollisionShape3DType::Cylinder;
        case LC_COLLISION3D_CONE:     return lupine::components::CollisionShape3DType::Cone;
        case LC_COLLISION3D_PLANE:    return lupine::components::CollisionShape3DType::Plane;
        case LC_COLLISION3D_MESH:     return lupine::components::CollisionShape3DType::Mesh;
        default:                      return lupine::components::CollisionShape3DType::Box;
    }
}

// Convert engine shape to C API shape
LCCollisionShape3D FromEngineShape3D(lupine::components::CollisionShape3DType shape) {
    switch (shape) {
        case lupine::components::CollisionShape3DType::Box:      return LC_COLLISION3D_BOX;
        case lupine::components::CollisionShape3DType::Sphere:   return LC_COLLISION3D_SPHERE;
        case lupine::components::CollisionShape3DType::Capsule:  return LC_COLLISION3D_CAPSULE;
        case lupine::components::CollisionShape3DType::Cylinder: return LC_COLLISION3D_CYLINDER;
        case lupine::components::CollisionShape3DType::Cone:     return LC_COLLISION3D_CONE;
        case lupine::components::CollisionShape3DType::Plane:    return LC_COLLISION3D_PLANE;
        case lupine::components::CollisionShape3DType::Mesh:     return LC_COLLISION3D_MESH;
        default:                                                  return LC_COLLISION3D_BOX;
    }
}

// Convert C API mesh solver to engine mesh solver
lupine::components::MeshSolverType ToEngineMeshSolver(LCMeshSolverType solver) {
    switch (solver) {
        case LC_MESH_SOLVER_BOUNDING_BOX: return lupine::components::MeshSolverType::BoundingBox;
        case LC_MESH_SOLVER_CONVEX:       return lupine::components::MeshSolverType::Convex;
        case LC_MESH_SOLVER_CONCAVE:      return lupine::components::MeshSolverType::Concave;
        case LC_MESH_SOLVER_TRIMESH:      return lupine::components::MeshSolverType::TriMesh;
        default:                          return lupine::components::MeshSolverType::BoundingBox;
    }
}

// Convert engine mesh solver to C API mesh solver
LCMeshSolverType FromEngineMeshSolver(lupine::components::MeshSolverType solver) {
    switch (solver) {
        case lupine::components::MeshSolverType::BoundingBox: return LC_MESH_SOLVER_BOUNDING_BOX;
        case lupine::components::MeshSolverType::Convex:      return LC_MESH_SOLVER_CONVEX;
        case lupine::components::MeshSolverType::Concave:     return LC_MESH_SOLVER_CONCAVE;
        case lupine::components::MeshSolverType::TriMesh:     return LC_MESH_SOLVER_TRIMESH;
        default:                                               return LC_MESH_SOLVER_BOUNDING_BOX;
    }
}

} // anonymous namespace

/* ============================================================================
 * CollisionMesh3D Creation
 * ============================================================================ */

LC_API LCResult lc_collision3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string compName = name ? name : "";
        auto comp = std::make_shared<lupine::components::CollisionMesh3DComponent>(compName);
        comp->DefineProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to create CollisionMesh3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Shape Configuration
 * ============================================================================ */

LC_API LCResult lc_collision3d_get_shape_type(LCComponentHandle component, LCCollisionShape3D* out_shape) {
    if (!out_shape) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_shape is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_shape = FromEngineShape3D(collision->GetShapeType());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get shape type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_shape_type(LCComponentHandle component, LCCollisionShape3D shape) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetShapeType(ToEngineShape3D(shape));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set shape type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_size(LCComponentHandle component, LCVec3* out_size) {
    if (!out_size) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_size = FromEngineVec3(collision->GetSize());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_size(LCComponentHandle component, LCVec3 size) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetSize(ToEngineVec3(size));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_radius(LCComponentHandle component, float* out_radius) {
    if (!out_radius) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_radius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_radius = collision->GetRadius();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get radius");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_radius(LCComponentHandle component, float radius) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetRadius(radius);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set radius");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_height(LCComponentHandle component, float* out_height) {
    if (!out_height) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_height is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_height = collision->GetHeight();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get height");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_height(LCComponentHandle component, float height) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetHeight(height);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set height");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_plane_normal(LCComponentHandle component, LCVec3* out_normal) {
    if (!out_normal) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_normal is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_normal = FromEngineVec3(collision->GetPlaneNormal());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get plane normal");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_plane_normal(LCComponentHandle component, LCVec3 normal) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetPlaneNormal(ToEngineVec3(normal));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set plane normal");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_plane_distance(LCComponentHandle component, float* out_distance) {
    if (!out_distance) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_distance is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_distance = collision->GetPlaneDistance();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get plane distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_plane_distance(LCComponentHandle component, float distance) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetPlaneDistance(distance);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set plane distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_plane_width(LCComponentHandle component, float* out_width) {
    if (!out_width) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_width is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_width = collision->GetPlaneWidth();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get plane width");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_plane_width(LCComponentHandle component, float width) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetPlaneWidth(width);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set plane width");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_plane_length(LCComponentHandle component, float* out_length) {
    if (!out_length) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_length is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_length = collision->GetPlaneLength();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get plane length");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_plane_length(LCComponentHandle component, float length) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetPlaneLength(length);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set plane length");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_offset(LCComponentHandle component, LCVec3* out_offset) {
    if (!out_offset) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_offset = FromEngineVec3(collision->GetOffset());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_offset(LCComponentHandle component, LCVec3 offset) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetOffset(ToEngineVec3(offset));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Mesh Shape Configuration
 * ============================================================================ */

LC_API LCResult lc_collision3d_get_mesh_path(LCComponentHandle component, char* out_path, int buffer_size) {
    if (!out_path || buffer_size <= 0) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_path is NULL or buffer_size invalid");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        std::string path = collision->GetMeshPath();
        CopyStringToBuffer(out_path, buffer_size, path.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get mesh path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_mesh_path(LCComponentHandle component, const char* path) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetMeshPath(path ? path : "");
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set mesh path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_mesh_solver(LCComponentHandle component, LCMeshSolverType* out_solver) {
    if (!out_solver) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_solver is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_solver = FromEngineMeshSolver(collision->GetMeshSolver());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get mesh solver");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_mesh_solver(LCComponentHandle component, LCMeshSolverType solver) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetMeshSolver(ToEngineMeshSolver(solver));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set mesh solver");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Physics Material Properties
 * ============================================================================ */

LC_API LCResult lc_collision3d_get_density(LCComponentHandle component, float* out_density) {
    if (!out_density) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_density is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_density = collision->GetDensity();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get density");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_density(LCComponentHandle component, float density) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetDensity(density);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set density");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_friction(LCComponentHandle component, float* out_friction) {
    if (!out_friction) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_friction is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_friction = collision->GetFriction();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get friction");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_friction(LCComponentHandle component, float friction) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetFriction(friction);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set friction");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_restitution(LCComponentHandle component, float* out_restitution) {
    if (!out_restitution) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_restitution is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_restitution = collision->GetRestitution();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get restitution");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_restitution(LCComponentHandle component, float restitution) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetRestitution(restitution);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set restitution");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_is_sensor(LCComponentHandle component, bool* out_is_sensor) {
    if (!out_is_sensor) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_is_sensor is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_is_sensor = collision->IsSensor();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get sensor state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_sensor(LCComponentHandle component, bool is_sensor) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetSensor(is_sensor);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set sensor state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_get_collision_layers(LCComponentHandle component, uint32_t* out_layers) {
    if (!out_layers) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_layers is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_layers = collision->GetCollisionLayers();
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get collision layers");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_collision_layers(LCComponentHandle component, uint32_t layers) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetCollisionLayers(layers);
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set collision layers");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Debug Visualization
 * ============================================================================ */

LC_API LCResult lc_collision3d_get_debug_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetCollision3DError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(collision->GetDebugColor());
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get debug color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_collision3d_set_debug_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCollision3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto collision = std::dynamic_pointer_cast<lupine::components::CollisionMesh3DComponent>(comp);
        if (!collision) {
            SetCollision3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CollisionMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        collision->SetDebugColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetCollision3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set debug color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
