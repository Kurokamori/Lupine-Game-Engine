/**
 * @file lc_layout_slot.cpp
 * @brief Implementation of LayoutSlot C API
 */

#include "ui/lc_layout_slot.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/LayoutSlot.hpp>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

LayoutSlot* GetLayoutSlot(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<LayoutSlot*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * Creation
 * ============================================================================ */

LC_API LCResult lc_layout_slot_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<LayoutSlot>(name) : std::make_shared<LayoutSlot>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create LayoutSlot component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Dock
 * ============================================================================ */

LC_API LCResult lc_layout_slot_get_dock_side(LCComponentHandle component, LCLayoutSlotDockSide* out_side) {
    if (!out_side) return LC_ERROR_NULL_POINTER;
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    *out_side = static_cast<LCLayoutSlotDockSide>(slot->GetDockSide());
    return LC_SUCCESS;
}

LC_API LCResult lc_layout_slot_set_dock_side(LCComponentHandle component, LCLayoutSlotDockSide side) {
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    slot->SetDockSide(static_cast<LayoutSlot::DockSide>(side));
    return LC_SUCCESS;
}

/* ============================================================================
 * Stack
 * ============================================================================ */

LC_API LCResult lc_layout_slot_get_alignment(LCComponentHandle component, LCLayoutSlotAlignment* out_alignment) {
    if (!out_alignment) return LC_ERROR_NULL_POINTER;
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    *out_alignment = static_cast<LCLayoutSlotAlignment>(slot->GetAlignment());
    return LC_SUCCESS;
}

LC_API LCResult lc_layout_slot_set_alignment(LCComponentHandle component, LCLayoutSlotAlignment alignment) {
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    slot->SetAlignment(static_cast<LayoutSlot::Alignment>(alignment));
    return LC_SUCCESS;
}

LC_API LCResult lc_layout_slot_get_z_index(LCComponentHandle component, int* out_z_index) {
    if (!out_z_index) return LC_ERROR_NULL_POINTER;
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    *out_z_index = slot->GetZIndex();
    return LC_SUCCESS;
}

LC_API LCResult lc_layout_slot_set_z_index(LCComponentHandle component, int z_index) {
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    slot->SetZIndex(z_index);
    return LC_SUCCESS;
}

LC_API LCResult lc_layout_slot_get_match_parent(LCComponentHandle component, bool* out_match_parent) {
    if (!out_match_parent) return LC_ERROR_NULL_POINTER;
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    *out_match_parent = slot->GetMatchParent();
    return LC_SUCCESS;
}

LC_API LCResult lc_layout_slot_set_match_parent(LCComponentHandle component, bool match_parent) {
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    slot->SetMatchParent(match_parent);
    return LC_SUCCESS;
}

/* ============================================================================
 * Layout participation
 * ============================================================================ */

LC_API LCResult lc_layout_slot_get_ignore_layout(LCComponentHandle component, bool* out_ignore) {
    if (!out_ignore) return LC_ERROR_NULL_POINTER;
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    *out_ignore = slot->GetIgnoreLayout();
    return LC_SUCCESS;
}

LC_API LCResult lc_layout_slot_set_ignore_layout(LCComponentHandle component, bool ignore) {
    LayoutSlot* slot = GetLayoutSlot(component);
    if (!slot) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid LayoutSlot handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    slot->SetIgnoreLayout(ignore);
    return LC_SUCCESS;
}
