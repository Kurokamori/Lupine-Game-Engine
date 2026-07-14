/**
 * @file lc_rich_text_label.cpp
 * @brief Implementation of Lupine Engine C API - RichTextLabel component
 */

#include "ui/lc_rich_text_label.h"
#include "../core/lc_internal.h"

#include <lupine/components/RichTextLabel.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetRichError(LCResult code, const char* message) { ::SetError(code, message); }

RichTextLabel* GetRich(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<RichTextLabel*>(component.get());
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

LC_API LCResult lc_rich_text_label_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) { SetRichError(LC_ERROR_NULL_POINTER, "outHandle is NULL"); return LC_ERROR_NULL_POINTER; }
    auto nodePtr = GetNode(node);
    if (!nodePtr) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid node handle"); return LC_ERROR_INVALID_HANDLE; }
    try {
        auto component = std::make_shared<RichTextLabel>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetRichError(LC_ERROR_INTERNAL_ERROR, "Failed to create RichTextLabel component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rich_text_label_get_text(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(r->GetText(), buffer, bufferSize, outRequired); }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "GetText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_set_text(LCComponentHandle handle, const char* text) {
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { r->SetText(text ? text : ""); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "SetText failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(r->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_set_font_path(LCComponentHandle handle, const char* path) {
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { r->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetRichError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = r->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_set_font_size(LCComponentHandle handle, float size) {
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { r->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_get_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetRichError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(r->GetColor()); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "GetColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_set_color(LCComponentHandle handle, LCColor color) {
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { r->SetColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "SetColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_get_word_wrap(LCComponentHandle handle, bool* outWrap) {
    if (!outWrap) { SetRichError(LC_ERROR_NULL_POINTER, "outWrap is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outWrap = r->GetWordWrap(); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "GetWordWrap failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_set_word_wrap(LCComponentHandle handle, bool wrap) {
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { r->SetWordWrap(wrap); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "SetWordWrap failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_get_line_spacing(LCComponentHandle handle, float* outSpacing) {
    if (!outSpacing) { SetRichError(LC_ERROR_NULL_POINTER, "outSpacing is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSpacing = r->GetLineSpacing(); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "GetLineSpacing failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_rich_text_label_set_line_spacing(LCComponentHandle handle, float spacing) {
    auto* r = GetRich(handle);
    if (!r) { SetRichError(LC_ERROR_INVALID_HANDLE, "Invalid RichTextLabel handle"); return LC_ERROR_INVALID_HANDLE; }
    try { r->SetLineSpacing(spacing); return LC_SUCCESS; }
    catch (...) { SetRichError(LC_ERROR_INTERNAL_ERROR, "SetLineSpacing failed"); return LC_ERROR_INTERNAL_ERROR; }
}
