/**
 * @file lc_text_edit.cpp
 * @brief Implementation of Lupine Engine C API - TextEdit component
 */

#include "ui/lc_text_edit.h"
#include "../core/lc_internal.h"

#include <lupine/components/TextEdit.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetTextEditError(LCResult code, const char* message) {
    ::SetError(code, message);
}

TextEdit* GetTextEdit(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<TextEdit*>(component.get());
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
        if (n > 0) {
            std::memcpy(buffer, s.data(), static_cast<size_t>(n));
        }
        buffer[n] = '\0';
    }
    return LC_SUCCESS;
}

} // anonymous namespace

LC_API LCResult lc_text_edit_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetTextEditError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        auto component = std::make_shared<TextEdit>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetTextEditError(LC_ERROR_INTERNAL_ERROR, "Failed to create TextEdit component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Text ----- */

LC_API LCResult lc_text_edit_get_text(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(te->GetText(), buffer, bufferSize, outRequired); }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_text(LCComponentHandle handle, const char* text) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetText(text ? text : ""); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_get_editable(LCComponentHandle handle, bool* outEditable) {
    if (!outEditable) { SetTextEditError(LC_ERROR_NULL_POINTER, "outEditable is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEditable = te->GetEditable(); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetEditable failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_editable(LCComponentHandle handle, bool editable) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetEditable(editable); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetEditable failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Caret / selection ----- */

LC_API LCResult lc_text_edit_get_caret_position(LCComponentHandle handle, int* outPosition) {
    if (!outPosition) { SetTextEditError(LC_ERROR_NULL_POINTER, "outPosition is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPosition = te->GetCaretPosition(); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetCaretPosition failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_caret_position(LCComponentHandle handle, int position) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetCaretPosition(position); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetCaretPosition failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_select_all(LCComponentHandle handle) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SelectAll(); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SelectAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_deselect(LCComponentHandle handle) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->Deselect(); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "Deselect failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Font ----- */

LC_API LCResult lc_text_edit_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(te->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_font_path(LCComponentHandle handle, const char* path) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetTextEditError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = te->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_font_size(LCComponentHandle handle, float size) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_get_line_spacing(LCComponentHandle handle, float* outSpacing) {
    if (!outSpacing) { SetTextEditError(LC_ERROR_NULL_POINTER, "outSpacing is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSpacing = te->GetLineSpacing(); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetLineSpacing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_line_spacing(LCComponentHandle handle, float spacing) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetLineSpacing(spacing); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetLineSpacing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Colors ----- */

LC_API LCResult lc_text_edit_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetTextEditError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(te->GetFontColor()); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_font_color(LCComponentHandle handle, LCColor color) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetFontColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetTextEditError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(te->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_get_selection_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetTextEditError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(te->GetSelectionColor()); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "GetSelectionColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_text_edit_set_selection_color(LCComponentHandle handle, LCColor color) {
    auto* te = GetTextEdit(handle);
    if (!te) { SetTextEditError(LC_ERROR_INVALID_HANDLE, "Invalid TextEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { te->SetSelectionColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetTextEditError(LC_ERROR_INTERNAL_ERROR, "SetSelectionColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}
