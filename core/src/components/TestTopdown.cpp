#include "lupine/components/TestTopdown.hpp"
#include "lupine/components/CharacterController2D.hpp"
#include "lupine/components/KinematicBody2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/logger/Logger.hpp"

using namespace lupine::core;
using namespace lupine::math;
using namespace lupine::input;

namespace lupine {
namespace components {

TestTopdown::TestTopdown()
    : Component("TestTopdown")
    , m_MoveSpeed(200.0f)
    , m_Velocity(Vec2::Zero())
    , m_CharacterController(nullptr)
{
}

TestTopdown::TestTopdown(const std::string& name)
    : Component(name)
    , m_MoveSpeed(200.0f)
    , m_Velocity(Vec2::Zero())
    , m_CharacterController(nullptr)
{
}

void TestTopdown::DefineProperties() {

}

void TestTopdown::OnAwake() {

    if (m_Owner) {
        auto controller = m_Owner->GetComponent<CharacterController2D>();
        m_CharacterController = controller.get();
    }
}

void TestTopdown::OnReady() {

    if (m_Owner) {

        if (m_CharacterController) {
            m_CharacterController->SetGravity(0.0f);
        }

        auto kinematicBody = m_Owner->GetComponent<KinematicBody2DComponent>();
        if (kinematicBody) {
            kinematicBody->SetGravityEnabled(false);
        }
    }
}

void TestTopdown::OnInput(float) {
    if (!m_Owner || !m_CharacterController) return;

    InputManager& input = InputManager::Get();

    Vec2 velocity = Vec2::Zero();

    if (input.IsActionPressed("move_left")) {
        velocity.x = -m_MoveSpeed;
    }
    if (input.IsActionPressed("move_right")) {
        velocity.x = m_MoveSpeed;
    }
    if (input.IsActionPressed("move_up")) {
        velocity.y = m_MoveSpeed;
    }
    if (input.IsActionPressed("move_down")) {
        velocity.y = -m_MoveSpeed;
    }

    m_CharacterController->SetVelocity(velocity);
}

void TestTopdown::OnPhysicsProcess(float) {

}

void TestTopdown::OnRender() {

}

void TestTopdown::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) return;

    Vec2 position = node2D->GetGlobalPosition();

    Vec2 size(100.0f, 100.0f);
    Color white(1.0f, 1.0f, 1.0f, 1.0f);

    Vec2 topLeft(position.x - size.x * 0.5f, position.y - size.y * 0.5f);

    Vec4 cornerRadius(0.0f, 0.0f, 0.0f, 0.0f);
    ctx.drawRoundedRect(topLeft, size, cornerRadius, white, 0);
}

AABB TestTopdown::getWorldBounds() const {
    if (!m_Owner) return AABB();

    auto* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) return AABB();

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size(100.0f, 100.0f);

    Vec3 min(position.x - size.x * 0.5f, position.y - size.y * 0.5f, 0.0f);
    Vec3 max(position.x + size.x * 0.5f, position.y + size.y * 0.5f, 0.0f);
    return AABB(min, max);
}

RenderLayer TestTopdown::getRenderLayer() const {
    return RenderLayer::Opaque;
}

SpatialType TestTopdown::getSpatialType() const {
    return SpatialType::World2D;
}

}
}

