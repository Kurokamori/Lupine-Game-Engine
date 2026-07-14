/**
 * @file lc_tree.cpp
 * @brief Implementation of Lupine Engine C API - Tree component
 */

#include "ui/lc_tree.h"
#include "../core/lc_internal.h"

#include <lupine/components/Tree.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetTreeError(LCResult code, const char* message) { ::SetError(code, message); }

Tree* GetTree(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<Tree*>(component.get());
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

LC_API LCResult lc_tree_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) { SetTreeError(LC_ERROR_NULL_POINTER, "outHandle is NULL"); return LC_ERROR_NULL_POINTER; }
    auto nodePtr = GetNode(node);
    if (!nodePtr) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid node handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        auto component = std::make_shared<Tree>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetTreeError(LC_ERROR_INTERNAL_ERROR, "Failed to create Tree component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_tree_get_item_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) { SetTreeError(LC_ERROR_NULL_POINTER, "outCount is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outCount = t->GetItemCount(); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetItemCount failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_item_text(LCComponentHandle handle, int index, char* buffer, int bufferSize, int* outRequired) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(t->GetItemText(index), buffer, bufferSize, outRequired); }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetItemText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_item_depth(LCComponentHandle handle, int index, int* outDepth) {
    if (!outDepth) { SetTreeError(LC_ERROR_NULL_POINTER, "outDepth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDepth = t->GetItemDepth(index); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetItemDepth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_add_item(LCComponentHandle handle, const char* text, int depth) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->AddItem(text ? text : "", depth); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "AddItem failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_clear_items(LCComponentHandle handle) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->ClearItems(); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "ClearItems failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_selected_index(LCComponentHandle handle, int* outIndex) {
    if (!outIndex) { SetTreeError(LC_ERROR_NULL_POINTER, "outIndex is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outIndex = t->GetSelectedIndex(); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetSelectedIndex failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_set_selected_index(LCComponentHandle handle, int index) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->SetSelectedIndex(index); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "SetSelectedIndex failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_is_expanded(LCComponentHandle handle, int index, bool* outExpanded) {
    if (!outExpanded) { SetTreeError(LC_ERROR_NULL_POINTER, "outExpanded is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outExpanded = t->IsExpanded(index); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "IsExpanded failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_set_expanded(LCComponentHandle handle, int index, bool expanded) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->SetExpanded(index, expanded); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "SetExpanded failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_toggle_expand(LCComponentHandle handle, int index) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->ToggleExpand(index); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "ToggleExpand failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_item_height(LCComponentHandle handle, float* outHeight) {
    if (!outHeight) { SetTreeError(LC_ERROR_NULL_POINTER, "outHeight is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outHeight = t->GetItemHeight(); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetItemHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_set_item_height(LCComponentHandle handle, float height) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->SetItemHeight(height); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "SetItemHeight failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_indent_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) { SetTreeError(LC_ERROR_NULL_POINTER, "outWidth is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWidth = t->GetIndentWidth(); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetIndentWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_set_indent_width(LCComponentHandle handle, float width) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->SetIndentWidth(width); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "SetIndentWidth failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(t->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_set_font_path(LCComponentHandle handle, const char* path) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetTreeError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = t->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_set_font_size(LCComponentHandle handle, float size) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetTreeError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(t->GetFontColor()); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_set_font_color(LCComponentHandle handle, LCColor color) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->SetFontColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "SetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_get_selection_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetTreeError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(t->GetSelectionColor()); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "GetSelectionColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_tree_set_selection_color(LCComponentHandle handle, LCColor color) {
    auto* t = GetTree(handle);
    if (!t) { SetTreeError(LC_ERROR_INVALID_HANDLE, "Invalid Tree handle"); return LC_ERROR_INVALID_HANDLE; }
    try { t->SetSelectionColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetTreeError(LC_ERROR_INTERNAL_ERROR, "SetSelectionColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}
