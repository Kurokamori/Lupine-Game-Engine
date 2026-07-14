#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * CenterContainer Component
 *
 * A container that centers its children within its bounds.
 * Supports automatic child fitting and aspect ratio preservation.
 *
 * Features:
 * - Centers all children at the container's center point
 * - Optional auto-fit to resize children to available space
 * - Optional aspect ratio maintenance when auto-fitting
 * - Inherits all Container features (padding, margin, borders, etc.)
 *
 * Usage:
 *   auto centerContainer = node->AddComponent<CenterContainer>();
 *   centerContainer->SetSize(math::Vec2(400.0f, 300.0f));
 *   centerContainer->SetAutoFitChild(true);
 *   centerContainer->SetMaintainAspectRatio(true);
 */
class CenterContainer : public Container {
public:
    CenterContainer();
    explicit CenterContainer(const std::string& name);
    virtual ~CenterContainer();

    // ISerializable interface
    std::string GetTypeName() const override { return "CenterContainer"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ========================================
    // CenterContainer Specific Properties
    // ========================================

    /**
     * Get whether children are automatically resized to fit container
     */
    bool GetAutoFitChild() const;
    void SetAutoFitChild(bool autoFit);

    /**
     * Get whether aspect ratio is maintained when auto-fitting
     */
    bool GetMaintainAspectRatio() const;
    void SetMaintainAspectRatio(bool maintain);

    /**
     * Get whether to center each child independently or stack them
     */
    bool GetStackChildren() const;
    void SetStackChildren(bool stack);

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
     * Calculate and apply centering layout to children
     */
    void CalculateLayout() override;

    /**
     * Calculate minimum size based on largest child
     */
    math::Vec2 GetMinimumSize() const override;

    /**
     * Called when layout needs recalculation
     */
    void OnLayoutInvalidated() override;

private:
    // Measurement, fitting and placement come from Container (GetChildSize / FitChildSize
    // / AlignRectIn / SetChildRect). The local copies wrote the fitted size into the
    // child's width/height properties and otherwise fell back to Node2D::SetScale(),
    // which compounded with any scale the user had already applied.

    // ========================================
    // Member Variables
    // ========================================

    bool m_AutoFitChild;
    bool m_MaintainAspectRatio;
    bool m_StackChildren;
};

} // namespace components
} // namespace lupine
