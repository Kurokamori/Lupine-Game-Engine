
#include "components/lc_light.h"
#include "../core/lc_internal.h"

#include <lupine/components/DirectionalLight3D.hpp>
#include <lupine/components/OmniLight3D.hpp>
#include <lupine/components/SpotLight3D.hpp>
#include <lupine/core/Node.hpp>

#include <unordered_map>
#include <mutex>

namespace {

// Component handle registry
static std::unordered_map<LCComponentHandle, std::shared_ptr<lupine::core::Component>> g_ComponentHandles;
static std::mutex g_ComponentHandlesMutex;
static LCComponentHandle g_NextComponentHandle = reinterpret_cast<LCComponentHandle>(1);

void SetLightError(LCResult code, const char* message) {
    ::SetError(code, message);

// Convert C API color to engine color
}
lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);

// Convert engine color to C API color
}
LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};

}
} // anonymous namespace

// Component handle management implementations
std::shared_ptr<lupine::core::Component> GetComponent(LCComponentHandle handle) {
    std::lock_guard<std::mutex> lock(g_ComponentHandlesMutex);
    auto it = g_ComponentHandles.find(handle);
    if (it != g_ComponentHandles.end()) {
        return it->second;
    }
    return nullptr;

}
LCComponentHandle CreateComponentHandle(std::shared_ptr<lupine::core::Component> component) {
    std::lock_guard<std::mutex> lock(g_ComponentHandlesMutex);
    LCComponentHandle handle = g_NextComponentHandle;
    g_NextComponentHandle = reinterpret_cast<LCComponentHandle>(
        reinterpret_cast<uintptr_t>(g_NextComponentHandle) + 1
    );
    g_ComponentHandles[handle] = component;
    return handle;

}
bool IsValidComponentHandle(LCComponentHandle handle) {
    std::lock_guard<std::mutex> lock(g_ComponentHandlesMutex);
    return g_ComponentHandles.find(handle) != g_ComponentHandles.end();

}
void DestroyComponentHandle(LCComponentHandle handle) {
    std::lock_guard<std::mutex> lock(g_ComponentHandlesMutex);
    g_ComponentHandles.erase(handle);


/* ============================================================================
 * DirectionalLight3D Functions
 * ============================================================================ */

}
LC_API LCResult lc_directional_light3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string lightName = name ? name : "";
        auto light = std::make_shared<lupine::components::DirectionalLight3D>(lightName);
        light->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(light);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to create DirectionalLight3D");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_get_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(light->GetColor());
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_set_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_get_intensity(LCComponentHandle component, float* out_intensity) {
    if (!out_intensity) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_intensity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_intensity = light->GetIntensity();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get intensity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_set_intensity(LCComponentHandle component, float intensity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetIntensity(intensity);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set intensity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_is_negative(LCComponentHandle component, bool* out_negative) {
    if (!out_negative) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_negative is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_negative = light->IsNegative();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get negative mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_set_negative(LCComponentHandle component, bool negative) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetNegative(negative);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set negative mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_casts_shadows(LCComponentHandle component, bool* out_casts_shadows) {
    if (!out_casts_shadows) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_casts_shadows is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_casts_shadows = light->CastsShadows();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get shadow casting state");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_set_casts_shadows(LCComponentHandle component, bool casts_shadows) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetCastsShadows(casts_shadows);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set shadow casting state");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_get_shadow_opacity(LCComponentHandle component, float* out_opacity) {
    if (!out_opacity) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_opacity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_opacity = light->GetShadowOpacity();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get shadow opacity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_set_shadow_opacity(LCComponentHandle component, float opacity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetShadowOpacity(opacity);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set shadow opacity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_get_shadow_bias(LCComponentHandle component, float* out_bias) {
    if (!out_bias) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_bias is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_bias = light->GetShadowBias();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get shadow bias");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_directional_light3d_set_shadow_bias(LCComponentHandle component, float bias) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::DirectionalLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a DirectionalLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetShadowBias(bias);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set shadow bias");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ============================================================================
 * OmniLight3D (Point Light) Functions
 * ============================================================================ */

}
LC_API LCResult lc_omni_light3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string lightName = name ? name : "";
        auto light = std::make_shared<lupine::components::OmniLight3D>(lightName);
        light->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(light);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to create OmniLight3D");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_get_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(light->GetColor());
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_set_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_get_intensity(LCComponentHandle component, float* out_intensity) {
    if (!out_intensity) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_intensity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_intensity = light->GetIntensity();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get intensity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_set_intensity(LCComponentHandle component, float intensity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetIntensity(intensity);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set intensity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_get_range(LCComponentHandle component, float* out_range) {
    if (!out_range) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_range is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_range = light->GetRange();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get range");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_set_range(LCComponentHandle component, float range) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetRange(range);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set range");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_get_attenuation(LCComponentHandle component, float* out_attenuation) {
    if (!out_attenuation) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_attenuation is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_attenuation = light->GetAttenuation();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get attenuation");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_set_attenuation(LCComponentHandle component, float attenuation) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetAttenuation(attenuation);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set attenuation");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_is_negative(LCComponentHandle component, bool* out_negative) {
    if (!out_negative) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_negative is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_negative = light->IsNegative();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get negative mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_set_negative(LCComponentHandle component, bool negative) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetNegative(negative);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set negative mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_casts_shadows(LCComponentHandle component, bool* out_casts_shadows) {
    if (!out_casts_shadows) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_casts_shadows is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_casts_shadows = light->CastsShadows();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get shadow casting state");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_omni_light3d_set_casts_shadows(LCComponentHandle component, bool casts_shadows) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::OmniLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not an OmniLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetCastsShadows(casts_shadows);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set shadow casting state");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ============================================================================
 * SpotLight3D Functions
 * ============================================================================ */

}
LC_API LCResult lc_spot_light3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string lightName = name ? name : "";
        auto light = std::make_shared<lupine::components::SpotLight3D>(lightName);
        light->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(light);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to create SpotLight3D");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_get_color(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_color is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_color = FromEngineColor(light->GetColor());
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_set_color(LCComponentHandle component, LCColor color) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetColor(ToEngineColor(color));
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set color");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_get_intensity(LCComponentHandle component, float* out_intensity) {
    if (!out_intensity) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_intensity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_intensity = light->GetIntensity();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get intensity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_set_intensity(LCComponentHandle component, float intensity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetIntensity(intensity);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set intensity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_get_range(LCComponentHandle component, float* out_range) {
    if (!out_range) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_range is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_range = light->GetRange();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get range");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_set_range(LCComponentHandle component, float range) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetRange(range);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set range");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_get_attenuation(LCComponentHandle component, float* out_attenuation) {
    if (!out_attenuation) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_attenuation is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_attenuation = light->GetAttenuation();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get attenuation");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_set_attenuation(LCComponentHandle component, float attenuation) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetAttenuation(attenuation);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set attenuation");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_get_inner_cone_angle(LCComponentHandle component, float* out_angle) {
    if (!out_angle) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_angle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_angle = light->GetInnerConeAngle();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get inner cone angle");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_set_inner_cone_angle(LCComponentHandle component, float angle) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetInnerConeAngle(angle);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set inner cone angle");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_get_outer_cone_angle(LCComponentHandle component, float* out_angle) {
    if (!out_angle) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_angle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_angle = light->GetOuterConeAngle();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get outer cone angle");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_set_outer_cone_angle(LCComponentHandle component, float angle) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetOuterConeAngle(angle);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set outer cone angle");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_is_negative(LCComponentHandle component, bool* out_negative) {
    if (!out_negative) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_negative is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_negative = light->IsNegative();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get negative mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_set_negative(LCComponentHandle component, bool negative) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetNegative(negative);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set negative mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_casts_shadows(LCComponentHandle component, bool* out_casts_shadows) {
    if (!out_casts_shadows) {
        SetLightError(LC_ERROR_NULL_POINTER, "out_casts_shadows is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_casts_shadows = light->CastsShadows();
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to get shadow casting state");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_spot_light3d_set_casts_shadows(LCComponentHandle component, bool casts_shadows) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto light = std::dynamic_pointer_cast<lupine::components::SpotLight3D>(comp);
        if (!light) {
            SetLightError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a SpotLight3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        light->SetCastsShadows(casts_shadows);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to set shadow casting state");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ============================================================================
 * Component Management
 * ============================================================================ */

}
LC_API LCResult lc_component_destroy(LCComponentHandle component) {
    try {
        if (!IsValidComponentHandle(component)) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        DestroyComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to destroy component");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_node_add_component(LCNodeHandle node, LCComponentHandle component) {
    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto compPtr = GetComponent(component);
        if (!compPtr) {
            SetLightError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        nodePtr->AddComponent(compPtr);
        return LC_SUCCESS;
    } catch (...) {
        SetLightError(LC_ERROR_INTERNAL_ERROR, "Failed to add component to node");
        return LC_ERROR_INTERNAL_ERROR;
    }


}