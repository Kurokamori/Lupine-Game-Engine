#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * VerticalContainer Component
 *
 * A container that organizes its children in a vertical line from top to bottom.
 * Supports spacing between children and horizontal alignment options.
 *
 * Features:
 * - Arranges children vertically with configurable spacing
 * - Horizontal alignment options (Left, Center, Right, Fill)
 * - Vertical alignment options (Begin, Center, End, Fill)
 * - Inherits all Container features (padding, margin, borders, etc.)
 * - Automatic size calculation based on children
 *
 * Usage:
 *   auto vbox = node->AddComponent<VerticalContainer>();
 *   vbox->SetSeparation(10.0f);
 *   vbox->SetHorizontalAlignment(HorizontalAlignment::Center);
 *   vbox->SetVerticalAlignment(VerticalAlignment::Begin);
 */
class VerticalContainer : public Container {
public:
    enum class HorizontalAlignment {
        Left,
        Center,
        Right,
        Fill
    };

    // Main axis. The first four values keep their original meaning and index; the three
    // distributed-spacing modes are appended (no container offered space-between /
    // space-around / space-evenly at all).
    enum class VerticalAlignment {
        Begin = 0,
        Center = 1,
        End = 2,
        Fill = 3,
        SpaceBetween = 4,
        SpaceAround = 5,
        SpaceEvenly = 6
    };

    VerticalContainer();
    explicit VerticalContainer(const std::string& name);
    virtual ~VerticalContainer();

    // ISerializable interface
    std::string GetTypeName() const override { return "VerticalContainer"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ========================================
    // VerticalContainer Specific Properties
    // ========================================

    /**
     * Get horizontal alignment of children
     */
    HorizontalAlignment GetHorizontalAlignment() const;
    void SetHorizontalAlignment(HorizontalAlignment alignment);

    /**
     * Get vertical alignment of children
     */
    VerticalAlignment GetVerticalAlignment() const;
    void SetVerticalAlignment(VerticalAlignment alignment);

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
     * Calculate and apply vertical layout to children
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
    // inherited: this class used to carry its own copies, as did seven siblings.

    /// Map this container's cross-axis (horizontal) alignment onto the shared enum.
    static CrossAxisAlign ToCrossAxisAlign(HorizontalAlignment alignment);

    // ========================================
    // Member Variables
    // ========================================

    HorizontalAlignment m_HorizontalAlignment;
    VerticalAlignment m_VerticalAlignment;
};

} // namespace components
} // namespace lupine

