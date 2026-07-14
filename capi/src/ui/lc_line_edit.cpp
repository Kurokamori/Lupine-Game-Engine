/**
 * @file lc_line_edit.cpp
 * @brief Implementation of Lupine Engine C API - LineEdit component
 */

#include "ui/lc_line_edit.h"
#include "../core/lc_internal.h"

#include <lupine/components/LineEdit.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetLineEditError(LCResult code, const char* message) {
    ::SetError(code, message);
}

LineEdit* GetLineEdit(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<LineEdit*>(component.get());
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

LC_API LCResult lc_line_edit_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetLineEditError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        auto component = std::make_shared<LineEdit>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetLineEditError(LC_ERROR_INTERNAL_ERROR, "Failed to create LineEdit component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Text ----- */

LC_API LCResult lc_line_edit_get_text(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(le->GetText(), buffer, bufferSize, outRequired); }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_text(LCComponentHandle handle, const char* text) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetText(text ? text : ""); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_get_placeholder(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(le->GetPlaceholder(), buffer, bufferSize, outRequired); }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetPlaceholder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_placeholder(LCComponentHandle handle, const char* text) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetPlaceholder(text ? text : ""); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetPlaceholder failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_get_editable(LCComponentHandle handle, bool* outEditable) {
    if (!outEditable) { SetLineEditError(LC_ERROR_NULL_POINTER, "outEditable is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEditable = le->GetEditable(); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetEditable failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_editable(LCComponentHandle handle, bool editable) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetEditable(editable); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetEditable failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_get_secret(LCComponentHandle handle, bool* outSecret) {
    if (!outSecret) { SetLineEditError(LC_ERROR_NULL_POINTER, "outSecret is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSecret = le->GetSecret(); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetSecret failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_secret(LCComponentHandle handle, bool secret) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetSecret(secret); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetSecret failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_get_max_length(LCComponentHandle handle, int* outMaxLength) {
    if (!outMaxLength) { SetLineEditError(LC_ERROR_NULL_POINTER, "outMaxLength is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outMaxLength = le->GetMaxLength(); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetMaxLength failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_max_length(LCComponentHandle handle, int maxLength) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetMaxLength(maxLength); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetMaxLength failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Caret / selection ----- */

LC_API LCResult lc_line_edit_get_caret_position(LCComponentHandle handle, int* outPosition) {
    if (!outPosition) { SetLineEditError(LC_ERROR_NULL_POINTER, "outPosition is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outPosition = le->GetCaretPosition(); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetCaretPosition failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_caret_position(LCComponentHandle handle, int position) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetCaretPosition(position); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetCaretPosition failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_select_all(LCComponentHandle handle) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SelectAll(); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SelectAll failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_deselect(LCComponentHandle handle) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->Deselect(); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "Deselect failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Font ----- */

LC_API LCResult lc_line_edit_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(le->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_font_path(LCComponentHandle handle, const char* path) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetLineEditError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = le->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_font_size(LCComponentHandle handle, float size) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Colors ----- */

LC_API LCResult lc_line_edit_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetLineEditError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(le->GetFontColor()); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_font_color(LCComponentHandle handle, LCColor color) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetFontColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetLineEditError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(le->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_get_selection_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetLineEditError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(le->GetSelectionColor()); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "GetSelectionColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_line_edit_set_selection_color(LCComponentHandle handle, LCColor color) {
    auto* le = GetLineEdit(handle);
    if (!le) { SetLineEditError(LC_ERROR_INVALID_HANDLE, "Invalid LineEdit handle"); return LC_ERROR_INVALID_HANDLE; }
    try { le->SetSelectionColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetLineEditError(LC_ERROR_INTERNAL_ERROR, "SetSelectionColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}
