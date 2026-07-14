/**
 * @file lc_ysort.cpp
 * @brief Implementation of YSort C API
 */

#include "components/lc_ysort.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/YSort.hpp>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

YSort::SortAxis ToEngineSortAxis(LCYSortAxis axis) {
    return static_cast<YSort::SortAxis>(axis);
}

LCYSortAxis FromEngineSortAxis(YSort::SortAxis axis) {
    return static_cast<LCYSortAxis>(axis);
}

YSort::UpdateMode ToEngineUpdateMode(LCYSortUpdateMode mode) {
    return static_cast<YSort::UpdateMode>(mode);
}

LCYSortUpdateMode FromEngineUpdateMode(YSort::UpdateMode mode) {
    return static_cast<LCYSortUpdateMode>(mode);
}

YSort* GetYSort(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<YSort*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * YSort Creation
 * ============================================================================ */

LC_API LCResult lc_ysort_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<YSort>(name) : std::make_shared<YSort>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create YSort component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

LC_API LCResult lc_ysort_get_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) return LC_ERROR_NULL_POINTER;
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    *out_enabled = ysort->GetEnabled();
    return LC_SUCCESS;
}

LC_API LCResult lc_ysort_set_enabled(LCComponentHandle component, bool enabled) {
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    ysort->SetEnabled(enabled);
    return LC_SUCCESS;
}

/* ============================================================================
 * Sort Axis
 * ============================================================================ */

LC_API LCResult lc_ysort_get_sort_axis(LCComponentHandle component, LCYSortAxis* out_axis) {
    if (!out_axis) return LC_ERROR_NULL_POINTER;
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    *out_axis = FromEngineSortAxis(ysort->GetSortAxis());
    return LC_SUCCESS;
}

LC_API LCResult lc_ysort_set_sort_axis(LCComponentHandle component, LCYSortAxis axis) {
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    ysort->SetSortAxis(ToEngineSortAxis(axis));
    return LC_SUCCESS;
}

/* ============================================================================
 * Invert
 * ============================================================================ */

LC_API LCResult lc_ysort_get_invert(LCComponentHandle component, bool* out_invert) {
    if (!out_invert) return LC_ERROR_NULL_POINTER;
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    *out_invert = ysort->GetInvert();
    return LC_SUCCESS;
}

LC_API LCResult lc_ysort_set_invert(LCComponentHandle component, bool invert) {
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    ysort->SetInvert(invert);
    return LC_SUCCESS;
}

/* ============================================================================
 * Update Mode
 * ============================================================================ */

LC_API LCResult lc_ysort_get_update_mode(LCComponentHandle component, LCYSortUpdateMode* out_mode) {
    if (!out_mode) return LC_ERROR_NULL_POINTER;
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    *out_mode = FromEngineUpdateMode(ysort->GetUpdateMode());
    return LC_SUCCESS;
}

LC_API LCResult lc_ysort_set_update_mode(LCComponentHandle component, LCYSortUpdateMode mode) {
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    ysort->SetUpdateMode(ToEngineUpdateMode(mode));
    return LC_SUCCESS;
}

/* ============================================================================
 * Z Offset
 * ============================================================================ */

LC_API LCResult lc_ysort_get_z_offset(LCComponentHandle component, int* out_offset) {
    if (!out_offset) return LC_ERROR_NULL_POINTER;
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    *out_offset = ysort->GetZOffset();
    return LC_SUCCESS;
}

LC_API LCResult lc_ysort_set_z_offset(LCComponentHandle component, int offset) {
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    ysort->SetZOffset(offset);
    return LC_SUCCESS;
}

/* ============================================================================
 * Sprite Children Filter
 * ============================================================================ */

LC_API LCResult lc_ysort_get_affect_only_sprite_children(LCComponentHandle component, bool* out_sprite_only) {
    if (!out_sprite_only) return LC_ERROR_NULL_POINTER;
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    *out_sprite_only = ysort->GetAffectOnlySpriteChildren();
    return LC_SUCCESS;
}

LC_API LCResult lc_ysort_set_affect_only_sprite_children(LCComponentHandle component, bool sprite_only) {
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    ysort->SetAffectOnlySpriteChildren(sprite_only);
    return LC_SUCCESS;
}

/* ============================================================================
 * Manual Sorting
 * ============================================================================ */

LC_API LCResult lc_ysort_sort(LCComponentHandle component) {
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    ysort->Sort();
    return LC_SUCCESS;
}

LC_API LCResult lc_ysort_force_sort(LCComponentHandle component) {
    auto ysort = GetYSort(component);
    if (!ysort) return LC_ERROR_INVALID_HANDLE;
    ysort->ForceSort();
    return LC_SUCCESS;
}
