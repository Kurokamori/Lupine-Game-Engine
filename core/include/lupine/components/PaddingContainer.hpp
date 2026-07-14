#pragma once

#include "lupine/components/Container.hpp"

namespace lupine {
namespace components {

/**
 * PaddingContainer Component
 *
 * A container that applies padding to its children, ensuring they remain within
 * the padded space. Padding can be set uniformly (one value for all sides) or
 * individually per side (top, right, bottom, left) using the linked value system.
 *
 * Features:
 * - Uniform or per-side padding control
 * - Automatic child positioning within padded area
 * - Respects child size constraints
 * - Supports multiple children (all positioned within padded area)
 * - Size modes: Fixed, FitChildren (includes padding in calculation)
 *
 * Usage:
 *   auto padding = node->AddComponent<PaddingContainer>();
 *   padding->SetPadding(Vec4(10.0f, 10.0f, 10.0f, 10.0f)); // top, right, bottom, left
 *   padding->SetPaddingLinked(true); // Use uniform padding
 *
 * Layout Behavior:
 * - Children are positioned to fit within the padded content area
 * - Each child maintains its original size (unless it exceeds available space)
 * - If a child is too large, it will be scaled down to fit
 * - Multiple children are positioned at the top-left of the padded area
 */
class PaddingContainer : public Container {
public:
    PaddingContainer();
    explicit PaddingContainer(const std::string& name);
    virtual ~PaddingContainer();

    // ISerializable interface
    std::string GetTypeName() const override { return "PaddingContainer"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ========================================
    // Padding Container Specific Properties
    // ========================================

    /**
     * Enable/disable automatic child fitting
     * When enabled, children are automatically resized to fit the padded area
     */
    bool GetAutoFitChildren() const;
    void SetAutoFitChildren(bool autoFit);

    /**
     * Enable/disable maintaining aspect ratio when fitting children
     */
    bool GetMaintainAspectRatio() const;
    void SetMaintainAspectRatio(bool maintain);

    /**
     * Set alignment of children within the padded area
     */
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

    Alignment GetChildAlignment() const;
    void SetChildAlignment(Alignment alignment);

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
     * Calculate and apply layout to children
     * Positions children within the padded content area
     */
    void CalculateLayout() override;

    /**
     * Calculate minimum size required (includes padding)
     */
    math::Vec2 GetMinimumSize() const override;

    /**
     * Called when layout is invalidated
     */
    void OnLayoutInvalidated() override;

private:
    // Measurement, fitting and placement come from Container (GetChildSize / FitChildSize
    // / AlignRectIn / SetChildRect). The local copies wrote the fitted size into the
    // child's width/height properties -- destroying the authored design-time size -- and
    // fell back to Node2D::SetScale(), which compounded with any scale the user had set.

    /// Split this container's nine-way alignment into per-axis canvas-oriented alignments.
    static void SplitAlignment(Alignment alignment,
                               CrossAxisAlign& outHorizontal,
                               CrossAxisAlign& outVertical);

private:
    // ========================================
    // Member Variables
    // ========================================

    bool m_AutoFitChildren;
    bool m_MaintainAspectRatio;
    Alignment m_ChildAlignment;
};

} // namespace components
} // namespace lupine
