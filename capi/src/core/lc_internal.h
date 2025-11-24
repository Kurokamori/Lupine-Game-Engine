#pragma once

#include <core/lc_core.h>
#include <core/lc_node.h>
#include <components/lc_light.h>

#include <memory>

namespace lupine {
namespace core {
    class Node;
    class Component;
}
}

// Shared internal functions for C API implementation

// Error handling
void SetError(LCResult code, const char* message);

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
