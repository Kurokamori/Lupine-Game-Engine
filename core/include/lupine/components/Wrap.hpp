#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * Wrap Component
 *
 * A container that lays out children horizontally, wrapping to the next row
 * as needed when the available width is exceeded.
 *
 * Features:
 * - Horizontal layout with automatic wrapping
 * - Configurable horizontal and vertical spacing
 * - Line alignment control (left, center, right)
 * - Maximum lines limit
 * - Wrap direction (horizontal or vertical)
 * - Inherits all Container features (padding, margin, borders, etc.)
 *
 * Usage:
 *   auto wrap = node->AddComponent<Wrap>();
 *   wrap->SetSize(math::Vec2(400.0f, 300.0f));
 *   wrap->SetSpacingX(10.0f);
 *   wrap->SetSpacingY(10.0f);
 *   wrap->SetLineAlignment(Wrap::LineAlignment::Center);
 */
class Wrap : public Container {
public:
    enum class LineAlignment {
        Left = 0,
        Center = 1,
        Right = 2,
        Justify = 3
    };

    enum class WrapDirection {
        Horizontal = 0,  // Wrap horizontally (default)
        Vertical = 1     // Wrap vertically
    };

    Wrap();
    explicit Wrap(const std::string& name);
    virtual ~Wrap();

    // ISerializable interface
    std::string GetTypeName() const override { return "Wrap"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ========================================
    // Wrap Specific Properties
    // ========================================

    /**
     * Get horizontal spacing between children
     */
    float GetSpacingX() const;
    void SetSpacingX(float spacing);

    /**
     * Get vertical spacing between lines
     */
    float GetSpacingY() const;
    void SetSpacingY(float spacing);

    /**
     * Get line alignment
     */
    LineAlignment GetLineAlignment() const;
    void SetLineAlignment(LineAlignment alignment);

    /**
     * Get maximum number of lines (0 = unlimited)
     */
    int GetMaxLines() const;
    void SetMaxLines(int maxLines);

    /**
     * Get wrap direction
     */
    WrapDirection GetWrapDirection() const;
    void SetWrapDirection(WrapDirection direction);

protected:
    // ========================================
    // Container Virtual Method Overrides
    // ========================================

    /**
     * Calculate and apply wrapping layout to children
     */
    void CalculateLayout() override;

    /**
     * Minimum size based on children. Genuinely height-for-width (or width-for-height when
     * wrapping vertically): the number of runs -- and therefore the extent across them --
     * depends on the extent along them, so this breaks the children into runs against the
     * currently available extent rather than pretending a single line.
     */
    math::Vec2 GetMinimumSize() const override;

    /**
     * Re-read spacing / alignment / maxLines / direction from the property registry.
     */
    void SyncDerivedProperties() override;

    /**
     * Called when layout needs recalculation
     */
    void OnLayoutInvalidated() override;

private:
    // ========================================
    // Internal Helper Methods
    // ========================================

    /**
     * One run of children: a line when wrapping horizontally, a column when wrapping
     * vertically. `mainExtent` is measured ALONG the run (including the inter-child
     * spacing); `crossExtent` is the thickest child ACROSS it.
     */
    struct Run {
        std::vector<core::Node*> children;
        float mainExtent = 0.0f;
        float crossExtent = 0.0f;
    };

    /**
     * Break the visible children into runs that each fit within `availableMain`.
     *
     * `vertical` selects the run's main axis: false = children flow along X and the runs
     * stack down the Y axis (horizontal wrap); true = the transpose.
     *
     * A non-positive `availableMain` means "unconstrained" and yields a single run --
     * without that, a container that has not resolved a size yet (or whose padding
     * exceeds its width) would break a line after every single child.
     *
     * Honors maxLines. The old code checked the cap BEFORE pushing the in-progress run and
     * then pushed it anyway on the way out, so maxLines = 2 produced 3 lines; the cap is
     * now checked immediately after a run is committed. Children that do not fit within the
     * cap are returned in `outOverflow`.
     */
    std::vector<Run> BuildRuns(bool vertical, float availableMain,
                               std::vector<core::Node*>* outOverflow) const;

    /**
     * Arrange one run. `runBegin`/`runExtent` describe the band the run occupies on the
     * CROSS axis in minimum-corner terms.
     */
    void ArrangeRun(const Run& run, bool vertical, const math::Rect& contentArea,
                    float runBegin, float runExtent) const;

    /**
     * Alignment of the children ALONG a run, plus the effective inter-child spacing.
     * Justify spreads the slack between the children; the spread is clamped at zero so an
     * overflowing run does not compute a NEGATIVE spacing and overlap its children.
     */
    void ResolveRunAlignment(const Run& run, float availableMain,
                             float& outLeadingOffset, float& outSpacing) const;

    // ========================================
    // Member Variables
    // ========================================

    float m_SpacingX;
    float m_SpacingY;
    LineAlignment m_LineAlignment;
    int m_MaxLines;
    WrapDirection m_WrapDirection;
};

} // namespace components
} // namespace lupine

