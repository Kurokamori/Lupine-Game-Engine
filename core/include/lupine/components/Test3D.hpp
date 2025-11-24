#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Color.hpp"

namespace lupine {
namespace components {

// Forward declaration
class CharacterController3D;

/**
 * Test3D Component (TEMPORARY - FOR TESTING ONLY)
 *
 * A simple 3D test component for testing CharacterController3D.
 * Renders a white 1x1 cube and responds to input actions:
 * - move_forward/move_backward/move_left/move_right: horizontal movement
 * - jump: vertical jump
 *
 * Requires CharacterController3D component on the same node.
 * This component will be removed once character controller testing is complete.
 */
class Test3D : public core::Component, public IRenderableComponent {
public:
    Test3D();
    explicit Test3D(const std::string& name);
    virtual ~Test3D() = default;

    // ISerializable interface
    std::string GetTypeName() const override { return "Test3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnInput(float deltaTime) override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnRender() override;

    // IRenderableComponent Implementation
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    float m_MoveSpeed;
    float m_JumpForce;
    math::Vec3 m_Velocity;
    bool m_IsOnGround;
    CharacterController3D* m_CharacterController;
};

} // namespace components
} // namespace lupine

