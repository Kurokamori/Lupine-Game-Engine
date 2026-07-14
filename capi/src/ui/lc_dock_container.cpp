/**
 * @file lc_dock_container.cpp
 * @brief Implementation of DockContainer C API
 */

#include "ui/lc_dock_container.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/DockContainer.hpp>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

DockContainer::DockSide ToEngineDockSide(LCDockSide side) {
    return static_cast<DockContainer::DockSide>(side);
}

LCDockSide FromEngineDockSide(DockContainer::DockSide side) {
    return static_cast<LCDockSide>(side);
}

DockContainer* GetDockContainer(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<DockContainer*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * DockContainer Creation
 * ============================================================================ */

LC_API LCResult lc_dock_container_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<DockContainer>(name) : std::make_shared<DockContainer>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create DockContainer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * DockContainer Properties
 * ============================================================================ */

LC_API LCResult lc_dock_container_get_dock_spacing(LCComponentHandle component, float* out_spacing) {
    if (!out_spacing) return LC_ERROR_NULL_POINTER;
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_spacing = container->GetDockSpacing();
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_set_dock_spacing(LCComponentHandle component, float spacing) {
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetDockSpacing(spacing);
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_get_default_dock_side(LCComponentHandle component, LCDockSide* out_side) {
    if (!out_side) return LC_ERROR_NULL_POINTER;
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_side = FromEngineDockSide(container->GetDefaultDockSide());
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_set_default_dock_side(LCComponentHandle component, LCDockSide side) {
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetDefaultDockSide(ToEngineDockSide(side));
    return LC_SUCCESS;
}

/* ============================================================================
 * Container Size
 * ============================================================================ */

LC_API LCResult lc_dock_container_get_width(LCComponentHandle component, float* out_width) {
    if (!out_width) return LC_ERROR_NULL_POINTER;
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_width = container->GetWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_set_width(LCComponentHandle component, float width) {
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetWidth(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_get_height(LCComponentHandle component, float* out_height) {
    if (!out_height) return LC_ERROR_NULL_POINTER;
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_height = container->GetHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_set_height(LCComponentHandle component, float height) {
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetHeight(height);
    return LC_SUCCESS;
}

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

LC_API LCResult lc_dock_container_get_layer(LCComponentHandle component, int* out_layer) {
    if (!out_layer) return LC_ERROR_NULL_POINTER;
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_layer = container->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_set_layer(LCComponentHandle component, int layer) {
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_get_sorting_order(LCComponentHandle component, int* out_order) {
    if (!out_order) return LC_ERROR_NULL_POINTER;
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_order = container->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_set_sorting_order(LCComponentHandle component, int order) {
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetSortingOrder(order);
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space) {
    if (!out_use_ui_space) return LC_ERROR_NULL_POINTER;
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_use_ui_space = container->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_dock_container_set_use_ui_space(LCComponentHandle component, bool use_ui_space) {
    auto container = GetDockContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetUseUISpace(use_ui_space);
    return LC_SUCCESS;
}
