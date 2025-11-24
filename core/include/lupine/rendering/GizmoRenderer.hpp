#pragma once

namespace lupine {

class GizmoRenderer {
public:
    virtual ~GizmoRenderer() = default;

    // 2D Gizmos
    virtual void drawScale2D() = 0;
    virtual void drawRotate2D() = 0;
    virtual void drawMove2D() = 0;
    virtual void drawComposite2D() = 0;

    // 3D Gizmos
    virtual void drawScale3D() = 0;
    virtual void drawRotate3D() = 0;
    virtual void drawMove3D() = 0;
    virtual void drawComposite3D() = 0;
};

} // namespace lupine
