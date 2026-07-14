#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/components/LayoutSlot.hpp"
#include "lupine/math/Math.hpp"
#include <unordered_map>

namespace lupine {
namespace components {

/**
 * DockContainer Component
 *
 * A container that arranges children by docking them to specific sides.
 * Children declare a dock side (top/bottom/left/right/center), and the
 * remaining area reduces as each child is allocated space.
 *
 * Features:
 * - Dock children to top, bottom, left, right, or center
 * - Processes children in order, shrinking available space
 * - Spacing between docked elements
 * - Inherits all Container features (padding, margin, borders, etc.)
 *
 * Usage:
 *   auto dockContainer = node->AddComponent<DockContainer>();
 *   dockContainer->SetSize(math::Vec2(800.0f, 600.0f));
 *   dockContainer->SetSpacing(5.0f);
 *   dockContainer->SetDefaultDockSide(DockContainer::DockSide::Top);
 *   child3->SetMetadata("dock", "center");
 */
class DockContainer : public Container {
public:
    enum class DockSide {
        Top = 0,
        Bottom = 1,
        Left = 2,
        Right = 3,
        Center = 4
    };

    DockContainer();
    explicit DockContainer(const std::string& name);
    virtual ~DockContainer();

    // ISerializable interface
    std::string GetTypeName() const override { return "DockContainer"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ========================================
    // DockContainer Specific Properties
    // ========================================

    /**
     * Get spacing between docked elements
     */
    float GetDockSpacing() const;
    void SetDockSpacing(float spacing);

    /**
     * Get default dock side for children without explicit dock metadata
     */
    DockSide GetDefaultDockSide() const;
    void SetDefaultDockSide(DockSide side);

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
     * Calculate and apply docking layout to children
     */
    void CalculateLayout() override;

    /**
     * Calculate minimum size based on docked children
     */
    math::Vec2 GetMinimumSize() const override;

    /**
     * Called when layout needs recalculation
     */
    void OnLayoutInvalidated() override;

private:
    // ========================================
    // Internal Helper Methods
    // ========================================

    /**
     * The dock side of a specific child: its LayoutSlot's side when it carries one, else
     * this container's defaultDockSide. Measurement (Container::GetChildSize) and
     * placement (Container::SetChildRect) are inherited.
     */
    DockSide GetChildDockSide(core::Node* child) const;

    /// Map the child-side LayoutSlot dock side onto this container's enum.
    static DockSide ToDockSide(LayoutSlot::DockSide side);

    /**
     * Position a child in the docked area, shrinking `availableRect` by what it claims.
     */
    void DockChild(core::Node* child, DockSide side, math::Rect& availableRect);

    /**
     * Convert DockSide enum to string
     */
    static std::string DockSideToString(DockSide side);

    /**
     * Convert string to DockSide enum
     */
    static DockSide StringToDockSide(const std::string& str);

    // ========================================
    // Member Variables
    // ========================================

    float m_DockSpacing;
    DockSide m_DefaultDockSide;
};

} // namespace components
} // namespace lupine

