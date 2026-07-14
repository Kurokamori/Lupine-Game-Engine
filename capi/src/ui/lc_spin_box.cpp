/**
 * @file lc_spin_box.cpp
 * @brief Implementation of Lupine Engine C API - SpinBox component
 */

#include "ui/lc_spin_box.h"
#include "../core/lc_internal.h"

#include <lupine/components/SpinBox.hpp>
#include <lupine/core/Node.hpp>

#include <cstring>
#include <algorithm>
#include <string>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetSpinBoxError(LCResult code, const char* message) {
    ::SetError(code, message);
}

SpinBox* GetSpinBox(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) return nullptr;
    return dynamic_cast<SpinBox*>(component.get());
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

LC_API LCResult lc_spin_box_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetSpinBoxError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        auto component = std::make_shared<SpinBox>();
        component->RegisterProperties();
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;
    } catch (...) {
        SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "Failed to create SpinBox component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Value ----- */

LC_API LCResult lc_spin_box_get_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = sb->GetValue(); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_value(LCComponentHandle handle, float value) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetValue(value); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_min_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = sb->GetMinValue(); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetMinValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_min_value(LCComponentHandle handle, float value) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetMinValue(value); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetMinValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_max_value(LCComponentHandle handle, float* outValue) {
    if (!outValue) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outValue is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outValue = sb->GetMaxValue(); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetMaxValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_max_value(LCComponentHandle handle, float value) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetMaxValue(value); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetMaxValue failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_step(LCComponentHandle handle, float* outStep) {
    if (!outStep) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outStep is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outStep = sb->GetStep(); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetStep failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_step(LCComponentHandle handle, float step) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetStep(step); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetStep failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_decimals(LCComponentHandle handle, int* outDecimals) {
    if (!outDecimals) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outDecimals is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outDecimals = sb->GetDecimals(); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetDecimals failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_decimals(LCComponentHandle handle, int decimals) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetDecimals(decimals); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetDecimals failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_editable(LCComponentHandle handle, bool* outEditable) {
    if (!outEditable) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outEditable is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outEditable = sb->GetEditable(); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetEditable failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_editable(LCComponentHandle handle, bool editable) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetEditable(editable); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetEditable failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_prefix(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(sb->GetPrefix(), buffer, bufferSize, outRequired); }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetPrefix failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_prefix(LCComponentHandle handle, const char* prefix) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetPrefix(prefix ? prefix : ""); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetPrefix failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_suffix(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(sb->GetSuffix(), buffer, bufferSize, outRequired); }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetSuffix failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_suffix(LCComponentHandle handle, const char* suffix) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetSuffix(suffix ? suffix : ""); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetSuffix failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Font ----- */

LC_API LCResult lc_spin_box_get_font_path(LCComponentHandle handle, char* buffer, int bufferSize, int* outRequired) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { return WriteString(sb->GetFontPath(), buffer, bufferSize, outRequired); }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_font_path(LCComponentHandle handle, const char* path) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetFontPath(path ? path : ""); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetFontPath failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_font_size(LCComponentHandle handle, float* outSize) {
    if (!outSize) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outSize is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outSize = sb->GetFontSize(); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_font_size(LCComponentHandle handle, float size) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetFontSize(size); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetFontSize failed"); return LC_ERROR_INTERNAL_ERROR; }
}

/* ----- Colors ----- */

LC_API LCResult lc_spin_box_get_font_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(sb->GetFontColor()); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_font_color(LCComponentHandle handle, LCColor color) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetFontColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetFontColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_background_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(sb->GetBackgroundColor()); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_background_color(LCComponentHandle handle, LCColor color) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetBackgroundColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetBackgroundColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_get_button_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) { SetSpinBoxError(LC_ERROR_NULL_POINTER, "outColor is NULL"); return LC_ERROR_NULL_POINTER; }
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { *outColor = FromEngineColor(sb->GetButtonColor()); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "GetButtonColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}

LC_API LCResult lc_spin_box_set_button_color(LCComponentHandle handle, LCColor color) {
    auto* sb = GetSpinBox(handle);
    if (!sb) { SetSpinBoxError(LC_ERROR_INVALID_HANDLE, "Invalid SpinBox handle"); return LC_ERROR_INVALID_HANDLE; }
    try { sb->SetButtonColor(ToEngineColor(color)); return LC_SUCCESS; }
    catch (...) { SetSpinBoxError(LC_ERROR_INTERNAL_ERROR, "SetButtonColor failed"); return LC_ERROR_INTERNAL_ERROR; }
}
