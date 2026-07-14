/**
 * @file lc_tab_container.cpp
 * @brief Implementation of Lupine Engine C API - TabContainer component
 */

#include "ui/lc_tab_container.h"
#include "../core/lc_internal.h"

#include <lupine/components/TabContainer.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetTabError(LCResult code, const char* message) {
    ::SetError(code, message);
}

TabContainer* GetTabContainer(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<TabContainer*>(component.get());
}

LCColor FromEngineColor(const Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

Color ToEngineColor(const LCColor& color) {
    return Color(color.r, color.g, color.b, color.a);
}

LCResult WriteString(const std::string& s, char* buffer, int bufferSize, int* outRequired) {
    if (outRequired) {
        *outRequired = static_cast<int>(s.size());
    }
    if (buffer && bufferSize > 0) {
        int n = std::min(static_cast<int>(s.size()), bufferSize - 1);
        if (n > 0) std::memcpy(buffer, s.data(), static_cast<size_t>(n));
        buffer[n] = '\0';
    }
    return LC_SUCCESS;
}

} // anonymous namespace

LC_API LCResult lc_tab_container_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetTabError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        auto component = std::make_shared<TabContainer>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetTabError(LC_ERROR_INTERNAL_ERROR, "Failed to create TabContainer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_tab_container_get_current_tab(LCComponentHandle handle, int* outIndex) {
    if (!outIndex) { SetTabError(LC_ERROR_NULL_POINTER, "outIndex is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outIndex = tc->GetCurrentTab(); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "GetCurrentTab failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_set_current_tab(LCComponentHandle handle, int index) {
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { tc->SetCurrentTab(index); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "SetCurrentTab failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_get_tab_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) { SetTabError(LC_ERROR_NULL_POINTER, "outCount is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCount = tc->GetTabCount(); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "GetTabCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_get_tab_title(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired) {
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(tc->GetTabTitle(index), buffer, bufferSize, outRequired); }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "GetTabTitle failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_get_tab_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetTabError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = tc->GetTabHeight(); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "GetTabHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_set_tab_height(LCComponentHandle handle, float height) {
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { tc->SetTabHeight(height); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "SetTabHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(tc->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_set_font_path(LCComponentHandle handle, const char* path) {
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { tc->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetTabError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = tc->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_set_font_size(LCComponentHandle handle, float size) {
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { tc->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetTabError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(tc->GetFontColor()); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "GetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_set_font_color(LCComponentHandle handle, LCColor color) {
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { tc->SetFontColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "SetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_get_active_tab_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetTabError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(tc->GetActiveTabColor()); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "GetActiveTabColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tab_container_set_active_tab_color(LCComponentHandle handle, LCColor color) {
    auto* tc = GetTabContainer(handle);
    if (!tc) { SetTabError(LC_ERROR_INVALID_HANDLE, "Invalid TabContainer handle"); return LC_ERROR_INVALID_HANDLE; }
    try { tc->SetActiveTabColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetTabError(LC_ERROR_INTERNAL_ERROR, "SetActiveTabColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}
