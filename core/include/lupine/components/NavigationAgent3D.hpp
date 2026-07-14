#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Vec3.hpp"
#include <cstdint>
#include <vector>

namespace lupine {
namespace components {

/**
 * NavigationAgent3D Component
 *
 * Requests and follows 3D paths produced by navigation::NavigationServer3D.
 * Attach it to a Node3D, set a target position, and either drive the owner
 * yourself with GetNextPathPosition() or enable autoMove to advance the owner
 * toward the target at maxSpeed. The path corridor follows the navmesh
 * elevation, so movement climbs ramps and steps naturally.
 *
 * Local avoidance (ORCA) is solved on the horizontal plane: agents steer around
 * one another and around dynamic NavigationObstacle3D discs while keeping the
 * vertical motion dictated by the path.
 *
 * The agent recomputes its path automatically when the target changes, when the
 * navigation map is rebaked, or when the owner strays too far from the corridor.
 */
class NavigationAgent3D : public core::Component {
public:
    NavigationAgent3D();
    explicit NavigationAgent3D(const std::string& name);
    virtual ~NavigationAgent3D();

    std::string GetTypeName() const override { return "NavigationAgent3D"; }
    void DefineProperties() override;
    void DefineSignals() override;

    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;
    void OnExitTree() override;

    nlohmann::json CallMethod(const std::string& method,
                              const nlohmann::json& args) override;

    void SetTargetPosition(const math::Vec3& target);
    math::Vec3 GetTargetPosition() const;

    math::Vec3 GetNextPathPosition() const;
    bool IsNavigationFinished() const { return m_Finished; }
    bool IsTargetReachable() const { return m_PathValid; }
    float DistanceToTarget() const;
    const std::vector<math::Vec3>& GetCurrentPath() const { return m_Path; }

    float GetMaxSpeed() const;
    void SetMaxSpeed(float speed);
    float GetPathDesiredDistance() const;
    float GetTargetDesiredDistance() const;
    bool GetAutoMove() const;
    void SetAutoMove(bool autoMove);

    float GetRadius() const;
    void SetRadius(float radius);
    bool GetAvoidanceEnabled() const;
    void SetAvoidanceEnabled(bool enabled);
    float GetNeighborDistance() const;
    int GetMaxNeighbors() const;
    float GetTimeHorizon() const;
    float GetTimeHorizonObstacle() const;

    math::Vec3 GetVelocity() const { return m_Velocity; }

    void ResetPath();

private:
    std::string AvoidanceId() const;
    void RecomputePath(const math::Vec3& from, const math::Vec3& to);
    math::Vec3 ComputeDesiredVelocity(const math::Vec3& currentPos) const;
    void ApplyMovement(float deltaTime, const math::Vec3& currentPos, const math::Vec3& velocity);
    void UnregisterAvoidance();

    std::vector<math::Vec3> m_Path;
    size_t m_PathIndex = 0;
    bool m_HasTarget = false;
    bool m_PathValid = false;
    bool m_Finished = false;
    bool m_FinishEmitted = false;
    bool m_AvoidanceRegistered = false;

    math::Vec3 m_Velocity;

    uint64_t m_LastMapVersion = 0;
    math::Vec3 m_LastQueryTarget;
    math::Vec3 m_LastQueryStart;
};

} // namespace components
} // namespace lupine
