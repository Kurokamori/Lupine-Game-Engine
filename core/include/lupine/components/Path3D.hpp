#pragma once

#include "lupine/components/Curve3D.hpp"

namespace lupine {
namespace components {

/**
 * Path3D Component
 *
 * Extends Curve3D with pathing functionality for AI, camera rails, moving
 * platforms, and cutscene tracks. Provides path following with progress
 * tracking plus position/direction/closest-point queries in both local and
 * world space.
 *
 * Features:
 * - All Curve3D functionality (points, bezier, debug drawing, transform-aware)
 * - Path following with progress tracking, speed, loop and ping-pong modes
 * - Distance-based position/direction sampling
 * - Closest point/progress/distance queries
 * - Start/end markers and direction arrows in the 3D viewport
 * - Script-accessible via CallMethod (all scripting languages)
 */
class Path3D : public Curve3D {
public:
    Path3D();
    explicit Path3D(const std::string& name);
    virtual ~Path3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Path3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnUpdate(float deltaTime) override;

    // Scripting bridge
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Path Following =====

    /** Start following the path */
    void StartFollowing();

    /** Stop following the path */
    void StopFollowing();

    /** Reset to beginning of path */
    void Reset();

    /** Check if currently following the path */
    bool IsFollowing() const { return m_IsFollowing; }

    /** Get/Set current progress (0-1) */
    float GetProgress() const { return m_Progress; }
    void SetProgress(float progress);

    /** Get/Set speed (units per second) */
    float GetSpeed() const;
    void SetSpeed(float speed);

    /** Get/Set loop mode */
    bool GetLoop() const;
    void SetLoop(bool loop);

    /** Get/Set ping-pong mode (reverse direction at ends) */
    bool GetPingPong() const;
    void SetPingPong(bool pingPong);

    /** Get/Set auto-start */
    bool GetAutoStart() const;
    void SetAutoStart(bool autoStart);

    // ===== Path Queries (local space) =====

    /** Get local-space position at current progress */
    math::Vec3 GetCurrentPosition() const;

    /** Get local-space direction at current progress */
    math::Vec3 GetCurrentDirection() const;

    /** Get local-space position at specific progress (0-1) */
    math::Vec3 GetPositionAtProgress(float t) const;

    /** Get local-space direction at specific progress (0-1) */
    math::Vec3 GetDirectionAtProgress(float t) const;

    /** Get local-space position at specific distance along the path */
    math::Vec3 GetPositionAtDistance(float distance) const;

    /** Get local-space direction at specific distance along the path */
    math::Vec3 GetDirectionAtDistance(float distance) const;

    /** Get start position (first point, local space) */
    math::Vec3 GetStartPosition() const;

    /** Get end position (last point, local space) */
    math::Vec3 GetEndPosition() const;

    // ===== Path Queries (world space) =====

    /** Get world-space position at current progress */
    math::Vec3 GetCurrentPositionWorld() const;

    /** Get world-space direction at current progress */
    math::Vec3 GetCurrentDirectionWorld() const;

    // ===== Closest-point queries (local space) =====

    /** Get the closest point on the path to a given local-space position */
    math::Vec3 GetClosestPoint(const math::Vec3& position) const;

    /** Get the closest progress value (0-1) to a given local-space position */
    float GetClosestProgress(const math::Vec3& position) const;

    /** Get distance along path to the closest point from a local-space position */
    float GetClosestDistance(const math::Vec3& position) const;

    /** Get remaining distance from current progress to end */
    float GetRemainingDistance() const;

    /** Get distance traveled from start */
    float GetTraveledDistance() const;

    // ===== Path Modification =====

    /** Reverse the path direction */
    void ReversePath();

    /** Get whether path is reversed */
    bool IsReversed() const { return m_IsReversed; }

    // ===== Rendering Override =====

    void buildDrawCommands(RenderContext& ctx) override;

protected:
    void RenderPathMarkers(RenderContext& ctx);

private:
    float m_Progress = 0.0f;
    bool m_IsFollowing = false;
    bool m_IsReversed = false;
    int m_Direction = 1;  // 1 = forward, -1 = backward (for ping-pong)
};

} // namespace components
} // namespace lupine
