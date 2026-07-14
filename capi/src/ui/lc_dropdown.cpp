/**
 * @file lc_dropdown.cpp
 * @brief Implementation of Lupine Engine C API - Dropdown component
 */

#include "ui/lc_dropdown.h"
#include "../core/lc_internal.h"

#include <lupine/components/Dropdown.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetDropdownError(LCResult code, const char* message) {
    ::SetError(code, message);
}

Dropdown* GetDropdown(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<Dropdown*>(component.get());
}

LCColor FromEngineColor(const Color& color) { return LCColor{color.r, color.g, color.b, color.a}; }
Color ToEngineColor(const LCColor& color) { return Color(color.r, color.g, color.b, color.a); }

LCResult WriteString(const std::string& s, char* buffer, int bufferSize, int* outRequired) {
    if (outRequired) *outRequired = static_cast<int>(s.size());
    if (buffer && bufferSize > 0) {
        int n = std::min(static_cast<int>(s.size()), bufferSize - 1);
        if (n > 0) std::memcpy(buffer, s.data(), static_cast<size_t>(n));
        buffer[n] = '\0';
    }
    return LC_SUCCESS;
}

} // anonymous namespace

LC_API LCResult lc_dropdown_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) { SetDropdownError(LC_ERROR_NULL_POINTER, "outHandle is NULL"); return LC_ERROR_NULL_POINTER; }
    auto nodePtr = GetNode(node);
    if (!nodePtr) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid node handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        auto component = std::make_shared<Dropdown>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetDropdownError(LC_ERROR_INTERNAL_ERROR, "Failed to create Dropdown component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_dropdown_get_item_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) { SetDropdownError(LC_ERROR_NULL_POINTER, "outCount is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCount = d->GetItemCount(); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetItemCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_item(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(d->GetItem(index), buffer, bufferSize, outRequired); }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_add_item(LCComponentHandle handle, const char* text) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->AddItem(text ? text : ""); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "AddItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_remove_item(LCComponentHandle handle, int index) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->RemoveItem(index); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "RemoveItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_clear_items(LCComponentHandle handle) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->ClearItems(); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "ClearItems failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_selected_index(LCComponentHandle handle, int* outIndex) {
    if (!outIndex) { SetDropdownError(LC_ERROR_NULL_POINTER, "outIndex is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outIndex = d->GetSelectedIndex(); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetSelectedIndex failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_set_selected_index(LCComponentHandle handle, int index) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->SetSelectedIndex(index); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "SetSelectedIndex failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_selected_text(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(d->GetSelectedText(), buffer, bufferSize, outRequired); }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetSelectedText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_placeholder(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(d->GetPlaceholder(), buffer, bufferSize, outRequired); }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetPlaceholder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_set_placeholder(LCComponentHandle handle, const char* text) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->SetPlaceholder(text ? text : ""); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "SetPlaceholder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_item_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetDropdownError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = d->GetItemHeight(); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetItemHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_set_item_height(LCComponentHandle handle, float height) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->SetItemHeight(height); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "SetItemHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(d->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_set_font_path(LCComponentHandle handle, const char* path) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetDropdownError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = d->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_set_font_size(LCComponentHandle handle, float size) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetDropdownError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(d->GetFontColor()); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_set_font_color(LCComponentHandle handle, LCColor color) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->SetFontColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "SetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetDropdownError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(d->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_dropdown_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* d = GetDropdown(handle);
    if (!d) { SetDropdownError(LC_ERROR_INVALID_HANDLE, "Invalid Dropdown handle"); return LC_ERROR_INVALID_HANDLE; }
    try { d->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetDropdownError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}
