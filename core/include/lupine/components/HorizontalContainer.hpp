#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * HorizontalContainer Component
 *
 * A container that organizes its children in a horizontal line from left to right.
 * Supports spacing between children and vertical alignment options.
 *
 * Features:
 * - Arranges children horizontally with configurable spacing
 * - Vertical alignment options (Top, Center, Bottom, Fill)
 * - Horizontal alignment options (Begin, Center, End, Fill)
 * - Inherits all Container features (padding, margin, borders, etc.)
 * - Automatic size calculation based on children
 *
 * Usage:
 *   auto hbox = node->AddComponent<HorizontalContainer>();
 *   hbox->SetSeparation(10.0f);
 *   hbox->SetVerticalAlignment(VerticalAlignment::Center);
 *   hbox->SetHorizontalAlignment(HorizontalAlignment::Begin);
 */
class HorizontalContainer : public Container {
public:
    enum class VerticalAlignment {
        Top,
        Center,
        Bottom,
        Fill
    };

    // Main axis. The first four values keep their original meaning and index; the three
    // distributed-spacing modes are appended (no container offered space-between /
    // space-around / space-evenly at all).
    enum class HorizontalAlignment {
        Begin = 0,
        Center = 1,
        End = 2,
        Fill = 3,
        SpaceBetween = 4,
        SpaceAround = 5,
        SpaceEvenly = 6
    };

    HorizontalContainer();
    explicit HorizontalContainer(const std::string& name);
    virtual ~HorizontalContainer();

    // ISerializable interface
    std::string GetTypeName() const override { return "HorizontalContainer"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ========================================
    // HorizontalContainer Specific Properties
    // ========================================

    /**
     * Get vertical alignment of children
     */
    VerticalAlignment GetVerticalAlignment() const;
    void SetVerticalAlignment(VerticalAlignment alignment);

    /**
     * Get horizontal alignment of children
     */
    HorizontalAlignment GetHorizontalAlignment() const;
    void SetHorizontalAlignment(HorizontalAlignment alignment);

protected:

    /**
     * Re-read this container's own cached properties from the property registry on every
     * layout pass. See Container::SyncDerivedProperties.
     */
    void SyncDerivedProperties() override;
    // ========================================
    // Container Virtual Method Overrides
    // ========================================

    /**
     * Calculate and apply horizontal layout to children
     */
    void CalculateLayout() override;

    /**
     * Calculate minimum size based on children
     */
    math::Vec2 GetMinimumSize() const override;

    /**
     * Called when layout needs recalculation
     */
    void OnLayoutInvalidated() override;

private:
    // Measurement (Container::GetChildSize) and placement (Container::SetChildRect) are
    // inherited. The local copies used to scan the child's components and take the FIRST
    // one carrying width/height, so a Label with width=0 (auto-sized from its text)
    // measured as zero and every label in the row stacked at the same x.

    /// Map this container's cross-axis (vertical) alignment onto the shared enum.
    static CrossAxisAlign ToCrossAxisAlign(VerticalAlignment alignment);

    // ========================================
    // Member Variables
    // ========================================

    VerticalAlignment m_VerticalAlignment;
    HorizontalAlignment m_HorizontalAlignment;
};

} // namespace components
} // namespace lupine

