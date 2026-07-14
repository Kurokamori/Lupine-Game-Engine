/**
 * @file lc_popup_menu.cpp
 * @brief Implementation of Lupine Engine C API - PopupMenu component
 */

#include "ui/lc_popup_menu.h"
#include "../core/lc_internal.h"

#include <lupine/components/PopupMenu.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetPopupError(LCResult code, const char* message) {
    ::SetError(code, message);
}

PopupMenu* GetPopupMenu(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<PopupMenu*>(component.get());
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

LC_API LCResult lc_popup_menu_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) { SetPopupError(LC_ERROR_NULL_POINTER, "outHandle is NULL"); return LC_ERROR_NULL_POINTER; }
    auto nodePtr = GetNode(node);
    if (!nodePtr) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid node handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        auto component = std::make_shared<PopupMenu>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetPopupError(LC_ERROR_INTERNAL_ERROR, "Failed to create PopupMenu component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_popup_menu_get_item_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) { SetPopupError(LC_ERROR_NULL_POINTER, "outCount is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCount = p->GetItemCount(); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "GetItemCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_get_item(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(p->GetItem(index), buffer, bufferSize, outRequired); }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "GetItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_add_item(LCComponentHandle handle, const char* text) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->AddItem(text ? text : ""); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "AddItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_remove_item(LCComponentHandle handle, int index) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->RemoveItem(index); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "RemoveItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_clear_items(LCComponentHandle handle) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->ClearItems(); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "ClearItems failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_popup(LCComponentHandle handle) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->Popup(); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "Popup failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_popup_at(LCComponentHandle handle, LCVec2 position) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->PopupAt(Vec2(position.x, position.y)); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "PopupAt failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_close(LCComponentHandle handle) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->Close(); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "Close failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_is_open(LCComponentHandle handle, bool* outOpen) {
    if (!outOpen) { SetPopupError(LC_ERROR_NULL_POINTER, "outOpen is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outOpen = p->IsOpen(); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "IsOpen failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_get_item_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetPopupError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = p->GetItemHeight(); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "GetItemHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_set_item_height(LCComponentHandle handle, float height) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->SetItemHeight(height); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "SetItemHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(p->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_set_font_path(LCComponentHandle handle, const char* path) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetPopupError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = p->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_set_font_size(LCComponentHandle handle, float size) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetPopupError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(p->GetFontColor()); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "GetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_set_font_color(LCComponentHandle handle, LCColor color) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->SetFontColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "SetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetPopupError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(p->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_popup_menu_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* p = GetPopupMenu(handle);
    if (!p) { SetPopupError(LC_ERROR_INVALID_HANDLE, "Invalid PopupMenu handle"); return LC_ERROR_INVALID_HANDLE; }
    try { p->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetPopupError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}
