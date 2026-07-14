#pragma once

#include "lupine/core/Component.hpp"

namespace lupine {
namespace components {

/**
 * LayoutSlot Component - per-child layout hints read by the parent container.
 *
 * This is the engine's "attached property" mechanism (the WPF `DockPanel.Dock` /
 * `Grid.Row` pattern): data that logically belongs to the PARENT's layout algorithm but
 * must be stored PER CHILD. Add it to a child node to tell whichever container holds that
 * node how this particular child should be treated.
 *
 * It exists because containers had no way to store per-child data at all. DockContainer
 * and Stack both shipped with per-child getters that ignored the child entirely and
 * returned a single container-wide constant, so:
 *   - every DockContainer child docked to the same side (and with the default `Center`
 *     side the first child consumed the whole rect, collapsing every sibling to 0x0);
 *   - Stack's sortByZIndex sorted by an all-zero key, matchParent was never true, and
 *     per-child alignment did not exist.
 *
 * A container reads the slot off a child with LayoutSlot::For(child) and falls back to its
 * own container-wide default when the child carries no slot -- so adding this component is
 * always optional and never changes existing behaviour by itself.
 *
 * Usage:
 *   auto slot = childNode->AddComponent<LayoutSlot>();
 *   slot->SetDockSide(LayoutSlot::DockSide::Left);
 */
class LayoutSlot : public core::Component {
public:
    /// Which edge of a DockContainer this child claims. Fill consumes the remaining rect.
    enum class DockSide {
        Left,
        Right,
        Top,
        Bottom,
        Fill
    };

    /// Where this child sits inside a Stack's content rect.
    enum class Alignment {
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    LayoutSlot();
    explicit LayoutSlot(const std::string& name);
    virtual ~LayoutSlot();

    // ISerializable interface
    std::string GetTypeName() const override { return "LayoutSlot"; }
    void DefineProperties() override;

    /**
     * The slot attached to `node`, or nullptr when it carries none. Containers use this
     * to read per-child hints, falling back to their own default when it is null.
     */
    static std::shared_ptr<LayoutSlot> For(core::Node* node);

    // ========================================
    // Dock (DockContainer)
    // ========================================
    DockSide GetDockSide() const;
    void SetDockSide(DockSide side);

    // ========================================
    // Stack
    // ========================================
    Alignment GetAlignment() const;
    void SetAlignment(Alignment alignment);

    /// Draw/sort order within a Stack whose sortByZIndex is enabled. Higher draws later.
    int GetZIndex() const;
    void SetZIndex(int zIndex);

    /// The container skips this child entirely: it is neither measured nor arranged.
    bool GetIgnoreLayout() const;
    void SetIgnoreLayout(bool ignore);

    /// The child is stretched to the container's full content rect rather than aligned.
    bool GetMatchParent() const;
    void SetMatchParent(bool matchParent);

private:
    // No member variables: all data lives in properties.
};

} // namespace components
} // namespace lupine
