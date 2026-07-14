/**
 * @file Physics3DComponentTests.cpp
 * @brief Headless console tests for the 3D physics COMPONENTS' public configuration API.
 *
 * These tests exercise the component-level config surface (mass, damping, gravity,
 * axis locks, collision shape/material/layers, character-controller tuning, etc.)
 * WITHOUT requiring a live Bullet physics world or an instantiated body. The
 * underlying world simulation (forces, impulses, raycasts, stepping) is already
 * covered by Physics3DTests.cpp and is intentionally NOT re-tested here.
 *
 * Components store their configuration either in the declarative property registry
 * (populated by DefineProperties()) or in plain member variables. For the property-
 * backed components DefineProperties() must be invoked once after construction so
 * that GetPropertyValue/SetPropertyValue have registered descriptors with defaults
 * to read and write; the production code calls this from RegisterProperties() /
 * Deserialize(), which the editor and scene loader drive automatically.
 *
 * Methods that mutate the live physics body (velocity setters, force/impulse
 * application, MoveBy/RotateBy, MoveAndSlide) are no-ops without a body and are
 * skipped here with a note, as they belong to the world-simulation tests.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/RigidBody3DComponent.hpp"
#include "lupine/components/StaticBody3DComponent.hpp"
#include "lupine/components/KinematicBody3DComponent.hpp"
#include "lupine/components/AreaTrigger3DComponent.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/components/CharacterController3D.hpp"
#include "lupine/core/Core.hpp"
#include "TestFramework.hpp"
#include <memory>
#include <cstdint>
#include <iostream>
#include <cmath>

using namespace lupine;
using namespace lupine::core;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool TestRigidBody3DConfig() {
    TEST_SECTION("RigidBody3D: Configuration");

    std::shared_ptr<components::RigidBody3DComponent> body =
        std::make_shared<components::RigidBody3DComponent>();
    body->DefineProperties();

    TEST_ASSERT(body->GetTypeName() == "RigidBody3DComponent", "Type name reported correctly");
    TEST_ASSERT(body->GetPhysicsBody() == nullptr, "No physics body before instantiation");

    TEST_ASSERT(FloatEq(body->GetMass(), 1.0f), "Default mass is 1.0");
    body->SetMass(7.5f);
    TEST_ASSERT(FloatEq(body->GetMass(), 7.5f), "Mass round-trips");

    TEST_ASSERT(FloatEq(body->GetGravityScale(), 1.0f), "Default gravity scale is 1.0");
    body->SetGravityScale(2.5f);
    TEST_ASSERT(FloatEq(body->GetGravityScale(), 2.5f), "Gravity scale round-trips");
    body->SetGravityScale(-3.0f);
    TEST_ASSERT(FloatEq(body->GetGravityScale(), -3.0f), "Negative gravity scale round-trips");

    TEST_ASSERT(FloatEq(body->GetLinearDamping(), 0.0f), "Default linear damping is 0.0");
    body->SetLinearDamping(0.4f);
    TEST_ASSERT(FloatEq(body->GetLinearDamping(), 0.4f), "Linear damping round-trips");

    TEST_ASSERT(FloatEq(body->GetAngularDamping(), 0.0f), "Default angular damping is 0.0");
    body->SetAngularDamping(0.6f);
    TEST_ASSERT(FloatEq(body->GetAngularDamping(), 0.6f), "Angular damping round-trips");

    TEST_ASSERT(!body->GetLockRotationX(), "Lock rotation X defaults off");
    TEST_ASSERT(!body->GetLockRotationY(), "Lock rotation Y defaults off");
    TEST_ASSERT(!body->GetLockRotationZ(), "Lock rotation Z defaults off");
    body->SetLockRotationX(true);
    body->SetLockRotationZ(true);
    TEST_ASSERT(body->GetLockRotationX(), "Lock rotation X round-trips");
    TEST_ASSERT(!body->GetLockRotationY(), "Lock rotation Y remains independent");
    TEST_ASSERT(body->GetLockRotationZ(), "Lock rotation Z round-trips");
    body->SetLockRotationX(false);
    TEST_ASSERT(!body->GetLockRotationX(), "Lock rotation X clears");

    TEST_ASSERT(body->GetCanSleep(), "Can-sleep defaults true");
    body->SetCanSleep(false);
    TEST_ASSERT(!body->GetCanSleep(), "Can-sleep round-trips");

    TEST_ASSERT(body->GetGravityEnabled(), "Gravity enabled defaults true");
    body->SetGravityEnabled(false);
    TEST_ASSERT(!body->GetGravityEnabled(), "Gravity enabled round-trips");

    TEST_ASSERT(!body->GetUseCCD(), "CCD defaults off");
    body->SetUseCCD(true);
    TEST_ASSERT(body->GetUseCCD(), "CCD round-trips");

    TEST_ASSERT(body->GetLinearVelocity() == math::Vec3::Zero(),
                "Linear velocity reads zero without a live body");
    TEST_ASSERT(body->GetAngularVelocity() == math::Vec3::Zero(),
                "Angular velocity reads zero without a live body");

    return true;
}

bool TestStaticBody3DConfig() {
    TEST_SECTION("StaticBody3D: Configuration");

    std::shared_ptr<components::StaticBody3DComponent> body =
        std::make_shared<components::StaticBody3DComponent>();
    body->DefineProperties();

    TEST_ASSERT(body->GetTypeName() == "StaticBody3DComponent", "Type name reported correctly");
    TEST_ASSERT(body->GetPhysicsBody() == nullptr, "No physics body before instantiation");

    TEST_ASSERT(body->GetConstantLinearVelocity() == math::Vec3::Zero(),
                "Constant linear velocity defaults to zero");
    body->SetConstantLinearVelocity(math::Vec3(1.0f, 0.0f, -2.0f));
    math::Vec3 linVel = body->GetConstantLinearVelocity();
    TEST_ASSERT(FloatEq(linVel.x, 1.0f) && FloatEq(linVel.z, -2.0f),
                "Constant linear velocity round-trips");

    TEST_ASSERT(body->GetConstantAngularVelocity() == math::Vec3::Zero(),
                "Constant angular velocity defaults to zero");
    body->SetConstantAngularVelocity(math::Vec3(0.0f, 3.0f, 0.0f));
    math::Vec3 angVel = body->GetConstantAngularVelocity();
    TEST_ASSERT(FloatEq(angVel.y, 3.0f), "Constant angular velocity round-trips");

    return true;
}

bool TestKinematicBody3DConfig() {
    TEST_SECTION("KinematicBody3D: Configuration");

    std::shared_ptr<components::KinematicBody3DComponent> body =
        std::make_shared<components::KinematicBody3DComponent>();
    body->DefineProperties();

    TEST_ASSERT(body->GetTypeName() == "KinematicBody3DComponent", "Type name reported correctly");
    TEST_ASSERT(body->GetPhysicsBody() == nullptr, "No physics body before instantiation");

    TEST_ASSERT(!body->GetGravityEnabled(), "Kinematic gravity defaults disabled");
    body->SetGravityEnabled(true);
    TEST_ASSERT(body->GetGravityEnabled(), "Gravity enabled round-trips");

    TEST_ASSERT(FloatEq(body->GetGravityScale(), 1.0f), "Default gravity scale is 1.0");
    body->SetGravityScale(0.5f);
    TEST_ASSERT(FloatEq(body->GetGravityScale(), 0.5f), "Gravity scale round-trips");

    TEST_ASSERT(body->GetLinearVelocity() == math::Vec3::Zero(),
                "Linear velocity reads zero without a live body");
    TEST_ASSERT(body->GetAngularVelocity() == math::Vec3::Zero(),
                "Angular velocity reads zero without a live body");

    return true;
}

bool TestAreaTrigger3DConfig() {
    TEST_SECTION("AreaTrigger3D: Configuration");

    std::shared_ptr<components::AreaTrigger3DComponent> area =
        std::make_shared<components::AreaTrigger3DComponent>();
    area->DefineProperties();

    TEST_ASSERT(area->GetTypeName() == "AreaTrigger3DComponent", "Type name reported correctly");
    TEST_ASSERT(area->GetPhysicsBody() == nullptr, "No physics body before instantiation");

    TEST_ASSERT(area->GetMonitoring(), "Monitoring defaults true");
    area->SetMonitoring(false);
    TEST_ASSERT(!area->GetMonitoring(), "Monitoring round-trips");

    TEST_ASSERT(area->GetMonitorable(), "Monitorable defaults true");
    area->SetMonitorable(false);
    TEST_ASSERT(!area->GetMonitorable(), "Monitorable round-trips");

    TEST_ASSERT(area->GetPriority() == 0, "Priority defaults to 0");
    area->SetPriority(42);
    TEST_ASSERT(area->GetPriority() == 42, "Priority round-trips");

    TEST_ASSERT(area->GetOverlappingBodies().empty(), "No overlapping bodies initially");
    UUID arbitrary;
    TEST_ASSERT(!area->IsOverlapping(arbitrary), "Arbitrary body is not reported overlapping");

    return true;
}

bool TestCollisionMesh3DConfig() {
    TEST_SECTION("CollisionMesh3D: Shape Configuration");

    std::shared_ptr<components::CollisionMesh3DComponent> mesh =
        std::make_shared<components::CollisionMesh3DComponent>();
    mesh->DefineProperties();

    TEST_ASSERT(mesh->GetTypeName() == "CollisionMesh3DComponent", "Type name reported correctly");

    TEST_ASSERT(mesh->GetShapeType() == components::CollisionShape3DType::Box,
                "Shape type defaults to Box");
    mesh->SetShapeType(components::CollisionShape3DType::Sphere);
    TEST_ASSERT(mesh->GetShapeType() == components::CollisionShape3DType::Sphere,
                "Shape type round-trips to Sphere");
    mesh->SetShapeType(components::CollisionShape3DType::Capsule);
    TEST_ASSERT(mesh->GetShapeType() == components::CollisionShape3DType::Capsule,
                "Shape type round-trips to Capsule");
    mesh->SetShapeType(components::CollisionShape3DType::Cylinder);
    TEST_ASSERT(mesh->GetShapeType() == components::CollisionShape3DType::Cylinder,
                "Shape type round-trips to Cylinder");
    mesh->SetShapeType(components::CollisionShape3DType::Cone);
    TEST_ASSERT(mesh->GetShapeType() == components::CollisionShape3DType::Cone,
                "Shape type round-trips to Cone");
    mesh->SetShapeType(components::CollisionShape3DType::Plane);
    TEST_ASSERT(mesh->GetShapeType() == components::CollisionShape3DType::Plane,
                "Shape type round-trips to Plane");
    mesh->SetShapeType(components::CollisionShape3DType::Mesh);
    TEST_ASSERT(mesh->GetShapeType() == components::CollisionShape3DType::Mesh,
                "Shape type round-trips to Mesh");
    mesh->SetShapeType(components::CollisionShape3DType::Box);

    math::Vec3 defaultSize = mesh->GetSize();
    TEST_ASSERT(FloatEq(defaultSize.x, 1.0f) && FloatEq(defaultSize.y, 1.0f) && FloatEq(defaultSize.z, 1.0f),
                "Default box size is (1,1,1)");
    mesh->SetSize(math::Vec3(2.0f, 3.0f, 4.0f));
    math::Vec3 newSize = mesh->GetSize();
    TEST_ASSERT(FloatEq(newSize.x, 2.0f) && FloatEq(newSize.y, 3.0f) && FloatEq(newSize.z, 4.0f),
                "Size round-trips");

    TEST_ASSERT(FloatEq(mesh->GetRadius(), 0.5f), "Default radius is 0.5");
    mesh->SetRadius(1.25f);
    TEST_ASSERT(FloatEq(mesh->GetRadius(), 1.25f), "Radius round-trips");

    TEST_ASSERT(FloatEq(mesh->GetHeight(), 1.0f), "Default height is 1.0");
    mesh->SetHeight(2.5f);
    TEST_ASSERT(FloatEq(mesh->GetHeight(), 2.5f), "Height round-trips");

    math::Vec3 defaultNormal = mesh->GetPlaneNormal();
    TEST_ASSERT(FloatEq(defaultNormal.y, 1.0f), "Default plane normal points up");
    mesh->SetPlaneNormal(math::Vec3(1.0f, 0.0f, 0.0f));
    TEST_ASSERT(FloatEq(mesh->GetPlaneNormal().x, 1.0f), "Plane normal round-trips");

    TEST_ASSERT(FloatEq(mesh->GetPlaneDistance(), 0.0f), "Default plane distance is 0.0");
    mesh->SetPlaneDistance(5.0f);
    TEST_ASSERT(FloatEq(mesh->GetPlaneDistance(), 5.0f), "Plane distance round-trips");

    TEST_ASSERT(FloatEq(mesh->GetPlaneWidth(), 10.0f), "Default plane width is 10.0");
    mesh->SetPlaneWidth(20.0f);
    TEST_ASSERT(FloatEq(mesh->GetPlaneWidth(), 20.0f), "Plane width round-trips");

    TEST_ASSERT(FloatEq(mesh->GetPlaneLength(), 10.0f), "Default plane length is 10.0");
    mesh->SetPlaneLength(30.0f);
    TEST_ASSERT(FloatEq(mesh->GetPlaneLength(), 30.0f), "Plane length round-trips");

    TEST_ASSERT(mesh->GetMeshPath().empty(), "Default mesh path is empty");
    mesh->SetMeshPath("res://models/collision.obj");
    TEST_ASSERT(mesh->GetMeshPath() == "res://models/collision.obj", "Mesh path round-trips");

    TEST_ASSERT(mesh->GetMeshSolver() == components::MeshSolverType::Convex,
                "Default mesh solver is Convex");
    mesh->SetMeshSolver(components::MeshSolverType::BoundingBox);
    TEST_ASSERT(mesh->GetMeshSolver() == components::MeshSolverType::BoundingBox,
                "Mesh solver round-trips to BoundingBox");
    mesh->SetMeshSolver(components::MeshSolverType::Concave);
    TEST_ASSERT(mesh->GetMeshSolver() == components::MeshSolverType::Concave,
                "Mesh solver round-trips to Concave");
    mesh->SetMeshSolver(components::MeshSolverType::TriMesh);
    TEST_ASSERT(mesh->GetMeshSolver() == components::MeshSolverType::TriMesh,
                "Mesh solver round-trips to TriMesh");

    TEST_ASSERT(mesh->GetOffset() == math::Vec3::Zero(), "Default offset is zero");
    mesh->SetOffset(math::Vec3(0.5f, -0.5f, 1.0f));
    math::Vec3 offset = mesh->GetOffset();
    TEST_ASSERT(FloatEq(offset.x, 0.5f) && FloatEq(offset.y, -0.5f) && FloatEq(offset.z, 1.0f),
                "Offset round-trips");

    return true;
}

bool TestCollisionMesh3DMaterial() {
    TEST_SECTION("CollisionMesh3D: Material and Layers");

    std::shared_ptr<components::CollisionMesh3DComponent> mesh =
        std::make_shared<components::CollisionMesh3DComponent>();
    mesh->DefineProperties();

    TEST_ASSERT(FloatEq(mesh->GetDensity(), 1.0f), "Default density is 1.0");
    mesh->SetDensity(3.0f);
    TEST_ASSERT(FloatEq(mesh->GetDensity(), 3.0f), "Density round-trips");

    TEST_ASSERT(FloatEq(mesh->GetFriction(), 0.5f), "Default friction is 0.5");
    mesh->SetFriction(0.9f);
    TEST_ASSERT(FloatEq(mesh->GetFriction(), 0.9f), "Friction round-trips");

    TEST_ASSERT(FloatEq(mesh->GetRestitution(), 0.0f), "Default restitution is 0.0");
    mesh->SetRestitution(0.7f);
    TEST_ASSERT(FloatEq(mesh->GetRestitution(), 0.7f), "Restitution round-trips");

    TEST_ASSERT(!mesh->IsSensor(), "Sensor mode defaults off");
    mesh->SetSensor(true);
    TEST_ASSERT(mesh->IsSensor(), "Sensor mode round-trips");

    TEST_ASSERT(mesh->GetCollisionLayers() == 1u, "Default collision layers is layer 1 only");

    uint32_t layerMask = (1u << 0) | (1u << 3) | (1u << 5);
    mesh->SetCollisionLayers(layerMask);
    TEST_ASSERT(mesh->GetCollisionLayers() == layerMask, "Collision layers bitmask round-trips");
    TEST_ASSERT((mesh->GetCollisionLayers() & (1u << 0)) != 0u, "Bit 0 is set in the mask");
    TEST_ASSERT((mesh->GetCollisionLayers() & (1u << 3)) != 0u, "Bit 3 is set in the mask");
    TEST_ASSERT((mesh->GetCollisionLayers() & (1u << 5)) != 0u, "Bit 5 is set in the mask");
    TEST_ASSERT((mesh->GetCollisionLayers() & (1u << 1)) == 0u, "Bit 1 is clear in the mask");
    TEST_ASSERT((mesh->GetCollisionLayers() & (1u << 4)) == 0u, "Bit 4 is clear in the mask");

    mesh->SetCollisionLayers(0u);
    TEST_ASSERT(mesh->GetCollisionLayers() == 0u, "Empty collision mask round-trips");
    mesh->SetCollisionLayers(0xFFFFFFFFu);
    TEST_ASSERT(mesh->GetCollisionLayers() == 0xFFFFFFFFu, "Full collision mask round-trips");

    math::Color defaultColor = mesh->GetDebugColor();
    TEST_ASSERT(FloatEq(defaultColor.g, 1.0f) && FloatEq(defaultColor.a, 0.5f),
                "Default debug color is semi-transparent green");
    mesh->SetDebugColor(math::Color(1.0f, 0.0f, 0.0f, 1.0f));
    math::Color newColor = mesh->GetDebugColor();
    TEST_ASSERT(FloatEq(newColor.r, 1.0f) && FloatEq(newColor.g, 0.0f) && FloatEq(newColor.a, 1.0f),
                "Debug color round-trips");

    return true;
}

bool TestCharacterController3DConfig() {
    TEST_SECTION("CharacterController3D: Configuration");

    std::shared_ptr<components::CharacterController3D> controller =
        std::make_shared<components::CharacterController3D>();
    controller->DefineProperties();

    TEST_ASSERT(controller->GetTypeName() == "CharacterController3D", "Type name reported correctly");

    TEST_ASSERT(FloatEq(controller->GetGravity(), -9.8f), "Default gravity is -9.8");
    controller->SetGravity(-15.0f);
    TEST_ASSERT(FloatEq(controller->GetGravity(), -15.0f), "Gravity round-trips");

    TEST_ASSERT(FloatEq(controller->GetMaxFallSpeed(), -50.0f), "Default max fall speed is -50.0");
    controller->SetMaxFallSpeed(-80.0f);
    TEST_ASSERT(FloatEq(controller->GetMaxFallSpeed(), -80.0f), "Max fall speed round-trips");

    TEST_ASSERT(FloatEq(controller->GetGroundDetectionDistance(), 0.1f),
                "Default ground detection distance is 0.1");
    controller->SetGroundDetectionDistance(0.25f);
    TEST_ASSERT(FloatEq(controller->GetGroundDetectionDistance(), 0.25f),
                "Ground detection distance round-trips");

    TEST_ASSERT(FloatEq(controller->GetWallDetectionDistance(), 0.1f),
                "Default wall detection distance is 0.1");
    controller->SetWallDetectionDistance(0.3f);
    TEST_ASSERT(FloatEq(controller->GetWallDetectionDistance(), 0.3f),
                "Wall detection distance round-trips");

    TEST_ASSERT(FloatEq(controller->GetMaxSlopeAngle(), 45.0f), "Default max slope angle is 45.0");
    controller->SetMaxSlopeAngle(60.0f);
    TEST_ASSERT(FloatEq(controller->GetMaxSlopeAngle(), 60.0f), "Max slope angle round-trips");

    TEST_ASSERT(FloatEq(controller->GetStepHeight(), 0.3f), "Default step height is 0.3");
    controller->SetStepHeight(0.5f);
    TEST_ASSERT(FloatEq(controller->GetStepHeight(), 0.5f), "Step height round-trips");

    TEST_ASSERT(controller->GetSnapToGround(), "Snap-to-ground defaults true");
    controller->SetSnapToGround(false);
    TEST_ASSERT(!controller->GetSnapToGround(), "Snap-to-ground round-trips");

    TEST_ASSERT(controller->GetMaxBounces() == 4, "Default max bounces is 4");
    controller->SetMaxBounces(8);
    TEST_ASSERT(controller->GetMaxBounces() == 8, "Max bounces round-trips");

    return true;
}

bool TestCharacterController3DState() {
    TEST_SECTION("CharacterController3D: Velocity and State");

    std::shared_ptr<components::CharacterController3D> controller =
        std::make_shared<components::CharacterController3D>();
    controller->DefineProperties();

    TEST_ASSERT(controller->GetVelocity() == math::Vec3::Zero(), "Velocity defaults to zero");
    controller->SetVelocity(math::Vec3(2.0f, -1.0f, 4.0f));
    math::Vec3 vel = controller->GetVelocity();
    TEST_ASSERT(FloatEq(vel.x, 2.0f) && FloatEq(vel.y, -1.0f) && FloatEq(vel.z, 4.0f),
                "Velocity round-trips");

    TEST_ASSERT(!controller->IsOnGround(), "Not on ground initially");
    TEST_ASSERT(!controller->IsOnWall(), "Not on wall initially");
    TEST_ASSERT(!controller->IsOnCeiling(), "Not on ceiling initially");

    math::Vec3 groundNormal = controller->GetGroundNormal();
    TEST_ASSERT(FloatEq(groundNormal.y, 1.0f), "Default ground normal points up");
    TEST_ASSERT(controller->GetWallNormal() == math::Vec3::Zero(), "Default wall normal is zero");

    return true;
}

} // namespace

void RunPhysics3DComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "PHYSICS3D COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Physics3DComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestRigidBody3DConfig();
    allPassed &= TestStaticBody3DConfig();
    allPassed &= TestKinematicBody3DConfig();
    allPassed &= TestAreaTrigger3DConfig();
    allPassed &= TestCollisionMesh3DConfig();
    allPassed &= TestCollisionMesh3DMaterial();
    allPassed &= TestCharacterController3DConfig();
    allPassed &= TestCharacterController3DState();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL PHYSICS3D COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
