

#include "core/lc_node.h"
#include "core/lc_core.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/core/Scene.hpp>
#include <lupine/core/SceneManager.hpp>
#include <lupine/core/SignalDispatcher.hpp>
#include <lupine/core/Serialization.hpp>
#include <lupine/core/Component.hpp>

#include <nlohmann/json.hpp>

#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>

namespace {

constexpr float kDegToRad = 0.01745329251994329577f;
constexpr float kRadToDeg = 57.29577951308232087680f;

std::unordered_map<LCNodeHandle, std::shared_ptr<lupine::core::Node>> g_nodeHandles;
std::mutex g_nodeHandlesMutex;

void SetNodeError(LCResult code, const char* message) {
    ::SetError(code, message);
}

void CollectChildrenByType(const std::shared_ptr<lupine::core::Node>& node,
                           const std::string& typeName, bool recursive,
                           std::vector<std::shared_ptr<lupine::core::Node>>& out) {
    if (!node) return;
    for (const std::shared_ptr<lupine::core::Node>& child : node->GetChildren()) {
        if (!child) continue;
        if (typeName.empty() || child->GetTypeName() == typeName) {
            out.push_back(child);
        }
        if (recursive) {
            CollectChildrenByType(child, typeName, recursive, out);
        }
    }
}

LCResult WriteAllocatedJson(const nlohmann::json& value, char** out_json) {
    if (!out_json) {
        SetNodeError(LC_ERROR_NULL_POINTER, "out_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    std::string dumped = value.dump();
    char* buffer = DuplicateString(dumped);
    if (!buffer) {
        SetNodeError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate JSON result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    *out_json = buffer;
    return LC_SUCCESS;
}

std::shared_ptr<lupine::core::Component> FindComponentInChildren(
        const std::shared_ptr<lupine::core::Node>& node, const std::string& typeName) {
    if (!node) return nullptr;

    std::shared_ptr<lupine::core::Component> comp = node->GetComponent(typeName);
    if (comp) return comp;

    for (const std::shared_ptr<lupine::core::Node>& child : node->GetChildren()) {
        std::shared_ptr<lupine::core::Component> found = FindComponentInChildren(child, typeName);
        if (found) return found;
    }
    return nullptr;
}

// Recursively strip UUIDs from serialized node JSON so a cloned subtree receives
// fresh identifiers on deserialize. Mirrors core::ScriptAPI::DuplicateNode.
void StripNodeUUIDs(nlohmann::json& json) {
    if (json.contains("uuid")) {
        json.erase("uuid");
    }
    if (json.contains("components") && json["components"].is_array()) {
        for (nlohmann::json& componentJson : json["components"]) {
            if (componentJson.contains("uuid")) {
                componentJson.erase("uuid");
            }
        }
    }
    if (json.contains("children") && json["children"].is_array()) {
        for (nlohmann::json& childJson : json["children"]) {
            StripNodeUUIDs(childJson);
        }
    }
}

} // anonymous namespace

// Global node handle functions - exported for use by other CAPI source files
LCNodeHandle CreateHandle(std::shared_ptr<lupine::core::Node> node) {
    if (!node) return nullptr;

    std::lock_guard<std::mutex> lock(g_nodeHandlesMutex);
    LCNodeHandle handle = reinterpret_cast<LCNodeHandle>(node.get());
    g_nodeHandles[handle] = node;
    return handle;
}

std::shared_ptr<lupine::core::Node> GetNode(LCNodeHandle handle) {
    if (!handle) return nullptr;

    std::lock_guard<std::mutex> lock(g_nodeHandlesMutex);
    auto it = g_nodeHandles.find(handle);
    return (it != g_nodeHandles.end()) ? it->second : nullptr;
}

void DestroyHandle(LCNodeHandle handle) {
    if (!handle) return;

    std::lock_guard<std::mutex> lock(g_nodeHandlesMutex);
    g_nodeHandles.erase(handle);
}

bool IsValidHandle(LCNodeHandle handle) {
    if (!handle) return false;

    std::lock_guard<std::mutex> lock(g_nodeHandlesMutex);
    return g_nodeHandles.find(handle) != g_nodeHandles.end();
}


LC_API LCResult lc_node_create(LCNodeType type, const char* name, LCNodeHandle* out_node) {
    if (!out_node) {
        SetNodeError(LC_ERROR_NULL_POINTER, "out_node is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::shared_ptr<lupine::core::Node> node;
        std::string nodeName = name ? name : "";

        switch (type) {
            case LC_NODE_BASE:
                node = std::make_shared<lupine::core::Node>(nodeName);
                break;
            case LC_NODE_2D:
                node = std::make_shared<lupine::core::Node2D>(nodeName);
                break;
            case LC_NODE_3D:
                node = std::make_shared<lupine::core::Node3D>(nodeName);
                break;
            default:
                SetNodeError(LC_ERROR_NODE_INVALID_TYPE, "Invalid node type");
                return LC_ERROR_NODE_INVALID_TYPE;
        }

        *out_node = CreateHandle(node);
        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetNodeError(LC_ERROR_OUT_OF_MEMORY, e.what());
        return LC_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        SetNodeError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_node_create");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_destroy(LCNodeHandle node) {
    if (!IsValidHandle(node)) {
        SetNodeError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        if (nodePtr->GetParent()) {
            nodePtr->GetParent()->RemoveChild(nodePtr);
        }

        DestroyHandle(node);

        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetNodeError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    } catch (...) {
        SetNodeError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_node_destroy");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_type(LCNodeHandle node, LCNodeType* out_type) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_type) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::string typeName = nodePtr->GetTypeName();

        if (typeName == "Node") {
            *out_type = LC_NODE_BASE;
        } else if (typeName == "Node2D") {
            *out_type = LC_NODE_2D;
        } else if (typeName == "Node3D") {
            *out_type = LC_NODE_3D;
        } else {
            *out_type = LC_NODE_BASE;
        }

        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API const char* lc_node_get_name(LCNodeHandle node) {
    if (!IsValidHandle(node)) return nullptr;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return nullptr;
        return nodePtr->GetName().c_str();
    } catch (...) {
        return nullptr;
    }
}

LC_API LCResult lc_node_set_name(LCNodeHandle node, const char* name) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!name) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
        nodePtr->SetName(name);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API const char* lc_node_get_uuid(LCNodeHandle node) {
    if (!IsValidHandle(node)) return nullptr;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return nullptr;

        thread_local char uuid_buffer[64];
        std::string uuid_str = nodePtr->GetUUID().ToString();
        CopyStringToBuffer(uuid_buffer, sizeof(uuid_buffer), uuid_str.c_str());

        return uuid_buffer;
    } catch (...) {
        return nullptr;
    }
}

LC_API bool lc_node_is_active(LCNodeHandle node) {
    if (!IsValidHandle(node)) return false;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return false;
        return nodePtr->IsActive();
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_node_set_active(LCNodeHandle node, bool active) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
        nodePtr->SetActive(active);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_node_is_active_in_hierarchy(LCNodeHandle node) {
    if (!IsValidHandle(node)) return false;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return false;
        return nodePtr->IsActiveInHierarchy();
    } catch (...) {
        return false;
    }
}

LC_API bool lc_node_is_visible(LCNodeHandle node) {
    if (!IsValidHandle(node)) return false;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return false;
        return nodePtr->IsVisible();
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_node_set_visible(LCNodeHandle node, bool visible) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
        nodePtr->SetVisible(visible);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_node_is_visible_in_hierarchy(LCNodeHandle node) {
    if (!IsValidHandle(node)) return false;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return false;
        return nodePtr->IsVisibleInHierarchy();
    } catch (...) {
        return false;
    }
}

LC_API bool lc_node_is_unique_name_in_owner(LCNodeHandle node) {
    if (!IsValidHandle(node)) return false;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return false;
        return nodePtr->IsUniqueNameInOwner();
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_node_set_unique_name_in_owner(LCNodeHandle node, bool unique) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;
        nodePtr->SetUniqueNameInOwner(unique);
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_resolve_unique_name(LCNodeHandle node, const char* name, LCNodeHandle* out_found) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!name) return LC_ERROR_NULL_POINTER;
    if (!out_found) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::Node* found = nodePtr->ResolveUniqueName(name);
        if (found) {
            *out_found = CreateHandle(found->shared_from_this());
        } else {
            *out_found = nullptr;
        }
        return LC_SUCCESS;
    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_add_child(LCNodeHandle parent, LCNodeHandle child) {
    if (!IsValidHandle(parent)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(child)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto parentPtr = GetNode(parent);
        auto childPtr = GetNode(child);

        if (!parentPtr || !childPtr) return LC_ERROR_INVALID_HANDLE;

        parentPtr->AddChild(childPtr);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_insert_child(LCNodeHandle parent, LCNodeHandle child, size_t index) {
    if (!IsValidHandle(parent)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(child)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto parentPtr = GetNode(parent);
        auto childPtr = GetNode(child);

        if (!parentPtr || !childPtr) return LC_ERROR_INVALID_HANDLE;

        parentPtr->InsertChild(childPtr, index);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_move_child(LCNodeHandle parent, LCNodeHandle child, size_t new_index) {
    if (!IsValidHandle(parent)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(child)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto parentPtr = GetNode(parent);
        auto childPtr = GetNode(child);

        if (!parentPtr || !childPtr) return LC_ERROR_INVALID_HANDLE;

        parentPtr->MoveChild(childPtr, new_index);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_child_index(LCNodeHandle parent, LCNodeHandle child, size_t* out_index) {
    if (!IsValidHandle(parent)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(child)) return LC_ERROR_INVALID_HANDLE;
    if (!out_index) return LC_ERROR_NULL_POINTER;

    try {
        auto parentPtr = GetNode(parent);
        auto childPtr = GetNode(child);

        if (!parentPtr || !childPtr) return LC_ERROR_INVALID_HANDLE;

        int index = parentPtr->GetChildIndex(childPtr.get());
        *out_index = (index < 0) ? static_cast<size_t>(-1) : static_cast<size_t>(index);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_sibling_index(LCNodeHandle node, size_t* out_index) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_index) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        int index = nodePtr->GetIndexInParent();
        *out_index = (index < 0) ? static_cast<size_t>(-1) : static_cast<size_t>(index);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_set_sibling_index(LCNodeHandle node, size_t index) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        nodePtr->SetSiblingIndex(index);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_remove_child(LCNodeHandle parent, LCNodeHandle child) {
    if (!IsValidHandle(parent)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(child)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto parentPtr = GetNode(parent);
        auto childPtr = GetNode(child);

        if (!parentPtr || !childPtr) return LC_ERROR_INVALID_HANDLE;

        parentPtr->RemoveChild(childPtr);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_remove_child_by_name(LCNodeHandle parent, const char* child_name) {
    if (!IsValidHandle(parent)) return LC_ERROR_INVALID_HANDLE;
    if (!child_name) return LC_ERROR_NULL_POINTER;

    try {
        auto parentPtr = GetNode(parent);
        if (!parentPtr) return LC_ERROR_INVALID_HANDLE;

        parentPtr->RemoveChild(child_name);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_parent(LCNodeHandle node, LCNodeHandle* out_parent) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_parent) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::Node* parent = nodePtr->GetParent();
        if (parent) {

            std::lock_guard<std::mutex> lock(g_nodeHandlesMutex);
            for (const auto& pair : g_nodeHandles) {
                if (pair.second.get() == parent) {
                    *out_parent = pair.first;
                    return LC_SUCCESS;
                }
            }
            *out_parent = nullptr;
        } else {
            *out_parent = nullptr;
        }

        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_child_count(LCNodeHandle node, size_t* out_count) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_count) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_count = nodePtr->GetChildCount();
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_child(LCNodeHandle node, size_t index, LCNodeHandle* out_child) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_child) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto childPtr = nodePtr->GetChild(index);
        if (childPtr) {
            *out_child = CreateHandle(childPtr);
            return LC_SUCCESS;
        } else {
            *out_child = nullptr;
            return LC_ERROR_INVALID_PARAMETER;
        }

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_child_by_name(LCNodeHandle node, const char* name, LCNodeHandle* out_child) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!name) return LC_ERROR_NULL_POINTER;
    if (!out_child) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto childPtr = nodePtr->GetChild(name);
        if (childPtr) {
            *out_child = CreateHandle(childPtr);
            return LC_SUCCESS;
        } else {
            *out_child = nullptr;
            return LC_ERROR_INVALID_PARAMETER;
        }

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_children(LCNodeHandle node, LCNodeHandle* out_children, size_t max_count, size_t* out_count) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_count) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        const auto& children = nodePtr->GetChildren();
        *out_count = children.size();

        if (out_children && max_count > 0) {
            size_t count = std::min(max_count, children.size());
            for (size_t i = 0; i < count; ++i) {
                out_children[i] = CreateHandle(children[i]);
            }
        }

        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_find(LCNodeHandle node, const char* path, LCNodeHandle* out_found) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!path) return LC_ERROR_NULL_POINTER;
    if (!out_found) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto foundPtr = nodePtr->FindNode(path);
        if (foundPtr) {
            *out_found = CreateHandle(foundPtr);
            return LC_SUCCESS;
        } else {
            *out_found = nullptr;
            return LC_ERROR_INVALID_PARAMETER;
        }

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API const char* lc_node_get_path(LCNodeHandle node) {
    if (!IsValidHandle(node)) return nullptr;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return nullptr;

        thread_local char path_buffer[512];
        std::string path = nodePtr->GetPath();
        CopyStringToBuffer(path_buffer, sizeof(path_buffer), path.c_str());

        return path_buffer;

    } catch (...) {
        return nullptr;
    }
}

LC_API LCResult lc_node_get_node_or_null(LCNodeHandle node, const char* path, LCNodeHandle* out_found) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!path) return LC_ERROR_NULL_POINTER;
    if (!out_found) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto foundPtr = nodePtr->FindNode(path);
        *out_found = foundPtr ? CreateHandle(foundPtr) : nullptr;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_first_node_in_group(LCNodeHandle node, const char* group, LCNodeHandle* out_found) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!group) return LC_ERROR_NULL_POINTER;
    if (!out_found) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_found = nullptr;

        lupine::core::Scene* scene = nodePtr->GetScene();
        if (!scene) {
            return LC_SUCCESS;
        }

        std::vector<lupine::core::Node*> nodes = scene->GetNodesInGroup(group);
        if (!nodes.empty() && nodes.front()) {
            *out_found = CreateHandle(nodes.front()->shared_from_this());
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_find_children(LCNodeHandle node, const char* type_name, bool recursive,
                                      LCNodeHandle* out_children, size_t max_count, size_t* out_count) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_count) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::string typeName = type_name ? type_name : "";
        std::vector<std::shared_ptr<lupine::core::Node>> matches;
        CollectChildrenByType(nodePtr, typeName, recursive, matches);

        *out_count = matches.size();

        if (out_children && max_count > 0) {
            size_t count = std::min(max_count, matches.size());
            for (size_t i = 0; i < count; ++i) {
                out_children[i] = CreateHandle(matches[i]);
            }
        }

        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_node_is_ancestor_of(LCNodeHandle node, LCNodeHandle other) {
    if (!IsValidHandle(node) || !IsValidHandle(other)) return false;

    try {
        auto nodePtr = GetNode(node);
        auto otherPtr = GetNode(other);
        if (!nodePtr || !otherPtr) return false;

        lupine::core::Node* parent = otherPtr->GetParent();
        while (parent) {
            if (parent == nodePtr.get()) return true;
            parent = parent->GetParent();
        }
        return false;

    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_node2d_set_position(LCNodeHandle node, LCVec2 position) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        node2d->SetPosition(lupine::math::Vec2(position.x, position.y));
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_position(LCNodeHandle node, LCVec2* out_position) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_position) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& pos = node2d->GetPosition();
        out_position->x = pos.x;
        out_position->y = pos.y;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_set_rotation(LCNodeHandle node, float rotation) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        node2d->SetRotation(rotation);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_rotation(LCNodeHandle node, float* out_rotation) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_rotation) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_rotation = node2d->GetRotation();
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_set_scale(LCNodeHandle node, LCVec2 scale) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        node2d->SetScale(lupine::math::Vec2(scale.x, scale.y));
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_scale(LCNodeHandle node, LCVec2* out_scale) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_scale) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& scale = node2d->GetScale();
        out_scale->x = scale.x;
        out_scale->y = scale.y;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_set_z_index(LCNodeHandle node, int z_index) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        node2d->SetZIndex(z_index);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_z_index(LCNodeHandle node, int* out_z_index) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_z_index) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_z_index = node2d->GetZIndex();
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_global_position(LCNodeHandle node, LCVec2* out_position) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_position) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& pos = node2d->GetGlobalPosition();
        out_position->x = pos.x;
        out_position->y = pos.y;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_global_rotation(LCNodeHandle node, float* out_rotation) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_rotation) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        *out_rotation = node2d->GetGlobalRotation();
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_global_scale(LCNodeHandle node, LCVec2* out_scale) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_scale) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& scale = node2d->GetGlobalScale();
        out_scale->x = scale.x;
        out_scale->y = scale.y;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_transform_matrix(LCNodeHandle node, LCMat4* out_matrix) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_matrix) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& mat = node2d->GetTransformMatrix();
        const float* ptr = glm::value_ptr(mat.ToGLM());
        for (int i = 0; i < 16; ++i) {
            out_matrix->m[i] = ptr[i];
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_get_global_transform_matrix(LCNodeHandle node, LCMat4* out_matrix) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_matrix) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& mat = node2d->GetGlobalTransformMatrix();
        const float* ptr = glm::value_ptr(mat.ToGLM());
        for (int i = 0; i < 16; ++i) {
            out_matrix->m[i] = ptr[i];
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_set_position(LCNodeHandle node, LCVec3 position) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        node3d->SetPosition(lupine::math::Vec3(position.x, position.y, position.z));
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_position(LCNodeHandle node, LCVec3* out_position) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_position) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& pos = node3d->GetPosition();
        out_position->x = pos.x;
        out_position->y = pos.y;
        out_position->z = pos.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_set_rotation(LCNodeHandle node, LCQuat rotation) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Quat quat(rotation.w, rotation.x, rotation.y, rotation.z);
        node3d->SetRotation(quat);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_rotation(LCNodeHandle node, LCQuat* out_rotation) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_rotation) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& rot = node3d->GetRotation();
        out_rotation->x = rot.x();
        out_rotation->y = rot.y();
        out_rotation->z = rot.z();
        out_rotation->w = rot.w();
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_set_rotation_euler(LCNodeHandle node, LCVec3 euler) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Quat quat = lupine::math::Quat::FromEuler(euler.x, euler.y, euler.z);
        node3d->SetRotation(quat);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_rotation_euler(LCNodeHandle node, LCVec3* out_euler) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_euler) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& rot = node3d->GetRotation();
        lupine::math::Vec3 euler = rot.ToEuler();
        out_euler->x = euler.x;
        out_euler->y = euler.y;
        out_euler->z = euler.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_set_scale(LCNodeHandle node, LCVec3 scale) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        node3d->SetScale(lupine::math::Vec3(scale.x, scale.y, scale.z));
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_scale(LCNodeHandle node, LCVec3* out_scale) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_scale) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& scale = node3d->GetScale();
        out_scale->x = scale.x;
        out_scale->y = scale.y;
        out_scale->z = scale.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_global_position(LCNodeHandle node, LCVec3* out_position) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_position) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& pos = node3d->GetGlobalPosition();
        out_position->x = pos.x;
        out_position->y = pos.y;
        out_position->z = pos.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_global_rotation(LCNodeHandle node, LCQuat* out_rotation) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_rotation) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& rot = node3d->GetGlobalRotation();
        out_rotation->x = rot.x();
        out_rotation->y = rot.y();
        out_rotation->z = rot.z();
        out_rotation->w = rot.w();
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_global_scale(LCNodeHandle node, LCVec3* out_scale) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_scale) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& scale = node3d->GetGlobalScale();
        out_scale->x = scale.x;
        out_scale->y = scale.y;
        out_scale->z = scale.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_transform_matrix(LCNodeHandle node, LCMat4* out_matrix) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_matrix) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& mat = node3d->GetTransformMatrix();
        const float* ptr = glm::value_ptr(mat.ToGLM());
        for (int i = 0; i < 16; ++i) {
            out_matrix->m[i] = ptr[i];
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_global_transform_matrix(LCNodeHandle node, LCMat4* out_matrix) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_matrix) return LC_ERROR_NULL_POINTER;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        const auto& mat = node3d->GetGlobalTransformMatrix();
        const float* ptr = glm::value_ptr(mat.ToGLM());
        for (int i = 0; i < 16; ++i) {
            out_matrix->m[i] = ptr[i];
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_look_at(LCNodeHandle node, LCVec3 target, LCVec3 up) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        auto nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        auto node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 pos = node3d->GetPosition();
        lupine::math::Vec3 targetVec(target.x, target.y, target.z);
        lupine::math::Vec3 upVec(up.x, up.y, up.z);

        lupine::math::Vec3 forward = (targetVec - pos).Normalized();
        lupine::math::Quat rotation = lupine::math::Quat::LookRotation(forward, upVec);

        node3d->SetRotation(rotation);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_translate(LCNodeHandle node, LCVec2 delta) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node2D> node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec2 pos = node2d->GetPosition();
        node2d->SetPosition(lupine::math::Vec2(pos.x + delta.x, pos.y + delta.y));
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_translate(LCNodeHandle node, LCVec3 delta) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 pos = node3d->GetPosition();
        node3d->SetPosition(lupine::math::Vec3(pos.x + delta.x, pos.y + delta.y, pos.z + delta.z));
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_rotate(LCNodeHandle node, float radians) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node2D> node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        node2d->SetRotation(node2d->GetRotation() + radians);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_rotate_euler(LCNodeHandle node, LCVec3 euler_delta_radians) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 euler = node3d->GetRotation().ToEuler();
        lupine::math::Quat quat = lupine::math::Quat::FromEuler(
            euler.x + euler_delta_radians.x,
            euler.y + euler_delta_radians.y,
            euler.z + euler_delta_radians.z);
        node3d->SetRotation(quat);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_set_global_position(LCNodeHandle node, LCVec2 position) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node2D> node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec2 globalPos(position.x, position.y);
        lupine::core::Node2D* parent2d = dynamic_cast<lupine::core::Node2D*>(nodePtr->GetParent());
        if (parent2d) {
            lupine::math::Vec2 parentGlobal = parent2d->GetGlobalPosition();
            node2d->SetPosition(globalPos - parentGlobal);
        } else {
            node2d->SetPosition(globalPos);
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_set_global_position(LCNodeHandle node, LCVec3 position) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 globalPos(position.x, position.y, position.z);
        lupine::core::Node3D* parent3d = dynamic_cast<lupine::core::Node3D*>(nodePtr->GetParent());
        if (parent3d) {
            lupine::math::Vec3 parentGlobal = parent3d->GetGlobalPosition();
            node3d->SetPosition(globalPos - parentGlobal);
        } else {
            node3d->SetPosition(globalPos);
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_set_global_rotation(LCNodeHandle node, float radians) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node2D> node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::core::Node2D* parent2d = dynamic_cast<lupine::core::Node2D*>(nodePtr->GetParent());
        if (parent2d) {
            node2d->SetRotation(radians - parent2d->GetGlobalRotation());
        } else {
            node2d->SetRotation(radians);
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_set_global_rotation(LCNodeHandle node, LCQuat rotation) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Quat target(rotation.w, rotation.x, rotation.y, rotation.z);

        // Node3D composes world = parentWorld * local, so local = parentWorld⁻¹ * world.
        lupine::core::Node3D* parent3d = dynamic_cast<lupine::core::Node3D*>(nodePtr->GetParent());
        if (parent3d) {
            node3d->SetRotation((parent3d->GetGlobalRotation().Inverse() * target).Normalized());
        } else {
            node3d->SetRotation(target);
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_global_rotation_euler(LCNodeHandle node, LCVec3* out_euler) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_euler) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 euler = node3d->GetGlobalRotation().ToEuler();
        out_euler->x = euler.x;
        out_euler->y = euler.y;
        out_euler->z = euler.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_set_global_rotation_euler(LCNodeHandle node, LCVec3 euler) {
    lupine::math::Quat quat = lupine::math::Quat::FromEuler(euler.x, euler.y, euler.z);
    LCQuat out;
    out.x = quat.x();
    out.y = quat.y();
    out.z = quat.z();
    out.w = quat.w();
    return lc_node3d_set_global_rotation(node, out);
}

// ---------------------------------------------------------------------------
// Degree-based convenience rotation API. These mirror the radian/quaternion
// functions above but take/return degrees, matching the scripting API
// convention. They are thin wrappers over the same Node setters.
// ---------------------------------------------------------------------------

LC_API LCResult lc_node2d_set_rotation_degrees(LCNodeHandle node, float degrees) {
    return lc_node2d_set_rotation(node, degrees * kDegToRad);
}

LC_API LCResult lc_node2d_get_rotation_degrees(LCNodeHandle node, float* out_degrees) {
    if (!out_degrees) return LC_ERROR_NULL_POINTER;
    float radians = 0.0f;
    LCResult result = lc_node2d_get_rotation(node, &radians);
    if (result == LC_SUCCESS) *out_degrees = radians * kRadToDeg;
    return result;
}

LC_API LCResult lc_node2d_rotate_degrees(LCNodeHandle node, float degrees) {
    return lc_node2d_rotate(node, degrees * kDegToRad);
}

LC_API LCResult lc_node2d_get_global_rotation_degrees(LCNodeHandle node, float* out_degrees) {
    if (!out_degrees) return LC_ERROR_NULL_POINTER;
    float radians = 0.0f;
    LCResult result = lc_node2d_get_global_rotation(node, &radians);
    if (result == LC_SUCCESS) *out_degrees = radians * kRadToDeg;
    return result;
}

LC_API LCResult lc_node2d_set_global_rotation_degrees(LCNodeHandle node, float degrees) {
    return lc_node2d_set_global_rotation(node, degrees * kDegToRad);
}

LC_API LCResult lc_node3d_set_rotation_euler_degrees(LCNodeHandle node, LCVec3 euler_degrees) {
    LCVec3 radians;
    radians.x = euler_degrees.x * kDegToRad;
    radians.y = euler_degrees.y * kDegToRad;
    radians.z = euler_degrees.z * kDegToRad;
    return lc_node3d_set_rotation_euler(node, radians);
}

LC_API LCResult lc_node3d_get_rotation_euler_degrees(LCNodeHandle node, LCVec3* out_euler_degrees) {
    if (!out_euler_degrees) return LC_ERROR_NULL_POINTER;
    LCVec3 radians;
    LCResult result = lc_node3d_get_rotation_euler(node, &radians);
    if (result == LC_SUCCESS) {
        out_euler_degrees->x = radians.x * kRadToDeg;
        out_euler_degrees->y = radians.y * kRadToDeg;
        out_euler_degrees->z = radians.z * kRadToDeg;
    }
    return result;
}

LC_API LCResult lc_node3d_rotate_euler_degrees(LCNodeHandle node, LCVec3 euler_delta_degrees) {
    LCVec3 radians;
    radians.x = euler_delta_degrees.x * kDegToRad;
    radians.y = euler_delta_degrees.y * kDegToRad;
    radians.z = euler_delta_degrees.z * kDegToRad;
    return lc_node3d_rotate_euler(node, radians);
}

LC_API LCResult lc_node3d_get_global_rotation_euler_degrees(LCNodeHandle node, LCVec3* out_euler_degrees) {
    if (!out_euler_degrees) return LC_ERROR_NULL_POINTER;
    LCVec3 radians;
    LCResult result = lc_node3d_get_global_rotation_euler(node, &radians);
    if (result == LC_SUCCESS) {
        out_euler_degrees->x = radians.x * kRadToDeg;
        out_euler_degrees->y = radians.y * kRadToDeg;
        out_euler_degrees->z = radians.z * kRadToDeg;
    }
    return result;
}

LC_API LCResult lc_node3d_set_global_rotation_euler_degrees(LCNodeHandle node, LCVec3 euler_degrees) {
    LCVec3 radians;
    radians.x = euler_degrees.x * kDegToRad;
    radians.y = euler_degrees.y * kDegToRad;
    radians.z = euler_degrees.z * kDegToRad;
    return lc_node3d_set_global_rotation_euler(node, radians);
}

LC_API LCResult lc_node3d_get_forward(LCNodeHandle node, LCVec3* out_forward) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_forward) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 forward = node3d->GetRotation() * lupine::math::Vec3(0.0f, 0.0f, -1.0f);
        out_forward->x = forward.x;
        out_forward->y = forward.y;
        out_forward->z = forward.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_right(LCNodeHandle node, LCVec3* out_right) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_right) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 right = node3d->GetRotation() * lupine::math::Vec3(1.0f, 0.0f, 0.0f);
        out_right->x = right.x;
        out_right->y = right.y;
        out_right->z = right.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_get_up(LCNodeHandle node, LCVec3* out_up) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_up) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 up = node3d->GetRotation() * lupine::math::Vec3(0.0f, 1.0f, 0.0f);
        out_up->x = up.x;
        out_up->y = up.y;
        out_up->z = up.z;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_look_at(LCNodeHandle node, LCVec2 target) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node2D> node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec2 pos = node2d->GetPosition();
        node2d->SetRotation(std::atan2(target.y - pos.y, target.x - pos.x));
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_distance_to_point(LCNodeHandle node, LCVec2 point, float* out_distance) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_distance) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node2D> node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec2 pos = node2d->GetPosition();
        float dx = point.x - pos.x;
        float dy = point.y - pos.y;
        *out_distance = std::sqrt(dx * dx + dy * dy);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_distance_to_node(LCNodeHandle node, LCNodeHandle other, float* out_distance) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(other)) return LC_ERROR_INVALID_HANDLE;
    if (!out_distance) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        std::shared_ptr<lupine::core::Node> otherPtr = GetNode(other);
        if (!nodePtr || !otherPtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node2D> node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        std::shared_ptr<lupine::core::Node2D> other2d = std::dynamic_pointer_cast<lupine::core::Node2D>(otherPtr);
        if (!node2d || !other2d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec2 pos = node2d->GetPosition();
        lupine::math::Vec2 otherPos = other2d->GetPosition();
        float dx = otherPos.x - pos.x;
        float dy = otherPos.y - pos.y;
        *out_distance = std::sqrt(dx * dx + dy * dy);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_distance_to_point(LCNodeHandle node, LCVec3 point, float* out_distance) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_distance) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 pos = node3d->GetPosition();
        float dx = point.x - pos.x;
        float dy = point.y - pos.y;
        float dz = point.z - pos.z;
        *out_distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_distance_to_node(LCNodeHandle node, LCNodeHandle other, float* out_distance) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(other)) return LC_ERROR_INVALID_HANDLE;
    if (!out_distance) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        std::shared_ptr<lupine::core::Node> otherPtr = GetNode(other);
        if (!nodePtr || !otherPtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        std::shared_ptr<lupine::core::Node3D> other3d = std::dynamic_pointer_cast<lupine::core::Node3D>(otherPtr);
        if (!node3d || !other3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 pos = node3d->GetPosition();
        lupine::math::Vec3 otherPos = other3d->GetPosition();
        float dx = otherPos.x - pos.x;
        float dy = otherPos.y - pos.y;
        float dz = otherPos.z - pos.z;
        *out_distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node2d_move_toward(LCNodeHandle node, LCVec2 target, float max_delta) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node2D> node2d = std::dynamic_pointer_cast<lupine::core::Node2D>(nodePtr);
        if (!node2d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec2 pos = node2d->GetPosition();
        lupine::math::Vec2 targetVec(target.x, target.y);
        lupine::math::Vec2 diff = targetVec - pos;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

        if (dist <= max_delta || dist < 0.0001f) {
            node2d->SetPosition(targetVec);
        } else {
            lupine::math::Vec2 direction = diff / dist;
            node2d->SetPosition(pos + direction * max_delta);
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node3d_move_toward(LCNodeHandle node, LCVec3 target, float max_delta) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::shared_ptr<lupine::core::Node3D> node3d = std::dynamic_pointer_cast<lupine::core::Node3D>(nodePtr);
        if (!node3d) return LC_ERROR_NODE_INVALID_TYPE;

        lupine::math::Vec3 pos = node3d->GetPosition();
        lupine::math::Vec3 targetVec(target.x, target.y, target.z);
        lupine::math::Vec3 diff = targetVec - pos;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        if (dist <= max_delta || dist < 0.0001f) {
            node3d->SetPosition(targetVec);
        } else {
            lupine::math::Vec3 direction = diff / dist;
            node3d->SetPosition(pos + direction * max_delta);
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_scene(LCNodeHandle node, LCSceneHandle* out_scene) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_scene) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::Scene* scene = nodePtr->GetScene();
        *out_scene = scene ? CreateSceneHandleNonOwning(scene) : nullptr;
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_root(LCNodeHandle node, LCNodeHandle* out_root) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_root) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_root = nullptr;
        lupine::core::Scene* scene = nodePtr->GetScene();
        if (scene) {
            std::shared_ptr<lupine::core::Node> root = scene->GetRoot();
            if (root) {
                *out_root = CreateHandle(root);
            }
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_queue_free(LCNodeHandle node) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::SignalDispatcher::Get().QueueFree(nodePtr.get());
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_queue_free_deferred(LCNodeHandle node) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::SignalDispatcher::Get().QueueFreeDeferred(nodePtr.get());
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_free(LCNodeHandle node) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::SignalDispatcher::Get().Free(nodePtr.get());
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_add_sibling(LCNodeHandle node, LCNodeHandle sibling) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(sibling)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        std::shared_ptr<lupine::core::Node> siblingPtr = GetNode(sibling);
        if (!nodePtr || !siblingPtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::Node* parent = nodePtr->GetParent();
        if (!parent) {
            SetNodeError(LC_ERROR_OPERATION_FAILED, "Node has no parent to add a sibling to");
            return LC_ERROR_OPERATION_FAILED;
        }

        parent->AddChild(siblingPtr);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_reparent(LCNodeHandle node, LCNodeHandle new_parent) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!IsValidHandle(new_parent)) return LC_ERROR_INVALID_HANDLE;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        std::shared_ptr<lupine::core::Node> newParentPtr = GetNode(new_parent);
        if (!nodePtr || !newParentPtr) return LC_ERROR_INVALID_HANDLE;

        lupine::core::Node* oldParent = nodePtr->GetParent();
        if (!oldParent) {
            SetNodeError(LC_ERROR_OPERATION_FAILED, "Node has no parent to reparent from");
            return LC_ERROR_OPERATION_FAILED;
        }

        oldParent->RemoveChild(nodePtr);
        newParentPtr->AddChild(nodePtr);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_singleton(const char* name, LCNodeHandle* out_node) {
    if (!name) return LC_ERROR_NULL_POINTER;
    if (!out_node) return LC_ERROR_NULL_POINTER;

    try {
        *out_node = nullptr;

        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            return LC_SUCCESS;
        }

        lupine::core::Node* singleton = sceneManager->GetSingletonNode(name);
        if (singleton) {
            *out_node = CreateHandle(singleton->shared_from_this());
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_find_by_uuid(LCNodeHandle node, const char* uuid, LCNodeHandle* out_found) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!uuid) return LC_ERROR_NULL_POINTER;
    if (!out_found) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_found = nullptr;

        lupine::core::Scene* scene = nodePtr->GetScene();
        if (!scene) {
            return LC_SUCCESS;
        }

        lupine::core::UUID uuidObj = lupine::core::UUID::FromString(uuid);
        std::shared_ptr<lupine::core::Node> found = scene->FindNodeByUUID(uuidObj);
        if (found) {
            *out_found = CreateHandle(found);
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_component_in_children(LCNodeHandle node, const char* type_name, LCComponentHandle* out_component) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!type_name) return LC_ERROR_NULL_POINTER;
    if (!out_component) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_component = nullptr;

        std::shared_ptr<lupine::core::Component> comp = FindComponentInChildren(nodePtr, type_name);
        if (comp) {
            *out_component = CreateComponentHandle(comp);
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_component_in_parent(LCNodeHandle node, const char* type_name, LCComponentHandle* out_component) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!type_name) return LC_ERROR_NULL_POINTER;
    if (!out_component) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_component = nullptr;

        lupine::core::Node* parent = nodePtr->GetParent();
        while (parent) {
            std::shared_ptr<lupine::core::Component> comp = parent->GetComponent(type_name);
            if (comp) {
                *out_component = CreateComponentHandle(comp);
                break;
            }
            parent = parent->GetParent();
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_add_to_group(LCNodeHandle node, const char* group) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!group) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        nodePtr->AddToGroup(group);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_remove_from_group(LCNodeHandle node, const char* group) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!group) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        nodePtr->RemoveFromGroup(group);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_is_in_group(LCNodeHandle node, const char* group, bool* out_in_group) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!group) return LC_ERROR_NULL_POINTER;
    if (!out_in_group) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_in_group = nodePtr->IsInGroup(group);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_groups_json(LCNodeHandle node, char** out_json) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_json) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        std::vector<std::string> groups = nodePtr->GetGroups();
        nlohmann::json arr = nlohmann::json::array();
        for (const std::string& group : groups) {
            arr.push_back(group);
        }
        return WriteAllocatedJson(arr, out_json);

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_get_nodes_in_group(LCNodeHandle node, const char* group,
                                           LCNodeHandle* out_nodes, size_t max_count, size_t* out_count) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!group) return LC_ERROR_NULL_POINTER;
    if (!out_count) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_count = 0;

        lupine::core::Scene* scene = nodePtr->GetScene();
        if (!scene) {
            lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
            if (sceneManager) {
                scene = sceneManager->GetCurrentScene();
            }
        }
        if (!scene) {
            return LC_SUCCESS;
        }

        std::vector<lupine::core::Node*> nodes = scene->GetNodesInGroup(group);
        *out_count = nodes.size();

        if (out_nodes && max_count > 0) {
            size_t count = std::min(max_count, nodes.size());
            for (size_t i = 0; i < count; ++i) {
                out_nodes[i] = nodes[i] ? CreateHandle(nodes[i]->shared_from_this()) : nullptr;
            }
        }
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_duplicate(LCNodeHandle node, LCNodeHandle* out_node) {
    if (!IsValidHandle(node)) return LC_ERROR_INVALID_HANDLE;
    if (!out_node) return LC_ERROR_NULL_POINTER;

    try {
        std::shared_ptr<lupine::core::Node> nodePtr = GetNode(node);
        if (!nodePtr) return LC_ERROR_INVALID_HANDLE;

        *out_node = nullptr;

        nlohmann::json data = nodePtr->Serialize();
        StripNodeUUIDs(data);

        if (!data.contains("type")) {
            SetNodeError(LC_ERROR_OPERATION_FAILED, "Serialized node has no type to clone");
            return LC_ERROR_OPERATION_FAILED;
        }
        std::string typeName = data["type"].get<std::string>();

        std::shared_ptr<lupine::core::Node> clone = std::dynamic_pointer_cast<lupine::core::Node>(
            lupine::core::TypeRegistry::GetInstance().CreateInstance(typeName));
        if (!clone) {
            SetNodeError(LC_ERROR_OPERATION_FAILED, "Failed to instantiate clone type");
            return LC_ERROR_OPERATION_FAILED;
        }

        clone->Deserialize(data);

        lupine::core::Node* parent = nodePtr->GetParent();
        std::shared_ptr<lupine::core::Node> parentShared;
        if (!parent) {
            lupine::core::Scene* scene = nodePtr->GetScene();
            if (scene && scene->GetRoot()) {
                parentShared = scene->GetRoot();
            } else {
                lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
                if (sceneManager) {
                    lupine::core::Scene* current = sceneManager->GetCurrentScene();
                    if (current && current->GetRoot()) {
                        parentShared = current->GetRoot();
                    }
                }
            }
            if (parentShared) {
                parent = parentShared.get();
            }
        }

        if (!parent) {
            SetNodeError(LC_ERROR_OPERATION_FAILED, "No parent or scene root to attach clone");
            return LC_ERROR_OPERATION_FAILED;
        }

        parent->AddChild(clone);
        *out_node = CreateHandle(clone);
        return LC_SUCCESS;

    } catch (...) {
        return LC_ERROR_INTERNAL_ERROR;
    }
}
