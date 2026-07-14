#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/components/LayoutSlot.hpp"
#include "lupine/math/Math.hpp"
#include <unordered_map>

namespace lupine {
namespace components {

/**
 * Stack Component
 *
 * A container that stacks children in the same rect, allowing control over
 * per-child alignment, visibility layers, and layout behavior.
 * Good for overlays, modal backgrounds, and layered UI.
 *
 * Features:
 * - Stack all children in the same position
 * - Default alignment control for all children
 * - Optional z-index sorting
 * - Inherits all Container features (padding, margin, borders, etc.)
 *
 * Usage:
 *   auto stack = node->AddComponent<Stack>();
 *   stack->SetSize(math::Vec2(400.0f, 300.0f));
 *   stack->SetDefaultAlignment(Stack::Alignment::Center);
 *   stack->SetSortByZIndex(true);
 */
class Stack : public Container {
public:
    enum class Alignment {
        TopLeft = 0,
        TopCenter = 1,
        TopRight = 2,
        CenterLeft = 3,
        Center = 4,
        CenterRight = 5,
        BottomLeft = 6,
        BottomCenter = 7,
        BottomRight = 8
    };

    Stack();
    explicit Stack(const std::string& name);
    virtual ~Stack();

    // ISerializable interface
    std::string GetTypeName() const override { return "Stack"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ========================================
    // Stack Specific Properties
    // ========================================

    /**
     * Get default alignment for children without explicit alignment metadata
     */
    Alignment GetDefaultAlignment() const;
    void SetDefaultAlignment(Alignment alignment);

    /**
     * Get whether to sort children by z-index
     */
    bool GetSortByZIndex() const;
    void SetSortByZIndex(bool sort);

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
     * Calculate and apply stacking layout to children
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
    // ========================================
    // Internal Helper Methods
    // ========================================

    /**
     * Get the alignment for a specific child
     */
    Alignment GetChildAlignment(core::Node* child) const;

    /**
     * Get the z-index for a specific child
     */
    int GetChildZIndex(core::Node* child) const;

    /**
     * Get whether child should ignore layout
     */
    bool GetChildIgnoreLayout(core::Node* child) const;

    /**
     * Get whether child should match parent size
     */
    bool GetChildMatchParent(core::Node* child) const;

    // Measurement (Container::GetChildSize) and placement (Container::SetChildRect /
    // AlignRectIn) are inherited.

    /// Map the child-side LayoutSlot alignment onto this container's enum.
    static Alignment ToAlignment(LayoutSlot::Alignment alignment);

    /// Split the nine-way alignment into per-axis canvas-oriented alignments.
    static void SplitAlignment(Alignment alignment,
                               CrossAxisAlign& outHorizontal,
                               CrossAxisAlign& outVertical);

    /**
     * Convert Alignment enum to string
     */
    static std::string AlignmentToString(Alignment alignment);

    /**
     * Convert string to Alignment enum
     */
    static Alignment StringToAlignment(const std::string& str);

    // ========================================
    // Member Variables
    // ========================================

    Alignment m_DefaultAlignment;
    bool m_SortByZIndex;
};

} // namespace components
} // namespace lupine

