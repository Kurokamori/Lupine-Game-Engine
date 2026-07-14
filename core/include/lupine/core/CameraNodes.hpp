#pragma once

#include "lupine/core/Node.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Quat.hpp"

namespace lupine {
namespace core {

/**
 * CameraShake - transient camera shake/rumble state.
 *
 * Produces a per-frame positional offset that decays over its lifetime. The
 * offset is generated from layered sine waves (smooth, deterministic) with a
 * per-instance phase seed so different cameras and successive shakes look
 * distinct. This is purely runtime state and is never serialized.
 */
struct CameraShake {
    float amplitude = 0.0f;   // Peak offset (world units for 2D, canvas px for UI)
    float duration = 0.0f;    // Total duration of the active shake, in seconds
    float timer = 0.0f;       // Remaining time, in seconds
    float frequency = 30.0f;  // Oscillations per second
    float elapsed = 0.0f;     // Running time, drives the noise phase
    float seed = 0.0f;        // Per-instance phase offset so cameras differ
    math::Vec2 offset = math::Vec2(0.0f, 0.0f); // Current computed offset

    // Begin (or reinforce) a shake. A request with a larger amplitude or a longer
    // remaining duration takes over; a weaker request is ignored so a small bump
    // never weakens a big ongoing shake.
    void Trigger(float newAmplitude, float newDuration, float newFrequency);
    // Advance by deltaTime and recompute offset. No-op when inactive.
    void Update(float deltaTime);
    // End immediately and clear the offset.
    void Stop();
    // True while the shake is producing an offset.
    bool IsActive() const { return timer > 0.0f; }
};

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

    // --- Stacked camera effects ------------------------------------------------
    // Master toggle + composite/separate render mode for the CameraEffect components
    // attached to this camera node. See components::CameraEffect.
    bool GetEffectsEnabled() const { return m_EffectsEnabled; }
    void SetEffectsEnabled(bool enabled) { m_EffectsEnabled = enabled; }
    int GetEffectRenderMode() const { return m_EffectRenderMode; }
    void SetEffectRenderMode(int mode) { m_EffectRenderMode = mode; }

    // Debug rendering (called by editor via OnRender)
    void OnRender() override;

protected:
    ProjectionType m_ProjectionType;
    bool m_EffectsEnabled = true;
    int m_EffectRenderMode = 0;  // 0 = Separate, 1 = Composite
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

    // --- Stacked camera effects ------------------------------------------------
    bool GetEffectsEnabled() const { return m_EffectsEnabled; }
    void SetEffectsEnabled(bool enabled) { m_EffectsEnabled = enabled; }
    int GetEffectRenderMode() const { return m_EffectRenderMode; }
    void SetEffectRenderMode(int mode) { m_EffectRenderMode = mode; }

    // Static offset added to the camera centre at render time (world units).
    const math::Vec2& GetOffset() const { return m_Offset; }
    void SetOffset(const math::Vec2& offset) { m_Offset = offset; }

    // --- Follow target ---------------------------------------------------------
    // When enabled, the camera tracks a target node each frame (runtime only).
    bool IsFollowEnabled() const { return m_FollowEnabled; }
    void SetFollowEnabled(bool enabled) { m_FollowEnabled = enabled; }

    // Target referenced by node path or "%UniqueName"; resolved lazily at runtime.
    const std::string& GetFollowTargetPath() const { return m_FollowTargetPath; }
    void SetFollowTargetPath(const std::string& path);

    // Set/clear the follow target directly with a live node (e.g. from scripts).
    void SetFollowTarget(const std::shared_ptr<Node>& target);
    void ClearFollowTarget();

    // Position smoothing (lerp). When off, the camera snaps to the target.
    bool IsFollowSmoothingEnabled() const { return m_FollowSmoothing; }
    void SetFollowSmoothingEnabled(bool enabled) { m_FollowSmoothing = enabled; }

    // Smoothing responsiveness; higher reaches the target faster. Frame-rate
    // independent (used as 1 - exp(-speed * dt)).
    float GetFollowSpeed() const { return m_FollowSpeed; }
    void SetFollowSpeed(float speed) { m_FollowSpeed = speed; }

    // --- Drag margins ----------------------------------------------------------
    // A dead zone (fraction of the half-view 0..1) the target may move within
    // before the camera starts to follow on that axis.
    bool IsDragHorizontalEnabled() const { return m_DragHorizontalEnabled; }
    void SetDragHorizontalEnabled(bool enabled) { m_DragHorizontalEnabled = enabled; }
    bool IsDragVerticalEnabled() const { return m_DragVerticalEnabled; }
    void SetDragVerticalEnabled(bool enabled) { m_DragVerticalEnabled = enabled; }

    float GetDragLeft() const { return m_DragLeft; }
    void SetDragLeft(float v) { m_DragLeft = v; }
    float GetDragRight() const { return m_DragRight; }
    void SetDragRight(float v) { m_DragRight = v; }
    float GetDragTop() const { return m_DragTop; }
    void SetDragTop(float v) { m_DragTop = v; }
    float GetDragBottom() const { return m_DragBottom; }
    void SetDragBottom(float v) { m_DragBottom = v; }

    // --- Limits ----------------------------------------------------------------
    // Clamp the visible region so its edges stay within these world coordinates.
    bool IsLimitEnabled() const { return m_LimitEnabled; }
    void SetLimitEnabled(bool enabled) { m_LimitEnabled = enabled; }

    float GetLimitLeft() const { return m_LimitLeft; }
    void SetLimitLeft(float v) { m_LimitLeft = v; }
    float GetLimitRight() const { return m_LimitRight; }
    void SetLimitRight(float v) { m_LimitRight = v; }
    float GetLimitTop() const { return m_LimitTop; }
    void SetLimitTop(float v) { m_LimitTop = v; }
    float GetLimitBottom() const { return m_LimitBottom; }
    void SetLimitBottom(float v) { m_LimitBottom = v; }

    // --- Shake / rumble --------------------------------------------------------
    void Shake(float amplitude, float duration, float frequency = 30.0f);
    void StopShake();
    bool IsShaking() const { return m_Shake.IsActive(); }

    // Half-extents (half width, half height) of the visible region in world units,
    // accounting for ortho size, aspect ratio and zoom.
    math::Vec2 GetViewHalfExtents() const;

    // The world-space centre the renderer should use this frame: the clamped
    // global position plus the static offset plus the current shake offset.
    math::Vec2 GetEffectivePosition() const;

    // Per-frame update (follow, smoothing, drag, limits, shake). Runtime only.
    void OnProcess(float deltaTime) override;

    // Debug rendering (called by editor via OnRender)
    void OnRender() override;

    math::AABB getWorldBounds() const;

protected:
    // Resolve the follow target node (cached weak ref, else by path). May return null.
    Node* ResolveFollowTarget();
    // Clamp a desired camera centre so the visible region stays within the limits.
    math::Vec2 ClampCenterToLimits(const math::Vec2& center) const;

    float m_Zoom;
    float m_OrthoSize;
    float m_AspectRatio;  // Width / Height ratio
    bool m_IsActive;
    bool m_EffectsEnabled = true;
    int m_EffectRenderMode = 0;  // 0 = Separate, 1 = Composite

    math::Vec2 m_Offset;

    bool m_FollowEnabled;
    std::string m_FollowTargetPath;
    std::weak_ptr<Node> m_FollowTarget;
    bool m_FollowSmoothing;
    float m_FollowSpeed;

    bool m_DragHorizontalEnabled;
    bool m_DragVerticalEnabled;
    float m_DragLeft;
    float m_DragRight;
    float m_DragTop;
    float m_DragBottom;

    bool m_LimitEnabled;
    float m_LimitLeft;
    float m_LimitRight;
    float m_LimitTop;
    float m_LimitBottom;

    CameraShake m_Shake;
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

    // --- Stacked camera effects ------------------------------------------------
    bool GetEffectsEnabled() const { return m_EffectsEnabled; }
    void SetEffectsEnabled(bool enabled) { m_EffectsEnabled = enabled; }
    int GetEffectRenderMode() const { return m_EffectRenderMode; }
    void SetEffectRenderMode(int mode) { m_EffectRenderMode = mode; }

    // Position in screen space (for moving the camera)
    const math::Vec2& GetPosition() const { return m_Position; }
    void SetPosition(const math::Vec2& position) { m_Position = position; }

    // Rotation of the whole canvas about its origin, in radians (matches the
    // Camera2D/Node2D rotation convention). Rotates all UI drawn through this camera.
    float GetRotation() const { return m_Rotation; }
    void SetRotation(float rotation) { m_Rotation = rotation; }

    // Uniform zoom of the whole canvas about its origin: 1 = normal, 2 = zoom in 2x
    // (UI appears twice as big), 0.5 = zoom out. Distinct from scale_factor (HiDPI).
    float GetZoom() const { return m_Zoom; }
    void SetZoom(float zoom) { m_Zoom = zoom; }

    // Static offset added to the canvas position at render time (canvas pixels).
    const math::Vec2& GetOffset() const { return m_Offset; }
    void SetOffset(const math::Vec2& offset) { m_Offset = offset; }

    // --- Follow target ---------------------------------------------------------
    // When enabled, the canvas position tracks a target node each frame (runtime).
    bool IsFollowEnabled() const { return m_FollowEnabled; }
    void SetFollowEnabled(bool enabled) { m_FollowEnabled = enabled; }

    const std::string& GetFollowTargetPath() const { return m_FollowTargetPath; }
    void SetFollowTargetPath(const std::string& path);

    void SetFollowTarget(const std::shared_ptr<Node>& target);
    void ClearFollowTarget();

    bool IsFollowSmoothingEnabled() const { return m_FollowSmoothing; }
    void SetFollowSmoothingEnabled(bool enabled) { m_FollowSmoothing = enabled; }

    float GetFollowSpeed() const { return m_FollowSpeed; }
    void SetFollowSpeed(float speed) { m_FollowSpeed = speed; }

    // Smoothly move the canvas to a target position (lerp). Enables follow toward
    // a fixed point; handy for UI slide transitions from scripts.
    void SmoothMoveTo(const math::Vec2& target, float speed);

    // --- Drag margins ----------------------------------------------------------
    bool IsDragHorizontalEnabled() const { return m_DragHorizontalEnabled; }
    void SetDragHorizontalEnabled(bool enabled) { m_DragHorizontalEnabled = enabled; }
    bool IsDragVerticalEnabled() const { return m_DragVerticalEnabled; }
    void SetDragVerticalEnabled(bool enabled) { m_DragVerticalEnabled = enabled; }

    float GetDragLeft() const { return m_DragLeft; }
    void SetDragLeft(float v) { m_DragLeft = v; }
    float GetDragRight() const { return m_DragRight; }
    void SetDragRight(float v) { m_DragRight = v; }
    float GetDragTop() const { return m_DragTop; }
    void SetDragTop(float v) { m_DragTop = v; }
    float GetDragBottom() const { return m_DragBottom; }
    void SetDragBottom(float v) { m_DragBottom = v; }

    // --- Limits ----------------------------------------------------------------
    bool IsLimitEnabled() const { return m_LimitEnabled; }
    void SetLimitEnabled(bool enabled) { m_LimitEnabled = enabled; }

    float GetLimitLeft() const { return m_LimitLeft; }
    void SetLimitLeft(float v) { m_LimitLeft = v; }
    float GetLimitRight() const { return m_LimitRight; }
    void SetLimitRight(float v) { m_LimitRight = v; }
    float GetLimitTop() const { return m_LimitTop; }
    void SetLimitTop(float v) { m_LimitTop = v; }
    float GetLimitBottom() const { return m_LimitBottom; }
    void SetLimitBottom(float v) { m_LimitBottom = v; }

    // --- Shake / rumble --------------------------------------------------------
    void Shake(float amplitude, float duration, float frequency = 30.0f);
    void StopShake();
    bool IsShaking() const { return m_Shake.IsActive(); }

    // Half-extents (half the canvas size) used for drag margins and limits.
    math::Vec2 GetViewHalfExtents() const;

    // The screen-space position the renderer should use this frame: the clamped
    // position plus the static offset plus the current shake offset.
    math::Vec2 GetEffectivePosition() const;

    // Per-frame update (follow, smoothing, drag, limits, shake). Runtime only.
    void OnProcess(float deltaTime) override;

    // Debug rendering (called by editor via OnRender)
    void OnRender() override;

    math::AABB getWorldBounds() const;

protected:
    Node* ResolveFollowTarget();
    math::Vec2 ClampCenterToLimits(const math::Vec2& center) const;

    math::Vec2 m_CanvasSize;
    math::Vec2 m_Origin;
    math::Vec2 m_Position;  // Screen-space position
    float m_Rotation;       // Canvas rotation about the origin, radians
    float m_Zoom;           // Uniform canvas zoom about the origin
    float m_ScaleFactor;
    bool m_PixelPerfect;
    bool m_IsActive;
    bool m_EffectsEnabled = true;
    int m_EffectRenderMode = 0;  // 0 = Separate, 1 = Composite

    math::Vec2 m_Offset;

    bool m_FollowEnabled;
    std::string m_FollowTargetPath;
    std::weak_ptr<Node> m_FollowTarget;
    bool m_FollowSmoothing;
    float m_FollowSpeed;
    bool m_HasSmoothTarget;
    math::Vec2 m_SmoothTarget;

    bool m_DragHorizontalEnabled;
    bool m_DragVerticalEnabled;
    float m_DragLeft;
    float m_DragRight;
    float m_DragTop;
    float m_DragBottom;

    bool m_LimitEnabled;
    float m_LimitLeft;
    float m_LimitRight;
    float m_LimitTop;
    float m_LimitBottom;

    CameraShake m_Shake;
};

} // namespace core
} // namespace lupine

