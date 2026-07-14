/**
 * @file lc_radiolist.cpp
 * @brief Implementation of RadioList C API
 */

#include "ui/lc_radiolist.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/RadioList.hpp>
#include <cstring>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

RadioList::Orientation ToEngineOrientation(LCRadioListOrientation orientation) {
    return static_cast<RadioList::Orientation>(orientation);
}

LCRadioListOrientation FromEngineOrientation(RadioList::Orientation orientation) {
    return static_cast<LCRadioListOrientation>(orientation);
}

RadioList* GetRadioList(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<RadioList*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * RadioList Creation
 * ============================================================================ */

LC_API LCResult lc_radiolist_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<RadioList>(name) : std::make_shared<RadioList>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create RadioList component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Group Properties
 * ============================================================================ */

LC_API LCResult lc_radiolist_get_group_name(LCComponentHandle component, char* out_buffer, size_t buffer_size) {
    if (!out_buffer) return LC_ERROR_NULL_POINTER;
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;

    std::string groupName = radiolist->GetGroupName();
    CopyStringToBuffer(out_buffer, buffer_size, groupName.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_set_group_name(LCComponentHandle component, const char* group_name) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->SetGroupName(group_name ? group_name : "");
    return LC_SUCCESS;
}

/* ============================================================================
 * Selection Management
 * ============================================================================ */

LC_API LCResult lc_radiolist_get_selected_index(LCComponentHandle component, int* out_index) {
    if (!out_index) return LC_ERROR_NULL_POINTER;
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    *out_index = radiolist->GetSelectedIndex();
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_set_selected_index(LCComponentHandle component, int index) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->SetSelectedIndex(index);
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_get_selected_value(LCComponentHandle component, int* out_value) {
    if (!out_value) return LC_ERROR_NULL_POINTER;
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    *out_value = radiolist->GetSelectedValue();
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_set_selected_value(LCComponentHandle component, int value) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->SetSelectedValue(value);
    return LC_SUCCESS;
}

/* ============================================================================
 * Layout Properties
 * ============================================================================ */

LC_API LCResult lc_radiolist_get_orientation(LCComponentHandle component, LCRadioListOrientation* out_orientation) {
    if (!out_orientation) return LC_ERROR_NULL_POINTER;
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    *out_orientation = FromEngineOrientation(radiolist->GetOrientation());
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_set_orientation(LCComponentHandle component, LCRadioListOrientation orientation) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->SetOrientation(ToEngineOrientation(orientation));
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_get_spacing(LCComponentHandle component, float* out_spacing) {
    if (!out_spacing) return LC_ERROR_NULL_POINTER;
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    *out_spacing = radiolist->GetSpacing();
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_set_spacing(LCComponentHandle component, float spacing) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->SetSpacing(spacing);
    return LC_SUCCESS;
}

/* ============================================================================
 * Item Management
 * ============================================================================ */

LC_API LCResult lc_radiolist_add_item(LCComponentHandle component, const char* text) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->AddItem(text ? text : "");
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_remove_item(LCComponentHandle component, int index) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->RemoveItem(index);
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_clear_items(LCComponentHandle component) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->ClearItems();
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_get_item_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    *out_count = radiolist->GetItemCount();
    return LC_SUCCESS;
}

/* ============================================================================
 * Auto-Create Settings
 * ============================================================================ */

LC_API LCResult lc_radiolist_get_auto_create_buttons(LCComponentHandle component, bool* out_auto_create) {
    if (!out_auto_create) return LC_ERROR_NULL_POINTER;
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    *out_auto_create = radiolist->GetAutoCreateButtons();
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_set_auto_create_buttons(LCComponentHandle component, bool auto_create) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->SetAutoCreateButtons(auto_create);
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_get_manage_layout(LCComponentHandle component, bool* out_manage) {
    if (!out_manage) return LC_ERROR_NULL_POINTER;
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    *out_manage = radiolist->GetManageLayout();
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_set_manage_layout(LCComponentHandle component, bool manage) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->SetManageLayout(manage);
    return LC_SUCCESS;
}

LC_API LCResult lc_radiolist_regenerate_buttons(LCComponentHandle component) {
    auto radiolist = GetRadioList(component);
    if (!radiolist) return LC_ERROR_INVALID_HANDLE;
    radiolist->RegenerateRadioButtons();
    return LC_SUCCESS;
}
