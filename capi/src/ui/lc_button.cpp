
#include "ui/lc_button.h"
#include "../core/lc_internal.h"

#include <lupine/components/Button.hpp>

#include <cstring>
#include <unordered_map>
#include <mutex>

namespace {

void SetButtonError(LCResult code, const char* message) {
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

// Convert C API button state to engine button state
}
lupine::components::ButtonState ToEngineButtonState(LCButtonState state) {
    return static_cast<lupine::components::ButtonState>(state);

// Convert engine button state to C API button state
}
LCButtonState FromEngineButtonState(lupine::components::ButtonState state) {
    return static_cast<LCButtonState>(state);

// Convert C API button style mode to engine button style mode
}
lupine::components::ButtonStyleMode ToEngineButtonStyleMode(LCButtonStyleMode mode) {
    return static_cast<lupine::components::ButtonStyleMode>(mode);

// Convert engine button style mode to C API button style mode
}
LCButtonStyleMode FromEngineButtonStyleMode(lupine::components::ButtonStyleMode mode) {
    return static_cast<LCButtonStyleMode>(mode);

// Convert C API button scale mode to engine button scale mode
}
lupine::components::ButtonScaleMode ToEngineButtonScaleMode(LCButtonScaleMode mode) {
    return static_cast<lupine::components::ButtonScaleMode>(mode);

// Convert engine button scale mode to C API button scale mode
}
LCButtonScaleMode FromEngineButtonScaleMode(lupine::components::ButtonScaleMode mode) {
    return static_cast<LCButtonScaleMode>(mode);

// Convert C API button state sound to engine button state sound
}
lupine::components::ButtonStateSound ToEngineButtonStateSound(const LCButtonStateSound& sound) {
    lupine::components::ButtonStateSound result;
    result.audioPath = sound.audioPath;
    result.busName = sound.busName;
    result.volume = sound.volume;
    return result;

// Convert engine button state sound to C API button state sound
}
LCButtonStateSound FromEngineButtonStateSound(const lupine::components::ButtonStateSound& sound) {
    LCButtonStateSound result{};
    CopyStringToBuffer(result.audioPath, sizeof(result.audioPath), sound.audioPath.c_str());
    CopyStringToBuffer(result.busName, sizeof(result.busName), sound.busName.c_str());
    result.volume = sound.volume;
    return result;

// Convert C API button state tween to engine button state tween
}
lupine::components::ButtonStateTween ToEngineButtonStateTween(const LCButtonStateTween& tween) {
    lupine::components::ButtonStateTween result;
    result.enabled = tween.enabled;
    result.scaleOffset = ToEngineVec2(tween.scaleOffset);
    result.rotationOffset = tween.rotationOffset;
    result.positionOffset = ToEngineVec2(tween.positionOffset);
    result.duration = tween.duration;
    return result;

// Convert engine button state tween to C API button state tween
}
LCButtonStateTween FromEngineButtonStateTween(const lupine::components::ButtonStateTween& tween) {
    LCButtonStateTween result{};
    result.enabled = tween.enabled;
    result.scaleOffset = FromEngineVec2(tween.scaleOffset);
    result.rotationOffset = tween.rotationOffset;
    result.positionOffset = FromEngineVec2(tween.positionOffset);
    result.duration = tween.duration;
    return result;

// Callback storage
struct CallbackData {
    LCButtonStateCallback callback;
    void* user_data;
};

std::unordered_map<LCComponentHandle, std::unordered_map<LCButtonState, CallbackData>> g_ButtonCallbacks;
std::mutex g_ButtonCallbacksMutex;

}
} // anonymous namespace


/* ============================================================================
 * Button Functions
 * ============================================================================ */

LC_API LCResult lc_button_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string buttonName = name ? name : "";
        auto button = std::make_shared<lupine::components::Button>(buttonName);
        button->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(button);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to create Button");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Size Properties ===== */

}
LC_API LCResult lc_button_get_width(LCComponentHandle component, float* out_width) {
    if (!out_width) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_width is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_width = button->GetWidth();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get width");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_width(LCComponentHandle component, float width) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetWidth(width);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set width");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_height(LCComponentHandle component, float* out_height) {
    if (!out_height) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_height is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_height = button->GetHeight();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get height");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_height(LCComponentHandle component, float height) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetHeight(height);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set height");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Layer Properties ===== */

}
LC_API LCResult lc_button_get_layer(LCComponentHandle component, int* out_layer) {
    if (!out_layer) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_layer is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_layer = button->GetLayer();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get layer");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_layer(LCComponentHandle component, int layer) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetLayer(layer);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set layer");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_sorting_order(LCComponentHandle component, int* out_order) {
    if (!out_order) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_order is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_order = button->GetSortingOrder();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get sorting order");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_sorting_order(LCComponentHandle component, int order) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetSortingOrder(order);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set sorting order");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== UI Space ===== */

}
LC_API LCResult lc_button_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space) {
    if (!out_use_ui_space) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_use_ui_space is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_use_ui_space = button->GetUseUISpace();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get use UI space");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_use_ui_space(LCComponentHandle component, bool use_ui_space) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetUseUISpace(use_ui_space);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set use UI space");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Button State ===== */

}
LC_API LCResult lc_button_get_current_state(LCComponentHandle component, LCButtonState* out_state) {
    if (!out_state) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_state is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_state = FromEngineButtonState(button->GetCurrentState());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get current state");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_enabled(LCComponentHandle component, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_is_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_enabled = button->IsButtonEnabled();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to check if enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Style Mode ===== */

}
LC_API LCResult lc_button_get_style_mode(LCComponentHandle component, LCButtonStyleMode* out_mode) {
    if (!out_mode) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_mode is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_mode = FromEngineButtonStyleMode(button->GetStyleMode());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get style mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_style_mode(LCComponentHandle component, LCButtonStyleMode mode) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetStyleMode(ToEngineButtonStyleMode(mode));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set style mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Scale Mode ===== */

}
LC_API LCResult lc_button_get_scale_mode(LCComponentHandle component, LCButtonScaleMode* out_mode) {
    if (!out_mode) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_mode is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_mode = FromEngineButtonScaleMode(button->GetScaleMode());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get scale mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_scale_mode(LCComponentHandle component, LCButtonScaleMode mode) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetScaleMode(ToEngineButtonScaleMode(mode));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set scale mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_text_padding(LCComponentHandle component, LCVec2* out_padding) {
    if (!out_padding) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_padding is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_padding = FromEngineVec2(button->GetTextPadding());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get text padding");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_text_padding(LCComponentHandle component, LCVec2 padding) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetTextPadding(ToEngineVec2(padding));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set text padding");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Base Style (Automatic Mode) ===== */

}
LC_API LCResult lc_button_get_background_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(button->GetBackgroundColor());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get background color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_background_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetBackgroundColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set background color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_opacity(LCComponentHandle component, float* out_opacity) {
    if (!out_opacity) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_opacity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_opacity = button->GetOpacity();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get opacity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_opacity(LCComponentHandle component, float opacity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetOpacity(opacity);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set opacity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_border_width_linked(LCComponentHandle component, bool* out_linked) {
    if (!out_linked) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_linked is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_linked = button->GetBorderWidthLinked();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get border width linked");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_border_width_linked(LCComponentHandle component, bool linked) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetBorderWidthLinked(linked);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set border width linked");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_border_width_left(LCComponentHandle component, float* out_width) {
    if (!out_width) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_width is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_width = button->GetBorderWidthLeft();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get border width left");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_border_width_left(LCComponentHandle component, float width) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetBorderWidthLeft(width);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set border width left");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_border_width_right(LCComponentHandle component, float* out_width) {
    if (!out_width) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_width is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_width = button->GetBorderWidthRight();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get border width right");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_border_width_right(LCComponentHandle component, float width) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetBorderWidthRight(width);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set border width right");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_border_width_top(LCComponentHandle component, float* out_width) {
    if (!out_width) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_width is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_width = button->GetBorderWidthTop();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get border width top");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_border_width_top(LCComponentHandle component, float width) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetBorderWidthTop(width);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set border width top");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_border_width_bottom(LCComponentHandle component, float* out_width) {
    if (!out_width) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_width is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_width = button->GetBorderWidthBottom();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get border width bottom");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_border_width_bottom(LCComponentHandle component, float width) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetBorderWidthBottom(width);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set border width bottom");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_border_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_enabled = button->GetBorderEnabled();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get border enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_border_enabled(LCComponentHandle component, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetBorderEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set border enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_border_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(button->GetBorderColor());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get border color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_border_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetBorderColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set border color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_corner_radius_linked(LCComponentHandle component, bool* out_linked) {
    if (!out_linked) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_linked is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_linked = button->GetCornerRadiusLinked();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get corner radius linked");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_corner_radius_linked(LCComponentHandle component, bool linked) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetCornerRadiusLinked(linked);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set corner radius linked");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_corner_radius_top_left(LCComponentHandle component, float* out_radius) {
    if (!out_radius) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_radius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_radius = button->GetCornerRadiusTopLeft();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get corner radius top left");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_corner_radius_top_left(LCComponentHandle component, float radius) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetCornerRadiusTopLeft(radius);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set corner radius top left");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_corner_radius_top_right(LCComponentHandle component, float* out_radius) {
    if (!out_radius) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_radius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_radius = button->GetCornerRadiusTopRight();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get corner radius top right");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_corner_radius_top_right(LCComponentHandle component, float radius) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetCornerRadiusTopRight(radius);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set corner radius top right");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_corner_radius_bottom_left(LCComponentHandle component, float* out_radius) {
    if (!out_radius) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_radius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_radius = button->GetCornerRadiusBottomLeft();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get corner radius bottom left");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_corner_radius_bottom_left(LCComponentHandle component, float radius) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetCornerRadiusBottomLeft(radius);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set corner radius bottom left");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_corner_radius_bottom_right(LCComponentHandle component, float* out_radius) {
    if (!out_radius) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_radius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_radius = button->GetCornerRadiusBottomRight();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get corner radius bottom right");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_corner_radius_bottom_right(LCComponentHandle component, float radius) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetCornerRadiusBottomRight(radius);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set corner radius bottom right");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Text Properties ===== */

}
LC_API LCResult lc_button_get_text(LCComponentHandle component, char* out_text, size_t text_size) {
    if (!out_text) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_text is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        const std::string& text = button->GetText();
        CopyStringToBuffer(out_text, text_size, text.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get text");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_text(LCComponentHandle component, const char* text) {
    if (!text) {
        SetButtonError(LC_ERROR_NULL_POINTER, "text is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetText(std::string(text));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set text");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_font_path(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        const std::string& path = button->GetFontPath();
        CopyStringToBuffer(out_path, path_size, path.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get font path");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_font_path(LCComponentHandle component, const char* path) {
    if (!path) {
        SetButtonError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetFontPath(std::string(path));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set font path");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_font_size(LCComponentHandle component, float* out_size) {
    if (!out_size) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_size = button->GetFontSize();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get font size");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_font_size(LCComponentHandle component, float size) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetFontSize(size);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set font size");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_font_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(button->GetFontColor());
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get font color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_font_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetFontColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set font color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_get_word_wrap(LCComponentHandle component, bool* out_wrap) {
    if (!out_wrap) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_wrap is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_wrap = button->GetWordWrap();
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get word wrap");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_word_wrap(LCComponentHandle component, bool wrap) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetWordWrap(wrap);
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set word wrap");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Per-State Modulation (Automatic Mode) ===== */

}
LC_API LCResult lc_button_get_state_modulation(LCComponentHandle component, LCButtonState state, LCColor* out_color) {
    if (!out_color) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(button->GetStateModulation(ToEngineButtonState(state)));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get state modulation");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_state_modulation(LCComponentHandle component, LCButtonState state, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetStateModulation(ToEngineButtonState(state), ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set state modulation");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Per-State Sounds ===== */

}
LC_API LCResult lc_button_get_state_sound(LCComponentHandle component, LCButtonState state, LCButtonStateSound* out_sound) {
    if (!out_sound) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_sound is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_sound = FromEngineButtonStateSound(button->GetStateSound(ToEngineButtonState(state)));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get state sound");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_state_sound(LCComponentHandle component, LCButtonState state, const LCButtonStateSound* sound) {
    if (!sound) {
        SetButtonError(LC_ERROR_NULL_POINTER, "sound is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetStateSound(ToEngineButtonState(state), ToEngineButtonStateSound(*sound));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set state sound");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_state_sound_path(LCComponentHandle component, LCButtonState state, const char* path) {
    if (!path) {
        SetButtonError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetStateSoundPath(ToEngineButtonState(state), std::string(path));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set state sound path");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Per-State Tweens ===== */

}
LC_API LCResult lc_button_get_state_tween(LCComponentHandle component, LCButtonState state, LCButtonStateTween* out_tween) {
    if (!out_tween) {
        SetButtonError(LC_ERROR_NULL_POINTER, "out_tween is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_tween = FromEngineButtonStateTween(button->GetStateTween(ToEngineButtonState(state)));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to get state tween");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_button_set_state_tween(LCComponentHandle component, LCButtonState state, const LCButtonStateTween* tween) {
    if (!tween) {
        SetButtonError(LC_ERROR_NULL_POINTER, "tween is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetStateTween(ToEngineButtonState(state), ToEngineButtonStateTween(*tween));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set state tween");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ===== Per-State Callbacks ===== */

}
LC_API LCResult lc_button_set_state_callback(LCComponentHandle component, LCButtonState state, LCButtonStateCallback callback, void* user_data) {
    if (!callback) {
        SetButtonError(LC_ERROR_NULL_POINTER, "callback is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->SetStateCallback(ToEngineButtonState(state),
                                 [callback, user_data]() { callback(user_data); });
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to set state callback");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_button_clear_state_callback(LCComponentHandle component, LCButtonState state) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetButtonError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto button = std::dynamic_pointer_cast<lupine::components::Button>(comp);
        if (!button) {
            SetButtonError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Button");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        button->ClearStateCallback(ToEngineButtonState(state));
        return LC_SUCCESS;
    } catch (...) {
        SetButtonError(LC_ERROR_INTERNAL_ERROR, "Failed to clear state callback");
        return LC_ERROR_INTERNAL_ERROR;
    }
}