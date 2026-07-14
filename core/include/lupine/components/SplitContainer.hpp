#pragma once

#include "lupine/components/Container.hpp"

namespace lupine {
namespace components {

/**
 * SplitContainer - two panes divided by a draggable splitter.
 *
 * Arranges its first two visible children on either side of a grabbable bar. Dragging the bar
 * moves the split; the two panes resize to match. Every other child is ignored (a split has
 * exactly two sides).
 *
 * `splitOffset` is measured in pixels from the START of the main axis -- the left edge when
 * horizontal, the TOP edge when vertical (reading order, not the Y-up canvas order). It is
 * clamped so neither pane is driven below its own minimum size.
 *
 * Signals:
 * - dragged(offset): emitted while the splitter is being dragged
 */
class SplitContainer : public Container {
public:
    enum class Orientation {
        Horizontal = 0,   // panes side by side, splitter is vertical
        Vertical = 1      // panes stacked, splitter is horizontal
    };

    SplitContainer();
    explicit SplitContainer(const std::string& name);
    virtual ~SplitContainer();

    std::string GetTypeName() const override { return "SplitContainer"; }
    void DefineProperties() override;
    void DefineSignals() override;
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnAwake() override;
    void OnInput(float deltaTime) override;

    void buildDrawCommands(RenderContext& ctx) override;

    Orientation GetOrientation() const { return m_Orientation; }
    void SetOrientation(Orientation orientation);

    /** Pixels from the start edge (left / top) to the near side of the splitter. */
    float GetSplitOffset() const { return m_SplitOffset; }
    void SetSplitOffset(float offset);

    float GetSplitterWidth() const { return m_SplitterWidth; }
    void SetSplitterWidth(float width);

    bool GetDraggable() const { return m_Draggable; }
    void SetDraggable(bool draggable);

    math::Color GetSplitterColor() const;
    void SetSplitterColor(const math::Color& color);

protected:
    void CalculateLayout() override;
    math::Vec2 GetMinimumSize() const override;
    void SyncDerivedProperties() override;

private:
    /** The splitter's rect, in global coordinates. */
    math::Rect GetSplitterRect() const;

    /**
     * `m_SplitOffset` clamped so that neither pane is squeezed below its own minimum, and so
     * the splitter itself always fits. Returns the offset in pixels from the start edge.
     */
    float ResolveSplitOffset() const;

    Orientation m_Orientation;
    float m_SplitOffset;
    float m_SplitterWidth;
    bool m_Draggable;

    bool m_Dragging{false};
    float m_DragStartMouse{0.0f};
    float m_DragStartOffset{0.0f};
};

} // namespace components
} // namespace lupine
