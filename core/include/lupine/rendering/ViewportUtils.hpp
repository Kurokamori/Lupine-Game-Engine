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

// The inverse of the active CameraUI's full canvas view-projection matrix, published
// each frame by the runtime. UI hit-testing (GetCanvasMousePosition) maps the cursor's
// NDC position through this to land directly in control space, accounting for the UI
// camera's origin, scale_factor, pixel snapping, zoom, rotation and position in one go.
// SetCanvasPickMatrix marks it valid; GetCanvasPickMatrix returns false until then so
// callers (e.g. the editor, which never publishes one) fall back to a centered mapping.
void SetCanvasPickMatrix(const math::Mat4& inverseViewProj);
void ClearCanvasPickMatrix();
bool GetCanvasPickMatrix(math::Mat4& outInverseViewProj);
}
