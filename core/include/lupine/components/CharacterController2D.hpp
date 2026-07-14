#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/components/KinematicBody2DComponent.hpp"

namespace lupine {
namespace components {

/**
 * CharacterController2D
 * 
 * A robust character controller for 2D platformers and top-down games.
 * Uses swept collision detection to prevent tunneling and provides
 * proper collision response for kinematic bodies.
 * 
 * Features:
 * - Swept collision detection (no tunneling)
 * - Ground detection with configurable tolerance
 * - Wall sliding
 * - Slope handling
 * - Snap to ground
 * - Configurable gravity
 * - Jump buffering
 * - Coyote time (grace period for jumping after leaving ground)
 * 
 * Requires:
 * - KinematicBody2DComponent on the same node
 * - CollisionBody2DComponent on the same node
 */
class CharacterController2D : public core::Component {
public:
    CharacterController2D();
    explicit CharacterController2D(const std::string& name);
    virtual ~CharacterController2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "CharacterController2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnDestroy() override;
    void OnPhysicsProcess(float deltaTime) override;

    // ===== Movement API =====
    
    // Move the character with collision detection
    // Returns the actual movement after collision response
    math::Vec2 MoveAndSlide(const math::Vec2& velocity, float deltaTime);
    
    // Move the character and stop on collision
    // Returns true if collision occurred
    bool MoveAndCollide(const math::Vec2& velocity, float deltaTime, math::Vec2& outActualMovement);
    
    // Apply velocity (will be processed in next physics update)
    void SetVelocity(const math::Vec2& velocity);
    math::Vec2 GetVelocity() const { return m_Velocity; }
    
    // ===== Ground Detection =====
    
    bool IsOnGround() const { return m_IsOnGround; }
    bool IsOnWall() const { return m_IsOnWall; }
    bool IsOnCeiling() const { return m_IsOnCeiling; }
    
    math::Vec2 GetGroundNormal() const { return m_GroundNormal; }
    math::Vec2 GetWallNormal() const { return m_WallNormal; }
    
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
    
    bool GetSnapToGround() const;
    void SetSnapToGround(bool snap);
    
    int GetMaxBounces() const;
    void SetMaxBounces(int bounces);

private:
    // Physics body reference
    KinematicBody2DComponent* m_KinematicBody;
    class CollisionBody2DComponent* m_CollisionShape;

    // Movement state
    math::Vec2 m_Velocity;
    math::Vec2 m_LastMovement;

    // Ground detection
    bool m_IsOnGround;
    bool m_IsOnWall;
    bool m_IsOnCeiling;
    math::Vec2 m_GroundNormal;
    math::Vec2 m_WallNormal;
    float m_TimeSinceGrounded;

    // Configuration (exposed as properties)
    float m_Gravity;
    float m_MaxFallSpeed;
    float m_GroundDetectionDistance;
    float m_WallDetectionDistance;
    float m_MaxSlopeAngle;
    bool m_SnapToGround;
    int m_MaxBounces;

    // Helper methods
    void FindKinematicBody();
    void FindCollisionShape();

    // Build the body's real collider outline in world space for swept collision
    // and depenetration queries. `bodyCenter` is the kinematic body origin. A
    // circle collider yields a single point + radius; every other shape yields
    // its polygon corners (radius 0).
    void BuildColliderProxy(const math::Vec2& bodyCenter, std::vector<math::Vec2>& outPoints,
                            float& outRadius, bool& outIsCircle) const;

    // Axis-aligned half extents of the collider, used by the ground/wall ray probes.
    void GetColliderHalfExtents(float& outHalfWidth, float& outHalfHeight) const;

    // Resolve the collision mask of the body's collider (which layers it blocks on).
    uint64_t GetCollisionMaskBits() const;

    void ClearKinematicBodyReference();
    void UpdateGroundDetection();
    void UpdateWallDetection();
    math::Vec2 PerformSweptCollision(const math::Vec2& movement, float deltaTime);
    math::Vec2 SlideAlongSurface(const math::Vec2& velocity, const math::Vec2& normal);
    bool IsFloor(const math::Vec2& normal) const;
    bool IsWall(const math::Vec2& normal) const;
    bool IsCeiling(const math::Vec2& normal) const;
};

} // namespace components
} // namespace lupine

