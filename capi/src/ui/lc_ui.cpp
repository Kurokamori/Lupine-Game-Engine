
#include "ui/lc_ui.h"
#include "../core/lc_internal.h"

#include <lupine/components/Label.hpp>

#include <cstring>

namespace {

void SetUIError(LCResult code, const char* message) {
    ::SetError(code, message);

// Convert C API color to engine color
}
lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);

// Convert engine color to C API color
}
LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};

// Convert C API Vec2 to engine Vec2
}
lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);

// Convert engine Vec2 to C API Vec2
}
LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};

}
} // anonymous namespace

/* ============================================================================
 * Label Functions
 * ============================================================================ */

LC_API LCResult lc_label_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetUIError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string labelName = name ? name : "";
        auto label = std::make_shared<lupine::components::Label>(labelName);
        label->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(label);
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to create Label");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_load_font(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetUIError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        bool success = label->LoadFont(std::string(filepath));
        if (!success) {
            SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to load font");
            return LC_ERROR_INTERNAL_ERROR;
        }

        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to load font");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_get_text(LCComponentHandle component, char* out_text, size_t text_size) {
    if (!out_text) {
        SetUIError(LC_ERROR_NULL_POINTER, "out_text is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        const std::string& text = label->GetText();
        CopyStringToBuffer(out_text, text_size, text.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to get text");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_set_text(LCComponentHandle component, const char* text) {
    if (!text) {
        SetUIError(LC_ERROR_NULL_POINTER, "text is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        label->SetText(std::string(text));
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to set text");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_get_font_path(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) {
        SetUIError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        const std::string& path = label->GetFontPath();
        CopyStringToBuffer(out_path, path_size, path.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to get font path");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_set_font_path(LCComponentHandle component, const char* path) {
    if (!path) {
        SetUIError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        label->SetFontPath(std::string(path));
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to set font path");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_get_font_size(LCComponentHandle component, float* out_size) {
    if (!out_size) {
        SetUIError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_size = label->GetFontSize();
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to get font size");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_set_font_size(LCComponentHandle component, float size) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        label->SetFontSize(size);
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to set font size");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_get_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetUIError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(label->GetColor());
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to get color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_set_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        label->SetColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to set color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_get_centered(LCComponentHandle component, bool* out_centered) {
    if (!out_centered) {
        SetUIError(LC_ERROR_NULL_POINTER, "out_centered is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_centered = label->GetCentered();
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to get centered");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_set_centered(LCComponentHandle component, bool centered) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        label->SetCentered(centered);
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to set centered");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_get_offset(LCComponentHandle component, LCVec2* out_offset) {
    if (!out_offset) {
        SetUIError(LC_ERROR_NULL_POINTER, "out_offset is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_offset = FromEngineVec2(label->GetOffset());
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to get offset");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_label_set_offset(LCComponentHandle component, LCVec2 offset) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUIError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto label = std::dynamic_pointer_cast<lupine::components::Label>(comp);
        if (!label) {
            SetUIError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Label");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        label->SetOffset(ToEngineVec2(offset));
        return LC_SUCCESS;
    } catch (...) {
        SetUIError(LC_ERROR_INTERNAL_ERROR, "Failed to set offset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
