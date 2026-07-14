#include "lupine/components/TestPlatform.hpp"
#include "lupine/components/CharacterController2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/logger/Logger.hpp"

using namespace lupine::core;
using namespace lupine::math;
using namespace lupine::input;

namespace lupine {
namespace components {

TestPlatform::TestPlatform()
    : Component("TestPlatform")
    , m_MoveSpeed(200.0f)
    , m_JumpForce(400.0f)
    , m_Velocity(Vec2::Zero())
    , m_IsOnGround(false)
    , m_CharacterController(nullptr)
{
}

TestPlatform::TestPlatform(const std::string& name)
    : Component(name)
    , m_MoveSpeed(200.0f)
    , m_JumpForce(400.0f)
    , m_Velocity(Vec2::Zero())
    , m_IsOnGround(false)
    , m_CharacterController(nullptr)
{
}

void TestPlatform::DefineProperties() {

}

void TestPlatform::OnAwake() {

    if (m_Owner) {
        auto controller = m_Owner->GetComponent<CharacterController2D>();
        m_CharacterController = controller.get();
        if (!m_CharacterController) {

        }
    }
}

void TestPlatform::OnInput(float) {
    if (!m_Owner || !m_CharacterController) return;

    InputManager& input = InputManager::Get();

    Vec2 velocity = m_CharacterController->GetVelocity();

    velocity.x = 0.0f;

    if (input.IsActionPressed("move_left")) {
        velocity.x = -m_MoveSpeed;
    }
    if (input.IsActionPressed("move_right")) {
        velocity.x = m_MoveSpeed;
    }

    bool isOnGround = m_CharacterController->IsOnGround();
    bool jumpPressed = input.IsActionJustPressed("jump");
    if (jumpPressed && isOnGround) {
        velocity.y = m_JumpForce;

    }

    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {

    }

    m_CharacterController->SetVelocity(velocity);
}

void TestPlatform::OnPhysicsProcess(float) {

    if (m_CharacterController) {
        m_IsOnGround = m_CharacterController->IsOnGround();
    }
}

void TestPlatform::OnRender() {

}

void TestPlatform::buildDrawCommands(RenderContext& ctx) {
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

AABB TestPlatform::getWorldBounds() const {
    if (!m_Owner) return AABB();

    auto* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) return AABB();

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size(100.0f, 100.0f);

    Vec3 min(position.x - size.x * 0.5f, position.y - size.y * 0.5f, 0.0f);
    Vec3 max(position.x + size.x * 0.5f, position.y + size.y * 0.5f, 0.0f);
    return AABB(min, max);
}

RenderLayer TestPlatform::getRenderLayer() const {
    return RenderLayer::Opaque;
}

SpatialType TestPlatform::getSpatialType() const {
    return SpatialType::World2D;
}

}
}

