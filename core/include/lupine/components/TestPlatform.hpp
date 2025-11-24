#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Color.hpp"

namespace lupine {
namespace components {

// Forward declaration
class CharacterController2D;

/**
 * TestPlatform Component (TEMPORARY - FOR TESTING ONLY)
 *
 * A simple 2D test component for testing CharacterController2D.
 * Renders a white 100x100 square and responds to input actions:
 * - move_left/move_right: horizontal movement
 * - jump: vertical jump
 *
 * Requires CharacterController2D component on the same node.
 * This component will be removed once character controller testing is complete.
 */
class TestPlatform : public core::Component, public IRenderableComponent {
public:
    TestPlatform();
    explicit TestPlatform(const std::string& name);
    virtual ~TestPlatform() = default;

    // ISerializable interface
    std::string GetTypeName() const override { return "TestPlatform"; }
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
    math::Vec2 m_Velocity;
    bool m_IsOnGround;
    CharacterController2D* m_CharacterController;
};

} // namespace components
} // namespace lupine

