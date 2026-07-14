/**
 * @file lc_item_list.cpp
 * @brief Implementation of Lupine Engine C API - ItemList component
 */

#include "ui/lc_item_list.h"
#include "../core/lc_internal.h"

#include <lupine/components/ItemList.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetItemListError(LCResult code, const char* message) {
    ::SetError(code, message);
}

ItemList* GetItemList(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<ItemList*>(component.get());
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

LC_API LCResult lc_item_list_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetItemListError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        auto component = std::make_shared<ItemList>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetItemListError(LC_ERROR_INTERNAL_ERROR, "Failed to create ItemList component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Items ----- */

LC_API LCResult lc_item_list_get_item_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) { SetItemListError(LC_ERROR_NULL_POINTER, "outCount is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCount = il->GetItemCount(); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetItemCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_get_item(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(il->GetItem(index), buffer, bufferSize, outRequired); }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_add_item(LCComponentHandle handle, const char* text) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->AddItem(text ? text : ""); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "AddItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_remove_item(LCComponentHandle handle, int index) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->RemoveItem(index); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "RemoveItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_clear_items(LCComponentHandle handle) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->ClearItems(); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "ClearItems failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_get_selected_index(LCComponentHandle handle, int* outIndex) {
    if (!outIndex) { SetItemListError(LC_ERROR_NULL_POINTER, "outIndex is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outIndex = il->GetSelectedIndex(); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetSelectedIndex failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_set_selected_index(LCComponentHandle handle, int index) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->SetSelectedIndex(index); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "SetSelectedIndex failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Appearance ----- */

LC_API LCResult lc_item_list_get_item_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetItemListError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = il->GetItemHeight(); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetItemHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_set_item_height(LCComponentHandle handle, float height) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->SetItemHeight(height); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "SetItemHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(il->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_set_font_path(LCComponentHandle handle, const char* path) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetItemListError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = il->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_set_font_size(LCComponentHandle handle, float size) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetItemListError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(il->GetFontColor()); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_set_font_color(LCComponentHandle handle, LCColor color) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->SetFontColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "SetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetItemListError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(il->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_get_selection_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetItemListError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(il->GetSelectionColor()); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "GetSelectionColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_item_list_set_selection_color(LCComponentHandle handle, LCColor color) {
    auto* il = GetItemList(handle);
    if (!il) { SetItemListError(LC_ERROR_INVALID_HANDLE, "Invalid ItemList handle"); return LC_ERROR_INVALID_HANDLE; }
    try { il->SetSelectionColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetItemListError(LC_ERROR_INTERNAL_ERROR, "SetSelectionColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}
