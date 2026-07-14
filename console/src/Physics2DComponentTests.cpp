/**
 * @file Physics2DComponentTests.cpp
 * @brief Headless console tests for the 2D physics COMPONENTS.
 *
 * Where Physics2DTests.cpp drives the physics WORLD (body creation, simulation,
 * forces, collision/trigger callbacks, raycasts, overlaps), this suite focuses on
 * the COMPONENT-level public configuration surface: property/config round-trips
 * (mass, gravity scale, damping, fixed rotation, bullet, body velocities,
 * collision layer/mask, shape config, character-controller tuning).
 *
 * The body components store their tunable state in the declarative property
 * registry and only forward it to a live Box2D body once one is created
 * (CreatePhysicsBody requires a running SceneManager + Physics2DWorld). Without a
 * world, the getters fall back to the stored property values, so a detached
 * component still reports the configuration it was given. These tests therefore
 * construct components in isolation, populate their property registry via
 * RegisterProperties(), and verify that configuration round-trips. Methods that
 * require an active body (live velocities, force/impulse application, swept
 * movement) are exercised in Physics2DTests.cpp and are intentionally skipped
 * here, with the storage-only paths covered instead.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/RigidBody2DComponent.hpp"
#include "lupine/components/StaticBody2DComponent.hpp"
#include "lupine/components/KinematicBody2DComponent.hpp"
#include "lupine/components/AreaTrigger2DComponent.hpp"
#include "lupine/components/CollisionBody2DComponent.hpp"
#include "lupine/components/CharacterController2D.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool Vec2Eq(const math::Vec2& a, const math::Vec2& b, float tol = 1e-4f) {
    return FloatEq(a.x, b.x, tol) && FloatEq(a.y, b.y, tol);
}

bool TestRigidBody2D() {
    TEST_SECTION("RigidBody2DComponent: Configuration");

    std::shared_ptr<RigidBody2DComponent> body = std::make_shared<RigidBody2DComponent>();
    body->RegisterProperties();

    TEST_ASSERT(body->GetTypeName() == "RigidBody2DComponent", "Reports its type name");
    TEST_ASSERT(body->GetPhysicsBody() == nullptr, "No live physics body when detached from a world");

    TEST_ASSERT(FloatEq(body->GetGravityScale(), 1.0f), "Default gravity scale is 1.0");
    TEST_ASSERT(FloatEq(body->GetLinearDamping(), 0.0f), "Default linear damping is 0.0");
    TEST_ASSERT(FloatEq(body->GetAngularDamping(), 0.0f), "Default angular damping is 0.0");
    TEST_ASSERT(!body->GetFixedRotation(), "Default fixed rotation is false");
    TEST_ASSERT(!body->GetBullet(), "Default bullet mode is false");
    TEST_ASSERT(body->GetCanSleep(), "Default can-sleep is true");
    TEST_ASSERT(body->GetGravityEnabled(), "Default gravity enabled is true");
    TEST_ASSERT(FloatEq(body->GetMass(), 0.0f), "Detached body reports zero mass (mass comes from a live body)");

    body->SetGravityScale(2.5f);
    TEST_ASSERT(FloatEq(body->GetGravityScale(), 2.5f), "Gravity scale round-trips");
    body->SetGravityScale(-1.0f);
    TEST_ASSERT(FloatEq(body->GetGravityScale(), -1.0f), "Negative gravity scale round-trips");

    body->SetLinearDamping(0.75f);
    TEST_ASSERT(FloatEq(body->GetLinearDamping(), 0.75f), "Linear damping round-trips");

    body->SetAngularDamping(0.35f);
    TEST_ASSERT(FloatEq(body->GetAngularDamping(), 0.35f), "Angular damping round-trips");

    body->SetFixedRotation(true);
    TEST_ASSERT(body->GetFixedRotation(), "Fixed rotation round-trips to true");
    body->SetFixedRotation(false);
    TEST_ASSERT(!body->GetFixedRotation(), "Fixed rotation round-trips to false");

    body->SetBullet(true);
    TEST_ASSERT(body->GetBullet(), "Bullet mode round-trips to true");
    body->SetBullet(false);
    TEST_ASSERT(!body->GetBullet(), "Bullet mode round-trips to false");

    body->SetCanSleep(false);
    TEST_ASSERT(!body->GetCanSleep(), "Can-sleep round-trips to false");
    body->SetCanSleep(true);
    TEST_ASSERT(body->GetCanSleep(), "Can-sleep round-trips to true");

    body->SetGravityEnabled(false);
    TEST_ASSERT(!body->GetGravityEnabled(), "Gravity-enabled round-trips to false");
    body->SetGravityEnabled(true);
    TEST_ASSERT(body->GetGravityEnabled(), "Gravity-enabled round-trips to true");

    TEST_ASSERT(Vec2Eq(body->GetLinearVelocity(), math::Vec2(0.0f, 0.0f)),
                "Detached body reports zero linear velocity");
    TEST_ASSERT(FloatEq(body->GetAngularVelocity(), 0.0f),
                "Detached body reports zero angular velocity");

    return true;
}

bool TestStaticBody2D() {
    TEST_SECTION("StaticBody2DComponent: Configuration");

    std::shared_ptr<StaticBody2DComponent> body = std::make_shared<StaticBody2DComponent>();
    body->RegisterProperties();

    TEST_ASSERT(body->GetTypeName() == "StaticBody2DComponent", "Reports its type name");
    TEST_ASSERT(body->GetPhysicsBody() == nullptr, "No live physics body when detached from a world");

    TEST_ASSERT(Vec2Eq(body->GetConstantLinearVelocity(), math::Vec2(0.0f, 0.0f)),
                "Default constant linear velocity is zero");
    TEST_ASSERT(FloatEq(body->GetConstantAngularVelocity(), 0.0f),
                "Default constant angular velocity is zero");

    body->SetConstantLinearVelocity(math::Vec2(3.0f, -4.0f));
    TEST_ASSERT(Vec2Eq(body->GetConstantLinearVelocity(), math::Vec2(3.0f, -4.0f)),
                "Constant linear velocity round-trips");

    body->SetConstantAngularVelocity(1.5f);
    TEST_ASSERT(FloatEq(body->GetConstantAngularVelocity(), 1.5f),
                "Constant angular velocity round-trips");
    body->SetConstantAngularVelocity(-2.25f);
    TEST_ASSERT(FloatEq(body->GetConstantAngularVelocity(), -2.25f),
                "Negative constant angular velocity round-trips");

    return true;
}

bool TestKinematicBody2D() {
    TEST_SECTION("KinematicBody2DComponent: Configuration");

    std::shared_ptr<KinematicBody2DComponent> body = std::make_shared<KinematicBody2DComponent>();
    body->RegisterProperties();

    TEST_ASSERT(body->GetTypeName() == "KinematicBody2DComponent", "Reports its type name");
    TEST_ASSERT(body->GetPhysicsBody() == nullptr, "No live physics body when detached from a world");

    TEST_ASSERT(!body->GetGravityEnabled(), "Default gravity enabled is false for kinematic bodies");
    TEST_ASSERT(FloatEq(body->GetGravityScale(), 1.0f), "Default gravity scale is 1.0");

    body->SetGravityEnabled(true);
    TEST_ASSERT(body->GetGravityEnabled(), "Gravity-enabled round-trips to true");
    body->SetGravityEnabled(false);
    TEST_ASSERT(!body->GetGravityEnabled(), "Gravity-enabled round-trips to false");

    body->SetGravityScale(0.5f);
    TEST_ASSERT(FloatEq(body->GetGravityScale(), 0.5f), "Gravity scale round-trips");
    body->SetGravityScale(-3.0f);
    TEST_ASSERT(FloatEq(body->GetGravityScale(), -3.0f), "Negative gravity scale round-trips");

    TEST_ASSERT(Vec2Eq(body->GetLinearVelocity(), math::Vec2(0.0f, 0.0f)),
                "Detached body reports zero linear velocity");
    TEST_ASSERT(FloatEq(body->GetAngularVelocity(), 0.0f),
                "Detached body reports zero angular velocity");

    return true;
}

bool TestAreaTrigger2D() {
    TEST_SECTION("AreaTrigger2DComponent: Configuration");

    std::shared_ptr<AreaTrigger2DComponent> area = std::make_shared<AreaTrigger2DComponent>();
    area->RegisterProperties();

    TEST_ASSERT(area->GetTypeName() == "AreaTrigger2DComponent", "Reports its type name");
    TEST_ASSERT(area->GetPhysicsBody() == nullptr, "No live physics body when detached from a world");

    TEST_ASSERT(area->GetMonitoring(), "Default monitoring is true");
    TEST_ASSERT(area->GetMonitorable(), "Default monitorable is true");
    TEST_ASSERT(area->GetPriority() == 0, "Default priority is 0");
    TEST_ASSERT(area->GetShapeType() == CollisionShape2DType::Rectangle, "Default shape type is Rectangle");
    TEST_ASSERT(Vec2Eq(area->GetSize(), math::Vec2(100.0f, 100.0f)), "Default size is 100x100");
    TEST_ASSERT(FloatEq(area->GetRadius(), 50.0f), "Default radius is 50");
    TEST_ASSERT(Vec2Eq(area->GetOffset(), math::Vec2(0.0f, 0.0f)), "Default offset is zero");
    TEST_ASSERT(area->GetCollisionLayers() == 1u, "Default collision layer is 1");
    TEST_ASSERT(area->GetCollisionMask() == 0xFFFFFFFFu, "Default collision mask detects all layers");

    area->SetMonitoring(false);
    TEST_ASSERT(!area->GetMonitoring(), "Monitoring round-trips to false");
    area->SetMonitoring(true);
    TEST_ASSERT(area->GetMonitoring(), "Monitoring round-trips to true");

    area->SetMonitorable(false);
    TEST_ASSERT(!area->GetMonitorable(), "Monitorable round-trips to false");

    area->SetPriority(42);
    TEST_ASSERT(area->GetPriority() == 42, "Priority round-trips");

    area->SetShapeType(CollisionShape2DType::Circle);
    TEST_ASSERT(area->GetShapeType() == CollisionShape2DType::Circle, "Shape type round-trips to Circle");
    area->SetShapeType(CollisionShape2DType::Hexagon);
    TEST_ASSERT(area->GetShapeType() == CollisionShape2DType::Hexagon, "Shape type round-trips to Hexagon");

    area->SetSize(math::Vec2(64.0f, 32.0f));
    TEST_ASSERT(Vec2Eq(area->GetSize(), math::Vec2(64.0f, 32.0f)), "Size round-trips");

    area->SetRadius(12.5f);
    TEST_ASSERT(FloatEq(area->GetRadius(), 12.5f), "Radius round-trips");

    area->SetOffset(math::Vec2(5.0f, -7.0f));
    TEST_ASSERT(Vec2Eq(area->GetOffset(), math::Vec2(5.0f, -7.0f)), "Offset round-trips");

    uint32_t layerBits = (1u << 2) | (1u << 4);
    area->SetCollisionLayers(layerBits);
    TEST_ASSERT(area->GetCollisionLayers() == layerBits, "Collision layer bitmask round-trips");
    TEST_ASSERT((area->GetCollisionLayers() & (1u << 2)) != 0u, "Layer bit 2 set");
    TEST_ASSERT((area->GetCollisionLayers() & (1u << 3)) == 0u, "Layer bit 3 clear");

    uint32_t maskBits = (1u << 0) | (1u << 5);
    area->SetCollisionMask(maskBits);
    TEST_ASSERT(area->GetCollisionMask() == maskBits, "Collision mask bitmask round-trips");
    TEST_ASSERT((area->GetCollisionMask() & (1u << 5)) != 0u, "Mask bit 5 set");

    std::vector<math::Vec2> verts = {
        math::Vec2(-1.0f, -1.0f),
        math::Vec2(1.0f, -1.0f),
        math::Vec2(0.0f, 1.0f)
    };
    area->SetVertices(verts);
    TEST_ASSERT(area->GetVertices().size() == 3, "Vertices round-trip (count)");
    TEST_ASSERT(Vec2Eq(area->GetVertices()[2], math::Vec2(0.0f, 1.0f)), "Vertices round-trip (value)");

    TEST_ASSERT(area->GetOverlappingBodies().empty(), "No overlapping bodies when detached");
    TEST_ASSERT(!area->IsOverlapping(core::UUID()), "IsOverlapping reports false for an unknown body");

    return true;
}

bool TestCollisionBody2D() {
    TEST_SECTION("CollisionBody2DComponent: Configuration");

    std::shared_ptr<CollisionBody2DComponent> shape = std::make_shared<CollisionBody2DComponent>();
    shape->RegisterProperties();

    TEST_ASSERT(shape->GetTypeName() == "CollisionBody2DComponent", "Reports its type name");

    TEST_ASSERT(shape->GetShapeType() == CollisionShape2DType::Rectangle, "Default shape type is Rectangle");
    TEST_ASSERT(Vec2Eq(shape->GetSize(), math::Vec2(100.0f, 100.0f)), "Default size is 100x100");
    TEST_ASSERT(FloatEq(shape->GetRadius(), 50.0f), "Default radius is 50");
    TEST_ASSERT(Vec2Eq(shape->GetOffset(), math::Vec2(0.0f, 0.0f)), "Default offset is zero");
    TEST_ASSERT(FloatEq(shape->GetDensity(), 1.0f), "Default density is 1.0");
    TEST_ASSERT(FloatEq(shape->GetFriction(), 0.3f), "Default friction is 0.3");
    TEST_ASSERT(FloatEq(shape->GetRestitution(), 0.0f), "Default restitution is 0.0");
    TEST_ASSERT(!shape->IsSensor(), "Default sensor mode is false");
    TEST_ASSERT(shape->GetCollisionLayers() == 1u, "Default collision layer is 1");
    TEST_ASSERT(shape->GetCollisionMask() == 0xFFFFFFFFu, "Default collision mask detects all layers");

    shape->SetSize(math::Vec2(48.0f, 24.0f));
    TEST_ASSERT(Vec2Eq(shape->GetSize(), math::Vec2(48.0f, 24.0f)), "Size round-trips");

    shape->SetShapeType(CollisionShape2DType::Circle);
    TEST_ASSERT(shape->GetShapeType() == CollisionShape2DType::Circle, "Shape type round-trips to Circle");
    shape->SetRadius(8.0f);
    TEST_ASSERT(FloatEq(shape->GetRadius(), 8.0f), "Radius round-trips");

    shape->SetOffset(math::Vec2(-2.0f, 3.0f));
    TEST_ASSERT(Vec2Eq(shape->GetOffset(), math::Vec2(-2.0f, 3.0f)), "Offset round-trips");

    shape->SetDensity(2.5f);
    TEST_ASSERT(FloatEq(shape->GetDensity(), 2.5f), "Density round-trips");
    shape->SetFriction(0.6f);
    TEST_ASSERT(FloatEq(shape->GetFriction(), 0.6f), "Friction round-trips");
    shape->SetRestitution(0.8f);
    TEST_ASSERT(FloatEq(shape->GetRestitution(), 0.8f), "Restitution round-trips");

    shape->SetSensor(true);
    TEST_ASSERT(shape->IsSensor(), "Sensor mode round-trips to true");
    shape->SetSensor(false);
    TEST_ASSERT(!shape->IsSensor(), "Sensor mode round-trips to false");

    uint32_t layerBits = (1u << 1) | (1u << 6);
    shape->SetCollisionLayers(layerBits);
    TEST_ASSERT(shape->GetCollisionLayers() == layerBits, "Collision layer bitmask round-trips");
    TEST_ASSERT((shape->GetCollisionLayers() & (1u << 1)) != 0u, "Layer bit 1 set");
    TEST_ASSERT((shape->GetCollisionLayers() & (1u << 7)) == 0u, "Layer bit 7 clear");

    uint32_t maskBits = (1u << 1) | (1u << 2) | (1u << 3);
    shape->SetCollisionMask(maskBits);
    TEST_ASSERT(shape->GetCollisionMask() == maskBits, "Collision mask bitmask round-trips");

    math::Color debugColor(0.25f, 0.5f, 0.75f, 1.0f);
    shape->SetDebugColor(debugColor);
    math::Color retrieved = shape->GetDebugColor();
    TEST_ASSERT(FloatEq(retrieved.r, 0.25f) && FloatEq(retrieved.g, 0.5f) &&
                FloatEq(retrieved.b, 0.75f) && FloatEq(retrieved.a, 1.0f),
                "Debug color round-trips");

    return true;
}

bool TestCollisionBody2DVertices() {
    TEST_SECTION("CollisionBody2DComponent: Vertex Editing");

    std::shared_ptr<CollisionBody2DComponent> shape = std::make_shared<CollisionBody2DComponent>();
    shape->RegisterProperties();
    shape->SetShapeType(CollisionShape2DType::Polygon);

    shape->ClearVertices();
    TEST_ASSERT(shape->GetVertices().empty(), "ClearVertices empties the vertex list");

    shape->AddVertex(math::Vec2(0.0f, 0.0f));
    shape->AddVertex(math::Vec2(2.0f, 0.0f));
    shape->AddVertex(math::Vec2(2.0f, 2.0f));
    shape->AddVertex(math::Vec2(0.0f, 2.0f));
    TEST_ASSERT(shape->GetVertices().size() == 4, "AddVertex appends vertices");
    TEST_ASSERT(Vec2Eq(shape->GetVertices()[2], math::Vec2(2.0f, 2.0f)), "Appended vertex stored at index");

    shape->UpdateVertex(1, math::Vec2(3.0f, -1.0f));
    TEST_ASSERT(Vec2Eq(shape->GetVertices()[1], math::Vec2(3.0f, -1.0f)), "UpdateVertex modifies the vertex");

    shape->UpdateVertex(99, math::Vec2(9.0f, 9.0f));
    TEST_ASSERT(shape->GetVertices().size() == 4, "UpdateVertex ignores an out-of-range index");

    shape->RemoveVertex(0);
    TEST_ASSERT(shape->GetVertices().size() == 3, "RemoveVertex drops a vertex");
    TEST_ASSERT(Vec2Eq(shape->GetVertices()[0], math::Vec2(3.0f, -1.0f)), "RemoveVertex shifts remaining vertices");

    shape->RemoveVertex(-1);
    TEST_ASSERT(shape->GetVertices().size() == 3, "RemoveVertex ignores a negative index");

    std::vector<math::Vec2> replacement = {
        math::Vec2(0.0f, 0.0f),
        math::Vec2(1.0f, 0.0f),
        math::Vec2(0.5f, 1.0f)
    };
    shape->SetVertices(replacement);
    TEST_ASSERT(shape->GetVertices().size() == 3, "SetVertices replaces the whole list");
    TEST_ASSERT(Vec2Eq(shape->GetVertices()[2], math::Vec2(0.5f, 1.0f)), "SetVertices preserves values");

    return true;
}

bool TestCollisionShapeGeneration() {
    TEST_SECTION("CollisionBody2DComponent: Shape Vertex Generation");

    std::vector<math::Vec2> out;
    math::Vec2 size(40.0f, 20.0f);
    float radius = 10.0f;

    CollisionBody2DComponent::GenerateShapeVertices(CollisionShape2DType::Rectangle, size, radius, out);
    TEST_ASSERT(out.size() == 4, "Rectangle generates 4 vertices");
    TEST_ASSERT(Vec2Eq(out[0], math::Vec2(-20.0f, -10.0f)), "Rectangle corner uses half extents");

    CollisionBody2DComponent::GenerateShapeVertices(CollisionShape2DType::Triangle, size, radius, out);
    TEST_ASSERT(out.size() == 3, "Triangle generates 3 vertices");

    CollisionBody2DComponent::GenerateShapeVertices(CollisionShape2DType::Pentagon, size, radius, out);
    TEST_ASSERT(out.size() == 5, "Pentagon generates 5 vertices");

    CollisionBody2DComponent::GenerateShapeVertices(CollisionShape2DType::Hexagon, size, radius, out);
    TEST_ASSERT(out.size() == 6, "Hexagon generates 6 vertices");

    CollisionBody2DComponent::GenerateShapeVertices(CollisionShape2DType::Circle, size, radius, out);
    TEST_ASSERT(out.empty(), "Circle generates no outline vertices");

    CollisionBody2DComponent::GenerateShapeVertices(CollisionShape2DType::Polygon, size, radius, out);
    TEST_ASSERT(out.empty(), "Polygon leaves the outline to user-supplied vertices");

    return true;
}

bool TestCollisionBody2DSerialization() {
    TEST_SECTION("CollisionBody2DComponent: Serialization Round-Trip");

    std::shared_ptr<CollisionBody2DComponent> shape = std::make_shared<CollisionBody2DComponent>();
    shape->RegisterProperties();
    shape->SetShapeType(CollisionShape2DType::Circle);
    shape->SetRadius(16.0f);
    shape->SetDensity(3.0f);
    shape->SetFriction(0.5f);
    shape->SetCollisionLayers((1u << 2));

    std::vector<math::Vec2> verts = {
        math::Vec2(-1.0f, -1.0f),
        math::Vec2(1.0f, -1.0f),
        math::Vec2(1.0f, 1.0f),
        math::Vec2(-1.0f, 1.0f)
    };
    shape->SetVertices(verts);

    nlohmann::json json = shape->Serialize();
    TEST_ASSERT(json.contains("properties"), "Serialization emits a properties object");
    TEST_ASSERT(json["properties"].contains("vertices") && json["properties"]["vertices"].is_array(),
                "Serialization includes the vertices array");

    std::shared_ptr<CollisionBody2DComponent> restored = std::make_shared<CollisionBody2DComponent>();
    restored->RegisterProperties();
    restored->Deserialize(json);

    TEST_ASSERT(restored->GetShapeType() == CollisionShape2DType::Circle, "Deserialized shape type preserved");
    TEST_ASSERT(FloatEq(restored->GetRadius(), 16.0f), "Deserialized radius preserved");
    TEST_ASSERT(FloatEq(restored->GetDensity(), 3.0f), "Deserialized density preserved");
    TEST_ASSERT(FloatEq(restored->GetFriction(), 0.5f), "Deserialized friction preserved");
    TEST_ASSERT(restored->GetCollisionLayers() == (1u << 2), "Deserialized collision layer preserved");
    TEST_ASSERT(restored->GetVertices().size() == 4, "Deserialized vertices preserved");

    return true;
}

bool TestCharacterController2D() {
    TEST_SECTION("CharacterController2D: Configuration");

    std::shared_ptr<CharacterController2D> controller = std::make_shared<CharacterController2D>();
    controller->RegisterProperties();

    TEST_ASSERT(controller->GetTypeName() == "CharacterController2D", "Reports its type name");

    TEST_ASSERT(FloatEq(controller->GetGravity(), -980.0f), "Default gravity is -980");
    TEST_ASSERT(FloatEq(controller->GetMaxFallSpeed(), -1000.0f), "Default max fall speed is -1000");
    TEST_ASSERT(FloatEq(controller->GetGroundDetectionDistance(), 2.0f), "Default ground detection distance is 2");
    TEST_ASSERT(FloatEq(controller->GetWallDetectionDistance(), 2.0f), "Default wall detection distance is 2");
    TEST_ASSERT(FloatEq(controller->GetMaxSlopeAngle(), 45.0f), "Default max slope angle is 45");
    TEST_ASSERT(controller->GetSnapToGround(), "Default snap-to-ground is true");
    TEST_ASSERT(controller->GetMaxBounces() == 4, "Default max bounces is 4");

    controller->SetGravity(-500.0f);
    TEST_ASSERT(FloatEq(controller->GetGravity(), -500.0f), "Gravity round-trips");

    controller->SetMaxFallSpeed(-1500.0f);
    TEST_ASSERT(FloatEq(controller->GetMaxFallSpeed(), -1500.0f), "Max fall speed round-trips");

    controller->SetGroundDetectionDistance(4.0f);
    TEST_ASSERT(FloatEq(controller->GetGroundDetectionDistance(), 4.0f), "Ground detection distance round-trips");

    controller->SetWallDetectionDistance(3.5f);
    TEST_ASSERT(FloatEq(controller->GetWallDetectionDistance(), 3.5f), "Wall detection distance round-trips");

    controller->SetMaxSlopeAngle(60.0f);
    TEST_ASSERT(FloatEq(controller->GetMaxSlopeAngle(), 60.0f), "Max slope angle round-trips");

    controller->SetSnapToGround(false);
    TEST_ASSERT(!controller->GetSnapToGround(), "Snap-to-ground round-trips to false");
    controller->SetSnapToGround(true);
    TEST_ASSERT(controller->GetSnapToGround(), "Snap-to-ground round-trips to true");

    controller->SetMaxBounces(8);
    TEST_ASSERT(controller->GetMaxBounces() == 8, "Max bounces round-trips");

    controller->SetVelocity(math::Vec2(120.0f, -45.0f));
    TEST_ASSERT(Vec2Eq(controller->GetVelocity(), math::Vec2(120.0f, -45.0f)), "Velocity round-trips");

    TEST_ASSERT(!controller->IsOnGround(), "Default not on ground");
    TEST_ASSERT(!controller->IsOnWall(), "Default not on wall");
    TEST_ASSERT(!controller->IsOnCeiling(), "Default not on ceiling");
    TEST_ASSERT(Vec2Eq(controller->GetGroundNormal(), math::Vec2(0.0f, 1.0f)),
                "Default ground normal points up");
    TEST_ASSERT(Vec2Eq(controller->GetWallNormal(), math::Vec2(0.0f, 0.0f)),
                "Default wall normal is zero");

    return true;
}

} // namespace

void RunPhysics2DComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "PHYSICS2D COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Physics2DComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestRigidBody2D();
    allPassed &= TestStaticBody2D();
    allPassed &= TestKinematicBody2D();
    allPassed &= TestAreaTrigger2D();
    allPassed &= TestCollisionBody2D();
    allPassed &= TestCollisionBody2DVertices();
    allPassed &= TestCollisionShapeGeneration();
    allPassed &= TestCollisionBody2DSerialization();
    allPassed &= TestCharacterController2D();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL PHYSICS2D COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
