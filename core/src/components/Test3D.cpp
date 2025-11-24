#include "lupine/components/Test3D.hpp"
#include "lupine/components/CharacterController3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/logger/Logger.hpp"

using namespace lupine::core;
using namespace lupine::math;
using namespace lupine::input;

namespace lupine {
namespace components {

Test3D::Test3D()
    : Component("Test3D")
    , m_MoveSpeed(5.0f)
    , m_JumpForce(10.0f)
    , m_Velocity(Vec3::Zero())
    , m_IsOnGround(false)
    , m_CharacterController(nullptr)
{
}

Test3D::Test3D(const std::string& name)
    : Component(name)
    , m_MoveSpeed(5.0f)
    , m_JumpForce(10.0f)
    , m_Velocity(Vec3::Zero())
    , m_IsOnGround(false)
    , m_CharacterController(nullptr)
{
}

void Test3D::DefineProperties() {

}

void Test3D::OnAwake() {

    if (m_Owner) {
        auto controller = m_Owner->GetComponent<CharacterController3D>();
        m_CharacterController = controller.get();
        if (!m_CharacterController) {

        }
    }
}

void Test3D::OnInput(float deltaTime) {
    if (!m_Owner || !m_CharacterController) return;

    InputManager& input = InputManager::Get();

    Vec3 velocity = m_CharacterController->GetVelocity();

    velocity.x = 0.0f;
    velocity.z = 0.0f;

    if (input.IsActionPressed("move_forward")) {
        velocity.z = -m_MoveSpeed;
    }
    if (input.IsActionPressed("move_backward")) {
        velocity.z = m_MoveSpeed;
    }
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

void Test3D::OnPhysicsProcess(float deltaTime) {

    if (m_CharacterController) {
        m_IsOnGround = m_CharacterController->IsOnGround();
    }
}

void Test3D::OnRender() {

}

void Test3D::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return;

    Vec3 position = node3D->GetGlobalPosition();

    Vec3 size(1.0f, 1.0f, 1.0f);
    Color white(1.0f, 1.0f, 1.0f, 1.0f);

    float collisionRadius = 0.7f;
    float cubeHalfHeight = size.y * 0.5f;
    float yOffset = -(collisionRadius - cubeHalfHeight);

    Vec3 visualPosition = position + Vec3(0.0f, yOffset, 0.0f);

    ctx.drawBox(visualPosition, size, white, false);
}

AABB Test3D::getWorldBounds() const {
    if (!m_Owner) return AABB();

    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) return AABB();

    Vec3 position = node3D->GetGlobalPosition();
    Vec3 size(1.0f, 1.0f, 1.0f);

    return AABB(position - size * 0.5f, position + size * 0.5f);
}

RenderLayer Test3D::getRenderLayer() const {
    return RenderLayer::Opaque;
}

SpatialType Test3D::getSpatialType() const {
    return SpatialType::World3D;
}

}
}

