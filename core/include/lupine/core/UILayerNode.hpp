#pragma once

#include "lupine/core/Node.hpp"

namespace lupine {
namespace core {

/**
 * UILayer - deterministic render-layer container.
 *
 * A UILayer groups its entire subtree onto a single, distinct rendering layer.
 * Every 2D/Canvas renderable beneath a UILayer is drawn as one ordered block at
 * the layer's index, regardless of where the layer sits in the scene tree. This
 * makes it straightforward to build HUDs, pause overlays, screen transitions and
 * other stacked UI: a higher layer always draws over a lower one.
 *
 * Within a single layer, the usual ordering rules still apply (per-node Z-index,
 * then tree order). Nested UILayers override their ancestor's layer for their own
 * subtree, then ordering resumes at the ancestor layer for following siblings.
 *
 * UILayer carries no transform of its own; its children provide their own
 * positions (Node2D transforms, or UIControl anchors/offsets for canvas-space UI).
 */
class UILayer : public Node {
public:
    UILayer();
    explicit UILayer(const std::string& name);

    std::string GetTypeName() const override { return "UILayer"; }
    void RegisterProperties() override;

    // Layer index. Higher layers render on top of lower layers. Content that is
    // not under any UILayer is treated as layer 0.
    int GetLayer() const { return m_Layer; }
    void SetLayer(int layer) { m_Layer = layer; }

protected:
    int m_Layer;
};

} // namespace core
} // namespace lupine
