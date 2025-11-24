

#include "core/lc_node.h"
#include "core/lc_core.h"

#include <lupine\core\Node.hpp>

#include <unordered_map>
#include <mutex>
#include <memory>

namespace {

std::unordered_map<LCNodeHandle, std::shared_ptr<lupine::core::Node>> g_nodeHandles;
std::mutex g_nodeHandlesMutex;

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

void SetNodeError(LCResult code, const char* message) {
    extern void SetError(LCResult code, const char* message);
    SetError(code, message);
}

}

extern "C" {

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
        strncpy(uuid_buffer, uuid_str.c_str(), sizeof(uuid_buffer) - 1);
        uuid_buffer[sizeof(uuid_buffer) - 1] = '\0';

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
        strncpy(path_buffer, path.c_str(), sizeof(path_buffer) - 1);
        path_buffer[sizeof(path_buffer) - 1] = '\0';

        return path_buffer;

    } catch (...) {
        return nullptr;
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
        for (int i = 0; i < 16; ++i) {
            out_matrix->m[i] = mat.m[i];
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
        for (int i = 0; i < 16; ++i) {
            out_matrix->m[i] = mat.m[i];
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

}
