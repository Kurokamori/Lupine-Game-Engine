/**
 * @file lc_checklist.cpp
 * @brief Implementation of CheckList C API
 */

#include "ui/lc_checklist.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/CheckList.hpp>
#include <cstring>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

CheckListOrientation ToEngineOrientation(LCCheckListOrientation orientation) {
    return static_cast<CheckListOrientation>(orientation);
}

LCCheckListOrientation FromEngineOrientation(CheckListOrientation orientation) {
    return static_cast<LCCheckListOrientation>(orientation);
}

CheckList* GetCheckList(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<CheckList*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * CheckList Creation
 * ============================================================================ */

LC_API LCResult lc_checklist_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<CheckList>(name) : std::make_shared<CheckList>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create CheckList component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Group Properties
 * ============================================================================ */

LC_API LCResult lc_checklist_get_group_name(LCComponentHandle component, char* out_buffer, size_t buffer_size) {
    if (!out_buffer) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;

    std::string groupName = checklist->GetGroupName();
    CopyStringToBuffer(out_buffer, buffer_size, groupName.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_set_group_name(LCComponentHandle component, const char* group_name) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->SetGroupName(group_name ? group_name : "");
    return LC_SUCCESS;
}

/* ============================================================================
 * Layout Properties
 * ============================================================================ */

LC_API LCResult lc_checklist_get_orientation(LCComponentHandle component, LCCheckListOrientation* out_orientation) {
    if (!out_orientation) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_orientation = FromEngineOrientation(checklist->GetOrientation());
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_set_orientation(LCComponentHandle component, LCCheckListOrientation orientation) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->SetOrientation(ToEngineOrientation(orientation));
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_get_spacing(LCComponentHandle component, float* out_spacing) {
    if (!out_spacing) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_spacing = checklist->GetSpacing();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_set_spacing(LCComponentHandle component, float spacing) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->SetSpacing(spacing);
    return LC_SUCCESS;
}

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

LC_API LCResult lc_checklist_get_layer(LCComponentHandle component, int* out_layer) {
    if (!out_layer) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_layer = checklist->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_set_layer(LCComponentHandle component, int layer) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_get_sorting_order(LCComponentHandle component, int* out_order) {
    if (!out_order) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_order = checklist->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_set_sorting_order(LCComponentHandle component, int order) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->SetSortingOrder(order);
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space) {
    if (!out_use_ui_space) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_use_ui_space = checklist->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_set_use_ui_space(LCComponentHandle component, bool use_ui_space) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->SetUseUISpace(use_ui_space);
    return LC_SUCCESS;
}

/* ============================================================================
 * Group Operations
 * ============================================================================ */

LC_API LCResult lc_checklist_check_all(LCComponentHandle component) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->CheckAll();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_uncheck_all(LCComponentHandle component) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->UncheckAll();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_toggle_all(LCComponentHandle component) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->ToggleAll();
    return LC_SUCCESS;
}

/* ============================================================================
 * State Queries
 * ============================================================================ */

LC_API LCResult lc_checklist_are_all_checked(LCComponentHandle component, bool* out_all_checked) {
    if (!out_all_checked) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_all_checked = checklist->AreAllChecked();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_are_all_unchecked(LCComponentHandle component, bool* out_all_unchecked) {
    if (!out_all_unchecked) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_all_unchecked = checklist->AreAllUnchecked();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_is_any_checked(LCComponentHandle component, bool* out_any_checked) {
    if (!out_any_checked) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_any_checked = checklist->IsAnyChecked();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_get_checked_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_count = checklist->GetCheckedCount();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_get_total_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_count = checklist->GetTotalCount();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_get_checked_values(LCComponentHandle component, int* out_values, int max_values, int* out_count) {
    if (!out_values || !out_count) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;

    std::vector<int> values = checklist->GetCheckedValues();
    int count = static_cast<int>(std::min(static_cast<size_t>(max_values), values.size()));
    for (int i = 0; i < count; i++) {
        out_values[i] = values[i];
    }
    *out_count = count;
    return LC_SUCCESS;
}

/* ============================================================================
 * Item Management
 * ============================================================================ */

LC_API LCResult lc_checklist_add_item(LCComponentHandle component, const char* text, int value) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->AddItem(text ? text : "", value);
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_remove_item(LCComponentHandle component, int index) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->RemoveItem(index);
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_clear_items(LCComponentHandle component) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->ClearItems();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_get_item_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_count = checklist->GetItemCount();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_get_auto_create_checkboxes(LCComponentHandle component, bool* out_auto_create) {
    if (!out_auto_create) return LC_ERROR_NULL_POINTER;
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    *out_auto_create = checklist->GetAutoCreateCheckboxes();
    return LC_SUCCESS;
}

LC_API LCResult lc_checklist_set_auto_create_checkboxes(LCComponentHandle component, bool auto_create) {
    auto checklist = GetCheckList(component);
    if (!checklist) return LC_ERROR_INVALID_HANDLE;
    checklist->SetAutoCreateCheckboxes(auto_create);
    return LC_SUCCESS;
}
