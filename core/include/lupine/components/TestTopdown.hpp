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
 * TestTopdown Component (TEMPORARY - FOR TESTING ONLY)
 *
 * A simple 2D test component for testing top-down RPG-style movement.
 * Renders a white 100x100 square and responds to input actions:
 * - move_up/move_down/move_left/move_right: 4-directional movement
 * - No gravity or jumping
 *
 * Requires CharacterController2D component on the same node.
 * This component will be removed once character controller testing is complete.
 */
class TestTopdown : public core::Component, public IRenderableComponent {
public:
    TestTopdown();
    explicit TestTopdown(const std::string& name);
    virtual ~TestTopdown() = default;

    // ISerializable interface
    std::string GetTypeName() const override { return "TestTopdown"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
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
    math::Vec2 m_Velocity;
    CharacterController2D* m_CharacterController;
};

} // namespace components
} // namespace lupine

