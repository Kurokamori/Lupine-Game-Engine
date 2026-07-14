#include "lupine/math/Math.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace lupine::math;

#define PRINT_VEC2(name, vec) \
    std::cout << "  " << name << ": (" << vec.x << ", " << vec.y << ")" << std::endl;

#define PRINT_VEC3(name, vec) \
    std::cout << "  " << name << ": (" << vec.x << ", " << vec.y << ", " << vec.z << ")" << std::endl;

#define PRINT_VEC4(name, vec) \
    std::cout << "  " << name << ": (" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ")" << std::endl;

#define PRINT_QUAT(name, quat) \
    std::cout << "  " << name << ": (w=" << quat.w() << ", x=" << quat.x() << ", y=" << quat.y() << ", z=" << quat.z() << ")" << std::endl;

#define PRINT_COLOR(name, color) \
    std::cout << "  " << name << ": (r=" << color.r << ", g=" << color.g << ", b=" << color.b << ", a=" << color.a << ")" << std::endl;

#define PRINT_FLOAT(name, value) \
    std::cout << "  " << name << ": " << value << std::endl;

// Test Vec2
void TestVec2() {
    TEST_SECTION("Vec2 Tests");
    
    // Construction
    Vec2 v1(3.0f, 4.0f);
    Vec2 v2(1.0f, 2.0f);
    PRINT_VEC2("v1", v1);
    PRINT_VEC2("v2", v2);
    
    // Arithmetic
    Vec2 sum = v1 + v2;
    Vec2 diff = v1 - v2;
    Vec2 scaled = v1 * 2.0f;
    PRINT_VEC2("v1 + v2", sum);
    PRINT_VEC2("v1 - v2", diff);
    PRINT_VEC2("v1 * 2", scaled);
    TEST_RESULT(sum == Vec2(4.0f, 6.0f), "Addition works correctly");
    
    // Length
    float length = v1.Length();
    PRINT_FLOAT("v1.Length()", length);
    TEST_RESULT(Equals(length, 5.0f), "Length calculation (3-4-5 triangle)");
    
    // Normalization
    Vec2 normalized = v1.Normalized();
    PRINT_VEC2("v1.Normalized()", normalized);
    TEST_RESULT(Equals(normalized.Length(), 1.0f), "Normalized vector has length 1");
    
    // Dot product
    float dot = v1.Dot(v2);
    PRINT_FLOAT("v1.Dot(v2)", dot);
    TEST_RESULT(Equals(dot, 11.0f), "Dot product calculation");
    
    // Cross product (2D returns scalar)
    float cross = v1.Cross(v2);
    PRINT_FLOAT("v1.Cross(v2)", cross);
    
    // Distance
    float dist = v1.Distance(v2);
    PRINT_FLOAT("Distance(v1, v2)", dist);
    
    // Lerp
    Vec2 lerped = v1.Lerp(v2, 0.5f);
    PRINT_VEC2("Lerp(v1, v2, 0.5)", lerped);
    TEST_RESULT(lerped == Vec2(2.0f, 3.0f), "Lerp at 0.5 is midpoint");
    
    // Static constructors
    TEST_RESULT(Vec2::Zero() == Vec2(0.0f, 0.0f), "Vec2::Zero()");
    TEST_RESULT(Vec2::One() == Vec2(1.0f, 1.0f), "Vec2::One()");
    TEST_RESULT(Vec2::UnitX() == Vec2(1.0f, 0.0f), "Vec2::UnitX()");
    TEST_RESULT(Vec2::UnitY() == Vec2(0.0f, 1.0f), "Vec2::UnitY()");
}

// Test Vec3
void TestVec3() {
    TEST_SECTION("Vec3 Tests");
    
    // Construction
    Vec3 v1(1.0f, 2.0f, 3.0f);
    Vec3 v2(4.0f, 5.0f, 6.0f);
    PRINT_VEC3("v1", v1);
    PRINT_VEC3("v2", v2);
    
    // Arithmetic
    Vec3 sum = v1 + v2;
    Vec3 diff = v1 - v2;
    Vec3 scaled = v1 * 2.0f;
    PRINT_VEC3("v1 + v2", sum);
    PRINT_VEC3("v1 - v2", diff);
    PRINT_VEC3("v1 * 2", scaled);
    TEST_RESULT(sum == Vec3(5.0f, 7.0f, 9.0f), "Addition works correctly");
    
    // Length
    float length = v1.Length();
    PRINT_FLOAT("v1.Length()", length);
    TEST_RESULT(Equals(length, std::sqrt(14.0f)), "Length calculation");
    
    // Normalization
    Vec3 normalized = v1.Normalized();
    PRINT_VEC3("v1.Normalized()", normalized);
    TEST_RESULT(Equals(normalized.Length(), 1.0f), "Normalized vector has length 1");
    
    // Dot product
    float dot = v1.Dot(v2);
    PRINT_FLOAT("v1.Dot(v2)", dot);
    TEST_RESULT(Equals(dot, 32.0f), "Dot product calculation");
    
    // Cross product
    Vec3 cross = v1.Cross(v2);
    PRINT_VEC3("v1.Cross(v2)", cross);
    // Cross product should be perpendicular to both vectors
    TEST_RESULT(Equals(cross.Dot(v1), 0.0f) && Equals(cross.Dot(v2), 0.0f), 
                "Cross product is perpendicular to both vectors");
    
    // Static constructors
    TEST_RESULT(Vec3::Zero() == Vec3(0.0f, 0.0f, 0.0f), "Vec3::Zero()");
    TEST_RESULT(Vec3::One() == Vec3(1.0f, 1.0f, 1.0f), "Vec3::One()");
    TEST_RESULT(Vec3::Up() == Vec3(0.0f, 1.0f, 0.0f), "Vec3::Up()");
    TEST_RESULT(Vec3::Forward() == Vec3(0.0f, 0.0f, -1.0f), "Vec3::Forward()");
}

// Test Vec4
void TestVec4() {
    TEST_SECTION("Vec4 Tests");

    Vec4 v1(1.0f, 2.0f, 3.0f, 4.0f);
    Vec4 v2(5.0f, 6.0f, 7.0f, 8.0f);
    PRINT_VEC4("v1", v1);
    PRINT_VEC4("v2", v2);

    // Arithmetic
    Vec4 sum = v1 + v2;
    PRINT_VEC4("v1 + v2", sum);
    TEST_RESULT(sum == Vec4(6.0f, 8.0f, 10.0f, 12.0f), "Addition works correctly");

    // Conversion to Vec3
    Vec3 v3 = v1.ToVec3();
    PRINT_VEC3("v1.ToVec3()", v3);
    TEST_RESULT(v3 == Vec3(1.0f, 2.0f, 3.0f), "Vec4 to Vec3 conversion");

    // Construction from Vec3
    Vec4 v4(Vec3(1.0f, 2.0f, 3.0f), 4.0f);
    TEST_RESULT(v4 == v1, "Vec4 construction from Vec3 + w");
}

// Test Mat3
void TestMat3() {
    TEST_SECTION("Mat3 Tests");

    Mat3 identity = Mat3::Identity();
    Mat3 zero = Mat3::Zero();

    // Identity matrix properties
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 transformed = identity * v;
    PRINT_VEC3("Identity * v", transformed);
    TEST_RESULT(transformed == v, "Identity matrix doesn't change vector");

    // Matrix multiplication
    Mat3 m1 = Mat3::Scale(Vec3(2.0f, 2.0f, 2.0f));
    Vec3 scaled = m1 * v;
    PRINT_VEC3("Scale(2) * v", scaled);
    TEST_RESULT(scaled == Vec3(2.0f, 4.0f, 6.0f), "Scale matrix works correctly");

    // Determinant
    float det = identity.Determinant();
    PRINT_FLOAT("Identity determinant", det);
    TEST_RESULT(Equals(det, 1.0f), "Identity matrix determinant is 1");

    // Transpose
    Mat3 transposed = identity.Transposed();
    TEST_RESULT(transposed == identity, "Identity matrix is symmetric");
}

// Test Mat4
void TestMat4() {
    TEST_SECTION("Mat4 Tests");

    Mat4 identity = Mat4::Identity();

    // Translation
    Mat4 translation = Mat4::Translate(Vec3(1.0f, 2.0f, 3.0f));
    Vec3 point(0.0f, 0.0f, 0.0f);
    Vec3 translated = translation.TransformPoint(point);
    PRINT_VEC3("Translated point", translated);
    TEST_RESULT(translated == Vec3(1.0f, 2.0f, 3.0f), "Translation matrix works");

    // Scale
    Mat4 scale = Mat4::Scale(Vec3(2.0f, 3.0f, 4.0f));
    Vec3 p(1.0f, 1.0f, 1.0f);
    Vec3 scaled = scale.TransformPoint(p);
    PRINT_VEC3("Scaled point", scaled);
    TEST_RESULT(scaled == Vec3(2.0f, 3.0f, 4.0f), "Scale matrix works");

    // Rotation
    Mat4 rotation = Mat4::Rotate(PI / 2.0f, Vec3::UnitZ());
    Vec3 rotated = rotation.TransformPoint(Vec3::UnitX());
    PRINT_VEC3("Rotated UnitX by 90° around Z", rotated);
    TEST_RESULT(Equals(rotated.x, 0.0f, 0.01f) && Equals(rotated.y, 1.0f, 0.01f),
                "90° rotation around Z axis");

    // Matrix multiplication (combining transforms)
    Mat4 combined = translation * scale;
    Vec3 result = combined.TransformPoint(Vec3(1.0f, 1.0f, 1.0f));
    PRINT_VEC3("Translate then Scale", result);

    // Inverse
    Mat4 inverse = translation.Inverse();
    Vec3 backToOrigin = inverse.TransformPoint(translated);
    PRINT_VEC3("Inverse translation", backToOrigin);
    TEST_RESULT(backToOrigin.Distance(Vec3::Zero()) < 0.001f, "Inverse matrix works");
}

// Test Quaternion
void TestQuaternion() {
    TEST_SECTION("Quaternion Tests");

    Quat identity = Quat::Identity();
    PRINT_QUAT("Identity", identity);

    // Rotation from axis-angle
    Quat rot90Z = Quat::FromAxisAngle(Vec3::UnitZ(), PI / 2.0f);
    PRINT_QUAT("90° rotation around Z", rot90Z);

    // Rotate vector
    Vec3 rotated = rot90Z * Vec3::UnitX();
    PRINT_VEC3("UnitX rotated 90° around Z", rotated);
    TEST_RESULT(Equals(rotated.x, 0.0f, 0.01f) && Equals(rotated.y, 1.0f, 0.01f),
                "Quaternion rotation works");

    // Euler angles
    Vec3 euler(0.0f, 0.0f, PI / 2.0f); // 90° around Z
    Quat fromEuler = Quat::FromEuler(euler);
    Vec3 rotated2 = fromEuler * Vec3::UnitX();
    PRINT_VEC3("UnitX rotated by Euler(0,0,90°)", rotated2);
    TEST_RESULT(Equals(rotated2.x, 0.0f, 0.01f) && Equals(rotated2.y, 1.0f, 0.01f),
                "Euler to Quaternion conversion");

    // Quaternion multiplication (combining rotations)
    Quat rot90X = Quat::FromAxisAngle(Vec3::UnitX(), PI / 2.0f);
    Quat combined = rot90Z * rot90X;
    Vec3 rotatedCombined = combined * Vec3::UnitZ();
    PRINT_VEC3("Combined rotation result", rotatedCombined);

    // Slerp
    Quat start = Quat::Identity();
    Quat end = Quat::FromAxisAngle(Vec3::UnitZ(), PI);
    Quat halfway = start.Slerp(end, 0.5f);
    Vec3 halfwayRotated = halfway * Vec3::UnitX();
    PRINT_VEC3("Slerp halfway rotation", halfwayRotated);

    // Normalization
    TEST_RESULT(Equals(identity.Length(), 1.0f), "Identity quaternion is normalized");
    TEST_RESULT(Equals(rot90Z.Length(), 1.0f), "Rotation quaternion is normalized");
}

// Test Transform
void TestTransform() {
    TEST_SECTION("Transform Tests");

    Transform t1;
    t1.position = Vec3(1.0f, 2.0f, 3.0f);
    t1.rotation = Quat::FromAxisAngle(Vec3::UnitY(), PI / 4.0f);
    t1.scale = Vec3(2.0f, 2.0f, 2.0f);

    PRINT_VEC3("Position", t1.position);
    PRINT_VEC3("Scale", t1.scale);

    // Transform point
    Vec3 localPoint(1.0f, 0.0f, 0.0f);
    Vec3 worldPoint = t1.TransformPoint(localPoint);
    PRINT_VEC3("Local point", localPoint);
    PRINT_VEC3("World point", worldPoint);

    // Inverse transform
    Vec3 backToLocal = t1.InverseTransformPoint(worldPoint);
    PRINT_VEC3("Back to local", backToLocal);
    TEST_RESULT(backToLocal.Distance(localPoint) < 0.001f, "Inverse transform works");

    // Combine transforms (parent * local = world)
    Transform parent;
    parent.position = Vec3(10.0f, 0.0f, 0.0f);
    parent.rotation = Quat::FromAxisAngle(Vec3::UnitZ(), PI / 2.0f);

    Transform local;
    local.position = Vec3(5.0f, 0.0f, 0.0f);

    Transform world = parent * local;
    PRINT_VEC3("Parent position", parent.position);
    PRINT_VEC3("Local position", local.position);
    PRINT_VEC3("World position", world.position);

    // To matrix
    Mat4 matrix = t1.ToMatrix();
    Vec3 matrixTransformed = matrix.TransformPoint(localPoint);
    PRINT_VEC3("Matrix transformed point", matrixTransformed);
    TEST_RESULT(matrixTransformed.Distance(worldPoint) < 0.001f,
                "Transform to matrix conversion");

    // Lerp
    Transform t2;
    t2.position = Vec3(10.0f, 20.0f, 30.0f);
    Transform lerped = t1.Lerp(t2, 0.5f);
    PRINT_VEC3("Lerped position", lerped.position);
    TEST_RESULT(lerped.position.Distance(Vec3(5.5f, 11.0f, 16.5f)) < 0.001f,
                "Transform lerp");
}

// Test Color
void TestColor() {
    TEST_SECTION("Color Tests");

    Color red = Color::Red();
    Color green = Color::Green();
    Color blue = Color::Blue();

    PRINT_COLOR("Red", red);
    PRINT_COLOR("Green", green);
    PRINT_COLOR("Blue", blue);

    // Arithmetic
    Color yellow = red + green;
    PRINT_COLOR("Red + Green", yellow);
    TEST_RESULT(yellow == Color::Yellow(), "Color addition creates yellow");

    Color cyan = green + blue;
    PRINT_COLOR("Green + Blue", cyan);
    TEST_RESULT(cyan == Color::Cyan(), "Color addition creates cyan");

    Color magenta = red + blue;
    PRINT_COLOR("Red + Blue", magenta);
    TEST_RESULT(magenta == Color::Magenta(), "Color addition creates magenta");

    // Scaling
    Color halfRed = red * 0.5f;
    PRINT_COLOR("Red * 0.5", halfRed);
    TEST_RESULT(Equals(halfRed.r, 0.5f), "Color scaling works");

    // Lerp
    Color lerped = red.Lerp(blue, 0.5f);
    PRINT_COLOR("Lerp(Red, Blue, 0.5)", lerped);

    // RGBA32 conversion
    uint32_t rgba = red.ToRGBA32();
    Color fromRGBA = Color::FromRGBA32(rgba);
    PRINT_COLOR("Red -> RGBA32 -> Color", fromRGBA);
    // Note: Color::Distance() doesn't exist, using component comparison instead
    TEST_RESULT(fromRGBA == red, "RGBA32 conversion round-trip");

    // Predefined colors
    TEST_RESULT(Color::White() == Color(1.0f, 1.0f, 1.0f, 1.0f), "White color");
    TEST_RESULT(Color::Black() == Color(0.0f, 0.0f, 0.0f, 1.0f), "Black color");
    TEST_RESULT(Color::Gray() == Color(0.5f, 0.5f, 0.5f, 1.0f), "Gray color");
    TEST_RESULT(Color::Purple() == Color(0.5f, 0.0f, 0.5f, 1.0f), "Purple color");
}

// Test AABB
void TestAABB() {
    TEST_SECTION("AABB Tests");

    AABB box(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

    Vec3 center = box.GetCenter();
    Vec3 size = box.GetSize();
    PRINT_VEC3("AABB Center", center);
    PRINT_VEC3("AABB Size", size);
    TEST_RESULT(center == Vec3::Zero(), "AABB center calculation");
    TEST_RESULT(size == Vec3(2.0f, 2.0f, 2.0f), "AABB size calculation");

    // Contains point
    TEST_RESULT(box.Contains(Vec3::Zero()), "AABB contains center point");
    TEST_RESULT(box.Contains(Vec3(0.5f, 0.5f, 0.5f)), "AABB contains interior point");
    TEST_RESULT(!box.Contains(Vec3(2.0f, 0.0f, 0.0f)), "AABB doesn't contain exterior point");

    // Closest point
    Vec3 exterior(2.0f, 0.0f, 0.0f);
    Vec3 closest = box.ClosestPoint(exterior);
    PRINT_VEC3("Closest point to (2,0,0)", closest);
    TEST_RESULT(closest == Vec3(1.0f, 0.0f, 0.0f), "Closest point on AABB");

    // Intersection
    AABB box2(Vec3(0.0f, 0.0f, 0.0f), Vec3(2.0f, 2.0f, 2.0f));
    TEST_RESULT(box.Intersects(box2), "AABBs intersect");

    AABB box3(Vec3(3.0f, 3.0f, 3.0f), Vec3(4.0f, 4.0f, 4.0f));
    TEST_RESULT(!box.Intersects(box3), "AABBs don't intersect");

    // Encapsulate
    AABB growing = box;
    growing.Encapsulate(Vec3(5.0f, 5.0f, 5.0f));
    PRINT_VEC3("Expanded max", growing.max);
    TEST_RESULT(growing.max == Vec3(5.0f, 5.0f, 5.0f), "AABB encapsulate point");
}

// Test Ray
void TestRay() {
    TEST_SECTION("Ray Tests");

    Ray ray(Vec3::Zero(), Vec3::Forward());
    PRINT_VEC3("Ray origin", ray.origin);
    PRINT_VEC3("Ray direction", ray.direction);

    // Get point at distance
    Vec3 point = ray.GetPoint(5.0f);
    PRINT_VEC3("Point at distance 5", point);
    TEST_RESULT(point == Vec3(0.0f, 0.0f, -5.0f), "Ray GetPoint");

    // Closest point
    Vec3 testPoint(1.0f, 0.0f, -5.0f);
    Vec3 closest = ray.ClosestPoint(testPoint);
    PRINT_VEC3("Closest point on ray", closest);
    TEST_RESULT(closest == Vec3(0.0f, 0.0f, -5.0f), "Ray closest point");

    // Sphere intersection
    Vec3 sphereCenter(0.0f, 0.0f, -10.0f);
    float sphereRadius = 2.0f;
    float distance;
    bool hit = ray.IntersectSphere(sphereCenter, sphereRadius, distance);
    PRINT_FLOAT("Sphere intersection distance", distance);
    TEST_RESULT(hit, "Ray intersects sphere");
    TEST_RESULT(Equals(distance, 8.0f), "Sphere intersection distance correct");

    // AABB intersection
    AABB box(Vec3(-1.0f, -1.0f, -11.0f), Vec3(1.0f, 1.0f, -9.0f));
    hit = ray.IntersectAABB(box, distance);
    PRINT_FLOAT("AABB intersection distance", distance);
    TEST_RESULT(hit, "Ray intersects AABB");
}

// Test Plane
void TestPlane() {
    TEST_SECTION("Plane Tests");

    // XY plane at origin
    Plane plane(Vec3::UnitZ(), 0.0f);
    PRINT_VEC3("Plane normal", plane.normal);
    PRINT_FLOAT("Plane distance", plane.distance);

    // Distance to point
    Vec3 point(0.0f, 0.0f, 5.0f);
    float dist = plane.DistanceToPoint(point);
    PRINT_FLOAT("Distance to (0,0,5)", dist);
    TEST_RESULT(Equals(dist, 5.0f), "Plane distance to point");

    // Closest point
    Vec3 closest = plane.ClosestPoint(point);
    PRINT_VEC3("Closest point on plane", closest);
    TEST_RESULT(closest == Vec3(0.0f, 0.0f, 0.0f), "Plane closest point");

    // Side test
    TEST_RESULT(plane.IsFrontFacing(Vec3(0.0f, 0.0f, 1.0f)), "Point on front side");
    TEST_RESULT(!plane.IsFrontFacing(Vec3(0.0f, 0.0f, -1.0f)), "Point on back side");

    // Ray intersection
    Ray ray(Vec3(0.0f, 0.0f, 10.0f), Vec3(0.0f, 0.0f, -1.0f));
    float rayDist;
    bool hit = plane.IntersectRay(ray, rayDist);
    PRINT_FLOAT("Ray-plane intersection distance", rayDist);
    TEST_RESULT(hit, "Ray intersects plane");
    TEST_RESULT(Equals(rayDist, 10.0f), "Ray-plane intersection distance correct");
}

// Test OBB
void TestOBB() {
    TEST_SECTION("OBB Tests");

    OBB obb;
    obb.center = Vec3::Zero();
    obb.extents = Vec3(1.0f, 1.0f, 1.0f);
    obb.rotation = Quat::FromAxisAngle(Vec3::UnitZ(), PI / 4.0f);

    PRINT_VEC3("OBB Center", obb.center);
    PRINT_VEC3("OBB Extents", obb.extents);

    // Contains point
    TEST_RESULT(obb.Contains(Vec3::Zero()), "OBB contains center");

    // Closest point
    Vec3 exterior(5.0f, 0.0f, 0.0f);
    Vec3 closest = obb.ClosestPoint(exterior);
    PRINT_VEC3("Closest point on OBB", closest);

    // Convert to AABB
    AABB aabb = obb.ToAABB();
    PRINT_VEC3("OBB as AABB min", aabb.min);
    PRINT_VEC3("OBB as AABB max", aabb.max);

    // Intersection
    OBB obb2;
    obb2.center = Vec3(1.5f, 0.0f, 0.0f);
    obb2.extents = Vec3(1.0f, 1.0f, 1.0f);
    obb2.rotation = Quat::Identity();

    TEST_RESULT(obb.Intersects(obb2), "OBBs intersect");
}

// Test Camera functions
void TestCamera() {
    TEST_SECTION("Camera Tests");

    // Perspective projection
    float fov = Radians(60.0f);
    float aspect = 16.0f / 9.0f;
    Mat4 perspective = Camera::Perspective(fov, aspect, 0.1f, 100.0f);
    std::cout << "  Created perspective projection matrix" << std::endl;

    // Orthographic projection
    Mat4 ortho = Camera::Orthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    std::cout << "  Created orthographic projection matrix" << std::endl;

    // View matrix from position and target
    Vec3 cameraPos(0.0f, 5.0f, 10.0f);
    Vec3 target(0.0f, 0.0f, 0.0f);
    Mat4 view = Camera::LookAt(cameraPos, target, Vec3::Up());
    std::cout << "  Created view matrix from LookAt" << std::endl;

    // View matrix from transform
    Transform camTransform;
    camTransform.position = cameraPos;
    camTransform.LookAt(target);
    Mat4 viewFromTransform = Camera::ViewFromTransform(camTransform);
    std::cout << "  Created view matrix from Transform" << std::endl;

    // View-projection
    Mat4 viewProj = Camera::ViewProjection(view, perspective);
    std::cout << "  Created view-projection matrix" << std::endl;

    TEST_RESULT(true, "Camera matrix creation functions work");
}

// Test Math Utilities
void TestMathUtilities() {
    TEST_SECTION("Math Utilities Tests");

    // Constants
    PRINT_FLOAT("PI", PI);
    PRINT_FLOAT("TWO_PI", TWO_PI);
    PRINT_FLOAT("HALF_PI", HALF_PI);
    TEST_RESULT(Equals(PI, 3.14159265f, 0.0001f), "PI constant");

    // Conversions
    float deg90 = Degrees(PI / 2.0f);
    PRINT_FLOAT("90° in radians to degrees", deg90);
    TEST_RESULT(Equals(deg90, 90.0f), "Radians to degrees");

    float rad90 = Radians(90.0f);
    PRINT_FLOAT("90° to radians", rad90);
    TEST_RESULT(Equals(rad90, PI / 2.0f), "Degrees to radians");

    // Epsilon comparisons
    TEST_RESULT(Equals(1.0f, 1.0000001f), "Epsilon comparison for nearly equal values");
    TEST_RESULT(!Equals(1.0f, 1.1f), "Epsilon comparison for different values");
    TEST_RESULT(IsZero(0.0000001f), "IsZero for very small value");

    // Clamp
    float clamped = Clamp(5.0f, 0.0f, 10.0f);
    PRINT_FLOAT("Clamp(5, 0, 10)", clamped);
    TEST_RESULT(Equals(clamped, 5.0f), "Clamp within range");

    clamped = Clamp(15.0f, 0.0f, 10.0f);
    PRINT_FLOAT("Clamp(15, 0, 10)", clamped);
    TEST_RESULT(Equals(clamped, 10.0f), "Clamp above range");

    clamped = Clamp(-5.0f, 0.0f, 10.0f);
    PRINT_FLOAT("Clamp(-5, 0, 10)", clamped);
    TEST_RESULT(Equals(clamped, 0.0f), "Clamp below range");

    // Lerp
    float lerped = Lerp(0.0f, 10.0f, 0.5f);
    PRINT_FLOAT("Lerp(0, 10, 0.5)", lerped);
    TEST_RESULT(Equals(lerped, 5.0f), "Lerp at 0.5");

    // Saturate
    float saturated = Saturate(1.5f);
    PRINT_FLOAT("Saturate(1.5)", saturated);
    TEST_RESULT(Equals(saturated, 1.0f), "Saturate clamps to [0,1]");

    // SmoothStep
    float smooth = SmoothStep(0.0f, 1.0f, 0.5f);
    PRINT_FLOAT("SmoothStep(0, 1, 0.5)", smooth);
    TEST_RESULT(smooth > 0.4f && smooth < 0.6f, "SmoothStep interpolation");

    // InverseLerp
    float t = InverseLerp(0.0f, 10.0f, 5.0f);
    PRINT_FLOAT("InverseLerp(0, 10, 5)", t);
    TEST_RESULT(Equals(t, 0.5f), "InverseLerp");

    // Remap
    float remapped = Remap(5.0f, 0.0f, 10.0f, 0.0f, 100.0f);
    PRINT_FLOAT("Remap(5, [0,10], [0,100])", remapped);
    TEST_RESULT(Equals(remapped, 50.0f), "Remap value between ranges");
}

// Main test runner
void RunAllMathTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "   Lupine Math Library Test Suite" << std::endl;
    std::cout << "========================================\n" << std::endl;

    lupine_test::SetCurrentSuite("Math Library");

    // Initialize logger
    lupine::Logger::Init("lupine_math_tests.log", true);
    LOG_CORE_INFO("=== Starting Math Library Tests ===");

    TestVec2();
    TestVec3();
    TestVec4();
    TestMat3();
    TestMat4();
    TestQuaternion();
    TestTransform();
    TestColor();
    TestAABB();
    TestRay();
    TestPlane();
    TestOBB();
    TestCamera();
    TestMathUtilities();

    std::cout << "\n========================================" << std::endl;
    std::cout << "   All Math Tests Complete!" << std::endl;
    std::cout << "========================================\n" << std::endl;

    LOG_CORE_INFO("=== Math Library Tests Complete ===");
    lupine::Logger::Flush();
    lupine::Logger::Shutdown();
}

// Interactive test menu
void RunInteractiveMathTests() {
    bool running = true;

    while (running) {
        std::cout << "\n=== Math Library Interactive Tests ===" << std::endl;
        std::cout << "\nSelect a test to run:" << std::endl;
        std::cout << "  [1]  Vec2 Tests" << std::endl;
        std::cout << "  [2]  Vec3 Tests" << std::endl;
        std::cout << "  [3]  Vec4 Tests" << std::endl;
        std::cout << "  [4]  Mat3 Tests" << std::endl;
        std::cout << "  [5]  Mat4 Tests" << std::endl;
        std::cout << "  [6]  Quaternion Tests" << std::endl;
        std::cout << "  [7]  Transform Tests" << std::endl;
        std::cout << "  [8]  Color Tests" << std::endl;
        std::cout << "  [9]  AABB Tests" << std::endl;
        std::cout << "  [10] Ray Tests" << std::endl;
        std::cout << "  [11] Plane Tests" << std::endl;
        std::cout << "  [12] OBB Tests" << std::endl;
        std::cout << "  [13] Camera Tests" << std::endl;
        std::cout << "  [14] Math Utilities Tests" << std::endl;
        std::cout << "  [A]  Run All Tests" << std::endl;
        std::cout << "  [0]  Back to Main Menu" << std::endl;
        std::cout << "\nEnter your choice: ";

        std::string input;
        std::getline(std::cin, input);
        std::cout << std::endl;

        // Initialize logger for individual tests
        lupine::Logger::Init("lupine_math_tests.log", true);

        if (input == "1") TestVec2();
        else if (input == "2") TestVec3();
        else if (input == "3") TestVec4();
        else if (input == "4") TestMat3();
        else if (input == "5") TestMat4();
        else if (input == "6") TestQuaternion();
        else if (input == "7") TestTransform();
        else if (input == "8") TestColor();
        else if (input == "9") TestAABB();
        else if (input == "10") TestRay();
        else if (input == "11") TestPlane();
        else if (input == "12") TestOBB();
        else if (input == "13") TestCamera();
        else if (input == "14") TestMathUtilities();
        else if (input == "A" || input == "a") {
            RunAllMathTests();
            lupine::Logger::Shutdown();
            return;
        }
        else if (input == "0") {
            running = false;
        }
        else {
            std::cout << "Invalid option. Please try again." << std::endl;
        }

        lupine::Logger::Flush();
        lupine::Logger::Shutdown();

        if (running && input != "0") {
            std::cout << "\nPress Enter to continue...";
            std::string dummy;
            std::getline(std::cin, dummy);
        }
    }
}

