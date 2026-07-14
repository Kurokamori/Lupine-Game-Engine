#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"

namespace lupine {
namespace components {

/**
 * ShapeCast3D Component
 *
 * A 3D swept-sphere cast that queries the physics world every physics frame and
 * reports the first body the swept sphere touches. This is the thick-ray
 * counterpart to RayCast3D: instead of an infinitely thin line it sweeps a
 * sphere of shapeRadius from the node along targetPosition, which is the right
 * tool for "can this body fit / move here" checks and forgiving line-of-sight.
 *
 * Features:
 * - Cast defined in node-local space (targetPosition), so it follows the node.
 * - Configurable swept-sphere radius (0 degenerates to a thin ray).
 * - Optional exclusion of the owner's own body (excludeParent) via the physics
 *   world's ignore-body filter.
 * - Editor visualisation drawn through the standard renderable interface, with
 *   an opt-in to keep the gizmo visible while the game is running.
 * - Collision layer mask, applied inside the Bullet query callback: a body is
 *   only considered when its collision layers intersect the mask.
 */
class ShapeCast3D : public core::Component, public IRenderableComponent {
public:
    ShapeCast3D();
    explicit ShapeCast3D(const std::string& name);
    virtual ~ShapeCast3D() = default;

    // ISerializable interface
    std::string GetTypeName() const override { return "ShapeCast3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnReady() override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Scripting bridge (shared by Lua / MRuby / MicroPython / C-API)
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Configuration =====

    /** The cast endpoint, relative to the node in local space. */
    math::Vec3 GetTargetPosition() const { return m_TargetPosition; }
    void SetTargetPosition(const math::Vec3& target);

    /** Radius of the swept sphere. */
    float GetShapeRadius() const { return m_ShapeRadius; }
    void SetShapeRadius(float radius);

    /** When true, hits against the owner's own body hierarchy are ignored. */
    bool GetExcludeParent() const { return m_ExcludeParent; }
    void SetExcludeParent(bool exclude) { m_ExcludeParent = exclude; }

    /** Collision layers the cast is allowed to hit (bitmask; default: every layer). */
    uint32_t GetCollisionMask() const { return m_CollisionMask; }
    void SetCollisionMask(uint32_t mask);

    /** When true, the debug gizmo is also drawn while the game runs. */
    bool GetVisibleInGame() const { return m_VisibleInGame; }
    void SetVisibleInGame(bool visible) { m_VisibleInGame = visible; }

    // ===== Query Results =====

    /** True if the most recent cast touched a body. */
    bool IsColliding() const { return m_IsColliding; }

    /** The node owning the body that was hit, or nullptr if none. */
    core::Node* GetCollider() const { return m_Collider; }

    /** World-space point where the sphere first touched, valid when IsColliding(). */
    math::Vec3 GetCollisionPoint() const { return m_CollisionPoint; }

    /** Surface normal at the contact point, valid when IsColliding(). */
    math::Vec3 GetCollisionNormal() const { return m_CollisionNormal; }

    /** Fraction along the cast [0,1] where contact occurred (1 if no hit). */
    float GetCollisionFraction() const { return m_CollisionFraction; }

    /**
     * Re-run the cast immediately instead of waiting for the next physics
     * frame. Lets gameplay code sample the cast right after moving the node.
     */
    void ForceShapecastUpdate();

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    // Refresh the cached configuration members from the serialized properties.
    void SyncFromProperties();

    // Perform the cast against the physics world and update the cached result.
    void UpdateShapecast();

    // Resolve the body to ignore (owner's nearest body ancestor) or zero UUID.
    core::UUID ResolveIgnoreBody() const;

    // Resolve the local-space cast into world-space origin and endpoint.
    void ComputeWorldRay(math::Vec3& outOrigin, math::Vec3& outTarget) const;

    math::Vec3 m_TargetPosition;
    float m_ShapeRadius;
    bool m_ExcludeParent;
    uint32_t m_CollisionMask;
    bool m_VisibleInGame;
    math::Color m_DebugColor;
    math::Color m_DebugColorHit;

    bool m_IsColliding;
    math::Vec3 m_CollisionPoint;
    math::Vec3 m_CollisionNormal;
    float m_CollisionFraction;
    core::Node* m_Collider;
    core::UUID m_ColliderBodyId;
};

} // namespace components
} // namespace lupine
