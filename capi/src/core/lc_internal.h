#pragma once

#include <core/lc_core.h>
#include <core/lc_node.h>
#include <core/lc_scene.h>
#include <components/lc_light.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace lupine {
namespace core {
    class Node;
    class Component;
    class Scene;
}
}

// Shared internal functions for C API implementation

// Error handling
void SetError(LCResult code, const char* message);

// ===== String marshalling =====

// Copies `src` into the `capacity`-byte buffer `dst`, truncating as needed and always
// NUL-terminating. Writes nothing when `dst` is null or `capacity` is 0, so a caller
// passing a zero-sized buffer cannot overflow it. Returns false when `src` did not fit.
inline bool CopyStringToBuffer(char* dst, size_t capacity, const char* src) {
    if (!dst || capacity == 0) {
        return false;
    }
    const size_t srcLength = src ? std::strlen(src) : 0;
    const size_t copyLength = (srcLength < capacity - 1) ? srcLength : capacity - 1;
    if (copyLength > 0) {
        std::memcpy(dst, src, copyLength);
    }
    dst[copyLength] = '\0';
    return srcLength < capacity;
}

inline bool CopyStringToBuffer(char* dst, size_t capacity, const std::string& src) {
    return CopyStringToBuffer(dst, capacity, src.c_str());
}

// Returns a malloc'd NUL-terminated copy of `src` for handing ownership to C callers,
// or nullptr if the allocation fails. Free with free().
inline char* DuplicateString(const std::string& src) {
    char* copy = static_cast<char*>(std::malloc(src.size() + 1));
    if (!copy) {
        return nullptr;
    }
    std::memcpy(copy, src.c_str(), src.size() + 1);
    return copy;
}

// Node handle management
std::shared_ptr<lupine::core::Node> GetNode(LCNodeHandle handle);
LCNodeHandle CreateHandle(std::shared_ptr<lupine::core::Node> node);
bool IsValidHandle(LCNodeHandle handle);
void DestroyHandle(LCNodeHandle handle);

// Component handle management
std::shared_ptr<lupine::core::Component> GetComponent(LCComponentHandle handle);
LCComponentHandle CreateComponentHandle(std::shared_ptr<lupine::core::Component> component);
bool IsValidComponentHandle(LCComponentHandle handle);
void DestroyComponentHandle(LCComponentHandle handle);

// Scene handle management
// Registers a non-owning handle for an engine-owned scene (e.g. a scene reached
// through Node::GetScene) so it can be used with the lc_scene_* API. Returns an
// existing handle when one is already registered for the scene.
LCSceneHandle CreateSceneHandleNonOwning(lupine::core::Scene* scene);
