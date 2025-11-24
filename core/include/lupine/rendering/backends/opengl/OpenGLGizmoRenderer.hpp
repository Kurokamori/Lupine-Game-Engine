#pragma once
#include "../../GizmoRenderer.hpp"

namespace lupine {

class OpenGLGizmoRenderer : public GizmoRenderer {
public:
    // 2D Gizmos
    void drawScale2D() override;
    void drawRotate2D() override;
    void drawMove2D() override;
    void drawComposite2D() override;

    // 3D Gizmos
    void drawScale3D() override;
    void drawRotate3D() override;
    void drawMove3D() override;
    void drawComposite3D() override;

    // Overlay helpers
    void begin2DGizmoOverlay(int viewportWidth, int viewportHeight);
    void end2DGizmoOverlay();
    void drawMove2DAt(float x, float y);
    void drawScale2DAt(float x, float y);
    void drawRotate2DAt(float x, float y);
    void drawComposite2DAt(float x, float y);
};
} // namespace lupine
