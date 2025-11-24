#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/components/KinematicBody3DComponent.hpp"

namespace lupine {
namespace components {

/**
 * CharacterController3D
 * 
 * A robust character controller for 3D games.
 * Uses swept collision detection (shape casting) to prevent tunneling
 * and provides proper collision response for kinematic bodies.
 * 
 * Features:
 * - Swept collision detection using Bullet's convex casting
 * - Ground detection with configurable tolerance
 * - Wall sliding
 * - Slope handling
 * - Stair stepping
 * - Snap to ground
 * - Configurable gravity
 * - Jump buffering
 * - Coyote time (grace period for jumping after leaving ground)
 * 
 * Requires:
 * - KinematicBody3DComponent on the same node
 * - CollisionMesh3DComponent on the same node
 */
class CharacterController3D : public core::Component {
public:
    CharacterController3D();
    explicit CharacterController3D(const std::string& name);
    virtual ~CharacterController3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "CharacterController3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnDestroy() override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Movement API =====
    
    // Move the character with collision detection
    // Returns the actual movement after collision response
    math::Vec3 MoveAndSlide(const math::Vec3& velocity, float deltaTime);
    
    // Move the character and stop on collision
    // Returns true if collision occurred
    bool MoveAndCollide(const math::Vec3& velocity, float deltaTime, math::Vec3& outActualMovement);
    
    // Apply velocity (will be processed in next physics update)
    void SetVelocity(const math::Vec3& velocity);
    math::Vec3 GetVelocity() const { return m_Velocity; }
    
    // ===== Ground Detection =====
    
    bool IsOnGround() const { return m_IsOnGround; }
    bool IsOnWall() const { return m_IsOnWall; }
    bool IsOnCeiling() const { return m_IsOnCeiling; }
    
    math::Vec3 GetGroundNormal() const { return m_GroundNormal; }
    math::Vec3 GetWallNormal() const { return m_WallNormal; }
    
    // ===== Property Accessors =====
    
    float GetGravity() const;
    void SetGravity(float gravity);
    
    float GetMaxFallSpeed() const;
    void SetMaxFallSpeed(float speed);
    
    float GetGroundDetectionDistance() const;
    void SetGroundDetectionDistance(float distance);
    
    float GetWallDetectionDistance() const;
    void SetWallDetectionDistance(float distance);
    
    float GetMaxSlopeAngle() const;
    void SetMaxSlopeAngle(float angle);
    
    float GetStepHeight() const;
    void SetStepHeight(float height);
    
    bool GetSnapToGround() const;
    void SetSnapToGround(bool snap);
    
    int GetMaxBounces() const;
    void SetMaxBounces(int bounces);

private:
    // Physics body reference
    KinematicBody3DComponent* m_KinematicBody;
    class CollisionMesh3DComponent* m_CollisionShape;

    // Movement state
    math::Vec3 m_Velocity;
    math::Vec3 m_LastMovement;

    // Ground detection
    bool m_IsOnGround;
    bool m_IsOnWall;
    bool m_IsOnCeiling;
    math::Vec3 m_GroundNormal;
    math::Vec3 m_WallNormal;
    float m_TimeSinceGrounded;

    // Configuration (exposed as properties)
    float m_Gravity;
    float m_MaxFallSpeed;
    float m_GroundDetectionDistance;
    float m_WallDetectionDistance;
    float m_MaxSlopeAngle;
    float m_StepHeight;
    bool m_SnapToGround;
    int m_MaxBounces;

    // Helper methods
    void FindKinematicBody();
    void FindCollisionShape();
    float GetCollisionRadius() const;
    void ClearKinematicBodyReference();
    void UpdateGroundDetection();
    void UpdateWallDetection();
    math::Vec3 PerformSweptCollision(const math::Vec3& movement, float deltaTime);
    math::Vec3 SlideAlongSurface(const math::Vec3& velocity, const math::Vec3& normal);
    bool TryStepUp(const math::Vec3& movement, math::Vec3& outMovement);
    bool IsFloor(const math::Vec3& normal) const;
    bool IsWall(const math::Vec3& normal) const;
    bool IsCeiling(const math::Vec3& normal) const;
};

} // namespace components
} // namespace lupine

