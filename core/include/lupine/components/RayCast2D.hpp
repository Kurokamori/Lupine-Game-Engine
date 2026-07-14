#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"

namespace lupine {
namespace components {

/**
 * RayCast2D Component
 *
 * A 2D ray that queries the physics world every physics frame and reports
 * the first body it intersects. This is the polling-style counterpart to the
 * one-shot Physics2D raycast query: attach it to a node, point it with
 * targetPosition, and read back the hit each frame with IsColliding(),
 * GetCollider(), GetCollisionPoint() and GetCollisionNormal().
 *
 * Features:
 * - Ray defined in node-local space (targetPosition), so it rotates and moves
 *   with the owning Node2D.
 * - Collision-layer mask to select which bodies are eligible.
 * - Optional exclusion of the owner's own body hierarchy (excludeParent), so a
 *   ray parented under a moving body does not immediately hit itself.
 * - Editor visualisation drawn through the standard renderable interface, with
 *   an opt-in to keep the gizmo visible while the game is running.
 *
 * The ray is only evaluated while the component is enabled and a physics world
 * exists (i.e. at runtime). In the editor the configured ray is drawn but no
 * hit information is produced, matching the behaviour of the one-shot query.
 */
class RayCast2D : public core::Component, public IRenderableComponent {
public:
    RayCast2D();
    explicit RayCast2D(const std::string& name);
    virtual ~RayCast2D() = default;

    // ISerializable interface
    std::string GetTypeName() const override { return "RayCast2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnReady() override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Scripting bridge (shared by Lua / MRuby / MicroPython / C-API)
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Configuration =====

    /** The ray endpoint, relative to the node in local space. */
    math::Vec2 GetTargetPosition() const { return m_TargetPosition; }
    void SetTargetPosition(const math::Vec2& target);

    /** Bitmask of collision layers the ray is allowed to hit. */
    uint32_t GetCollisionMask() const { return m_CollisionMask; }
    void SetCollisionMask(uint32_t mask);

    /** When true, hits against the owner's own body hierarchy are ignored. */
    bool GetExcludeParent() const { return m_ExcludeParent; }
    void SetExcludeParent(bool exclude) { m_ExcludeParent = exclude; }

    /** When true, the debug ray gizmo is also drawn while the game runs. */
    bool GetVisibleInGame() const { return m_VisibleInGame; }
    void SetVisibleInGame(bool visible) { m_VisibleInGame = visible; }

    // ===== Query Results =====

    /** True if the most recent cast intersected a body. */
    bool IsColliding() const { return m_IsColliding; }

    /** The node owning the body that was hit, or nullptr if none. */
    core::Node* GetCollider() const { return m_Collider; }

    /** World-space point where the ray hit, valid only when IsColliding(). */
    math::Vec2 GetCollisionPoint() const { return m_CollisionPoint; }

    /** Surface normal at the hit point, valid only when IsColliding(). */
    math::Vec2 GetCollisionNormal() const { return m_CollisionNormal; }

    /**
     * Re-run the cast immediately instead of waiting for the next physics
     * frame. Lets gameplay code sample the ray right after moving the node.
     */
    void ForceRaycastUpdate();

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    // Refresh the cached configuration members from the serialized properties.
    void SyncFromProperties();

    // Perform the cast against the physics world and update the cached result.
    void UpdateRaycast();

    // True if hitNode belongs to the owner's own hierarchy and should be
    // skipped while excludeParent is set.
    bool IsExcludedNode(const core::Node* hitNode) const;

    // Resolve the local-space ray into world-space origin and endpoint.
    void ComputeWorldRay(math::Vec2& outOrigin, math::Vec2& outTarget) const;

    math::Vec2 m_TargetPosition;
    uint32_t m_CollisionMask;
    bool m_ExcludeParent;
    bool m_VisibleInGame;
    math::Color m_DebugColor;
    math::Color m_DebugColorHit;

    bool m_IsColliding;
    math::Vec2 m_CollisionPoint;
    math::Vec2 m_CollisionNormal;
    core::Node* m_Collider;
    core::UUID m_ColliderBodyId;
};

} // namespace components
} // namespace lupine
