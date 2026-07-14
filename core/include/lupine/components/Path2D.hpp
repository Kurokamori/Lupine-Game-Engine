#pragma once

#include "lupine/components/Curve2D.hpp"

namespace lupine {
namespace components {

/**
 * Path2D Component
 *
 * Extends Curve2D with pathing functionality for AI, movement, and animation.
 * Provides methods for following paths, querying positions, distances, and directions.
 *
 * Features:
 * - All Curve2D functionality (points, bezier, debug drawing)
 * - Path following with progress tracking
 * - Distance-based position sampling
 * - Direction/tangent queries
 * - Closest point on path calculation
 * - Speed and loop settings for path following
 */
class Path2D : public Curve2D {
public:
    Path2D();
    explicit Path2D(const std::string& name);
    virtual ~Path2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Path2D"; }
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

    // ===== Path Queries =====

    /** Get position at current progress */
    math::Vec2 GetCurrentPosition() const;

    /** Get direction at current progress */
    math::Vec2 GetCurrentDirection() const;

    /** Get position at specific progress (0-1) */
    math::Vec2 GetPositionAtProgress(float t) const;

    /** Get direction at specific progress (0-1) */
    math::Vec2 GetDirectionAtProgress(float t) const;

    /** Get position at specific distance along the path */
    math::Vec2 GetPositionAtDistance(float distance) const;

    /** Get direction at specific distance along the path */
    math::Vec2 GetDirectionAtDistance(float distance) const;

    /** Get start position (first point) */
    math::Vec2 GetStartPosition() const;

    /** Get end position (last point) */
    math::Vec2 GetEndPosition() const;

    /** Get the closest point on the path to a given position */
    math::Vec2 GetClosestPoint(const math::Vec2& position) const;

    /** Get the closest progress value (0-1) to a given position */
    float GetClosestProgress(const math::Vec2& position) const;

    /** Get distance along path to closest point from a given position */
    float GetClosestDistance(const math::Vec2& position) const;

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

