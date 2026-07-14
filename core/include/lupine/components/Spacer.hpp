#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
namespace components {

/**
 * Spacer Component
 *
 * A non-rendering component that occupies a fixed amount of space in layouts.
 * Useful for creating spacing between UI elements in container layouts.
 *
 * Features:
 * - Configurable width and height
 * - No visual rendering (invisible)
 * - Participates in container layout calculations
 * - Minimum and maximum size constraints
 *
 * Usage:
 *   auto spacer = node->AddComponent<Spacer>();
 *   spacer->SetSize(math::Vec2(50.0f, 20.0f));
 */
class Spacer : public UIControl {
public:
    Spacer();
    explicit Spacer(const std::string& name);
    virtual ~Spacer();

    // ISerializable interface
    std::string GetTypeName() const override { return "Spacer"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;

    // ========================================
    // Size Management
    // ========================================
    // Width/height/size are provided by the UIControl base class. The spacer's content
    // minimum size is its customMinSize constraint.
    math::Vec2 GetContentMinSize() const override;

    // Min/max constraints. Like Container, Spacer used to declare its own minSize/maxSize
    // properties that SHADOWED the (now virtual) UIControl::GetMinSize()/GetMaxSize(), so
    // the anchor solver -- which reads the base -- ignored them entirely. There is one
    // storage now (customMinSize/customMaxSize on UIControl) and these setters write it.
    void SetMinSize(const math::Vec2& minSize) { SetCustomMinSize(minSize); }
    void SetMaxSize(const math::Vec2& maxSize) { SetCustomMaxSize(maxSize); }

    /**
     * Get whether the spacer expands to fill available space
     */
    bool GetExpand() const;
    void SetExpand(bool expand);

private:
    // No member variables needed - all data stored in properties
};

} // namespace components
} // namespace lupine

