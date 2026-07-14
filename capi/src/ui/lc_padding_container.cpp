/**
 * @file lc_padding_container.cpp
 * @brief Implementation of PaddingContainer C API
 */

#include "ui/lc_padding_container.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/PaddingContainer.hpp>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

PaddingContainer::Alignment ToEngineAlignment(LCPaddingAlignment alignment) {
    return static_cast<PaddingContainer::Alignment>(alignment);
}

LCPaddingAlignment FromEngineAlignment(PaddingContainer::Alignment alignment) {
    return static_cast<LCPaddingAlignment>(alignment);
}

PaddingContainer* GetPaddingContainer(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<PaddingContainer*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * PaddingContainer Creation
 * ============================================================================ */

LC_API LCResult lc_padding_container_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<PaddingContainer>(name) : std::make_shared<PaddingContainer>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create PaddingContainer component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * PaddingContainer Properties
 * ============================================================================ */

LC_API LCResult lc_padding_container_get_auto_fit_children(LCComponentHandle component, bool* out_auto_fit) {
    if (!out_auto_fit) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_auto_fit = container->GetAutoFitChildren();
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_auto_fit_children(LCComponentHandle component, bool auto_fit) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetAutoFitChildren(auto_fit);
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_get_maintain_aspect_ratio(LCComponentHandle component, bool* out_maintain) {
    if (!out_maintain) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_maintain = container->GetMaintainAspectRatio();
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_maintain_aspect_ratio(LCComponentHandle component, bool maintain) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetMaintainAspectRatio(maintain);
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_get_child_alignment(LCComponentHandle component, LCPaddingAlignment* out_alignment) {
    if (!out_alignment) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_alignment = FromEngineAlignment(container->GetChildAlignment());
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_child_alignment(LCComponentHandle component, LCPaddingAlignment alignment) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetChildAlignment(ToEngineAlignment(alignment));
    return LC_SUCCESS;
}

/* ============================================================================
 * Container Size
 * ============================================================================ */

LC_API LCResult lc_padding_container_get_width(LCComponentHandle component, float* out_width) {
    if (!out_width) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_width = container->GetWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_width(LCComponentHandle component, float width) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetWidth(width);
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_get_height(LCComponentHandle component, float* out_height) {
    if (!out_height) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_height = container->GetHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_height(LCComponentHandle component, float height) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetHeight(height);
    return LC_SUCCESS;
}

/* ============================================================================
 * Padding Properties
 * ============================================================================ */

LC_API LCResult lc_padding_container_get_padding(LCComponentHandle component, LCVec4* out_padding) {
    if (!out_padding) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    auto padding = container->GetPadding();
    out_padding->x = padding.x;
    out_padding->y = padding.y;
    out_padding->z = padding.z;
    out_padding->w = padding.w;
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_padding(LCComponentHandle component, LCVec4 padding) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetPadding(math::Vec4(padding.x, padding.y, padding.z, padding.w));
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_padding_uniform(LCComponentHandle component, float padding) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetPadding(math::Vec4(padding, padding, padding, padding));
    return LC_SUCCESS;
}

/* ============================================================================
 * Rendering Properties
 * ============================================================================ */

LC_API LCResult lc_padding_container_get_layer(LCComponentHandle component, int* out_layer) {
    if (!out_layer) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_layer = container->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_layer(LCComponentHandle component, int layer) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_get_sorting_order(LCComponentHandle component, int* out_order) {
    if (!out_order) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_order = container->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_sorting_order(LCComponentHandle component, int order) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetSortingOrder(order);
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_get_use_ui_space(LCComponentHandle component, bool* out_use_ui_space) {
    if (!out_use_ui_space) return LC_ERROR_NULL_POINTER;
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    *out_use_ui_space = container->GetUseUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_padding_container_set_use_ui_space(LCComponentHandle component, bool use_ui_space) {
    auto container = GetPaddingContainer(component);
    if (!container) return LC_ERROR_INVALID_HANDLE;
    container->SetUseUISpace(use_ui_space);
    return LC_SUCCESS;
}
