#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/math/MathCommon.hpp"
#include "lupine/math/Mat4.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lupine {
namespace core {

namespace {

// Drag-margin follow on a single axis. Returns the desired camera-centre coord.
// While the target stays inside the dead zone [cam - half*before, cam + half*after]
// the camera holds; once the target leaves it, the camera moves just enough to pin
// the target to that edge. With drag disabled the camera snaps to the target.
float ComputeDragAxis(float cam, float target, float half, bool dragEnabled,
                      float beforeFrac, float afterFrac) {
    if (!dragEnabled) {
        return target;
    }
    float lo = cam - half * beforeFrac;
    float hi = cam + half * afterFrac;
    if (target < lo) {
        return target + half * beforeFrac;
    }
    if (target > hi) {
        return target - half * afterFrac;
    }
    return cam;
}

// Clamp a centre coord so the visible span [center-half, center+half] stays within
// [minLimit, maxLimit]. When the limit range is narrower than the view, centre it.
float ClampAxisToLimits(float center, float half, float minLimit, float maxLimit) {
    float lo = minLimit + half;
    float hi = maxLimit - half;
    if (lo > hi) {
        return (minLimit + maxLimit) * 0.5f;
    }
    return math::Clamp(center, lo, hi);
}

} // namespace

void CameraShake::Trigger(float newAmplitude, float newDuration, float newFrequency) {
    if (newAmplitude <= 0.0f || newDuration <= 0.0f) {
        return;
    }
    bool wasActive = IsActive();
    amplitude = wasActive ? std::max(amplitude, newAmplitude) : newAmplitude;
    timer = wasActive ? std::max(timer, newDuration) : newDuration;
    duration = std::max(timer, wasActive ? duration : newDuration);
    if (newFrequency > 0.0f) {
        frequency = newFrequency;
    }
    if (!wasActive) {
        elapsed = 0.0f;
    }
}

void CameraShake::Update(float deltaTime) {
    if (timer <= 0.0f) {
        offset = math::Vec2(0.0f, 0.0f);
        return;
    }
    timer -= deltaTime;
    elapsed += deltaTime;
    if (timer <= 0.0f) {
        timer = 0.0f;
        offset = math::Vec2(0.0f, 0.0f);
        return;
    }

    float decay = (duration > 0.0f) ? math::Clamp(timer / duration, 0.0f, 1.0f) : 0.0f;
    float amp = amplitude * decay * decay;

    float phase = elapsed * frequency * math::TWO_PI;
    float ox = std::sin(phase + seed * 12.9898f) * 0.6f
             + std::sin(phase * 2.17f + seed * 4.1414f) * 0.4f;
    float oy = std::cos(phase * 1.13f + seed * 7.233f) * 0.6f
             + std::sin(phase * 1.87f + seed * 9.512f) * 0.4f;

    offset = math::Vec2(ox * amp, oy * amp);
}

void CameraShake::Stop() {
    timer = 0.0f;
    duration = 0.0f;
    amplitude = 0.0f;
    offset = math::Vec2(0.0f, 0.0f);
}

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

    RegisterProperty<bool>("effects_enabled", PropertyType::Bool,
        [this]() { return m_EffectsEnabled; },
        [this](const bool& value) { m_EffectsEnabled = value; });

    RegisterProperty<int>("effect_render_mode", PropertyType::Int,
        [this]() { return m_EffectRenderMode; },
        [this](const int& value) { m_EffectRenderMode = value; });
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
      m_IsActive(true),
      m_Offset(0.0f, 0.0f),
      m_FollowEnabled(false),
      m_FollowSmoothing(true),
      m_FollowSpeed(5.0f),
      m_DragHorizontalEnabled(false),
      m_DragVerticalEnabled(false),
      m_DragLeft(0.2f),
      m_DragRight(0.2f),
      m_DragTop(0.2f),
      m_DragBottom(0.2f),
      m_LimitEnabled(false),
      m_LimitLeft(-1.0e9f),
      m_LimitRight(1.0e9f),
      m_LimitTop(-1.0e9f),
      m_LimitBottom(1.0e9f) {
    m_Shake.seed = static_cast<float>(reinterpret_cast<std::uintptr_t>(this) & 0xFFFFu) * 0.001f;
}

Camera2D::Camera2D(const std::string& name)
    : Node2D(name),
      m_Zoom(1.0f),
      m_OrthoSize(1080.0f),
      m_AspectRatio(16.0f / 9.0f),
      m_IsActive(true),
      m_Offset(0.0f, 0.0f),
      m_FollowEnabled(false),
      m_FollowSmoothing(true),
      m_FollowSpeed(5.0f),
      m_DragHorizontalEnabled(false),
      m_DragVerticalEnabled(false),
      m_DragLeft(0.2f),
      m_DragRight(0.2f),
      m_DragTop(0.2f),
      m_DragBottom(0.2f),
      m_LimitEnabled(false),
      m_LimitLeft(-1.0e9f),
      m_LimitRight(1.0e9f),
      m_LimitTop(-1.0e9f),
      m_LimitBottom(1.0e9f) {
    m_Shake.seed = static_cast<float>(reinterpret_cast<std::uintptr_t>(this) & 0xFFFFu) * 0.001f;
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

    RegisterProperty<math::Vec2>("offset", PropertyType::Vec2,
        [this]() { return m_Offset; },
        [this](const math::Vec2& value) { m_Offset = value; });

    RegisterProperty<bool>("follow_enabled", PropertyType::Bool,
        [this]() { return m_FollowEnabled; },
        [this](const bool& value) { m_FollowEnabled = value; });

    RegisterProperty<std::string>("follow_target", PropertyType::String,
        [this]() { return m_FollowTargetPath; },
        [this](const std::string& value) { SetFollowTargetPath(value); });

    RegisterProperty<bool>("follow_smoothing", PropertyType::Bool,
        [this]() { return m_FollowSmoothing; },
        [this](const bool& value) { m_FollowSmoothing = value; });

    RegisterProperty<float>("follow_speed", PropertyType::Float,
        [this]() { return m_FollowSpeed; },
        [this](const float& value) { m_FollowSpeed = value; });

    RegisterProperty<bool>("drag_horizontal_enabled", PropertyType::Bool,
        [this]() { return m_DragHorizontalEnabled; },
        [this](const bool& value) { m_DragHorizontalEnabled = value; });

    RegisterProperty<bool>("drag_vertical_enabled", PropertyType::Bool,
        [this]() { return m_DragVerticalEnabled; },
        [this](const bool& value) { m_DragVerticalEnabled = value; });

    RegisterProperty<float>("drag_left", PropertyType::Float,
        [this]() { return m_DragLeft; },
        [this](const float& value) { m_DragLeft = value; });

    RegisterProperty<float>("drag_right", PropertyType::Float,
        [this]() { return m_DragRight; },
        [this](const float& value) { m_DragRight = value; });

    RegisterProperty<float>("drag_top", PropertyType::Float,
        [this]() { return m_DragTop; },
        [this](const float& value) { m_DragTop = value; });

    RegisterProperty<float>("drag_bottom", PropertyType::Float,
        [this]() { return m_DragBottom; },
        [this](const float& value) { m_DragBottom = value; });

    RegisterProperty<bool>("limit_enabled", PropertyType::Bool,
        [this]() { return m_LimitEnabled; },
        [this](const bool& value) { m_LimitEnabled = value; });

    RegisterProperty<float>("limit_left", PropertyType::Float,
        [this]() { return m_LimitLeft; },
        [this](const float& value) { m_LimitLeft = value; });

    RegisterProperty<float>("limit_right", PropertyType::Float,
        [this]() { return m_LimitRight; },
        [this](const float& value) { m_LimitRight = value; });

    RegisterProperty<float>("limit_top", PropertyType::Float,
        [this]() { return m_LimitTop; },
        [this](const float& value) { m_LimitTop = value; });

    RegisterProperty<float>("limit_bottom", PropertyType::Float,
        [this]() { return m_LimitBottom; },
        [this](const float& value) { m_LimitBottom = value; });

    RegisterProperty<bool>("effects_enabled", PropertyType::Bool,
        [this]() { return m_EffectsEnabled; },
        [this](const bool& value) { m_EffectsEnabled = value; });

    RegisterProperty<int>("effect_render_mode", PropertyType::Int,
        [this]() { return m_EffectRenderMode; },
        [this](const int& value) { m_EffectRenderMode = value; });
}

void Camera2D::SetFollowTargetPath(const std::string& path) {
    m_FollowTargetPath = path;
    m_FollowTarget.reset();
}

void Camera2D::SetFollowTarget(const std::shared_ptr<Node>& target) {
    m_FollowTarget = target;
    m_FollowTargetPath.clear();
    m_FollowEnabled = (target != nullptr);
}

void Camera2D::ClearFollowTarget() {
    m_FollowTarget.reset();
    m_FollowTargetPath.clear();
    m_FollowEnabled = false;
}

Node* Camera2D::ResolveFollowTarget() {
    if (auto sp = m_FollowTarget.lock()) {
        return sp.get();
    }
    if (m_FollowTargetPath.empty()) {
        return nullptr;
    }

    std::shared_ptr<Node> found;
    if (m_FollowTargetPath.front() == '%') {
        Node* unique = ResolveUniqueName(m_FollowTargetPath.substr(1));
        if (unique) {
            found = unique->shared_from_this();
        }
    } else {
        found = FindNode(m_FollowTargetPath);
    }

    if (found) {
        m_FollowTarget = found;
        return found.get();
    }
    return nullptr;
}

math::Vec2 Camera2D::GetViewHalfExtents() const {
    float zoom = (m_Zoom != 0.0f) ? m_Zoom : 1.0f;
    float halfHeight = (m_OrthoSize * 0.5f) / zoom;
    float halfWidth = halfHeight * m_AspectRatio;
    return math::Vec2(halfWidth, halfHeight);
}

math::Vec2 Camera2D::ClampCenterToLimits(const math::Vec2& center) const {
    if (!m_LimitEnabled) {
        return center;
    }
    math::Vec2 half = GetViewHalfExtents();
    return math::Vec2(
        ClampAxisToLimits(center.x, half.x, m_LimitLeft, m_LimitRight),
        ClampAxisToLimits(center.y, half.y, m_LimitTop, m_LimitBottom)
    );
}

math::Vec2 Camera2D::GetEffectivePosition() const {
    math::Vec2 center = GetGlobalPosition() + m_Offset;
    center = ClampCenterToLimits(center);
    return center + m_Shake.offset;
}

void Camera2D::Shake(float amplitude, float duration, float frequency) {
    m_Shake.Trigger(amplitude, duration, frequency);
}

void Camera2D::StopShake() {
    m_Shake.Stop();
}

void Camera2D::OnProcess(float deltaTime) {
    Scene* scene = GetScene();
    bool runtime = (scene != nullptr) && !scene->IsInEditor();

    if (runtime) {
        if (m_FollowEnabled) {
            Node2D* target2D = dynamic_cast<Node2D*>(ResolveFollowTarget());
            if (target2D) {
                math::Vec2 targetPos = target2D->GetGlobalPosition();
                math::Vec2 camGlobal = GetGlobalPosition();
                math::Vec2 half = GetViewHalfExtents();

                math::Vec2 desired(
                    ComputeDragAxis(camGlobal.x, targetPos.x, half.x,
                                    m_DragHorizontalEnabled, m_DragLeft, m_DragRight),
                    ComputeDragAxis(camGlobal.y, targetPos.y, half.y,
                                    m_DragVerticalEnabled, m_DragTop, m_DragBottom)
                );

                math::Vec2 newGlobal = desired;
                if (m_FollowSmoothing && m_FollowSpeed > 0.0f) {
                    float t = 1.0f - std::exp(-m_FollowSpeed * deltaTime);
                    newGlobal = camGlobal.Lerp(desired, t);
                }

                math::Vec2 delta = newGlobal - camGlobal;
                SetPosition(GetPosition() + delta);
            }
        }

        if (m_LimitEnabled) {
            math::Vec2 camGlobal = GetGlobalPosition();
            math::Vec2 clamped = ClampCenterToLimits(camGlobal);
            math::Vec2 delta = clamped - camGlobal;
            if (delta.x != 0.0f || delta.y != 0.0f) {
                SetPosition(GetPosition() + delta);
            }
        }

        m_Shake.Update(deltaTime);
    } else {
        m_Shake.Stop();
    }

    Node2D::OnProcess(deltaTime);
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

    // The framed region accounts for zoom; its corners are the visible world
    // rectangle, which counter-rotates against the camera's rotation (the view turns
    // the scene by +rotation, so the captured world region turns by -rotation). This
    // makes the editor preview enclose exactly what the camera will show at runtime.
    math::Vec2 half = GetViewHalfExtents();
    float halfWidth = half.x;
    float halfHeight = half.y;

    float rot = GetGlobalRotation();
    float cosR = std::cos(rot);
    float sinR = std::sin(rot);
    auto frameCorner = [&](float lx, float ly) -> math::Vec3 {
        return position + math::Vec3(lx * cosR + ly * sinR, -lx * sinR + ly * cosR, 0.0f);
    };

    math::Vec3 tl = frameCorner(-halfWidth, halfHeight);
    math::Vec3 tr = frameCorner(halfWidth, halfHeight);
    math::Vec3 br = frameCorner(halfWidth, -halfHeight);
    math::Vec3 bl = frameCorner(-halfWidth, -halfHeight);

    math::Color pinkColor(1.0f, 0.4f, 0.8f, 1.0f);
    DebugDraw::Line(tl, tr, pinkColor, 0.0f, false);
    DebugDraw::Line(tr, br, pinkColor, 0.0f, false);
    DebugDraw::Line(br, bl, pinkColor, 0.0f, false);
    DebugDraw::Line(bl, tl, pinkColor, 0.0f, false);

    float crossSize = 50.0f;
    DebugDraw::Line(frameCorner(-crossSize, 0.0f), frameCorner(crossSize, 0.0f),
                    pinkColor, 0.0f, false);
    DebugDraw::Line(frameCorner(0.0f, -crossSize), frameCorner(0.0f, crossSize),
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
      m_Origin(0.5f, 0.5f),
      m_Position(0.0f, 0.0f),
      m_Rotation(0.0f),
      m_Zoom(1.0f),
      m_ScaleFactor(1.0f),
      m_PixelPerfect(false),
      m_IsActive(true),
      m_Offset(0.0f, 0.0f),
      m_FollowEnabled(false),
      m_FollowSmoothing(true),
      m_FollowSpeed(5.0f),
      m_HasSmoothTarget(false),
      m_SmoothTarget(0.0f, 0.0f),
      m_DragHorizontalEnabled(false),
      m_DragVerticalEnabled(false),
      m_DragLeft(0.2f),
      m_DragRight(0.2f),
      m_DragTop(0.2f),
      m_DragBottom(0.2f),
      m_LimitEnabled(false),
      m_LimitLeft(-1.0e9f),
      m_LimitRight(1.0e9f),
      m_LimitTop(-1.0e9f),
      m_LimitBottom(1.0e9f) {
    m_Shake.seed = static_cast<float>(reinterpret_cast<std::uintptr_t>(this) & 0xFFFFu) * 0.001f;
}

CameraUI::CameraUI(const std::string& name)
    : Node(name),
      m_CanvasSize(1920.0f, 1080.0f),
      m_Origin(0.5f, 0.5f),
      m_Position(0.0f, 0.0f),
      m_Rotation(0.0f),
      m_Zoom(1.0f),
      m_ScaleFactor(1.0f),
      m_PixelPerfect(false),
      m_IsActive(true),
      m_Offset(0.0f, 0.0f),
      m_FollowEnabled(false),
      m_FollowSmoothing(true),
      m_FollowSpeed(5.0f),
      m_HasSmoothTarget(false),
      m_SmoothTarget(0.0f, 0.0f),
      m_DragHorizontalEnabled(false),
      m_DragVerticalEnabled(false),
      m_DragLeft(0.2f),
      m_DragRight(0.2f),
      m_DragTop(0.2f),
      m_DragBottom(0.2f),
      m_LimitEnabled(false),
      m_LimitLeft(-1.0e9f),
      m_LimitRight(1.0e9f),
      m_LimitTop(-1.0e9f),
      m_LimitBottom(1.0e9f) {
    m_Shake.seed = static_cast<float>(reinterpret_cast<std::uintptr_t>(this) & 0xFFFFu) * 0.001f;
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

    RegisterProperty<float>("rotation", PropertyType::Float,
        [this]() { return m_Rotation; },
        [this](const float& value) { m_Rotation = value; });

    RegisterProperty<float>("zoom", PropertyType::Float,
        [this]() { return m_Zoom; },
        [this](const float& value) { m_Zoom = value; });

    RegisterProperty<float>("scale_factor", PropertyType::Float,
        [this]() { return m_ScaleFactor; },
        [this](const float& value) { m_ScaleFactor = value; });

    RegisterProperty<bool>("pixel_perfect", PropertyType::Bool,
        [this]() { return m_PixelPerfect; },
        [this](const bool& value) { m_PixelPerfect = value; });

    RegisterProperty<bool>("is_active", PropertyType::Bool,
        [this]() { return m_IsActive; },
        [this](const bool& value) { m_IsActive = value; });

    RegisterProperty<math::Vec2>("offset", PropertyType::Vec2,
        [this]() { return m_Offset; },
        [this](const math::Vec2& value) { m_Offset = value; });

    RegisterProperty<bool>("follow_enabled", PropertyType::Bool,
        [this]() { return m_FollowEnabled; },
        [this](const bool& value) { m_FollowEnabled = value; });

    RegisterProperty<std::string>("follow_target", PropertyType::String,
        [this]() { return m_FollowTargetPath; },
        [this](const std::string& value) { SetFollowTargetPath(value); });

    RegisterProperty<bool>("follow_smoothing", PropertyType::Bool,
        [this]() { return m_FollowSmoothing; },
        [this](const bool& value) { m_FollowSmoothing = value; });

    RegisterProperty<float>("follow_speed", PropertyType::Float,
        [this]() { return m_FollowSpeed; },
        [this](const float& value) { m_FollowSpeed = value; });

    RegisterProperty<bool>("drag_horizontal_enabled", PropertyType::Bool,
        [this]() { return m_DragHorizontalEnabled; },
        [this](const bool& value) { m_DragHorizontalEnabled = value; });

    RegisterProperty<bool>("drag_vertical_enabled", PropertyType::Bool,
        [this]() { return m_DragVerticalEnabled; },
        [this](const bool& value) { m_DragVerticalEnabled = value; });

    RegisterProperty<float>("drag_left", PropertyType::Float,
        [this]() { return m_DragLeft; },
        [this](const float& value) { m_DragLeft = value; });

    RegisterProperty<float>("drag_right", PropertyType::Float,
        [this]() { return m_DragRight; },
        [this](const float& value) { m_DragRight = value; });

    RegisterProperty<float>("drag_top", PropertyType::Float,
        [this]() { return m_DragTop; },
        [this](const float& value) { m_DragTop = value; });

    RegisterProperty<float>("drag_bottom", PropertyType::Float,
        [this]() { return m_DragBottom; },
        [this](const float& value) { m_DragBottom = value; });

    RegisterProperty<bool>("limit_enabled", PropertyType::Bool,
        [this]() { return m_LimitEnabled; },
        [this](const bool& value) { m_LimitEnabled = value; });

    RegisterProperty<float>("limit_left", PropertyType::Float,
        [this]() { return m_LimitLeft; },
        [this](const float& value) { m_LimitLeft = value; });

    RegisterProperty<float>("limit_right", PropertyType::Float,
        [this]() { return m_LimitRight; },
        [this](const float& value) { m_LimitRight = value; });

    RegisterProperty<float>("limit_top", PropertyType::Float,
        [this]() { return m_LimitTop; },
        [this](const float& value) { m_LimitTop = value; });

    RegisterProperty<float>("limit_bottom", PropertyType::Float,
        [this]() { return m_LimitBottom; },
        [this](const float& value) { m_LimitBottom = value; });

    RegisterProperty<bool>("effects_enabled", PropertyType::Bool,
        [this]() { return m_EffectsEnabled; },
        [this](const bool& value) { m_EffectsEnabled = value; });

    RegisterProperty<int>("effect_render_mode", PropertyType::Int,
        [this]() { return m_EffectRenderMode; },
        [this](const int& value) { m_EffectRenderMode = value; });
}

void CameraUI::SetFollowTargetPath(const std::string& path) {
    m_FollowTargetPath = path;
    m_FollowTarget.reset();
}

void CameraUI::SetFollowTarget(const std::shared_ptr<Node>& target) {
    m_FollowTarget = target;
    m_FollowTargetPath.clear();
    m_FollowEnabled = (target != nullptr);
    m_HasSmoothTarget = false;
}

void CameraUI::ClearFollowTarget() {
    m_FollowTarget.reset();
    m_FollowTargetPath.clear();
    m_FollowEnabled = false;
}

void CameraUI::SmoothMoveTo(const math::Vec2& target, float speed) {
    m_SmoothTarget = target;
    m_HasSmoothTarget = true;
    m_FollowSmoothing = true;
    if (speed > 0.0f) {
        m_FollowSpeed = speed;
    }
}

Node* CameraUI::ResolveFollowTarget() {
    if (auto sp = m_FollowTarget.lock()) {
        return sp.get();
    }
    if (m_FollowTargetPath.empty()) {
        return nullptr;
    }

    std::shared_ptr<Node> found;
    if (m_FollowTargetPath.front() == '%') {
        Node* unique = ResolveUniqueName(m_FollowTargetPath.substr(1));
        if (unique) {
            found = unique->shared_from_this();
        }
    } else {
        found = FindNode(m_FollowTargetPath);
    }

    if (found) {
        m_FollowTarget = found;
        return found.get();
    }
    return nullptr;
}

math::Vec2 CameraUI::GetViewHalfExtents() const {
    return math::Vec2(m_CanvasSize.x * 0.5f, m_CanvasSize.y * 0.5f);
}

math::Vec2 CameraUI::ClampCenterToLimits(const math::Vec2& center) const {
    if (!m_LimitEnabled) {
        return center;
    }
    math::Vec2 half = GetViewHalfExtents();
    return math::Vec2(
        ClampAxisToLimits(center.x, half.x, m_LimitLeft, m_LimitRight),
        ClampAxisToLimits(center.y, half.y, m_LimitTop, m_LimitBottom)
    );
}

math::Vec2 CameraUI::GetEffectivePosition() const {
    math::Vec2 center = m_Position + m_Offset;
    center = ClampCenterToLimits(center);
    return center + m_Shake.offset;
}

void CameraUI::Shake(float amplitude, float duration, float frequency) {
    m_Shake.Trigger(amplitude, duration, frequency);
}

void CameraUI::StopShake() {
    m_Shake.Stop();
}

void CameraUI::OnProcess(float deltaTime) {
    Scene* scene = GetScene();
    bool runtime = (scene != nullptr) && !scene->IsInEditor();

    if (runtime) {
        bool haveTarget = false;
        math::Vec2 targetPos = m_Position;

        if (m_FollowEnabled) {
            Node2D* target2D = dynamic_cast<Node2D*>(ResolveFollowTarget());
            if (target2D) {
                targetPos = target2D->GetGlobalPosition();
                haveTarget = true;
            }
        }
        if (!haveTarget && m_HasSmoothTarget) {
            targetPos = m_SmoothTarget;
            haveTarget = true;
        }

        if (haveTarget) {
            math::Vec2 half = GetViewHalfExtents();
            math::Vec2 desired(
                ComputeDragAxis(m_Position.x, targetPos.x, half.x,
                                m_DragHorizontalEnabled, m_DragLeft, m_DragRight),
                ComputeDragAxis(m_Position.y, targetPos.y, half.y,
                                m_DragVerticalEnabled, m_DragTop, m_DragBottom)
            );

            if (m_FollowSmoothing && m_FollowSpeed > 0.0f) {
                float t = 1.0f - std::exp(-m_FollowSpeed * deltaTime);
                m_Position = m_Position.Lerp(desired, t);
            } else {
                m_Position = desired;
            }

            if (m_HasSmoothTarget && m_Position.DistanceSquared(m_SmoothTarget) < 0.01f) {
                m_Position = m_SmoothTarget;
                m_HasSmoothTarget = false;
            }
        }

        if (m_LimitEnabled) {
            m_Position = ClampCenterToLimits(m_Position);
        }

        m_Shake.Update(deltaTime);
    } else {
        m_Shake.Stop();
    }

    Node::OnProcess(deltaTime);
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

    // The canvas frame is the visible world rectangle: sized by canvas size, shrunk by
    // zoom and HiDPI scale, and counter-rotated against the camera rotation (the view
    // turns UI by +rotation, so the captured region turns by -rotation). This makes the
    // editor preview enclose exactly what the camera will show at runtime.
    float zoom = (m_Zoom != 0.0f) ? m_Zoom : 1.0f;
    float sf = (m_ScaleFactor != 0.0f) ? m_ScaleFactor : 1.0f;
    float inv = 1.0f / (zoom * sf);

    float lx0 = -m_Origin.x * m_CanvasSize.x;
    float lx1 = lx0 + m_CanvasSize.x;
    float ly0 = -m_Origin.y * m_CanvasSize.y;
    float ly1 = ly0 + m_CanvasSize.y;

    float cosR = std::cos(m_Rotation);
    float sinR = std::sin(m_Rotation);
    auto frameCorner = [&](float lx, float ly) -> math::Vec3 {
        float fx = lx * inv;
        float fy = ly * inv;
        return position + math::Vec3(fx * cosR + fy * sinR, -fx * sinR + fy * cosR, 0.0f);
    };

    math::Vec3 tl = frameCorner(lx0, ly1);
    math::Vec3 tr = frameCorner(lx1, ly1);
    math::Vec3 br = frameCorner(lx1, ly0);
    math::Vec3 bl = frameCorner(lx0, ly0);

    math::Color blueColor(0.3f, 0.6f, 1.0f, 1.0f);
    DebugDraw::Line(tl, tr, blueColor, 0.0f, false);
    DebugDraw::Line(tr, br, blueColor, 0.0f, false);
    DebugDraw::Line(br, bl, blueColor, 0.0f, false);
    DebugDraw::Line(bl, tl, blueColor, 0.0f, false);

    float crossSize = 20.0f;
    DebugDraw::Line(frameCorner(-crossSize, 0.0f), frameCorner(crossSize, 0.0f),
                    blueColor, 0.0f, false);
    DebugDraw::Line(frameCorner(0.0f, -crossSize), frameCorner(0.0f, crossSize),
                    blueColor, 0.0f, false);
}

}
}

