#pragma once
#include "lupine/rendering/RenderView.hpp"
#include "lupine/math/Math.hpp"

namespace lupine {
// Utility to get the current viewport (stub, to be implemented in engine/runtime)
class RuntimeApp;
Viewport GetCurrentViewport();
void SetCurrentViewport(const Viewport& viewport);

// Utility to get/set the logical canvas size (project settings window size)
// This is the coordinate space that UI elements are positioned in
math::Vec2 GetLogicalCanvasSize();
void SetLogicalCanvasSize(const math::Vec2& size);
}
