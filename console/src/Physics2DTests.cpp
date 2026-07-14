#include "lupine/physics2d/Physics2D.hpp"
#include "TestFramework.hpp"
#include <memory>
#include <iostream>
#include <iomanip>
#include <string>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::physics2d;
using namespace lupine::math;

#define PRINT_VEC2(name, vec) \
    std::cout << name << ": (" << vec.x << ", " << vec.y << ")" << std::endl

#define PRINT_FLOAT(name, value) \
    std::cout << name << ": " << value << std::endl

// ========================================
// Test: Physics World Creation
// ========================================

void TestPhysicsWorldCreation() {
    TEST_SECTION("Physics World Creation");

    Physics2DWorld world;
    
    Vec2 gravity = world.GetGravity();
    PRINT_VEC2("Default gravity", gravity);
    TEST_RESULT(gravity.y < 0.0f, "Gravity points downward");

    world.SetGravity(Vec2(0.0f, -20.0f));
    gravity = world.GetGravity();
    PRINT_VEC2("New gravity", gravity);
    TEST_RESULT(std::abs(gravity.y - (-20.0f)) < 0.001f, "Gravity updated correctly");

    PRINT_FLOAT("Time step", world.GetTimeStep());
    PRINT_FLOAT("Velocity iterations", world.GetVelocityIterations());
    PRINT_FLOAT("Position iterations", world.GetPositionIterations());

    std::cout << "\n[SUCCESS] Physics world created and configured\n";
}

// ========================================
// Test: Rigid Body Creation
// ========================================

void TestRigidBodyCreation() {
    TEST_SECTION("Rigid Body Creation");

    Physics2DWorld world;

    // Create static body
    UUID staticId;
    RigidBody2D* staticBody = world.CreateBody(staticId, BodyType::Static);
    TEST_RESULT(staticBody != nullptr, "Static body created");
    TEST_RESULT(staticBody->GetBodyType() == BodyType::Static, "Body type is Static");

    // Create dynamic body
    UUID dynamicId;
    RigidBody2D* dynamicBody = world.CreateBody(dynamicId, BodyType::Dynamic);
    TEST_RESULT(dynamicBody != nullptr, "Dynamic body created");
    TEST_RESULT(dynamicBody->GetBodyType() == BodyType::Dynamic, "Body type is Dynamic");

    // Create kinematic body
    UUID kinematicId;
    RigidBody2D* kinematicBody = world.CreateBody(kinematicId, BodyType::Kinematic);
    TEST_RESULT(kinematicBody != nullptr, "Kinematic body created");
    TEST_RESULT(kinematicBody->GetBodyType() == BodyType::Kinematic, "Body type is Kinematic");

    // Test position
    dynamicBody->SetPosition(Vec2(5.0f, 10.0f));
    Vec2 pos = dynamicBody->GetPosition();
    PRINT_VEC2("Dynamic body position", pos);
    TEST_RESULT(std::abs(pos.x - 5.0f) < 0.001f && std::abs(pos.y - 10.0f) < 0.001f,
                "Position set correctly");

    // Test rotation
    dynamicBody->SetRotation(1.57f); // ~90 degrees
    float angle = dynamicBody->GetRotation();
    PRINT_FLOAT("Dynamic body rotation", angle);
    TEST_RESULT(std::abs(angle - 1.57f) < 0.01f, "Rotation set correctly");

    std::cout << "\n[SUCCESS] Rigid bodies created and tested\n";
}

// ========================================
// Test: Colliders
// ========================================

void TestColliders() {
    TEST_SECTION("Collider Creation and Properties");

    Physics2DWorld world;
    UUID bodyId;
    RigidBody2D* body = world.CreateBody(bodyId, BodyType::Dynamic);

    // Create box collider
    UUID boxId;
    std::unique_ptr<BoxCollider2D> boxCollider = std::make_unique<BoxCollider2D>(body, boxId, Vec2(2.0f, 1.0f));
    TEST_RESULT(boxCollider != nullptr, "Box collider created");
    PRINT_VEC2("Box size", boxCollider->GetSize());

    // Test material properties
    PhysicsMaterial2D material(2.0f, 0.5f, 0.8f);
    boxCollider->SetMaterial(material);
    PhysicsMaterial2D retrievedMaterial = boxCollider->GetMaterial();
    TEST_RESULT(std::abs(retrievedMaterial.density - 2.0f) < 0.001f, "Density set correctly");
    TEST_RESULT(std::abs(retrievedMaterial.friction - 0.5f) < 0.001f, "Friction set correctly");
    TEST_RESULT(std::abs(retrievedMaterial.restitution - 0.8f) < 0.001f, "Restitution set correctly");

    // Create circle collider
    UUID circleId;
    std::unique_ptr<CircleCollider2D> circleCollider = std::make_unique<CircleCollider2D>(body, circleId, 1.5f);
    TEST_RESULT(circleCollider != nullptr, "Circle collider created");
    PRINT_FLOAT("Circle radius", circleCollider->GetRadius());

    // Test sensor
    circleCollider->SetSensor(true);
    TEST_RESULT(circleCollider->IsSensor(), "Collider set as sensor");

    // Create polygon collider
    std::vector<Vec2> vertices = {
        Vec2(-1.0f, -1.0f),
        Vec2(1.0f, -1.0f),
        Vec2(1.0f, 1.0f),
        Vec2(-1.0f, 1.0f)
    };
    UUID polyId;
    std::unique_ptr<PolygonCollider2D> polyCollider = std::make_unique<PolygonCollider2D>(body, polyId, vertices);
    TEST_RESULT(polyCollider != nullptr, "Polygon collider created");
    TEST_RESULT(polyCollider->GetVertices().size() == 4, "Polygon has correct vertex count");

    std::cout << "\n[SUCCESS] Colliders created and tested\n";
}

// ========================================
// Test: Physics Simulation
// ========================================

void TestPhysicsSimulation() {
    TEST_SECTION("Physics Simulation");

    Physics2DWorld world;
    world.SetGravity(Vec2(0.0f, -10.0f));

    // Create a dynamic body
    UUID bodyId;
    RigidBody2D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    body->SetPosition(Vec2(0.0f, 10.0f));

    // Add a box collider
    UUID colliderId;
    std::unique_ptr<BoxCollider2D> collider = std::make_unique<BoxCollider2D>(body, colliderId, Vec2(1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    Vec2 initialPos = body->GetPosition();
    PRINT_VEC2("Initial position", initialPos);

    // Simulate for 1 second
    for (int i = 0; i < 60; ++i) {
        world.Step(1.0f / 60.0f);
    }

    Vec2 finalPos = body->GetPosition();
    PRINT_VEC2("Final position after 1 second", finalPos);
    TEST_RESULT(finalPos.y < initialPos.y, "Body fell due to gravity");

    std::cout << "\n[SUCCESS] Physics simulation working\n";
}

// ========================================
// Test: Forces and Impulses
// ========================================

void TestForcesAndImpulses() {
    TEST_SECTION("Forces and Impulses");

    Physics2DWorld world;
    world.SetGravity(Vec2::Zero()); // No gravity for this test

    UUID bodyId;
    RigidBody2D* body = world.CreateBody(bodyId, BodyType::Dynamic);
    body->SetPosition(Vec2::Zero());

    UUID colliderId;
    std::unique_ptr<BoxCollider2D> collider = std::make_unique<BoxCollider2D>(body, colliderId, Vec2(1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    // Apply impulse
    body->ApplyLinearImpulseToCenter(Vec2(10.0f, 0.0f));

    Vec2 velocity = body->GetLinearVelocity();
    PRINT_VEC2("Velocity after impulse", velocity);
    TEST_RESULT(velocity.x > 0.0f, "Body has positive X velocity");

    // Simulate
    for (int i = 0; i < 30; ++i) {
        world.Step(1.0f / 60.0f);
    }

    Vec2 pos = body->GetPosition();
    PRINT_VEC2("Position after 0.5 seconds", pos);
    TEST_RESULT(pos.x > 0.0f, "Body moved in positive X direction");

    // Test force
    body->SetLinearVelocity(Vec2::Zero());
    body->SetPosition(Vec2::Zero());

    body->ApplyForceToCenter(Vec2(0.0f, 100.0f));

    for (int i = 0; i < 60; ++i) {
        world.Step(1.0f / 60.0f);
    }

    pos = body->GetPosition();
    PRINT_VEC2("Position after force application", pos);
    TEST_RESULT(pos.y > 0.0f, "Body moved in positive Y direction from force");

    std::cout << "\n[SUCCESS] Forces and impulses working\n";
}

// ========================================
// Test: Collision Callbacks
// ========================================

void TestCollisionCallbacks() {
    TEST_SECTION("Collision Callbacks");

    Physics2DWorld world;
    world.SetGravity(Vec2(0.0f, -10.0f));

    // Create ground (static)
    UUID groundId;
    RigidBody2D* ground = world.CreateBody(groundId, BodyType::Static);
    ground->SetPosition(Vec2(0.0f, -5.0f));
    UUID groundColliderId;
    std::unique_ptr<BoxCollider2D> groundCollider = std::make_unique<BoxCollider2D>(ground, groundColliderId, Vec2(10.0f, 1.0f));
    TEST_RESULT(!ground->GetColliders().empty() && ground->GetColliders().back() == groundCollider.get(), "groundCollider attached to ground");

    // Create falling box (dynamic)
    UUID boxId;
    RigidBody2D* box = world.CreateBody(boxId, BodyType::Dynamic);
    box->SetPosition(Vec2(0.0f, 5.0f));
    UUID boxColliderId;
    std::unique_ptr<BoxCollider2D> boxCollider = std::make_unique<BoxCollider2D>(box, boxColliderId, Vec2(1.0f, 1.0f));
    TEST_RESULT(!box->GetColliders().empty() && box->GetColliders().back() == boxCollider.get(), "boxCollider attached to box");

    // Track collision events
    bool collisionEntered = false;
    bool collisionStayed = false;
    bool collisionExited = false;

    world.RegisterCollisionEnter(boxId, [&](const CollisionInfo& info) {
        collisionEntered = true;
        std::cout << "Collision Enter: Body " << info.bodyA.ToString()
                  << " with Body " << info.bodyB.ToString() << std::endl;
    });

    world.RegisterCollisionStay(boxId, [&](const CollisionInfo&) {
        collisionStayed = true;
    });

    world.RegisterCollisionExit(boxId, [&](const CollisionInfo& info) {
        collisionExited = true;
        std::cout << "Collision Exit: Body " << info.bodyA.ToString()
                  << " with Body " << info.bodyB.ToString() << std::endl;
    });

    // Simulate until collision
    for (int i = 0; i < 120; ++i) {
        world.Step(1.0f / 60.0f);
    }

    TEST_RESULT(collisionEntered, "Collision enter callback triggered");
    TEST_RESULT(collisionStayed, "Collision stay callback triggered");

    // Move box away
    box->SetPosition(Vec2(0.0f, 10.0f));
    box->SetLinearVelocity(Vec2::Zero());

    for (int i = 0; i < 10; ++i) {
        world.Step(1.0f / 60.0f);
    }

    TEST_RESULT(collisionExited, "Collision exit callback triggered");

    std::cout << "\n[SUCCESS] Collision callbacks working\n";
}

// ========================================
// Test: Trigger Events
// ========================================

void TestTriggerEvents() {
    TEST_SECTION("Trigger Events");

    Physics2DWorld world;
    world.SetGravity(Vec2::Zero());

    // Create trigger zone (static sensor)
    UUID triggerId;
    RigidBody2D* trigger = world.CreateBody(triggerId, BodyType::Static);
    trigger->SetPosition(Vec2(5.0f, 0.0f));
    UUID triggerColliderId;
    std::unique_ptr<CircleCollider2D> triggerCollider = std::make_unique<CircleCollider2D>(trigger, triggerColliderId, 2.0f);
    triggerCollider->SetSensor(true);

    // Create moving box
    UUID boxId;
    RigidBody2D* box = world.CreateBody(boxId, BodyType::Dynamic);
    box->SetPosition(Vec2(0.0f, 0.0f));
    UUID boxColliderId;
    std::unique_ptr<BoxCollider2D> boxCollider = std::make_unique<BoxCollider2D>(box, boxColliderId, Vec2(0.5f, 0.5f));
    TEST_RESULT(!box->GetColliders().empty() && box->GetColliders().back() == boxCollider.get(), "boxCollider attached to box");

    bool triggerEntered = false;
    bool triggerStayed = false;
    bool triggerExited = false;

    world.RegisterTriggerEnter(boxId, [&](const CollisionInfo&) {
        triggerEntered = true;
        std::cout << "Trigger Enter!" << std::endl;
    });

    world.RegisterTriggerStay(boxId, [&](const CollisionInfo&) {
        triggerStayed = true;
    });

    world.RegisterTriggerExit(boxId, [&](const CollisionInfo&) {
        triggerExited = true;
        std::cout << "Trigger Exit!" << std::endl;
    });

    // Move box into trigger
    box->SetLinearVelocity(Vec2(2.0f, 0.0f));

    for (int i = 0; i < 120; ++i) {
        world.Step(1.0f / 60.0f);
    }

    TEST_RESULT(triggerEntered, "Trigger enter callback triggered");
    TEST_RESULT(triggerStayed, "Trigger stay callback triggered");
    TEST_RESULT(triggerExited, "Trigger exit callback triggered");

    std::cout << "\n[SUCCESS] Trigger events working\n";
}

// ========================================
// Test: Raycasting
// ========================================

void TestRaycasting() {
    TEST_SECTION("Raycasting");

    Physics2DWorld world;

    // Create a wall
    UUID wallId;
    RigidBody2D* wall = world.CreateBody(wallId, BodyType::Static);
    wall->SetPosition(Vec2(10.0f, 0.0f));
    UUID wallColliderId;
    std::unique_ptr<BoxCollider2D> wallCollider = std::make_unique<BoxCollider2D>(wall, wallColliderId, Vec2(1.0f, 10.0f));
    TEST_RESULT(!wall->GetColliders().empty() && wall->GetColliders().back() == wallCollider.get(), "wallCollider attached to wall");

    // Raycast that should hit
    RaycastHit2D hit;
    bool didHit = world.Raycast(Vec2(0.0f, 0.0f), Vec2(1.0f, 0.0f), 20.0f, hit);

    TEST_RESULT(didHit, "Raycast hit the wall");
    if (didHit) {
        PRINT_VEC2("Hit point", hit.point);
        PRINT_VEC2("Hit normal", hit.normal);
        PRINT_FLOAT("Hit fraction", hit.fraction);
        TEST_RESULT(hit.point.x < 10.0f && hit.point.x > 9.0f, "Hit point is near wall");
    }

    // Raycast that should miss
    RaycastHit2D missHit;
    bool didMiss = world.Raycast(Vec2(0.0f, 0.0f), Vec2(0.0f, 1.0f), 5.0f, missHit);
    TEST_RESULT(!didMiss, "Raycast missed (no hit in that direction)");

    // Raycast all
    std::vector<RaycastHit2D> hits = world.RaycastAll(Vec2(0.0f, 0.0f), Vec2(1.0f, 0.0f), 20.0f);
    std::cout << "RaycastAll found " << hits.size() << " hits" << std::endl;
    TEST_RESULT(hits.size() > 0, "RaycastAll found hits");

    std::cout << "\n[SUCCESS] Raycasting working\n";
}

// ========================================
// Test: Overlap Queries
// ========================================

void TestOverlapQueries() {
    TEST_SECTION("Overlap Queries");

    Physics2DWorld world;

    // Create some bodies
    UUID body1Id;
    RigidBody2D* body1 = world.CreateBody(body1Id, BodyType::Static);
    body1->SetPosition(Vec2(0.0f, 0.0f));
    UUID collider1Id;
    std::unique_ptr<BoxCollider2D> collider1 = std::make_unique<BoxCollider2D>(body1, collider1Id, Vec2(2.0f, 2.0f));
    TEST_RESULT(!body1->GetColliders().empty() && body1->GetColliders().back() == collider1.get(), "collider1 attached to body1");

    UUID body2Id;
    RigidBody2D* body2 = world.CreateBody(body2Id, BodyType::Static);
    body2->SetPosition(Vec2(5.0f, 0.0f));
    UUID collider2Id;
    std::unique_ptr<CircleCollider2D> collider2 = std::make_unique<CircleCollider2D>(body2, collider2Id, 1.0f);
    TEST_RESULT(!body2->GetColliders().empty() && body2->GetColliders().back() == collider2.get(), "collider2 attached to body2");

    // Overlap point
    std::vector<OverlapResult2D> pointResults = world.OverlapPoint(Vec2(0.0f, 0.0f));
    std::cout << "OverlapPoint found " << pointResults.size() << " bodies" << std::endl;
    TEST_RESULT(pointResults.size() > 0, "OverlapPoint found body at origin");

    // Overlap circle
    std::vector<OverlapResult2D> circleResults = world.OverlapCircle(Vec2(0.0f, 0.0f), 3.0f);
    std::cout << "OverlapCircle found " << circleResults.size() << " bodies" << std::endl;
    TEST_RESULT(circleResults.size() > 0, "OverlapCircle found bodies");

    // Overlap box
    std::vector<OverlapResult2D> boxResults = world.OverlapBox(Vec2(2.5f, 0.0f), Vec2(6.0f, 2.0f));
    std::cout << "OverlapBox found " << boxResults.size() << " bodies" << std::endl;
    TEST_RESULT(boxResults.size() >= 2, "OverlapBox found both bodies");

    std::cout << "\n[SUCCESS] Overlap queries working\n";
}

// ========================================
// Test: Body Properties
// ========================================

void TestBodyProperties() {
    TEST_SECTION("Body Properties");

    Physics2DWorld world;
    UUID bodyId;
    RigidBody2D* body = world.CreateBody(bodyId, BodyType::Dynamic);

    UUID colliderId;
    std::unique_ptr<BoxCollider2D> collider = std::make_unique<BoxCollider2D>(body, colliderId, Vec2(1.0f, 1.0f));
    TEST_RESULT(!body->GetColliders().empty() && body->GetColliders().back() == collider.get(), "collider attached to body");

    // Test mass
    float mass = body->GetMass();
    PRINT_FLOAT("Body mass", mass);
    TEST_RESULT(mass > 0.0f, "Dynamic body has mass");

    // Test damping
    body->SetLinearDamping(0.5f);
    body->SetAngularDamping(0.3f);
    TEST_RESULT(std::abs(body->GetLinearDamping() - 0.5f) < 0.001f, "Linear damping set");
    TEST_RESULT(std::abs(body->GetAngularDamping() - 0.3f) < 0.001f, "Angular damping set");

    // Test gravity scale
    body->SetGravityScale(2.0f);
    TEST_RESULT(std::abs(body->GetGravityScale() - 2.0f) < 0.001f, "Gravity scale set");

    // Test fixed rotation
    body->SetFixedRotation(true);
    TEST_RESULT(body->IsFixedRotation(), "Fixed rotation enabled");

    // Test bullet mode
    body->SetBullet(true);
    TEST_RESULT(body->IsBullet(), "Bullet mode enabled");

    // Test sleeping
    body->SetSleepingAllowed(false);
    TEST_RESULT(!body->IsSleepingAllowed(), "Sleeping disabled");

    std::cout << "\n[SUCCESS] Body properties working\n";
}

// ========================================
// Test: Transform Synchronization
// ========================================

void TestTransformSync() {
    TEST_SECTION("Transform Synchronization");

    Physics2DWorld world;
    UUID bodyId;
    RigidBody2D* body = world.CreateBody(bodyId, BodyType::Dynamic);

    // Set transform using engine Transform
    Transform transform;
    transform.position = Vec3(5.0f, 10.0f, 0.0f);
    transform.rotation = Quat::FromAxisAngle(Vec3::UnitZ(), 1.57f); // 90 degrees
    transform.scale = Vec3::One();

    body->SetTransform(transform);

    // Get transform back
    Transform retrieved = body->GetTransform();
    PRINT_VEC2("Position (2D)", Vec2(retrieved.position.x, retrieved.position.y));

    TEST_RESULT(std::abs(retrieved.position.x - 5.0f) < 0.001f, "Transform position X synced");
    TEST_RESULT(std::abs(retrieved.position.y - 10.0f) < 0.001f, "Transform position Y synced");

    std::cout << "\n[SUCCESS] Transform synchronization working\n";
}

// ========================================
// Main Test Runner
// ========================================

void RunPhysics2DTests() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║   Lupine Physics2D Test Suite         ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";

    lupine_test::SetCurrentSuite("Physics2D System");

    try {
        TestPhysicsWorldCreation();
        TestRigidBodyCreation();
        TestColliders();
        TestPhysicsSimulation();
        TestForcesAndImpulses();
        TestCollisionCallbacks();
        TestTriggerEvents();
        TestRaycasting();
        TestOverlapQueries();
        TestBodyProperties();
        TestTransformSync();

        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║   ALL PHYSICS2D TESTS PASSED! ✓        ║\n";
        std::cout << "╚════════════════════════════════════════╝\n";
    }
    catch (const std::exception& e) {
        std::cout << "\n[ERROR] Test failed with exception: " << e.what() << std::endl;
    }
}

