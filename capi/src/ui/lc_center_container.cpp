/**
 * @file lc_center_container.cpp
 * @brief Implementation of CenterContainer C API
 */

#include "ui/lc_center_container.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/CenterContainer.hpp>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

CenterContainer* GetCenterContainer(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<CenterContainer*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * CenterContainer Creation
 * ============================================================================ */

LC_API LCResult lc_center_container_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<CenterContainer>(name) : std::make_shared<CenterContainer>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create CenterContainer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * CenterContainer Properties
 * ============================================================================ */

LC_API LCResult lc_center_container_get_auto_fit_child(LCComponentHandle component, bool* out_auto_fit) {
    if (!out_auto_fit) return LC_ERROR_NULL_POINTER;
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_auto_fit = container->GetAutoFitChild();
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_set_auto_fit_child(LCComponentHandle component, bool auto_fit) {
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetAutoFitChild(auto_fit);
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_get_maintain_aspect_ratio(LCComponentHandle component, bool* out_maintain) {
    if (!out_maintain) return LC_ERROR_NULL_POINTER;
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_maintain = container->GetMaintainAspectRatio();
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_set_maintain_aspect_ratio(LCComponentHandle component, bool maintain) {
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetMaintainAspectRatio(maintain);
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_get_stack_children(LCComponentHandle component, bool* out_stack) {
    if (!out_stack) return LC_ERROR_NULL_POINTER;
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_stack = container->GetStackChildren();
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_set_stack_children(LCComponentHandle component, bool stack) {
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetStackChildren(stack);
    return LC_SUCCESS;
}

/* ============================================================================
 * Container Size
 * ============================================================================ */

LC_API LCResult lc_center_container_get_width(LCComponentHandle component, float* out_width) {
    if (!out_width) return LC_ERROR_NULL_POINTER;
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_width = container->GetWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_set_width(LCComponentHandle component, float width) {
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetWidth(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_get_height(LCComponentHandle component, float* out_height) {
    if (!out_height) return LC_ERROR_NULL_POINTER;
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_height = container->GetHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_set_height(LCComponentHandle component, float height) {
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetHeight(height);
    return LC_SUCCESS;
}

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

LC_API LCResult lc_center_container_get_layer(LCComponentHandle component, int* out_layer) {
    if (!out_layer) return LC_ERROR_NULL_POINTER;
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_layer = container->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_set_layer(LCComponentHandle component, int layer) {
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_get_sorting_order(LCComponentHandle component, int* out_order) {
    if (!out_order) return LC_ERROR_NULL_POINTER;
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_order = container->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_set_sorting_order(LCComponentHandle component, int order) {
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetSortingOrder(order);
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space) {
    if (!out_use_ui_space) return LC_ERROR_NULL_POINTER;
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_use_ui_space = container->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_center_container_set_use_ui_space(LCComponentHandle component, bool use_ui_space) {
    auto container = GetCenterContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetUseUISpace(use_ui_space);
    return LC_SUCCESS;
}
