#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Vec2.hpp"
#include <cstdint>
#include <vector>

namespace lupine {
namespace components {

/**
 * NavigationAgent2D Component
 *
 * Requests and follows paths produced by navigation::NavigationServer2D. Attach
 * it to a Node2D, set a target position, and either drive the owner yourself
 * using GetNextPathPosition() (the recommended pattern, matching how a physics
 * body is moved) or enable autoMove to have the agent advance the owner toward
 * the target at maxSpeed.
 *
 * The agent recomputes its path automatically when the target changes, when the
 * navigation map is rebaked, or when the owner strays too far from the corridor.
 */
class NavigationAgent2D : public core::Component {
public:
    NavigationAgent2D();
    explicit NavigationAgent2D(const std::string& name);
    virtual ~NavigationAgent2D();

    std::string GetTypeName() const override { return "NavigationAgent2D"; }
    void DefineProperties() override;
    void DefineSignals() override;

    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;
    void OnExitTree() override;

    nlohmann::json CallMethod(const std::string& method,
                              const nlohmann::json& args) override;

    // ===== Target =====
    void SetTargetPosition(const math::Vec2& target);
    math::Vec2 GetTargetPosition() const;

    // ===== Queries =====
    /** Next world-space point the owner should move toward. */
    math::Vec2 GetNextPathPosition() const;
    /** True once the owner is within targetDesiredDistance of the target. */
    bool IsNavigationFinished() const { return m_Finished; }
    /** True if a path to the current target exists. */
    bool IsTargetReachable() const { return m_PathValid; }
    /** Straight-line distance from the owner to the final target. */
    float DistanceToTarget() const;
    /** The full corridor of points for the current path. */
    const std::vector<math::Vec2>& GetCurrentPath() const { return m_Path; }

    // ===== Tuning =====
    float GetMaxSpeed() const;
    void SetMaxSpeed(float speed);
    float GetPathDesiredDistance() const;
    float GetTargetDesiredDistance() const;
    bool GetAutoMove() const;
    void SetAutoMove(bool autoMove);

    // ===== Avoidance =====
    float GetRadius() const;
    void SetRadius(float radius);
    bool GetAvoidanceEnabled() const;
    void SetAvoidanceEnabled(bool enabled);
    float GetNeighborDistance() const;
    int GetMaxNeighbors() const;
    float GetTimeHorizon() const;
    float GetTimeHorizonObstacle() const;

    /** The velocity chosen on the last update (ORCA-corrected when avoidance is on). */
    math::Vec2 GetVelocity() const { return m_Velocity; }

    /** Discard the current path so the next update recomputes it. */
    void ResetPath();

private:
    std::string AvoidanceId() const;
    void RecomputePath(const math::Vec2& from, const math::Vec2& to);
    math::Vec2 ComputeDesiredVelocity(const math::Vec2& currentPos) const;
    void ApplyMovement(float deltaTime, const math::Vec2& currentPos, const math::Vec2& velocity);
    void UnregisterAvoidance();

    std::vector<math::Vec2> m_Path;
    size_t m_PathIndex = 0;
    bool m_HasTarget = false;
    bool m_PathValid = false;
    bool m_Finished = false;
    bool m_FinishEmitted = false;
    bool m_AvoidanceRegistered = false;

    math::Vec2 m_Velocity;

    uint64_t m_LastMapVersion = 0;
    math::Vec2 m_LastQueryTarget;
    math::Vec2 m_LastQueryStart;
};

} // namespace components
} // namespace lupine
