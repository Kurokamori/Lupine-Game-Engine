#include "lupine/physics3d/Physics3D.hpp"
#include "TestFramework.hpp"
#include <memory>
#include <iostream>
#include <iomanip>
#include <string>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::physics3d;
using namespace lupine::math;

#define PRINT_VEC3(name, vec) \
    std::cout << name << ": (" << vec.x << ", " << vec.y << ", " << vec.z << ")" << std::endl

#define PRINT_QUAT(name, quat) \
    std::cout << name << ": (w=" << quat.w() << ", x=" << quat.x() << ", y=" << quat.y() << ", z=" << quat.z() << ")" << std::endl

#define PRINT_FLOAT(name, value) \
    std::cout << name << ": " << value << std::endl

// ========================================
// Test: Physics World Creation
// ========================================

void TestPhysics3DWorldCreation() {
    TEST_SECTION("Physics3D World Creation");

    Physics3DWorld world;
    
    Vec3 gravity = world.GetGravity();
    PRINT_VEC3("Default gravity", gravity);
    TEST_RESULT(gravity.y < 0.0f, "Gravity points downward");

    world.SetGravity(Vec3(0.0f, -20.0f, 0.0f));
    gravity = world.GetGravity();
    PRINT_VEC3("New gravity", gravity);
    TEST_RESULT(std::abs(gravity.y - (-20.0f)) < 0.001f, "Gravity updated correctly");

    PRINT_FLOAT("Time step", world.GetTimeStep());

    std::cout << "\n[SUCCESS] Physics3D world created and configured\n";
}

// ========================================
// Test: Rigid Body Creation
// ========================================

void TestRigidBody3DCreation() {
    TEST_SECTION("Rigid Body3D Creation");

    Physics3DWorld world;

    // Create static body
    UUID staticId;
    RigidBody3D* staticBody = world.CreateBody(staticId, BodyType::Static);
    TEST_RESULT(staticBody != nullptr, "Static body created");
    TEST_RESULT(staticBody->GetBodyType() == BodyType::Static, "Body type is Static");

    // Create dynamic body
    UUID dynamicId;
    RigidBody3D* dynamicBody = world.CreateBody(dynamicId, BodyType::Dynamic);
    TEST_RESULT(dynamicBody != nullptr, "Dynamic body created");
    TEST_RESULT(dynamicBody->GetBodyType() == BodyType::Dynamic, "Body type is Dynamic");

    // Create kinematic body
    UUID kinematicId;
    RigidBody3D* kinematicBody = world.CreateBody(kinematicId, BodyType::Kinematic);
    TEST_RESULT(kinematicBody != nullptr, "Kinematic body created");
    TEST_RESULT(kinematicBody->GetBodyType() == BodyType::Kinematic, "Body type is Kinematic");

    // Test position
    dynamicBody->SetPosition(Vec3(5.0f, 10.0f, 3.0f));
    Vec3 pos = dynamicBody->GetPosition();
    PRINT_VEC3("Dynamic body position", pos);
    TEST_RESULT(std::abs(pos.x - 5.0f) < 0.001f && std::abs(pos.y - 10.0f) < 0.001f && std::abs(pos.z - 3.0f) < 0.001f,
                "Position set correctly");

    // Test rotation
    Quat rotation = Quat::FromAxisAngle(Vec3::UnitY(), 1.57f); // 90 degrees around Y
    dynamicBody->SetRotation(rotation);
    Quat rot = dynamicBody->GetRotation();
    PRINT_QUAT("Dynamic body rotation", rot);
    TEST_RESULT(std::abs(rot.w() - rotation.w()) < 0.01f, "Rotation set correctly");

    std::cout << "\n[SUCCESS] Rigid bodies created and tested\n";
}

// ========================================
// Test: Colliders
// ========================================

void TestColliders3D() {
    TEST_SECTION("Collider3D Creation and Properties");

    Physics3DWorld world;
    UUID bodyId;
    RigidBody3D* body = world.CreateBody(bodyId, BodyType::Dynamic);

    // Create box collider
    UUID boxId;
    std::unique_ptr<BoxCollider3D> boxCollider = std::make_unique<BoxCollider3D>(body, boxId, Vec3(2.0f, 1.0f, 0.5f));
    TEST_RESULT(boxCollider != nullptr, "Box collider created");
    PRINT_VEC3("Box size", boxCollider->GetSize());

    // Create sphere collider
    UUID sphereId;
    RigidBody3D* sphereBody = world.CreateBody(sphereId, BodyType::Dynamic);
    UUID sphereColliderId;
    std::unique_ptr<SphereCollider3D> sphereCollider = std::make_unique<SphereCollider3D>(sphereBody, sphereColliderId, 1.5f);
    TEST_RESULT(sphereCollider != nullptr, "Sphere collider created");
    PRINT_FLOAT("Sphere radius", sphereCollider->GetRadius());

    // Create capsule collider
    UUID capsuleBodyId;
    RigidBody3D* capsuleBody = world.CreateBody(capsuleBodyId, BodyType::Dynamic);
    UUID capsuleId;
    std::unique_ptr<CapsuleCollider3D> capsuleCollider = std::make_unique<CapsuleCollider3D>(capsuleBody, capsuleId, 0.5f, 2.0f);
    TEST_RESULT(capsuleCollider != nullptr, "Capsule collider created");
    PRINT_FLOAT("Capsule radius", capsuleCollider->GetRadius());
    PRINT_FLOAT("Capsule height", capsuleCollider->GetHeight());

    // Test material properties
    PhysicsMaterial3D material(0.8f, 0.3f, 2.0f);
    boxCollider->SetMaterial(material);
    TEST_RESULT(std::abs(boxCollider->GetFriction() - 0.8f) < 0.001f, "Friction set correctly");
    TEST_RESULT(std::abs(boxCollider->GetRestitution() - 0.3f) < 0.001f, "Restitution set correctly");
    TEST_RESULT(std::abs(boxCollider->GetDensity() - 2.0f) < 0.001f, "Density set correctly");

    // Test sensor mode
    boxCollider->SetIsSensor(true);
    TEST_RESULT(boxCollider->IsSensor(), "Collider is sensor");

    std::cout << "\n[SUCCESS] Colliders created and tested\n";
}

// ========================================
// Test: Physics Simulation
// ========================================

void TestPhysics3DSimulation() {
    TEST_SECTION("Physics3D Simulation");

    Physics3DWorld world;
    world.SetGravity(Vec3(0.0f, -10.0f, 0.0f));

    // Create a dynamic body
    UUID bodyId;
    RigidBody3D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    body->SetPosition(Vec3(0.0f, 10.0f, 0.0f));

    // Add a box collider
    UUID colliderId;
    std::unique_ptr<BoxCollider3D> collider = std::make_unique<BoxCollider3D>(body, colliderId, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    Vec3 initialPos = body->GetPosition();
    PRINT_VEC3("Initial position", initialPos);

    // Simulate for 1 second
    for (int i = 0; i < 60; ++i) {
        world.Step(1.0f / 60.0f);
    }

    Vec3 finalPos = body->GetPosition();
    PRINT_VEC3("Final position after 1 second", finalPos);

    TEST_RESULT(finalPos.y < initialPos.y, "Body fell due to gravity");
    TEST_RESULT(finalPos.y < 5.0f, "Body fell significantly");

    std::cout << "\n[SUCCESS] Physics simulation working\n";
}

// ========================================
// Test: Forces and Impulses
// ========================================

void TestForcesAndImpulses3D() {
    TEST_SECTION("Forces and Impulses");

    Physics3DWorld world;
    world.SetGravity(Vec3(0.0f, 0.0f, 0.0f)); // No gravity for this test

    UUID bodyId;
    RigidBody3D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    body->SetPosition(Vec3(0.0f, 0.0f, 0.0f));

    UUID colliderId;
    std::unique_ptr<BoxCollider3D> collider = std::make_unique<BoxCollider3D>(body, colliderId, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    // Apply impulse
    body->ApplyImpulseToCenter(Vec3(10.0f, 0.0f, 0.0f));

    Vec3 velocity = body->GetLinearVelocity();
    PRINT_VEC3("Velocity after impulse", velocity);
    TEST_RESULT(velocity.x > 0.0f, "Body has positive X velocity");

    // Simulate
    for (int i = 0; i < 30; ++i) {
        world.Step(1.0f / 60.0f);
    }

    Vec3 finalPos = body->GetPosition();
    PRINT_VEC3("Position after 0.5 seconds", finalPos);
    TEST_RESULT(finalPos.x > 0.0f, "Body moved in positive X direction");

    // Test force application
    UUID body2Id;
    RigidBody3D* body2 = world.CreateBody(body2Id, BodyType::Dynamic);
    body2->SetPosition(Vec3(0.0f, 0.0f, 0.0f));
    UUID collider2Id;
    std::unique_ptr<BoxCollider3D> collider2 = std::make_unique<BoxCollider3D>(body2, collider2Id, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body2->GetColliders().empty() && body2->GetColliders().back() == collider2.get(), "collider2 attached to body2");

    // Apply continuous force
    for (int i = 0; i < 30; ++i) {
        body2->ApplyForceToCenter(Vec3(0.0f, 100.0f, 0.0f));
        world.Step(1.0f / 60.0f);
    }

    Vec3 body2Pos = body2->GetPosition();
    PRINT_VEC3("Body2 position after force application", body2Pos);
    TEST_RESULT(body2Pos.y > 0.0f, "Body2 moved upward from force");

    std::cout << "\n[SUCCESS] Forces and impulses working\n";
}

// ========================================
// Test: Velocity
// ========================================

void TestVelocity() {
    TEST_SECTION("Velocity Control");

    Physics3DWorld world;
    world.SetGravity(Vec3(0.0f, 0.0f, 0.0f));

    UUID bodyId;
    RigidBody3D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    UUID colliderId;
    std::unique_ptr<BoxCollider3D> collider = std::make_unique<BoxCollider3D>(body, colliderId, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    // Set linear velocity
    body->SetLinearVelocity(Vec3(5.0f, 3.0f, -2.0f));
    Vec3 vel = body->GetLinearVelocity();
    PRINT_VEC3("Linear velocity", vel);
    TEST_RESULT(std::abs(vel.x - 5.0f) < 0.1f, "Linear velocity X set correctly");
    TEST_RESULT(std::abs(vel.y - 3.0f) < 0.1f, "Linear velocity Y set correctly");
    TEST_RESULT(std::abs(vel.z - (-2.0f)) < 0.1f, "Linear velocity Z set correctly");

    // Set angular velocity
    body->SetAngularVelocity(Vec3(0.0f, 1.0f, 0.0f));
    Vec3 angVel = body->GetAngularVelocity();
    PRINT_VEC3("Angular velocity", angVel);
    TEST_RESULT(std::abs(angVel.y - 1.0f) < 0.1f, "Angular velocity set correctly");

    std::cout << "\n[SUCCESS] Velocity control working\n";
}

// ========================================
// Test: Mass Properties
// ========================================

void TestMassProperties() {
    TEST_SECTION("Mass Properties");

    Physics3DWorld world;

    UUID bodyId;
    RigidBody3D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    UUID colliderId;
    std::unique_ptr<BoxCollider3D> collider = std::make_unique<BoxCollider3D>(body, colliderId, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    // Set mass
    body->SetMass(5.0f);
    float mass = body->GetMass();
    PRINT_FLOAT("Mass", mass);
    TEST_RESULT(std::abs(mass - 5.0f) < 0.1f, "Mass set correctly");

    // Test damping
    body->SetLinearDamping(0.5f);
    body->SetAngularDamping(0.3f);
    PRINT_FLOAT("Linear damping", body->GetLinearDamping());
    PRINT_FLOAT("Angular damping", body->GetAngularDamping());
    TEST_RESULT(std::abs(body->GetLinearDamping() - 0.5f) < 0.01f, "Linear damping set correctly");
    TEST_RESULT(std::abs(body->GetAngularDamping() - 0.3f) < 0.01f, "Angular damping set correctly");

    std::cout << "\n[SUCCESS] Mass properties working\n";
}

// ========================================
// Test: Raycasting
// ========================================

void TestRaycasting3D() {
    TEST_SECTION("Raycasting3D");

    Physics3DWorld world;

    // Create a wall
    UUID wallId;
    RigidBody3D* wall = world.CreateBody(wallId, BodyType::Static);
    wall->SetPosition(Vec3(10.0f, 0.0f, 0.0f));
    UUID wallColliderId;
    std::unique_ptr<BoxCollider3D> wallCollider = std::make_unique<BoxCollider3D>(wall, wallColliderId, Vec3(1.0f, 10.0f, 10.0f));
    TEST_RESULT(!wall->GetColliders().empty() && wall->GetColliders().back() == wallCollider.get(), "wallCollider attached to wall");

    // Raycast that should hit
    RaycastHit3D hit;
    bool didHit = world.Raycast(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), 20.0f, hit);

    TEST_RESULT(didHit, "Raycast hit the wall");
    if (didHit) {
        PRINT_VEC3("Hit point", hit.point);
        PRINT_VEC3("Hit normal", hit.normal);
        PRINT_FLOAT("Hit fraction", hit.fraction);
        TEST_RESULT(hit.point.x < 10.0f && hit.point.x > 9.0f, "Hit point is near wall");
    }

    // Raycast that should miss
    RaycastHit3D missHit;
    bool didMiss = world.Raycast(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), 5.0f, missHit);
    TEST_RESULT(!didMiss, "Raycast missed (no object above)");

    // Test RaycastAll
    UUID wall2Id;
    RigidBody3D* wall2 = world.CreateBody(wall2Id, BodyType::Static);
    wall2->SetPosition(Vec3(15.0f, 0.0f, 0.0f));
    UUID wall2ColliderId;
    std::unique_ptr<BoxCollider3D> wall2Collider = std::make_unique<BoxCollider3D>(wall2, wall2ColliderId, Vec3(1.0f, 10.0f, 10.0f));
    TEST_RESULT(!wall2->GetColliders().empty() && wall2->GetColliders().back() == wall2Collider.get(), "wall2Collider attached to wall2");

    std::vector<RaycastHit3D> hits = world.RaycastAll(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), 20.0f);
    PRINT_FLOAT("Number of hits", static_cast<float>(hits.size()));
    TEST_RESULT(hits.size() >= 1, "RaycastAll found at least one hit");

    std::cout << "\n[SUCCESS] Raycasting working\n";
}

// ========================================
// Test: Overlap Queries
// ========================================

void TestOverlapQueries3D() {
    TEST_SECTION("Overlap Queries");

    Physics3DWorld world;

    // Create several bodies
    UUID body1Id;
    RigidBody3D* body1 = world.CreateBody(body1Id, BodyType::Static);
    body1->SetPosition(Vec3(0.0f, 0.0f, 0.0f));
    UUID collider1Id;
    std::unique_ptr<BoxCollider3D> collider1 = std::make_unique<BoxCollider3D>(body1, collider1Id, Vec3(2.0f, 2.0f, 2.0f));
    TEST_RESULT(!body1->GetColliders().empty() && body1->GetColliders().back() == collider1.get(), "collider1 attached to body1");

    UUID body2Id;
    RigidBody3D* body2 = world.CreateBody(body2Id, BodyType::Static);
    body2->SetPosition(Vec3(5.0f, 0.0f, 0.0f));
    UUID collider2Id;
    std::unique_ptr<SphereCollider3D> collider2 = std::make_unique<SphereCollider3D>(body2, collider2Id, 1.0f);
    TEST_RESULT(!body2->GetColliders().empty() && body2->GetColliders().back() == collider2.get(), "collider2 attached to body2");

    UUID body3Id;
    RigidBody3D* body3 = world.CreateBody(body3Id, BodyType::Static);
    body3->SetPosition(Vec3(0.0f, 5.0f, 0.0f));
    UUID collider3Id;
    std::unique_ptr<BoxCollider3D> collider3 = std::make_unique<BoxCollider3D>(body3, collider3Id, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body3->GetColliders().empty() && body3->GetColliders().back() == collider3.get(), "collider3 attached to body3");

    // Test OverlapSphere
    std::vector<OverlapResult3D> sphereResults = world.OverlapSphere(Vec3(0.0f, 0.0f, 0.0f), 3.0f);
    PRINT_FLOAT("Sphere overlap count", static_cast<float>(sphereResults.size()));
    TEST_RESULT(sphereResults.size() >= 1, "OverlapSphere found at least one body");

    // Test OverlapBox
    std::vector<OverlapResult3D> boxResults = world.OverlapBox(Vec3(0.0f, 0.0f, 0.0f), Vec3(2.0f, 2.0f, 2.0f));
    PRINT_FLOAT("Box overlap count", static_cast<float>(boxResults.size()));
    TEST_RESULT(boxResults.size() >= 1, "OverlapBox found at least one body");

    std::cout << "\n[SUCCESS] Overlap queries working\n";
}

// ========================================
// Test: Shape Casting
// ========================================

void TestShapeCasting() {
    TEST_SECTION("Shape Casting");

    Physics3DWorld world;

    // Create a wall
    UUID wallId;
    RigidBody3D* wall = world.CreateBody(wallId, BodyType::Static);
    wall->SetPosition(Vec3(10.0f, 0.0f, 0.0f));
    UUID wallColliderId;
    std::unique_ptr<BoxCollider3D> wallCollider = std::make_unique<BoxCollider3D>(wall, wallColliderId, Vec3(1.0f, 10.0f, 10.0f));
    TEST_RESULT(!wall->GetColliders().empty() && wall->GetColliders().back() == wallCollider.get(), "wallCollider attached to wall");

    // Sphere cast that should hit
    ShapeCastHit3D hit;
    bool didHit = world.SphereCast(Vec3(0.0f, 0.0f, 0.0f), Vec3(15.0f, 0.0f, 0.0f), 0.5f, hit);

    TEST_RESULT(didHit, "SphereCast hit the wall");
    if (didHit) {
        PRINT_VEC3("Hit point", hit.point);
        PRINT_VEC3("Hit normal", hit.normal);
        PRINT_FLOAT("Hit fraction", hit.fraction);
        TEST_RESULT(hit.point.x < 10.0f, "Hit point is before wall center");
    }

    std::cout << "\n[SUCCESS] Shape casting working\n";
}

// ========================================
// Test: Constraints and Factors
// ========================================

void TestConstraints() {
    TEST_SECTION("Constraints and Factors");

    Physics3DWorld world;

    UUID bodyId;
    RigidBody3D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    UUID colliderId;
    std::unique_ptr<BoxCollider3D> collider = std::make_unique<BoxCollider3D>(body, colliderId, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    // Test linear factor (lock movement on certain axes)
    body->SetLinearFactor(Vec3(1.0f, 0.0f, 1.0f)); // Lock Y axis
    Vec3 linearFactor = body->GetLinearFactor();
    PRINT_VEC3("Linear factor", linearFactor);
    TEST_RESULT(linearFactor.y == 0.0f, "Y axis locked");

    // Test angular factor
    body->SetAngularFactor(Vec3(0.0f, 1.0f, 0.0f)); // Only allow rotation around Y
    Vec3 angularFactor = body->GetAngularFactor();
    PRINT_VEC3("Angular factor", angularFactor);
    TEST_RESULT(angularFactor.y == 1.0f && angularFactor.x == 0.0f, "Only Y rotation allowed");

    std::cout << "\n[SUCCESS] Constraints working\n";
}

// ========================================
// Test: Gravity Control
// ========================================

void TestGravityControl() {
    TEST_SECTION("Gravity Control");

    Physics3DWorld world;
    world.SetGravity(Vec3(0.0f, -10.0f, 0.0f));

    UUID bodyId;
    RigidBody3D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    body->SetPosition(Vec3(0.0f, 10.0f, 0.0f));
    UUID colliderId;
    std::unique_ptr<BoxCollider3D> collider = std::make_unique<BoxCollider3D>(body, colliderId, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    // Test gravity scale
    body->SetGravityScale(2.0f);
    PRINT_FLOAT("Gravity scale", body->GetGravityScale());
    TEST_RESULT(std::abs(body->GetGravityScale() - 2.0f) < 0.01f, "Gravity scale set correctly");

    // Test disabling gravity
    body->SetUseGravity(false);
    TEST_RESULT(!body->GetUseGravity(), "Gravity disabled");

    body->SetUseGravity(true);
    TEST_RESULT(body->GetUseGravity(), "Gravity enabled");

    std::cout << "\n[SUCCESS] Gravity control working\n";
}

// ========================================
// Test: Sleep State
// ========================================

void TestSleepState() {
    TEST_SECTION("Sleep State Control");

    Physics3DWorld world;

    UUID bodyId;
    RigidBody3D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    UUID colliderId;
    std::unique_ptr<BoxCollider3D> collider = std::make_unique<BoxCollider3D>(body, colliderId, Vec3(1.0f, 1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    // Test wake up
    body->WakeUp();
    TEST_RESULT(body->IsAwake(), "Body is awake");

    // Test sleeping enabled
    body->SetSleepingEnabled(true);
    TEST_RESULT(body->IsSleepingEnabled(), "Sleeping is enabled");

    std::cout << "\n[SUCCESS] Sleep state control working\n";
}

// ========================================
// Main Test Runner
// ========================================

void RunPhysics3DTests() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  LUPINE ENGINE - PHYSICS3D TEST SUITE  \n";
    std::cout << "========================================\n";

    lupine_test::SetCurrentSuite("Physics3D System");

    InitializePhysics3D();

    try {
        TestPhysics3DWorldCreation();
        TestRigidBody3DCreation();
        TestColliders3D();
        TestPhysics3DSimulation();
        TestForcesAndImpulses3D();
        TestVelocity();
        TestMassProperties();
        TestRaycasting3D();
        TestOverlapQueries3D();
        TestShapeCasting();
        TestConstraints();
        TestGravityControl();
        TestSleepState();

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  ALL PHYSICS3D TESTS COMPLETED!       \n";
        std::cout << "========================================\n";
        std::cout << "\n";
    }
    catch (const std::exception& e) {
        std::cout << "\n[ERROR] Test failed with exception: " << e.what() << "\n";
    }

    ShutdownPhysics3D();
}


