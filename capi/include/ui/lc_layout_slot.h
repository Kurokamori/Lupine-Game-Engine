/**
 * @file lc_layout_slot.h
 * @brief Lupine Engine C API - LayoutSlot (per-child layout hints)
 *
 * LayoutSlot is the engine's "attached property" mechanism: layout data that belongs to
 * the PARENT container's algorithm but must be stored PER CHILD. Add a LayoutSlot to a
 * child node to tell whichever container holds it how that specific child is treated.
 *
 * Containers fall back to their own container-wide default for children with no
 * LayoutSlot, so attaching one is always optional.
 *
 * - DockContainer reads the dock side (which edge this child claims).
 * - Stack reads the alignment, z-index, match-parent and ignore-layout flags.
 */

#ifndef LUPINE_CAPI_LAYOUT_SLOT_H
#define LUPINE_CAPI_LAYOUT_SLOT_H

#include "core/lc_core.h"
#include "core/lc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Which edge of a DockContainer a child claims.
 *
 * LC_LAYOUT_SLOT_DOCK_FILL consumes whatever rect remains after the edge-docked
 * siblings have taken theirs.
 */
typedef enum LCLayoutSlotDockSide {
    LC_LAYOUT_SLOT_DOCK_LEFT = 0,
    LC_LAYOUT_SLOT_DOCK_RIGHT = 1,
    LC_LAYOUT_SLOT_DOCK_TOP = 2,
    LC_LAYOUT_SLOT_DOCK_BOTTOM = 3,
    LC_LAYOUT_SLOT_DOCK_FILL = 4
} LCLayoutSlotDockSide;

/**
 * @brief Where a child sits inside a Stack's content rect.
 */
typedef enum LCLayoutSlotAlignment {
    LC_LAYOUT_SLOT_ALIGN_TOP_LEFT = 0,
    LC_LAYOUT_SLOT_ALIGN_TOP_CENTER = 1,
    LC_LAYOUT_SLOT_ALIGN_TOP_RIGHT = 2,
    LC_LAYOUT_SLOT_ALIGN_CENTER_LEFT = 3,
    LC_LAYOUT_SLOT_ALIGN_CENTER = 4,
    LC_LAYOUT_SLOT_ALIGN_CENTER_RIGHT = 5,
    LC_LAYOUT_SLOT_ALIGN_BOTTOM_LEFT = 6,
    LC_LAYOUT_SLOT_ALIGN_BOTTOM_CENTER = 7,
    LC_LAYOUT_SLOT_ALIGN_BOTTOM_RIGHT = 8
} LCLayoutSlotAlignment;

/* ============================================================================
 * Creation
 * ============================================================================ */

/**
 * @brief Create a LayoutSlot component.
 * @param name Optional component name (may be NULL).
 * @param out_component Receives the new component handle.
 */
LC_API LCResult lc_layout_slot_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Dock (read by DockContainer)
 * ============================================================================ */

LC_API LCResult lc_layout_slot_get_dock_side(LCComponentHandle component, LCLayoutSlotDockSide* out_side);
LC_API LCResult lc_layout_slot_set_dock_side(LCComponentHandle component, LCLayoutSlotDockSide side);

/* ============================================================================
 * Stack
 * ============================================================================ */

LC_API LCResult lc_layout_slot_get_alignment(LCComponentHandle component, LCLayoutSlotAlignment* out_alignment);
LC_API LCResult lc_layout_slot_set_alignment(LCComponentHandle component, LCLayoutSlotAlignment alignment);

/** Draw/sort order within a Stack whose sortByZIndex is enabled. Higher draws later. */
LC_API LCResult lc_layout_slot_get_z_index(LCComponentHandle component, int* out_z_index);
LC_API LCResult lc_layout_slot_set_z_index(LCComponentHandle component, int z_index);

/** The child is stretched to the container's full content rect rather than aligned. */
LC_API LCResult lc_layout_slot_get_match_parent(LCComponentHandle component, bool* out_match_parent);
LC_API LCResult lc_layout_slot_set_match_parent(LCComponentHandle component, bool match_parent);

/* ============================================================================
 * Layout participation
 * ============================================================================ */

/** When true the container skips this child entirely: neither measured nor arranged. */
LC_API LCResult lc_layout_slot_get_ignore_layout(LCComponentHandle component, bool* out_ignore);
LC_API LCResult lc_layout_slot_set_ignore_layout(LCComponentHandle component, bool ignore);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_LAYOUT_SLOT_H */
