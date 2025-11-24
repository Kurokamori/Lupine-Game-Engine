
#include "components/lc_rendering.h"
#include "../core/lc_internal.h"

#include <lupine\components\Sprite2D.hpp>
#include <lupine\components\Sprite3D.hpp>
#include <lupine\components\StaticMesh3D.hpp>

#include <cstring>

namespace {

void SetRenderingError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API color to engine color
lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

// Convert engine color to C API color
LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

// Convert C API Vec2 to engine Vec2
lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);
}

// Convert engine Vec2 to C API Vec2
LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

// Convert C API Vec4 to engine Vec4
lupine::math::Vec4 ToEngineVec4(LCVec4 vec) {
    return lupine::math::Vec4(vec.x, vec.y, vec.z, vec.w);
}

// Convert engine Vec4 to C API Vec4
LCVec4 FromEngineVec4(const lupine::math::Vec4& vec) {
    return LCVec4{vec.x, vec.y, vec.z, vec.w};
}

// Convert billboard mode
lupine::components::BillboardMode ToBillboardMode(LCBillboardMode mode) {
    switch (mode) {
        case LC_BILLBOARD_DISABLED: return lupine::components::BillboardMode::Disabled;
        case LC_BILLBOARD_ENABLED: return lupine::components::BillboardMode::Enabled;
        case LC_BILLBOARD_Y_AXIS_ONLY: return lupine::components::BillboardMode::YAxisOnly;
        default: return lupine::components::BillboardMode::Disabled;
    }
}

LCBillboardMode FromBillboardMode(lupine::components::BillboardMode mode) {
    switch (mode) {
        case lupine::components::BillboardMode::Disabled: return LC_BILLBOARD_DISABLED;
        case lupine::components::BillboardMode::Enabled: return LC_BILLBOARD_ENABLED;
        case lupine::components::BillboardMode::YAxisOnly: return LC_BILLBOARD_Y_AXIS_ONLY;
        default: return LC_BILLBOARD_DISABLED;
    }
}

} // anonymous namespace


/* ============================================================================
 * Sprite2D Functions
 * ============================================================================ */

LC_API LCResult lc_sprite2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string spriteName = name ? name : "";
        auto sprite = std::make_shared<lupine::components::Sprite2D>(spriteName);
        *out_component = CreateComponentHandle(sprite);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to create Sprite2D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_load_texture(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        bool success = sprite->LoadTexture(std::string(filepath));
        if (!success) {
            SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to load texture");
            return LC_ERROR_INTERNAL_ERROR;
        }

        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to load texture");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_texture_path(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        const std::string& path = sprite->GetTexturePath();
        strncpy(out_path, path.c_str(), path_size - 1);
        out_path[path_size - 1] = '\0';
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get texture path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_texture_path(LCComponentHandle component, const char* path) {
    if (!path) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetTexturePath(std::string(path));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set texture path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_modulate(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_color = FromEngineColor(sprite->GetModulate());
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get modulate");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_modulate(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetModulate(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set modulate");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_size(LCComponentHandle component, LCVec2* out_size) {
    if (!out_size) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_size = FromEngineVec2(sprite->GetSize());
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_size(LCComponentHandle component, LCVec2 size) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetSize(ToEngineVec2(size));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_uv_rect(LCComponentHandle component, LCVec4* out_uv_rect) {
    if (!out_uv_rect) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_uv_rect is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_uv_rect = FromEngineVec4(sprite->GetUVRect());
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get UV rect");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_uv_rect(LCComponentHandle component, LCVec4 uv_rect) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetUVRect(ToEngineVec4(uv_rect));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set UV rect");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_alpha_cutoff(LCComponentHandle component, float* out_cutoff) {
    if (!out_cutoff) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_cutoff is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_cutoff = sprite->GetAlphaCutoff();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get alpha cutoff");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_alpha_cutoff(LCComponentHandle component, float cutoff) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetAlphaCutoff(cutoff);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set alpha cutoff");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_flip_h(LCComponentHandle component, bool* out_flip) {
    if (!out_flip) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_flip is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_flip = sprite->GetFlipH();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get flip H");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_flip_h(LCComponentHandle component, bool flip) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetFlipH(flip);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set flip H");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_flip_v(LCComponentHandle component, bool* out_flip) {
    if (!out_flip) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_flip is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_flip = sprite->GetFlipV();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get flip V");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_flip_v(LCComponentHandle component, bool flip) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetFlipV(flip);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set flip V");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_centered(LCComponentHandle component, bool* out_centered) {
    if (!out_centered) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_centered is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_centered = sprite->GetCentered();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get centered");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_centered(LCComponentHandle component, bool centered) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetCentered(centered);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set centered");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_get_offset(LCComponentHandle component, LCVec2* out_offset) {
    if (!out_offset) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_offset = FromEngineVec2(sprite->GetOffset());
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite2d_set_offset(LCComponentHandle component, LCVec2 offset) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite2D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite2D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetOffset(ToEngineVec2(offset));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Sprite3D Functions
 * ============================================================================ */

LC_API LCResult lc_sprite3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string spriteName = name ? name : "";
        auto sprite = std::make_shared<lupine::components::Sprite3D>(spriteName);
        *out_component = CreateComponentHandle(sprite);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to create Sprite3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_load_texture(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        bool success = sprite->LoadTexture(std::string(filepath));
        if (!success) {
            SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to load texture");
            return LC_ERROR_INTERNAL_ERROR;
        }

        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to load texture");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_texture_path(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        const std::string& path = sprite->GetTexturePath();
        strncpy(out_path, path.c_str(), path_size - 1);
        out_path[path_size - 1] = '\0';
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get texture path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_texture_path(LCComponentHandle component, const char* path) {
    if (!path) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetTexturePath(std::string(path));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set texture path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_modulate(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_color = FromEngineColor(sprite->GetModulate());
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get modulate");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_modulate(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetModulate(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set modulate");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_size(LCComponentHandle component, LCVec2* out_size) {
    if (!out_size) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_size = FromEngineVec2(sprite->GetSize());
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_size(LCComponentHandle component, LCVec2 size) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetSize(ToEngineVec2(size));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_uv_rect(LCComponentHandle component, LCVec4* out_uv_rect) {
    if (!out_uv_rect) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_uv_rect is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_uv_rect = FromEngineVec4(sprite->GetUVRect());
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get UV rect");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_uv_rect(LCComponentHandle component, LCVec4 uv_rect) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetUVRect(ToEngineVec4(uv_rect));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set UV rect");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_alpha_cutoff(LCComponentHandle component, float* out_cutoff) {
    if (!out_cutoff) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_cutoff is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_cutoff = sprite->GetAlphaCutoff();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get alpha cutoff");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_alpha_cutoff(LCComponentHandle component, float cutoff) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetAlphaCutoff(cutoff);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set alpha cutoff");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_flip_h(LCComponentHandle component, bool* out_flip) {
    if (!out_flip) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_flip is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_flip = sprite->GetFlipH();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get flip H");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_flip_h(LCComponentHandle component, bool flip) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetFlipH(flip);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set flip H");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_flip_v(LCComponentHandle component, bool* out_flip) {
    if (!out_flip) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_flip is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_flip = sprite->GetFlipV();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get flip V");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_flip_v(LCComponentHandle component, bool flip) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetFlipV(flip);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set flip V");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_billboard_mode(LCComponentHandle component, LCBillboardMode* out_mode) {
    if (!out_mode) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_mode is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_mode = FromBillboardMode(sprite->GetBillboardMode());
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get billboard mode");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_billboard_mode(LCComponentHandle component, LCBillboardMode mode) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetBillboardMode(ToBillboardMode(mode));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set billboard mode");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_double_sided(LCComponentHandle component, bool* out_double_sided) {
    if (!out_double_sided) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_double_sided is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_double_sided = sprite->GetDoubleSided();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get double-sided");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_double_sided(LCComponentHandle component, bool double_sided) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetDoubleSided(double_sided);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set double-sided");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_cast_shadow(LCComponentHandle component, bool* out_cast_shadow) {
    if (!out_cast_shadow) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_cast_shadow is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_cast_shadow = sprite->GetCastShadow();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get cast shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_cast_shadow(LCComponentHandle component, bool cast_shadow) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetCastShadow(cast_shadow);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set cast shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_get_receive_shadow(LCComponentHandle component, bool* out_receive_shadow) {
    if (!out_receive_shadow) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_receive_shadow is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_receive_shadow = sprite->GetReceiveShadow();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get receive shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_sprite3d_set_receive_shadow(LCComponentHandle component, bool receive_shadow) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto sprite = std::dynamic_pointer_cast<lupine::components::Sprite3D>(comp);
        if (!sprite) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a Sprite3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        sprite->SetReceiveShadow(receive_shadow);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set receive shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * StaticMesh3D Functions
 * ============================================================================ */

LC_API LCResult lc_static_mesh3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string meshName = name ? name : "";
        auto mesh = std::make_shared<lupine::components::StaticMesh3D>(meshName);
        *out_component = CreateComponentHandle(mesh);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to create StaticMesh3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_load_model(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        bool success = mesh->LoadModel(std::string(filepath));
        if (!success) {
            SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to load model");
            return LC_ERROR_INTERNAL_ERROR;
        }

        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to load model");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_get_model_path(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        std::string path = mesh->GetModelPath();
        strncpy(out_path, path.c_str(), path_size - 1);
        out_path[path_size - 1] = '\0';
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get model path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_set_model_path(LCComponentHandle component, const char* path) {
    if (!path) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        mesh->SetModelPath(std::string(path));
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set model path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_get_cast_shadow(LCComponentHandle component, bool* out_cast_shadow) {
    if (!out_cast_shadow) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_cast_shadow is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_cast_shadow = mesh->GetCastShadow();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get cast shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_set_cast_shadow(LCComponentHandle component, bool cast_shadow) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        mesh->SetCastShadow(cast_shadow);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set cast shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_get_receive_shadow(LCComponentHandle component, bool* out_receive_shadow) {
    if (!out_receive_shadow) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_receive_shadow is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_receive_shadow = mesh->GetReceiveShadow();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get receive shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_set_receive_shadow(LCComponentHandle component, bool receive_shadow) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        mesh->SetReceiveShadow(receive_shadow);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set receive shadow");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_get_double_sided(LCComponentHandle component, bool* out_double_sided) {
    if (!out_double_sided) {
        SetRenderingError(LC_ERROR_NULL_POINTER, "out_double_sided is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        *out_double_sided = mesh->GetDoubleSided();
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to get double-sided");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_mesh3d_set_double_sided(LCComponentHandle component, bool double_sided) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetRenderingError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto mesh = std::dynamic_pointer_cast<lupine::components::StaticMesh3D>(comp);
        if (!mesh) {
            SetRenderingError(LC_ERROR_TYPE_MISMATCH, "Component is not a StaticMesh3D");
            return LC_ERROR_TYPE_MISMATCH;
        }

        mesh->SetDoubleSided(double_sided);
        return LC_SUCCESS;
    } catch (...) {
        SetRenderingError(LC_ERROR_INTERNAL_ERROR, "Failed to set double-sided");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

