/**
 * @file lc_interface.cpp
 * @brief Implementation of the C API for the interface-types system.
 *
 * Reuses the engine's InterfaceRegistry / ArchetypeRegistry and the Node/Scene
 * conformance helpers so the C surface stays in lockstep with the script-facing
 * and editor-facing views of interfaces.
 */

#include "core/lc_interface.h"
#include "core/lc_core.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/core/Component.hpp>
#include <lupine/core/Scene.hpp>
#include <lupine/core/SceneManager.hpp>
#include <lupine/core/InterfaceRegistry.hpp>
#include <lupine/core/ArchetypeRegistry.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_set>
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
    std::size_t copyLength = std::min(static_cast<std::size_t>(max_length - 1), value.size());
    std::memcpy(buffer, value.c_str(), copyLength);
    buffer[copyLength] = '\0';
    return LC_SUCCESS;
}

nlohmann::json StringVectorToJson(const std::vector<std::string>& values) {
    nlohmann::json arr = nlohmann::json::array();
    for (const std::string& v : values) {
        arr.push_back(v);
    }
    return arr;
}

// Parse a JSON array-of-strings argument into a set. A null/empty/non-array
// payload yields an empty set.
std::unordered_set<std::string> ParseStringSet(const char* json_text) {
    std::unordered_set<std::string> result;
    if (!json_text || json_text[0] == '\0') {
        return result;
    }
    nlohmann::json parsed = nlohmann::json::parse(json_text, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_array()) {
        for (const nlohmann::json& v : parsed) {
            if (v.is_string()) {
                result.insert(v.get<std::string>());
            }
        }
    }
    return result;
}

lupine::core::Scene* CurrentScene() {
    lupine::core::SceneManager* manager = lupine::core::SceneManager::GetInstance();
    return manager ? manager->GetCurrentScene() : nullptr;
}

} // namespace

// ===========================================================================
// Interface registry / definitions
// ===========================================================================

LC_API LCResult lc_interface_exists(const char* interface_name, bool* out_exists) {
    if (!interface_name || !out_exists) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name or out_exists is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_exists = lupine::core::InterfaceRegistry::GetInstance().IsInterface(interface_name);
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_interface_get_count(int* out_count) {
    if (!out_count) {
        SetError(LC_ERROR_NULL_POINTER, "out_count is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_count = static_cast<int>(
            lupine::core::InterfaceRegistry::GetInstance().GetDefinitions().size());
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_interface_get_name_at(int index, char* buffer, uint32_t max_length) {
    try {
        const std::vector<lupine::core::InterfaceDefinition>& defs =
            lupine::core::InterfaceRegistry::GetInstance().GetDefinitions();
        if (index < 0 || static_cast<std::size_t>(index) >= defs.size()) {
            SetError(LC_ERROR_INVALID_PARAMETER, "interface index out of range");
            return LC_ERROR_INVALID_PARAMETER;
        }
        return WriteStringBuffer(defs[static_cast<std::size_t>(index)].name, buffer, max_length);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_interface_get_definition_json(const char* interface_name, char** out_json) {
    if (!interface_name) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        const lupine::core::InterfaceDefinition* def =
            lupine::core::InterfaceRegistry::GetInstance().GetDefinition(interface_name);
        if (!def) {
            SetError(LC_ERROR_NOT_FOUND, "interface not found");
            return LC_ERROR_NOT_FOUND;
        }
        return WriteAllocatedJson(def->Serialize(), out_json);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_interface_register_json(const char* definition_json) {
    if (!definition_json) {
        SetError(LC_ERROR_NULL_POINTER, "definition_json is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        nlohmann::json parsed = nlohmann::json::parse(definition_json, nullptr, false);
        if (!parsed.is_object()) {
            SetError(LC_ERROR_INVALID_PARAMETER, "definition_json is not a JSON object");
            return LC_ERROR_INVALID_PARAMETER;
        }
        lupine::core::InterfaceDefinition def = lupine::core::InterfaceDefinition::Deserialize(
            parsed, lupine::core::InterfaceSource::Native, "");
        if (!def.isValid) {
            SetError(LC_ERROR_INVALID_PARAMETER, "interface definition is missing 'interface_name'");
            return LC_ERROR_INVALID_PARAMETER;
        }
        lupine::core::InterfaceRegistry::GetInstance().RegisterRuntimeInterface(def);
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_interface_verify_members_json(const char* interface_name,
                                                 const char* present_methods_json,
                                                 const char* present_signals_json,
                                                 char** out_json) {
    if (!interface_name) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::unordered_set<std::string> methods = ParseStringSet(present_methods_json);
        std::unordered_set<std::string> signals = ParseStringSet(present_signals_json);
        nlohmann::json result = lupine::core::InterfaceRegistry::GetInstance()
            .VerifyMembers(interface_name, methods, signals);
        return WriteAllocatedJson(result, out_json);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

// ===========================================================================
// Native component-type conformance
// ===========================================================================

LC_API LCResult lc_interface_register_type_conformance(const char* type_name,
                                                       const char* interface_name) {
    if (!type_name || !interface_name) {
        SetError(LC_ERROR_NULL_POINTER, "type_name or interface_name is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::InterfaceRegistry::GetInstance().RegisterTypeConformance(type_name, interface_name);
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_interface_type_implements(const char* type_name, const char* interface_name,
                                             bool* out_implements) {
    if (!type_name || !interface_name || !out_implements) {
        SetError(LC_ERROR_NULL_POINTER, "type_name, interface_name or out_implements is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_implements = lupine::core::InterfaceRegistry::GetInstance()
            .TypeImplementsInterface(type_name, interface_name);
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_interface_get_types_implementing_json(const char* interface_name, char** out_json) {
    if (!interface_name) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::vector<std::string> types = lupine::core::InterfaceRegistry::GetInstance()
            .GetTypesImplementing(interface_name);
        return WriteAllocatedJson(StringVectorToJson(types), out_json);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

// ===========================================================================
// Node conformance
// ===========================================================================

LC_API LCResult lc_node_implements_interface(LCNodeHandle node, const char* interface_name,
                                             bool* out_implements) {
    if (!interface_name || !out_implements) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name or out_implements is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        *out_implements = n->ImplementsInterface(interface_name);
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_interfaces_json(LCNodeHandle node, char** out_json) {
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        return WriteAllocatedJson(StringVectorToJson(n->GetImplementedInterfaces()), out_json);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_verify_interface_json(LCNodeHandle node, const char* interface_name,
                                              char** out_json) {
    if (!interface_name) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        return WriteAllocatedJson(n->VerifyInterface(interface_name), out_json);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

// ===========================================================================
// Component conformance
// ===========================================================================

LC_API LCResult lc_component_implements_interface(LCComponentHandle component,
                                                  const char* interface_name,
                                                  bool* out_implements) {
    if (!interface_name || !out_implements) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name or out_implements is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto c = GetComponent(component);
    if (!c) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        lupine::core::InterfaceRegistry& registry = lupine::core::InterfaceRegistry::GetInstance();
        bool implements = false;
        for (const std::string& declared : c->GetImplementedInterfaces()) {
            if (registry.IsSubInterfaceOf(declared, interface_name)) {
                implements = true;
                break;
            }
        }
        *out_implements = implements;
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_get_interfaces_json(LCComponentHandle component, char** out_json) {
    auto c = GetComponent(component);
    if (!c) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        return WriteAllocatedJson(StringVectorToJson(c->GetImplementedInterfaces()), out_json);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

// ===========================================================================
// Scene queries
// ===========================================================================

LC_API LCResult lc_scene_find_nodes_by_interface(const char* interface_name,
                                                 LCNodeHandle* out_nodes, size_t max_count,
                                                 size_t* out_count) {
    if (!interface_name || !out_count) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name or out_count is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_count = 0;
        lupine::core::Scene* scene = CurrentScene();
        if (!scene) {
            return LC_SUCCESS;
        }
        std::vector<lupine::core::Node*> matches = scene->GetNodesImplementingInterface(interface_name);
        *out_count = matches.size();
        if (out_nodes && max_count > 0) {
            size_t count = std::min(max_count, matches.size());
            for (size_t i = 0; i < count; ++i) {
                out_nodes[i] = matches[i] ? CreateHandle(matches[i]->shared_from_this()) : nullptr;
            }
        }
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scene_get_first_node_by_interface(const char* interface_name,
                                                     LCNodeHandle* out_found) {
    if (!interface_name || !out_found) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name or out_found is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_found = nullptr;
        lupine::core::Scene* scene = CurrentScene();
        if (!scene) {
            return LC_SUCCESS;
        }
        std::vector<lupine::core::Node*> matches = scene->GetNodesImplementingInterface(interface_name);
        if (!matches.empty() && matches.front()) {
            *out_found = CreateHandle(matches.front()->shared_from_this());
        }
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

// ===========================================================================
// Archetype conformance
// ===========================================================================

LC_API LCResult lc_archetype_implements_interface(const char* class_name, const char* interface_name,
                                                  bool* out_implements) {
    if (!class_name || !interface_name || !out_implements) {
        SetError(LC_ERROR_NULL_POINTER, "class_name, interface_name or out_implements is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_implements = lupine::core::ArchetypeRegistry::GetInstance()
            .ArchetypeImplementsInterface(class_name, interface_name);
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_archetype_get_interfaces_json(const char* class_name, char** out_json) {
    if (!class_name) {
        SetError(LC_ERROR_NULL_POINTER, "class_name is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::vector<std::string> interfaces = lupine::core::ArchetypeRegistry::GetInstance()
            .GetImplementedInterfaces(class_name);
        return WriteAllocatedJson(StringVectorToJson(interfaces), out_json);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_archetype_get_implementing_json(const char* interface_name, char** out_json) {
    if (!interface_name) {
        SetError(LC_ERROR_NULL_POINTER, "interface_name is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::vector<std::string> classes = lupine::core::ArchetypeRegistry::GetInstance()
            .GetArchetypesImplementing(interface_name);
        return WriteAllocatedJson(StringVectorToJson(classes), out_json);
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}
