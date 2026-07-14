/**
 * @file lc_reflection.cpp
 * @brief Implementation of the generic reflection / object-model bridge.
 *
 * Reuses the engine's own dynamic logic (Node/Component property registries,
 * ISerializable serialization, Component::CallMethod and TypeRegistry) so the C
 * surface stays in lockstep with the script-facing NodeRef/ComponentRef model.
 */

#include "core/lc_reflection.h"
#include "core/lc_core.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/core/Component.hpp>
#include <lupine/core/ComponentProperty.hpp>
#include <lupine/core/PropertyDescriptor.hpp>
#include <lupine/core/Serialization.hpp>
#include <lupine/core/Scene.hpp>
#include <lupine/core/SceneManager.hpp>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>

namespace {

LCResult WriteAllocatedJson(const nlohmann::json& value, char** out_json) {
    if (!out_json) {
        SetError(LC_ERROR_NULL_POINTER, "out_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    std::string dumped = value.dump();
    char* buffer = DuplicateString(dumped);
    if (!buffer) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate JSON result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    *out_json = buffer;
    return LC_SUCCESS;
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
    size_t count = value.size();
    if (count >= max_length) {
        count = static_cast<size_t>(max_length) - 1;
    }
    std::memcpy(buffer, value.data(), count);
    buffer[count] = '\0';
    return LC_SUCCESS;
}

bool TryParseJson(const char* text, nlohmann::json& out) {
    try {
        out = nlohmann::json::parse(text ? text : "null");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

/* ============================================================================
 * Type registry
 * ============================================================================ */

LC_API bool lc_type_is_registered(const char* type_name) {
    if (!type_name) return false;
    try {
        return lupine::core::TypeRegistry::GetInstance().IsTypeRegistered(type_name);
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_type_get_count(int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    try {
        *out_count = static_cast<int>(
            lupine::core::TypeRegistry::GetInstance().GetRegisteredTypeNames().size());
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_type_get_count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_type_get_name_at(int index, char* buffer, uint32_t max_length) {
    if (index < 0) return LC_ERROR_INVALID_PARAMETER;
    try {
        std::vector<std::string> names =
            lupine::core::TypeRegistry::GetInstance().GetRegisteredTypeNames();
        if (static_cast<size_t>(index) >= names.size()) {
            SetError(LC_ERROR_INVALID_PARAMETER, "Type index out of range");
            return LC_ERROR_INVALID_PARAMETER;
        }
        return WriteStringBuffer(names[static_cast<size_t>(index)], buffer, max_length);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_type_get_name_at");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_type_create_node(const char* type_name, LCNodeHandle* out_node) {
    if (!type_name) return LC_ERROR_NULL_POINTER;
    if (!out_node) return LC_ERROR_NULL_POINTER;
    try {
        std::shared_ptr<lupine::core::ISerializable> instance =
            lupine::core::TypeRegistry::GetInstance().CreateInstance(type_name);
        if (!instance) {
            SetError(LC_ERROR_NOT_FOUND, "Type is not registered");
            return LC_ERROR_NOT_FOUND;
        }
        std::shared_ptr<lupine::core::Node> node =
            std::dynamic_pointer_cast<lupine::core::Node>(instance);
        if (!node) {
            SetError(LC_ERROR_NOT_SUPPORTED, "Registered type is not a Node");
            return LC_ERROR_NOT_SUPPORTED;
        }
        node->RegisterProperties();
        *out_node = CreateHandle(node);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_type_create_node");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_type_create_component(const char* type_name, LCComponentHandle* out_component) {
    if (!type_name) return LC_ERROR_NULL_POINTER;
    if (!out_component) return LC_ERROR_NULL_POINTER;
    try {
        std::shared_ptr<lupine::core::ISerializable> instance =
            lupine::core::TypeRegistry::GetInstance().CreateInstance(type_name);
        if (!instance) {
            SetError(LC_ERROR_NOT_FOUND, "Type is not registered");
            return LC_ERROR_NOT_FOUND;
        }
        std::shared_ptr<lupine::core::Component> component =
            std::dynamic_pointer_cast<lupine::core::Component>(instance);
        if (!component) {
            SetError(LC_ERROR_NOT_SUPPORTED, "Registered type is not a Component");
            return LC_ERROR_NOT_SUPPORTED;
        }
        component->RegisterProperties();
        *out_component = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_type_create_component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_create_node_child(const char* type_name, const char* name,
                                     LCNodeHandle parent_or_null, LCNodeHandle* out_node) {
    if (!type_name) return LC_ERROR_NULL_POINTER;
    if (!out_node) return LC_ERROR_NULL_POINTER;
    try {
        std::shared_ptr<lupine::core::Node> node =
            std::dynamic_pointer_cast<lupine::core::Node>(
                lupine::core::TypeRegistry::GetInstance().CreateInstance(type_name));
        if (!node) {
            SetError(LC_ERROR_NOT_FOUND, "Type is not a registered Node");
            return LC_ERROR_NOT_FOUND;
        }

        node->SetName(name ? name : type_name);
        node->RegisterProperties();

        std::shared_ptr<lupine::core::Node> attachParent;
        if (parent_or_null) {
            attachParent = GetNode(parent_or_null);
        } else {
            lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
            if (sceneManager) {
                lupine::core::Scene* scene = sceneManager->GetCurrentScene();
                if (scene) {
                    attachParent = scene->GetRoot();
                }
            }
        }

        if (!attachParent) {
            SetError(LC_ERROR_OPERATION_FAILED, "No parent or scene root to attach the new node to");
            return LC_ERROR_OPERATION_FAILED;
        }

        attachParent->AddChild(node);
        *out_node = CreateHandle(node);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_create_node_child");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Node reflection
 * ============================================================================ */

LC_API LCResult lc_node_get_type_name(LCNodeHandle node, char* buffer, uint32_t max_length) {
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    try {
        return WriteStringBuffer(nodePtr->GetTypeName(), buffer, max_length);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_get_type_name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_component_count(LCNodeHandle node, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    try {
        *out_count = static_cast<int>(nodePtr->GetComponents().size());
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_get_component_count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_component_at(LCNodeHandle node, int index, LCComponentHandle* out_component) {
    if (!out_component) return LC_ERROR_NULL_POINTER;
    if (index < 0) return LC_ERROR_INVALID_PARAMETER;
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    try {
        const std::vector<std::shared_ptr<lupine::core::Component>>& components = nodePtr->GetComponents();
        if (static_cast<size_t>(index) >= components.size()) {
            SetError(LC_ERROR_INVALID_PARAMETER, "Component index out of range");
            return LC_ERROR_INVALID_PARAMETER;
        }
        *out_component = CreateComponentHandle(components[static_cast<size_t>(index)]);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_get_component_at");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_component_by_type(LCNodeHandle node, const char* type_name,
                                              LCComponentHandle* out_component) {
    if (!type_name) return LC_ERROR_NULL_POINTER;
    if (!out_component) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    try {
        std::shared_ptr<lupine::core::Component> component = nodePtr->GetComponent(type_name);
        if (!component) {
            SetError(LC_ERROR_NOT_FOUND, "No component of that type on the node");
            return LC_ERROR_NOT_FOUND;
        }
        *out_component = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_get_component_by_type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_node_has_component(LCNodeHandle node, const char* type_name) {
    if (!type_name) return false;
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return false;
    try {
        return nodePtr->GetComponent(type_name) != nullptr;
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_node_add_component_by_type(LCNodeHandle node, const char* type_name,
                                              LCComponentHandle* out_component) {
    if (!type_name) return LC_ERROR_NULL_POINTER;
    if (!out_component) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    try {
        std::shared_ptr<lupine::core::Component> component =
            std::dynamic_pointer_cast<lupine::core::Component>(
                lupine::core::TypeRegistry::GetInstance().CreateInstance(type_name));
        if (!component) {
            SetError(LC_ERROR_NOT_FOUND, "Type is not a registered Component");
            return LC_ERROR_NOT_FOUND;
        }
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *out_component = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_add_component_by_type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_remove_component(LCNodeHandle node, LCComponentHandle component) {
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    std::shared_ptr<lupine::core::Component> componentPtr = GetComponent(component);
    if (!componentPtr) return LC_ERROR_INVALID_HANDLE;
    try {
        nodePtr->RemoveComponent(componentPtr);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_remove_component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_node_has_property(LCNodeHandle node, const char* name) {
    if (!name) return false;
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return false;
    try {
        for (const std::shared_ptr<lupine::core::Component>& comp : nodePtr->GetComponents()) {
            if (comp && comp->GetPropertyRegistry().HasProperty(name)) {
                return true;
            }
        }
        nodePtr->RegisterProperties();
        nlohmann::json serialized = nodePtr->Serialize();
        return serialized.contains("properties") && serialized["properties"].contains(name);
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_node_get_property_json(LCNodeHandle node, const char* name, char** out_json) {
    if (!name) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    try {
        for (const std::shared_ptr<lupine::core::Component>& comp : nodePtr->GetComponents()) {
            if (comp && comp->GetPropertyRegistry().HasProperty(name)) {
                const lupine::core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
                if (prop) {
                    return WriteAllocatedJson(prop->GetValueAsJson(), out_json);
                }
            }
        }
        nodePtr->RegisterProperties();
        nlohmann::json serialized = nodePtr->Serialize();
        if (serialized.contains("properties") && serialized["properties"].contains(name)) {
            return WriteAllocatedJson(serialized["properties"][name], out_json);
        }
        SetError(LC_ERROR_NOT_FOUND, "Node property not found");
        return LC_ERROR_NOT_FOUND;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_get_property_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_set_property_json(LCNodeHandle node, const char* name, const char* value_json) {
    if (!name) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    nlohmann::json value;
    if (!TryParseJson(value_json, value)) {
        SetError(LC_ERROR_INVALID_PARAMETER, "value_json is not valid JSON");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        for (const std::shared_ptr<lupine::core::Component>& comp : nodePtr->GetComponents()) {
            if (comp && comp->GetPropertyRegistry().HasProperty(name)) {
                lupine::core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
                if (prop) {
                    prop->SetValueFromJson(value);
                    comp->OnPropertyChanged(name, value);
                    return LC_SUCCESS;
                }
            }
        }
        nodePtr->RegisterProperties();
        nlohmann::json serialized = nodePtr->Serialize();
        if (!(serialized.contains("properties") && serialized["properties"].contains(name))) {
            SetError(LC_ERROR_NOT_FOUND, "Node property not found");
            return LC_ERROR_NOT_FOUND;
        }
        nlohmann::json patch;
        patch["properties"][name] = value;
        nodePtr->Deserialize(patch);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_set_property_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_serialize_json(LCNodeHandle node, char** out_json) {
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    try {
        return WriteAllocatedJson(nodePtr->Serialize(), out_json);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_serialize_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_deserialize_json(LCNodeHandle node, const char* json) {
    std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
    if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
    nlohmann::json parsed;
    if (!TryParseJson(json, parsed)) {
        SetError(LC_ERROR_INVALID_PARAMETER, "json is not valid JSON");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        nodePtr->Deserialize(parsed);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_node_deserialize_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Component reflection
 * ============================================================================ */

LC_API LCResult lc_component_get_type_name(LCComponentHandle component, char* buffer, uint32_t max_length) {
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        return WriteStringBuffer(comp->GetTypeName(), buffer, max_length);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_get_type_name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_get_name(LCComponentHandle component, char* buffer, uint32_t max_length) {
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        return WriteStringBuffer(comp->GetName(), buffer, max_length);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_get_name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_component_is_enabled(LCComponentHandle component) {
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return false;
    try {
        return comp->IsEnabled();
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_component_set_enabled(LCComponentHandle component, bool enabled) {
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        comp->SetEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_set_enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_component_has_property(LCComponentHandle component, const char* name) {
    if (!name) return false;
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return false;
    try {
        if (comp->GetPropertyRegistry().HasProperty(name)) {
            return true;
        }
        nlohmann::json serialized = comp->Serialize();
        return serialized.contains("properties") && serialized["properties"].contains(name);
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_component_get_property_json(LCComponentHandle component, const char* name, char** out_json) {
    if (!name) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        const lupine::core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
        if (prop) {
            return WriteAllocatedJson(prop->GetValueAsJson(), out_json);
        }
        nlohmann::json serialized = comp->Serialize();
        if (serialized.contains("properties") && serialized["properties"].contains(name)) {
            return WriteAllocatedJson(serialized["properties"][name], out_json);
        }
        SetError(LC_ERROR_NOT_FOUND, "Component property not found");
        return LC_ERROR_NOT_FOUND;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_get_property_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_set_property_json(LCComponentHandle component, const char* name, const char* value_json) {
    if (!name) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    nlohmann::json value;
    if (!TryParseJson(value_json, value)) {
        SetError(LC_ERROR_INVALID_PARAMETER, "value_json is not valid JSON");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        lupine::core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
        if (prop) {
            prop->SetValueFromJson(value);
            comp->OnPropertyChanged(name, value);
            return LC_SUCCESS;
        }
        nlohmann::json serialized = comp->Serialize();
        if (!(serialized.contains("properties") && serialized["properties"].contains(name))) {
            SetError(LC_ERROR_NOT_FOUND, "Component property not found");
            return LC_ERROR_NOT_FOUND;
        }
        nlohmann::json patch;
        patch["properties"][name] = value;
        comp->Deserialize(patch);
        comp->OnPropertyChanged(name, value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_set_property_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_get_property_type(LCComponentHandle component, const char* name,
                                               LCPropertyValueType* out_type) {
    if (!name) return LC_ERROR_NULL_POINTER;
    if (!out_type) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        const lupine::core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
        if (!prop) {
            SetError(LC_ERROR_NOT_FOUND, "Component custom property not found");
            return LC_ERROR_NOT_FOUND;
        }
        *out_type = static_cast<LCPropertyValueType>(static_cast<int>(prop->GetType()));
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_get_property_type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_get_property_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        *out_count = static_cast<int>(comp->GetPropertyDescriptors().size());
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_get_property_count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_get_property_name_at(LCComponentHandle component, int index,
                                                  char* buffer, uint32_t max_length) {
    if (index < 0) return LC_ERROR_INVALID_PARAMETER;
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        std::vector<lupine::core::PropertyDescriptor> descriptors = comp->GetPropertyDescriptors();
        if (static_cast<size_t>(index) >= descriptors.size()) {
            SetError(LC_ERROR_INVALID_PARAMETER, "Property index out of range");
            return LC_ERROR_INVALID_PARAMETER;
        }
        return WriteStringBuffer(descriptors[static_cast<size_t>(index)].name, buffer, max_length);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_get_property_name_at");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_get_property_descriptors_json(LCComponentHandle component, char** out_json) {
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        nlohmann::json array = nlohmann::json::array();
        for (const lupine::core::PropertyDescriptor& descriptor : comp->GetPropertyDescriptors()) {
            array.push_back(descriptor.Serialize());
        }
        return WriteAllocatedJson(array, out_json);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_get_property_descriptors_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_call_method_json(LCComponentHandle component, const char* method,
                                              const char* args_json, char** out_result_json) {
    if (!method) return LC_ERROR_NULL_POINTER;
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    nlohmann::json args;
    if (!TryParseJson(args_json, args)) {
        SetError(LC_ERROR_INVALID_PARAMETER, "args_json is not valid JSON");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        nlohmann::json result = comp->CallMethod(method, args);
        if (out_result_json) {
            return WriteAllocatedJson(result, out_result_json);
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_call_method_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_serialize_json(LCComponentHandle component, char** out_json) {
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    try {
        return WriteAllocatedJson(comp->Serialize(), out_json);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_serialize_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_deserialize_json(LCComponentHandle component, const char* json) {
    std::shared_ptr<lupine::core::Component> comp = GetComponent(component);
    if (!comp) return LC_ERROR_INVALID_HANDLE;
    nlohmann::json parsed;
    if (!TryParseJson(json, parsed)) {
        SetError(LC_ERROR_INVALID_PARAMETER, "json is not valid JSON");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        comp->Deserialize(parsed);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_component_deserialize_json");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
