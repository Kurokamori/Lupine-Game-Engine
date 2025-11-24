#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/rendering/RenderView.hpp"

namespace lupine {

DebugRenderer* DebugDraw::s_ActiveRenderer = nullptr;
CameraType DebugDraw::s_CurrentCameraType = CameraType::Camera3D;
RenderView* DebugDraw::s_CurrentRenderView = nullptr;

void DebugDraw::SetRenderer(DebugRenderer* renderer) {
    s_ActiveRenderer = renderer;
}

void DebugDraw::ClearRenderer() {
    s_ActiveRenderer = nullptr;
}

bool DebugDraw::IsAvailable() {
    return s_ActiveRenderer != nullptr;
}

void DebugDraw::SetCurrentCameraType(CameraType cameraType) {
    s_CurrentCameraType = cameraType;
}

CameraType DebugDraw::GetCurrentCameraType() {
    return s_CurrentCameraType;
}

void DebugDraw::SetCurrentRenderView(RenderView* view) {
    s_CurrentRenderView = view;
}

bool DebugDraw::IsCollisionShapesEnabled() {
    if (s_CurrentRenderView) {
        return s_CurrentRenderView->isCollisionShapesEnabled();
    }
    return true;
}

void DebugDraw::Line(const Vec3& start, const Vec3& end, const Color& color,
                     float duration, bool depthTest) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawLine(start, end, color, duration, depthTest);
    }
}

void DebugDraw::LineStrip(const std::vector<Vec3>& points, const Color& color,
                          float duration, bool depthTest) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawLineStrip(points, color, duration, depthTest);
    }
}

void DebugDraw::Ray(const Vec3& origin, const Vec3& direction, const Color& color,
                    float length, float duration, bool depthTest) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawRay(origin, direction, color, length, duration, depthTest);
    }
}

void DebugDraw::Box(const Vec3& center, const Vec3& size, const Color& color,
                    bool wireframe, float duration, bool depthTest) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawBox(center, size, color, wireframe, duration, depthTest);
    }
}

void DebugDraw::Sphere(const Vec3& center, float radius, const Color& color,
                       bool wireframe, int segments, float duration, bool depthTest) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawSphere(center, radius, color, wireframe, segments, duration, depthTest);
    }
}

void DebugDraw::Circle(const Vec3& center, const Vec3& normal, float radius, const Color& color,
                       int segments, float duration, bool depthTest) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawCircle(center, normal, radius, color, segments, duration, depthTest);
    }
}

void DebugDraw::Arrow(const Vec3& start, const Vec3& end, const Color& color,
                      float arrowSize, float duration, bool depthTest) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawArrow(start, end, color, arrowSize, duration, depthTest);
    }
}

void DebugDraw::Text(const Vec3& position, const std::string& text, const Color& color,
                     float size, float duration) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawText(position, text, color, size, duration);
    }
}

void DebugDraw::Axes(const Vec3& position, float size, float duration) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawAxes(position, size, duration);
    }
}

void DebugDraw::Transform(const Vec3& position, const Mat4& rotation, float size, float duration) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->drawTransform(position, rotation, size, duration);
    }
}

void DebugDraw::BoundingBox2D(const AABB& bounds, const Color& color, float duration) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->draw2DBoundingBox(bounds, color, duration);
    }
}

void DebugDraw::OrientedBox2D(const Vec2& center, const Vec2& size, float rotation, const Color& color, float duration) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->draw2DOrientedBoundingBox(center, size, rotation, color, duration);
    }
}

void DebugDraw::BoundingBox3D(const AABB& bounds, const Color& color, float duration) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->draw3DBoundingBox(bounds, color, duration);
    }
}

void DebugDraw::OrientedBox3D(const OBB& obb, const Color& color, float duration) {
    if (s_ActiveRenderer) {
        s_ActiveRenderer->draw3DOrientedBoundingBox(obb, color, duration);
    }
}

}

