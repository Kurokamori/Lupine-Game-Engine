#pragma once

#include "lupine/components/Container.hpp"

namespace lupine {
namespace components {

/**
 * AspectRatioContainer - forces its children to a fixed width : height ratio.
 *
 * Each visible child is given the largest box with the requested `ratio` that satisfies the
 * stretch mode, then aligned inside the content rect. The classic use is a game viewport or a
 * video pane that must not distort when the window is resized.
 */
class AspectRatioContainer : public Container {
public:
    enum class StretchMode {
        // The child fits ENTIRELY inside the content rect (letterboxed / pillarboxed).
        Fit = 0,
        // The child COVERS the content rect, overflowing on the axis that does not fit.
        Cover = 1,
        // The width is taken from the content rect and the height derived from the ratio.
        WidthControlsHeight = 2,
        // The height is taken from the content rect and the width derived from the ratio.
        HeightControlsWidth = 3
    };

    AspectRatioContainer();
    explicit AspectRatioContainer(const std::string& name);
    virtual ~AspectRatioContainer();

    std::string GetTypeName() const override { return "AspectRatioContainer"; }
    void DefineProperties() override;

    void OnAwake() override;

    /** Width divided by height. 16:9 is 1.7778. */
    float GetRatio() const { return m_Ratio; }
    void SetRatio(float ratio);

    StretchMode GetStretchMode() const { return m_StretchMode; }
    void SetStretchMode(StretchMode mode);

    CrossAxisAlign GetHorizontalAlignment() const { return m_HorizontalAlignment; }
    void SetHorizontalAlignment(CrossAxisAlign alignment);

    CrossAxisAlign GetVerticalAlignment() const { return m_VerticalAlignment; }
    void SetVerticalAlignment(CrossAxisAlign alignment);

protected:
    void CalculateLayout() override;
    math::Vec2 GetMinimumSize() const override;
    void SyncDerivedProperties() override;

    /** The ratio-correct box for FitChildren sizing. */
    math::Vec2 CalculateChildrenBounds() const override;

private:
    /** The ratio-correct size for a child inside `available`, per the stretch mode. */
    math::Vec2 ResolveChildSize(const math::Vec2& available) const;

    float m_Ratio;
    StretchMode m_StretchMode;
    CrossAxisAlign m_HorizontalAlignment;
    CrossAxisAlign m_VerticalAlignment;
};

} // namespace components
} // namespace lupine
