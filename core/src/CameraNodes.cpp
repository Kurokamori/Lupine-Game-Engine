#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/math/MathCommon.hpp"
#include "lupine/math/Mat4.hpp"
#include <cmath>

namespace lupine {
namespace core {

Camera3D::Camera3D()
    : Node3D(),
      m_ProjectionType(ProjectionType::Perspective),
      m_FOV(60.0f),
      m_NearPlane(0.1f),
      m_FarPlane(1000.0f),
      m_OrthoSize(100.0f),
      m_IsActive(true) {
}

Camera3D::Camera3D(const std::string& name)
    : Node3D(name),
      m_ProjectionType(ProjectionType::Perspective),
      m_FOV(60.0f),
      m_NearPlane(0.1f),
      m_FarPlane(1000.0f),
      m_OrthoSize(100.0f),
      m_IsActive(true) {
}

void Camera3D::RegisterProperties() {
    Node3D::RegisterProperties();

    RegisterProperty<int>("projection_type", PropertyType::Int,
        [this]() { return static_cast<int>(m_ProjectionType); },
        [this](const int& value) { m_ProjectionType = static_cast<ProjectionType>(value); });

    RegisterProperty<float>("fov", PropertyType::Float,
        [this]() { return m_FOV; },
        [this](const float& value) { m_FOV = value; });

    RegisterProperty<float>("near_plane", PropertyType::Float,
        [this]() { return m_NearPlane; },
        [this](const float& value) { m_NearPlane = value; });

    RegisterProperty<float>("far_plane", PropertyType::Float,
        [this]() { return m_FarPlane; },
        [this](const float& value) { m_FarPlane = value; });

    RegisterProperty<float>("ortho_size", PropertyType::Float,
        [this]() { return m_OrthoSize; },
        [this](const float& value) { m_OrthoSize = value; });

    RegisterProperty<bool>("is_active", PropertyType::Bool,
        [this]() { return m_IsActive; },
        [this](const bool& value) { m_IsActive = value; });
}

void Camera3D::OnRender() {
    Node3D::OnRender();

    if (!DebugDraw::IsAvailable()) {
        return;
    }

    if (!IsVisibleInHierarchy() || !IsActiveInHierarchy()) {
        return;
    }

    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera3D) {
        return;
    }

    math::Vec3 position = GetGlobalPosition();
    math::Quat rotation = GetGlobalRotation();

    math::Vec3 forward = rotation * math::Vec3(0, 0, -1);
    math::Vec3 right = rotation * math::Vec3(1, 0, 0);
    math::Vec3 up = rotation * math::Vec3(0, 1, 0);

    float nearSize = 0.3f;
    float farSize = 0.8f;
    float depth = 1.2f;

    math::Vec3 nearCenter = position + forward * (depth * 0.2f);
    math::Vec3 ntr = nearCenter + right * nearSize + up * nearSize;
    math::Vec3 ntl = nearCenter - right * nearSize + up * nearSize;
    math::Vec3 nbr = nearCenter + right * nearSize - up * nearSize;
    math::Vec3 nbl = nearCenter - right * nearSize - up * nearSize;

    math::Vec3 farCenter = position + forward * depth;
    math::Vec3 ftr = farCenter + right * farSize + up * farSize;
    math::Vec3 ftl = farCenter - right * farSize + up * farSize;
    math::Vec3 fbr = farCenter + right * farSize - up * farSize;
    math::Vec3 fbl = farCenter - right * farSize - up * farSize;

    math::Color cameraColor(0.2f, 0.8f, 1.0f, 1.0f);

    DebugDraw::Line(ntr, ntl, cameraColor);
    DebugDraw::Line(ntl, nbl, cameraColor);
    DebugDraw::Line(nbl, nbr, cameraColor);
    DebugDraw::Line(nbr, ntr, cameraColor);

    DebugDraw::Line(ftr, ftl, cameraColor);
    DebugDraw::Line(ftl, fbl, cameraColor);
    DebugDraw::Line(fbl, fbr, cameraColor);
    DebugDraw::Line(fbr, ftr, cameraColor);

    DebugDraw::Line(ntr, ftr, cameraColor);
    DebugDraw::Line(ntl, ftl, cameraColor);
    DebugDraw::Line(nbr, fbr, cameraColor);
    DebugDraw::Line(nbl, fbl, cameraColor);

    DebugDraw::Axes(position, 0.5f);
}

Camera2D::Camera2D()
    : Node2D(),
      m_Zoom(1.0f),
      m_OrthoSize(1080.0f),
      m_AspectRatio(16.0f / 9.0f),
      m_IsActive(true) {
}

Camera2D::Camera2D(const std::string& name)
    : Node2D(name),
      m_Zoom(1.0f),
      m_OrthoSize(1080.0f),
      m_AspectRatio(16.0f / 9.0f),
      m_IsActive(true) {
}

void Camera2D::RegisterProperties() {
    Node2D::RegisterProperties();

    RegisterProperty<float>("zoom", PropertyType::Float,
        [this]() { return m_Zoom; },
        [this](const float& value) { m_Zoom = value; });

    RegisterProperty<float>("ortho_size", PropertyType::Float,
        [this]() { return m_OrthoSize; },
        [this](const float& value) { m_OrthoSize = value; });

    RegisterProperty<float>("aspect_ratio", PropertyType::Float,
        [this]() { return m_AspectRatio; },
        [this](const float& value) { m_AspectRatio = value; });

    RegisterProperty<bool>("is_active", PropertyType::Bool,
        [this]() { return m_IsActive; },
        [this](const bool& value) { m_IsActive = value; });
}

void Camera2D::OnRender() {
    Node2D::OnRender();

    if (!DebugDraw::IsAvailable()) {
        return;
    }

    if (!IsVisibleInHierarchy() || !IsActiveInHierarchy()) {
        return;
    }

    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera2D) {
        return;
    }

    math::Vec2 position2D = GetGlobalPosition();
    math::Vec3 position(position2D.x, position2D.y, 0.0f);

    float height = m_OrthoSize / m_Zoom;
    float width = height * m_AspectRatio;

    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;

    math::AABB fullBounds(
        math::Vec3(position.x - halfWidth, position.y - halfHeight, 0.0f),
        math::Vec3(position.x + halfWidth, position.y + halfHeight, 0.0f)
    );

    math::Color pinkColor(1.0f, 0.4f, 0.8f, 1.0f);
    DebugDraw::BoundingBox2D(fullBounds, pinkColor, 0.0f);

    float edgeThickness = 1.0f;
    math::AABB bounds(
        math::Vec3(position.x - halfWidth - edgeThickness, position.y - halfHeight, 0.0f),
        math::Vec3(position.x - halfWidth + edgeThickness, position.y + halfHeight, 0.0f)
    );

    float crossSize = 50.0f;
    DebugDraw::Line(position + math::Vec3(-crossSize, 0, 0),
                    position + math::Vec3(crossSize, 0, 0),
                    pinkColor, 0.0f, false);
    DebugDraw::Line(position + math::Vec3(0, -crossSize, 0),
                    position + math::Vec3(0, crossSize, 0),
                    pinkColor, 0.0f, false);
}

math::AABB Camera2D::getWorldBounds() const {
    math::Vec2 position2D = GetGlobalPosition();
    math::Vec3 position(position2D.x, position2D.y, 0.0f);
    float height = m_OrthoSize / m_Zoom;
    float width = height * m_AspectRatio;
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    float edgeThickness = 1.0f;

    return math::AABB(
        math::Vec3(position.x - halfWidth - edgeThickness, position.y - halfHeight, 0.0f),
        math::Vec3(position.x - halfWidth + edgeThickness, position.y + halfHeight, 0.0f)
    );
}

math::AABB CameraUI::getWorldBounds() const {
    float minX = m_Position.x - (m_CanvasSize.x * m_Origin.x);
    float minY = m_Position.y - (m_CanvasSize.y * m_Origin.y);
    float maxX = minX + m_CanvasSize.x;
    float maxY = minY + m_CanvasSize.y;
    float edgeThickness = 1.0f;

    return math::AABB(
        math::Vec3(minX - edgeThickness, minY, 0.0f),
        math::Vec3(minX + edgeThickness, maxY, 0.0f)
    );
}

CameraUI::CameraUI()
    : Node(),
      m_CanvasSize(1920.0f, 1080.0f),
      m_Origin(0.0f, 0.0f),
      m_Position(0.0f, 0.0f),
      m_ScaleFactor(1.0f),
      m_PixelPerfect(false),
      m_IsActive(true) {
}

CameraUI::CameraUI(const std::string& name)
    : Node(name),
      m_CanvasSize(1920.0f, 1080.0f),
      m_Origin(0.0f, 0.0f),
      m_Position(0.0f, 0.0f),
      m_ScaleFactor(1.0f),
      m_PixelPerfect(false),
      m_IsActive(true) {
}

void CameraUI::RegisterProperties() {
    Node::RegisterProperties();

    RegisterProperty<math::Vec2>("canvas_size", PropertyType::Vec2,
        [this]() { return m_CanvasSize; },
        [this](const math::Vec2& value) { m_CanvasSize = value; });

    RegisterProperty<math::Vec2>("origin", PropertyType::Vec2,
        [this]() { return m_Origin; },
        [this](const math::Vec2& value) { m_Origin = value; });

    RegisterProperty<math::Vec2>("position", PropertyType::Vec2,
        [this]() { return m_Position; },
        [this](const math::Vec2& value) { m_Position = value; });

    RegisterProperty<float>("scale_factor", PropertyType::Float,
        [this]() { return m_ScaleFactor; },
        [this](const float& value) { m_ScaleFactor = value; });

    RegisterProperty<bool>("pixel_perfect", PropertyType::Bool,
        [this]() { return m_PixelPerfect; },
        [this](const bool& value) { m_PixelPerfect = value; });

    RegisterProperty<bool>("is_active", PropertyType::Bool,
        [this]() { return m_IsActive; },
        [this](const bool& value) { m_IsActive = value; });
}

void CameraUI::OnRender() {
    Node::OnRender();

    if (!DebugDraw::IsAvailable()) {
        return;
    }

    if (!IsVisibleInHierarchy() || !IsActiveInHierarchy()) {
        return;
    }

    if (DebugDraw::GetCurrentCameraType() != CameraType::Camera2D) {
        return;
    }

    math::Vec3 position(m_Position.x, m_Position.y, 0.0f);

    float width = m_CanvasSize.x * m_ScaleFactor;
    float height = m_CanvasSize.y * m_ScaleFactor;

    float minX = position.x - (width * m_Origin.x);
    float minY = position.y - (height * m_Origin.y);
    float maxX = minX + width;
    float maxY = minY + height;

    math::AABB fullBounds(
        math::Vec3(minX, minY, 0.0f),
        math::Vec3(maxX, maxY, 0.0f)
    );

    math::Color blueColor(0.3f, 0.6f, 1.0f, 1.0f);
    DebugDraw::BoundingBox2D(fullBounds, blueColor, 0.0f);

    float edgeThickness = 1.0f;
    math::AABB bounds(
        math::Vec3(minX - edgeThickness, minY, 0.0f),
        math::Vec3(minX + edgeThickness, maxY, 0.0f)
    );

    float crossSize = 20.0f;
    DebugDraw::Line(position + math::Vec3(-crossSize, 0, 0),
                    position + math::Vec3(crossSize, 0, 0),
                    blueColor, 0.0f, false);
    DebugDraw::Line(position + math::Vec3(0, -crossSize, 0),
                    position + math::Vec3(0, crossSize, 0),
                    blueColor, 0.0f, false);
}

}
}

