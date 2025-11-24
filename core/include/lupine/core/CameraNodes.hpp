#pragma once

#include "lupine/core/Node.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Quat.hpp"

namespace lupine {
namespace core {

/**
 * Camera3D - 3D camera node
 * Inherits from Node3D to get 3D transform (position, rotation, scale)
 * Used for rendering 3D scenes with perspective or orthographic projection
 */
class Camera3D : public Node3D {
public:
    Camera3D();
    explicit Camera3D(const std::string& name);

    std::string GetTypeName() const override { return "Camera3D"; }
    void RegisterProperties() override;

    // Projection type
    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    ProjectionType GetProjectionType() const { return m_ProjectionType; }
    void SetProjectionType(ProjectionType type) { m_ProjectionType = type; }

    // Perspective settings
    float GetFOV() const { return m_FOV; }
    void SetFOV(float fov) { m_FOV = fov; }

    float GetNearPlane() const { return m_NearPlane; }
    void SetNearPlane(float nearPlane) { m_NearPlane = nearPlane; }

    float GetFarPlane() const { return m_FarPlane; }
    void SetFarPlane(float farPlane) { m_FarPlane = farPlane; }

    // Orthographic settings
    float GetOrthoSize() const { return m_OrthoSize; }
    void SetOrthoSize(float size) { m_OrthoSize = size; }

    // Active camera flag
    bool IsActive() const { return m_IsActive; }
    void SetActive(bool active) { m_IsActive = active; }

    // Debug rendering (called by editor via OnRender)
    void OnRender() override;

protected:
    ProjectionType m_ProjectionType;
    float m_FOV;           // Field of view in degrees (for perspective)
    float m_NearPlane;
    float m_FarPlane;
    float m_OrthoSize;     // Height of orthographic view in world units
    bool m_IsActive;       // Whether this camera is active for rendering
};

/**
 * Camera2D - 2D camera node
 * Inherits from Node2D to get 2D transform (position, rotation, scale)
 * Used for rendering 2D scenes with orthographic projection
 */
class Camera2D : public Node2D {
public:
    Camera2D();
    explicit Camera2D(const std::string& name);

    std::string GetTypeName() const override { return "Camera2D"; }
    void RegisterProperties() override;

    // Zoom level (1.0 = normal, 2.0 = 2x zoom in, 0.5 = 2x zoom out)
    float GetZoom() const { return m_Zoom; }
    void SetZoom(float zoom) { m_Zoom = zoom; }

    // Orthographic size (height of view in world units)
    float GetOrthoSize() const { return m_OrthoSize; }
    void SetOrthoSize(float size) { m_OrthoSize = size; }

    // Aspect ratio (width / height) - defaults to 16:9 to match project settings
    float GetAspectRatio() const { return m_AspectRatio; }
    void SetAspectRatio(float aspectRatio) { m_AspectRatio = aspectRatio; }

    // Active camera flag
    bool IsActive() const { return m_IsActive; }
    void SetActive(bool active) { m_IsActive = active; }

    // Debug rendering (called by editor via OnRender)
    void OnRender() override;

    math::AABB getWorldBounds() const;

protected:
    float m_Zoom;
    float m_OrthoSize;
    float m_AspectRatio;  // Width / Height ratio
    bool m_IsActive;
};

/**
 * CameraUI - UI/Canvas camera node
 * Inherits from Node (base node) since UI is typically screen-space
 * Used for rendering UI elements in screen space
 */
class CameraUI : public Node {
public:
    CameraUI();
    explicit CameraUI(const std::string& name);

    std::string GetTypeName() const override { return "CameraUI"; }
    void RegisterProperties() override;

    // Canvas size (in logical pixels)
    const math::Vec2& GetCanvasSize() const { return m_CanvasSize; }
    void SetCanvasSize(const math::Vec2& size) { m_CanvasSize = size; }

    // Origin position (0,0 = top-left, 1,1 = bottom-right)
    const math::Vec2& GetOrigin() const { return m_Origin; }
    void SetOrigin(const math::Vec2& origin) { m_Origin = origin; }

    // Scale factor for HiDPI displays
    float GetScaleFactor() const { return m_ScaleFactor; }
    void SetScaleFactor(float factor) { m_ScaleFactor = factor; }

    // Pixel-perfect rendering
    bool IsPixelPerfect() const { return m_PixelPerfect; }
    void SetPixelPerfect(bool pixelPerfect) { m_PixelPerfect = pixelPerfect; }

    // Active camera flag
    bool IsActive() const { return m_IsActive; }
    void SetActive(bool active) { m_IsActive = active; }

    // Position in screen space (for moving the camera)
    const math::Vec2& GetPosition() const { return m_Position; }
    void SetPosition(const math::Vec2& position) { m_Position = position; }

    // Debug rendering (called by editor via OnRender)
    void OnRender() override;

    math::AABB getWorldBounds() const;

protected:
    math::Vec2 m_CanvasSize;
    math::Vec2 m_Origin;
    math::Vec2 m_Position;  // Screen-space position
    float m_ScaleFactor;
    bool m_PixelPerfect;
    bool m_IsActive;
};

} // namespace core
} // namespace lupine

