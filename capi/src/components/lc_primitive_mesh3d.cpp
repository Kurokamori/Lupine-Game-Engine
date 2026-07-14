
#include "components/lc_primitive_mesh3d.h"
#include "../core/lc_internal.h"

#include <lupine/components/PrimitiveMesh3D.hpp>

namespace {

void SetPrimitiveMeshError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API PrimitiveShape to engine PrimitiveShape
lupine::components::PrimitiveShape ToEnginePrimitiveShape(LCPrimitiveShape shape) {
    switch (shape) {
        case LC_PRIMITIVE_CUBE: return lupine::components::PrimitiveShape::Cube;
        case LC_PRIMITIVE_SPHERE: return lupine::components::PrimitiveShape::Sphere;
        case LC_PRIMITIVE_CYLINDER: return lupine::components::PrimitiveShape::Cylinder;
        case LC_PRIMITIVE_CONE: return lupine::components::PrimitiveShape::Cone;
        case LC_PRIMITIVE_PYRAMID: return lupine::components::PrimitiveShape::Pyramid;
        case LC_PRIMITIVE_TORUS: return lupine::components::PrimitiveShape::Torus;
        case LC_PRIMITIVE_CAPSULE: return lupine::components::PrimitiveShape::Capsule;
        default: return lupine::components::PrimitiveShape::Cube;
    }
}

// Convert engine PrimitiveShape to C API PrimitiveShape
LCPrimitiveShape FromEnginePrimitiveShape(lupine::components::PrimitiveShape shape) {
    switch (shape) {
        case lupine::components::PrimitiveShape::Cube: return LC_PRIMITIVE_CUBE;
        case lupine::components::PrimitiveShape::Sphere: return LC_PRIMITIVE_SPHERE;
        case lupine::components::PrimitiveShape::Cylinder: return LC_PRIMITIVE_CYLINDER;
        case lupine::components::PrimitiveShape::Cone: return LC_PRIMITIVE_CONE;
        case lupine::components::PrimitiveShape::Pyramid: return LC_PRIMITIVE_PYRAMID;
        case lupine::components::PrimitiveShape::Torus: return LC_PRIMITIVE_TORUS;
        case lupine::components::PrimitiveShape::Capsule: return LC_PRIMITIVE_CAPSULE;
        default: return LC_PRIMITIVE_CUBE;
    }
}

// Convert C API ShaderType to engine ShaderType
lupine::ShaderType ToEngineShaderType(LCShaderType type) {
    switch (type) {
        case LC_SHADER_PBR: return lupine::ShaderType::PBR;
        case LC_SHADER_TOON: return lupine::ShaderType::Toon;
        case LC_SHADER_STYLIZED: return lupine::ShaderType::Stylized;
        case LC_SHADER_UNLIT: return lupine::ShaderType::Unlit;
        case LC_SHADER_STANDARD3D: return lupine::ShaderType::Standard3D;
        case LC_SHADER_TRANSPARENT: return lupine::ShaderType::Transparent;
        case LC_SHADER_GLOW: return lupine::ShaderType::Glow;
        case LC_SHADER_CUSTOM: return lupine::ShaderType::Custom;
        default: return lupine::ShaderType::PBR;
    }
}

// Convert engine ShaderType to C API ShaderType
LCShaderType FromEngineShaderType(lupine::ShaderType type) {
    switch (type) {
        case lupine::ShaderType::PBR: return LC_SHADER_PBR;
        case lupine::ShaderType::Toon: return LC_SHADER_TOON;
        case lupine::ShaderType::Stylized: return LC_SHADER_STYLIZED;
        case lupine::ShaderType::Unlit: return LC_SHADER_UNLIT;
        case lupine::ShaderType::Standard3D: return LC_SHADER_STANDARD3D;
        case lupine::ShaderType::Transparent: return LC_SHADER_TRANSPARENT;
        case lupine::ShaderType::Glow: return LC_SHADER_GLOW;
        case lupine::ShaderType::Custom: return LC_SHADER_CUSTOM;
        default: return LC_SHADER_PBR;
    }
}

// Convert C API Color to engine Color
lupine::math::Color ToEngineColor(LCColor c) {
    return lupine::math::Color(c.r, c.g, c.b, c.a);
}

// Convert engine Color to C API Color
LCColor FromEngineColor(const lupine::math::Color& c) {
    return LCColor{c.r, c.g, c.b, c.a};
}

} // anonymous namespace


/* ============================================================================
 * PrimitiveMesh3D Creation/Destruction
 * ============================================================================ */

LC_API LCResult lc_primitive_mesh3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string meshName = name ? name : "";
        auto mesh = std::make_shared<lupine::components::PrimitiveMesh3D>(meshName);
        mesh->DefineProperties();
        *out_component = CreateComponentHandle(mesh);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to create PrimitiveMesh3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Shape Configuration
 * ============================================================================ */

LC_API LCResult lc_primitive_mesh3d_get_shape(LCComponentHandle component, LCPrimitiveShape* out_shape) {
    if (!out_shape) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_shape is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_shape = FromEnginePrimitiveShape(mesh->GetShape());
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get shape");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_shape(LCComponentHandle component, LCPrimitiveShape shape) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetShape(ToEnginePrimitiveShape(shape));
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set shape");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(mesh->GetColor());
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_size(LCComponentHandle component, float* out_size) {
    if (!out_size) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_size = mesh->GetSize();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_size(LCComponentHandle component, float size) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetSize(size);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_height(LCComponentHandle component, float* out_height) {
    if (!out_height) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_height is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_height = mesh->GetHeight();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get height");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_height(LCComponentHandle component, float height) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetHeight(height);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set height");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_detail(LCComponentHandle component, int* out_detail) {
    if (!out_detail) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_detail is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_detail = mesh->GetDetail();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get detail");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_detail(LCComponentHandle component, int detail) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetDetail(detail);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set detail");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_minor_radius(LCComponentHandle component, float* out_radius) {
    if (!out_radius) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_radius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_radius = mesh->GetMinorRadius();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get minor radius");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_minor_radius(LCComponentHandle component, float radius) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetMinorRadius(radius);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set minor radius");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Shadow Properties
 * ============================================================================ */

LC_API LCResult lc_primitive_mesh3d_get_cast_shadow(LCComponentHandle component, bool* out_cast_shadow) {
    if (!out_cast_shadow) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_cast_shadow is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_cast_shadow = mesh->GetCastShadow();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get cast shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_cast_shadow(LCComponentHandle component, bool cast_shadow) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetCastShadow(cast_shadow);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set cast shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_receive_shadow(LCComponentHandle component, bool* out_receive_shadow) {
    if (!out_receive_shadow) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_receive_shadow is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_receive_shadow = mesh->GetReceiveShadow();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get receive shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_receive_shadow(LCComponentHandle component, bool receive_shadow) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetReceiveShadow(receive_shadow);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set receive shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_double_sided(LCComponentHandle component, bool* out_double_sided) {
    if (!out_double_sided) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_double_sided is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_double_sided = mesh->GetDoubleSided();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get double sided");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_double_sided(LCComponentHandle component, bool double_sided) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetDoubleSided(double_sided);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set double sided");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Material Override
 * ============================================================================ */

LC_API LCResult lc_primitive_mesh3d_has_material_override(LCComponentHandle component, bool* out_has_override) {
    if (!out_has_override) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_has_override is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_has_override = mesh->HasMaterialOverride();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to check material override");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_material_override_enabled(LCComponentHandle component, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetMaterialOverrideEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set material override enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_albedo_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(mesh->GetAlbedoColor());
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get albedo color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_albedo_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetAlbedoColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set albedo color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_metallic(LCComponentHandle component, float* out_metallic) {
    if (!out_metallic) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_metallic is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_metallic = mesh->GetMetallic();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get metallic");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_metallic(LCComponentHandle component, float metallic) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetMetallic(metallic);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set metallic");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_roughness(LCComponentHandle component, float* out_roughness) {
    if (!out_roughness) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_roughness is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_roughness = mesh->GetRoughness();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get roughness");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_roughness(LCComponentHandle component, float roughness) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetRoughness(roughness);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set roughness");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_normal_scale(LCComponentHandle component, float* out_scale) {
    if (!out_scale) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_scale is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_scale = mesh->GetNormalScale();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get normal scale");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_normal_scale(LCComponentHandle component, float scale) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetNormalScale(scale);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set normal scale");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_emissive_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(mesh->GetEmissiveColor());
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get emissive color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_emissive_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetEmissiveColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set emissive color");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_emissive_strength(LCComponentHandle component, float* out_strength) {
    if (!out_strength) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_strength is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_strength = mesh->GetEmissiveStrength();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get emissive strength");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_emissive_strength(LCComponentHandle component, float strength) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetEmissiveStrength(strength);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set emissive strength");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Shader Selection
 * ============================================================================ */

LC_API LCResult lc_primitive_mesh3d_get_shader_type(LCComponentHandle component, LCShaderType* out_type) {
    if (!out_type) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_type is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_type = FromEngineShaderType(mesh->GetShaderType());
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get shader type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_shader_type(LCComponentHandle component, LCShaderType type) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetShaderType(ToEngineShaderType(type));
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set shader type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Toon Shader Parameters
 * ============================================================================ */

LC_API LCResult lc_primitive_mesh3d_get_shadow_bands(LCComponentHandle component, float* out_bands) {
    if (!out_bands) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_bands is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_bands = mesh->GetShadowBands();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get shadow bands");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_shadow_bands(LCComponentHandle component, float bands) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetShadowBands(bands);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set shadow bands");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_shadow_threshold(LCComponentHandle component, float* out_threshold) {
    if (!out_threshold) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_threshold is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_threshold = mesh->GetShadowThreshold();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get shadow threshold");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_shadow_threshold(LCComponentHandle component, float threshold) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetShadowThreshold(threshold);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set shadow threshold");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_shadow_softness(LCComponentHandle component, float* out_softness) {
    if (!out_softness) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_softness is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_softness = mesh->GetShadowSoftness();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get shadow softness");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_shadow_softness(LCComponentHandle component, float softness) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetShadowSoftness(softness);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set shadow softness");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_specular_bands(LCComponentHandle component, float* out_bands) {
    if (!out_bands) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_bands is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_bands = mesh->GetSpecularBands();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get specular bands");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_specular_bands(LCComponentHandle component, float bands) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetSpecularBands(bands);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set specular bands");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_specular_power(LCComponentHandle component, float* out_power) {
    if (!out_power) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_power is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_power = mesh->GetSpecularPower();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get specular power");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_specular_power(LCComponentHandle component, float power) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetSpecularPower(power);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set specular power");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_rim_intensity(LCComponentHandle component, float* out_intensity) {
    if (!out_intensity) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_intensity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_intensity = mesh->GetRimIntensity();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get rim intensity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_rim_intensity(LCComponentHandle component, float intensity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetRimIntensity(intensity);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set rim intensity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_get_rim_power(LCComponentHandle component, float* out_power) {
    if (!out_power) {
        SetPrimitiveMeshError(LC_ERROR_NULL_POINTER, "out_power is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_power = mesh->GetRimPower();
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to get rim power");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_primitive_mesh3d_set_rim_power(LCComponentHandle component, float power) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPrimitiveMeshError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::PrimitiveMesh3D>(comp);
        if (!mesh) {
            SetPrimitiveMeshError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a PrimitiveMesh3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        mesh->SetRimPower(power);
        return LC_SUCCESS;
    } catch (...) {
        SetPrimitiveMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to set rim power");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
