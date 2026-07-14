#include "lupine/components/Curve2D.hpp"
#include "lupine/components/Path2D.hpp"
#include "lupine/components/Curve3D.hpp"
#include "lupine/components/Path3D.hpp"
#include "lupine/components/PathFollow3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/math/Math.hpp"
#include "TestFramework.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace lupine;
using namespace lupine::components;

namespace {

std::shared_ptr<Curve2D> MakeSquareCurve() {
    std::shared_ptr<Curve2D> curve = std::make_shared<Curve2D>();
    curve->RegisterProperties();
    curve->AddPoint(math::Vec2(0.0f, 0.0f));
    curve->AddPoint(math::Vec2(10.0f, 0.0f));
    curve->AddPoint(math::Vec2(10.0f, 10.0f));
    curve->AddPoint(math::Vec2(0.0f, 10.0f));
    return curve;
}

bool TestPointManagement() {
    TEST_SECTION("Curve Point Management Tests");

    std::shared_ptr<Curve2D> curve = std::make_shared<Curve2D>();
    curve->RegisterProperties();
    TEST_ASSERT(curve->GetPointCount() == 0, "New curve has no points");

    curve->AddPoint(math::Vec2(1.0f, 2.0f));
    curve->AddPoint(math::Vec2(3.0f, 4.0f));
    TEST_ASSERT(curve->GetPointCount() == 2, "Two points added");
    TEST_ASSERT(math::Equals(curve->GetPointPosition(0).x, 1.0f), "First point X is correct");
    TEST_ASSERT(math::Equals(curve->GetPointPosition(1).y, 4.0f), "Second point Y is correct");

    curve->SetPointPosition(0, math::Vec2(5.0f, 5.0f));
    TEST_ASSERT(math::Equals(curve->GetPointPosition(0).x, 5.0f), "Point position can be updated");

    curve->RemovePoint(0);
    TEST_ASSERT(curve->GetPointCount() == 1, "RemovePoint reduces the count");

    curve->ClearPoints();
    TEST_ASSERT(curve->GetPointCount() == 0, "ClearPoints empties the curve");

    return true;
}

bool TestCurveLength() {
    TEST_SECTION("Curve Length Tests");

    std::shared_ptr<Curve2D> curve = MakeSquareCurve();
    TEST_ASSERT(math::Equals(curve->GetCurveLength(), 30.0f),
        "Open polyline length is 10+10+10 = 30");

    curve->SetClosedLoop(true);
    TEST_ASSERT(math::Equals(curve->GetCurveLength(), 40.0f),
        "Closed polyline length adds the closing segment (40)");

    return true;
}

bool TestCurveSampling() {
    TEST_SECTION("Curve Sampling Tests");

    std::shared_ptr<Curve2D> curve = MakeSquareCurve();

    math::Vec2 start = curve->SampleCurve(0.0f);
    TEST_ASSERT(math::Equals(start.x, 0.0f) && math::Equals(start.y, 0.0f),
        "Sample at t=0 is the first point");

    math::Vec2 end = curve->SampleCurve(1.0f);
    TEST_ASSERT(math::Equals(end.x, 0.0f) && math::Equals(end.y, 10.0f),
        "Sample at t=1 is the last point");

    math::Vec2 mid = curve->SampleCurve(0.5f);
    TEST_ASSERT(math::Equals(mid.x, 10.0f) && math::Equals(mid.y, 5.0f),
        "Sample at t=0.5 lands halfway along the 30-unit path (10,5)");

    std::vector<math::Vec2> tess = curve->GetTessellatedPoints();
    TEST_ASSERT(tess.size() == 4, "A 4-point non-bezier curve tessellates to 4 points");

    return true;
}

bool TestCurveTangent() {
    TEST_SECTION("Curve Tangent Tests");

    std::shared_ptr<Curve2D> curve = MakeSquareCurve();

    math::Vec2 t0 = curve->SampleTangent(0.0f);
    TEST_ASSERT(math::Equals(t0.x, 1.0f) && math::Equals(t0.y, 0.0f),
        "Tangent at the start points along +X (normalized)");

    float len = std::sqrt(t0.x * t0.x + t0.y * t0.y);
    TEST_ASSERT(math::Equals(len, 1.0f), "Tangent is unit length");

    return true;
}

bool TestEmptyCurve() {
    TEST_SECTION("Empty / Single-Point Curve Tests");

    std::shared_ptr<Curve2D> curve = std::make_shared<Curve2D>();
    curve->RegisterProperties();

    math::Vec2 sample = curve->SampleCurve(0.5f);
    TEST_ASSERT(math::Equals(sample.x, 0.0f) && math::Equals(sample.y, 0.0f),
        "Sampling an empty curve returns the origin");
    TEST_ASSERT(math::Equals(curve->GetCurveLength(), 0.0f), "Empty curve has zero length");

    curve->AddPoint(math::Vec2(7.0f, 8.0f));
    math::Vec2 single = curve->SampleCurve(0.3f);
    TEST_ASSERT(math::Equals(single.x, 7.0f) && math::Equals(single.y, 8.0f),
        "Sampling a single-point curve returns that point");

    return true;
}

bool TestBezierCurve() {
    TEST_SECTION("Bezier Curve Tests");

    std::shared_ptr<Curve2D> curve = std::make_shared<Curve2D>();
    curve->RegisterProperties();
    curve->AddCurvePoint(Curve2DPoint(math::Vec2(0.0f, 0.0f),
                                      math::Vec2(0.0f, 0.0f), math::Vec2(0.0f, 10.0f), true));
    curve->AddCurvePoint(Curve2DPoint(math::Vec2(10.0f, 0.0f),
                                      math::Vec2(0.0f, 10.0f), math::Vec2(0.0f, 0.0f), true));

    std::vector<math::Vec2> tess = curve->GetTessellatedPoints();
    TEST_ASSERT(tess.size() > 2, "A bezier segment tessellates into many points");

    math::Vec2 start = curve->SampleCurve(0.0f);
    math::Vec2 end = curve->SampleCurve(1.0f);
    TEST_ASSERT(math::Equals(start.x, 0.0f), "Bezier sample starts at the first point");
    TEST_ASSERT(math::Equals(end.x, 10.0f), "Bezier sample ends at the last point");
    TEST_ASSERT(curve->GetCurveLength() > 10.0f,
        "A bowed bezier is longer than the straight chord");

    return true;
}

bool TestPath2DQueries() {
    TEST_SECTION("Path2D Query Tests");

    std::shared_ptr<Path2D> path = std::make_shared<Path2D>();
    path->RegisterProperties();
    path->AddPoint(math::Vec2(0.0f, 0.0f));
    path->AddPoint(math::Vec2(10.0f, 0.0f));
    path->AddPoint(math::Vec2(10.0f, 10.0f));

    math::Vec2 startPos = path->GetStartPosition();
    math::Vec2 endPos = path->GetEndPosition();
    TEST_ASSERT(math::Equals(startPos.x, 0.0f) && math::Equals(startPos.y, 0.0f),
        "GetStartPosition is the first point");
    TEST_ASSERT(math::Equals(endPos.x, 10.0f) && math::Equals(endPos.y, 10.0f),
        "GetEndPosition is the last point");

    math::Vec2 atStart = path->GetPositionAtProgress(0.0f);
    TEST_ASSERT(math::Equals(atStart.x, 0.0f) && math::Equals(atStart.y, 0.0f),
        "GetPositionAtProgress(0) is the start");

    math::Vec2 closest = path->GetClosestPoint(math::Vec2(5.0f, -3.0f));
    TEST_ASSERT(math::Equals(closest.y, 0.0f) && math::Equals(closest.x, 5.0f),
        "Closest point to (5,-3) is (5,0) on the first segment");

    float prog = path->GetClosestProgress(math::Vec2(10.0f, 0.0f));
    TEST_ASSERT(prog > 0.0f && prog < 1.0f,
        "Closest progress to the mid-corner is strictly between 0 and 1");

    return true;
}

bool TestPath2DProgress() {
    TEST_SECTION("Path2D Progress / Speed Tests");

    std::shared_ptr<Path2D> path = std::make_shared<Path2D>();
    path->RegisterProperties();
    path->AddPoint(math::Vec2(0.0f, 0.0f));
    path->AddPoint(math::Vec2(100.0f, 0.0f));

    path->SetSpeed(50.0f);
    TEST_ASSERT(math::Equals(path->GetSpeed(), 50.0f), "Speed can be set and read");

    path->SetProgress(0.5f);
    TEST_ASSERT(math::Equals(path->GetProgress(), 0.5f), "Progress can be set and read");
    math::Vec2 mid = path->GetCurrentPosition();
    TEST_ASSERT(math::Equals(mid.x, 50.0f), "Current position at 0.5 progress is the midpoint");

    path->Reset();
    TEST_ASSERT(math::Equals(path->GetProgress(), 0.0f), "Reset returns progress to 0");

    return true;
}

// ===== Curve3D / Path3D =====

std::shared_ptr<Curve3D> MakeLShapedCurve3D() {
    std::shared_ptr<Curve3D> curve = std::make_shared<Curve3D>();
    curve->RegisterProperties();
    curve->AddPoint(math::Vec3(0.0f, 0.0f, 0.0f));
    curve->AddPoint(math::Vec3(10.0f, 0.0f, 0.0f));
    curve->AddPoint(math::Vec3(10.0f, 0.0f, 10.0f));
    return curve;
}

bool TestCurve3DSampling() {
    TEST_SECTION("Curve3D Sampling Tests");

    std::shared_ptr<Curve3D> curve = MakeLShapedCurve3D();
    TEST_ASSERT(math::Equals(curve->GetCurveLength(), 20.0f),
        "Open 3D polyline length is 10+10 = 20");

    math::Vec3 start = curve->SampleCurve(0.0f);
    TEST_ASSERT(math::Equals(start.x, 0.0f) && math::Equals(start.z, 0.0f),
        "Sample at t=0 is the first point");

    math::Vec3 mid = curve->SampleCurve(0.5f);
    TEST_ASSERT(math::Equals(mid.x, 10.0f) && math::Equals(mid.z, 0.0f),
        "Sample at t=0.5 lands on the corner (10,0,0)");

    math::Vec3 end = curve->SampleCurve(1.0f);
    TEST_ASSERT(math::Equals(end.x, 10.0f) && math::Equals(end.z, 10.0f),
        "Sample at t=1 is the last point");

    math::Vec3 t0 = curve->SampleTangent(0.0f);
    TEST_ASSERT(math::Equals(t0.x, 1.0f) && math::Equals(t0.y, 0.0f) && math::Equals(t0.z, 0.0f),
        "Tangent at the start points along +X");
    TEST_ASSERT(math::Equals(t0.Length(), 1.0f), "Tangent is unit length");

    return true;
}

bool TestCurve3DWorldHelpers() {
    TEST_SECTION("Curve3D World-Space Helper Tests");

    std::shared_ptr<Curve3D> curve = MakeLShapedCurve3D();

    // With no owning Node3D the world transform is identity, so world == local.
    math::Vec3 local = curve->SampleCurve(0.5f);
    math::Vec3 world = curve->SampleCurveWorld(0.5f);
    TEST_ASSERT(math::Equals(local.x, world.x) && math::Equals(local.y, world.y) && math::Equals(local.z, world.z),
        "Without an owner, SampleCurveWorld matches SampleCurve (identity transform)");

    math::Vec3 worldTan = curve->SampleTangentWorld(0.0f);
    TEST_ASSERT(math::Equals(worldTan.Length(), 1.0f), "World tangent is unit length");

    return true;
}

bool TestCurve3DBezier() {
    TEST_SECTION("Curve3D Bezier Tests");

    std::shared_ptr<Curve3D> curve = std::make_shared<Curve3D>();
    curve->RegisterProperties();
    curve->AddCurvePoint(Curve3DPoint(math::Vec3(0.0f, 0.0f, 0.0f),
                                      math::Vec3(0.0f, 0.0f, 0.0f), math::Vec3(0.0f, 5.0f, 0.0f), true));
    curve->AddCurvePoint(Curve3DPoint(math::Vec3(10.0f, 0.0f, 0.0f),
                                      math::Vec3(0.0f, 5.0f, 0.0f), math::Vec3(0.0f, 0.0f, 0.0f), true));

    std::vector<math::Vec3> tess = curve->GetTessellatedPoints();
    TEST_ASSERT(tess.size() > 2, "A 3D bezier segment tessellates into many points");
    TEST_ASSERT(curve->GetCurveLength() > 10.0f,
        "A bowed 3D bezier is longer than the straight chord");

    return true;
}

bool TestCurve3DSerialization() {
    TEST_SECTION("Curve3D Serialization Round-Trip");

    std::shared_ptr<Curve3D> curve = MakeLShapedCurve3D();
    curve->SetClosedLoop(true);

    std::string json = curve->GetPropertyValue<std::string>("pointsData");
    TEST_ASSERT(!json.empty() && json != "[]", "Points serialize to a non-empty JSON string");

    std::shared_ptr<Curve3D> restored = std::make_shared<Curve3D>();
    restored->RegisterProperties();
    restored->SetPropertyValue<std::string>("pointsData", json);
    restored->OnAwake();
    TEST_ASSERT(restored->GetPointCount() == 3, "Deserialized curve has all 3 points");
    TEST_ASSERT(math::Equals(restored->GetPointPosition(2).z, 10.0f),
        "Deserialized point Z is preserved");

    return true;
}

bool TestPath3DQueries() {
    TEST_SECTION("Path3D Query Tests");

    std::shared_ptr<Path3D> path = std::make_shared<Path3D>();
    path->RegisterProperties();
    path->AddPoint(math::Vec3(0.0f, 0.0f, 0.0f));
    path->AddPoint(math::Vec3(10.0f, 0.0f, 0.0f));
    path->AddPoint(math::Vec3(10.0f, 0.0f, 10.0f));

    math::Vec3 startPos = path->GetStartPosition();
    math::Vec3 endPos = path->GetEndPosition();
    TEST_ASSERT(math::Equals(startPos.x, 0.0f) && math::Equals(startPos.z, 0.0f),
        "GetStartPosition is the first point");
    TEST_ASSERT(math::Equals(endPos.x, 10.0f) && math::Equals(endPos.z, 10.0f),
        "GetEndPosition is the last point");

    math::Vec3 closest = path->GetClosestPoint(math::Vec3(5.0f, 3.0f, -3.0f));
    TEST_ASSERT(math::Equals(closest.x, 5.0f) && math::Equals(closest.y, 0.0f) && math::Equals(closest.z, 0.0f),
        "Closest point to (5,3,-3) is (5,0,0) on the first segment");

    float prog = path->GetClosestProgress(math::Vec3(10.0f, 0.0f, 0.0f));
    TEST_ASSERT(prog > 0.0f && prog < 1.0f,
        "Closest progress to the mid-corner is strictly between 0 and 1");

    path->SetProgress(0.5f);
    math::Vec3 mid = path->GetCurrentPosition();
    TEST_ASSERT(math::Equals(mid.x, 10.0f) && math::Equals(mid.z, 0.0f),
        "Current position at 0.5 progress is the corner");

    TEST_ASSERT(math::Equals(path->GetTraveledDistance(), 10.0f),
        "Traveled distance at 0.5 progress is half of 20");
    TEST_ASSERT(math::Equals(path->GetRemainingDistance(), 10.0f),
        "Remaining distance at 0.5 progress is the other half");

    return true;
}

bool TestPath3DReverse() {
    TEST_SECTION("Path3D Reverse Tests");

    std::shared_ptr<Path3D> path = std::make_shared<Path3D>();
    path->RegisterProperties();
    path->AddPoint(math::Vec3(0.0f, 0.0f, 0.0f));
    path->AddPoint(math::Vec3(10.0f, 0.0f, 0.0f));
    path->AddPoint(math::Vec3(10.0f, 0.0f, 10.0f));

    path->ReversePath();
    math::Vec3 newStart = path->GetStartPosition();
    TEST_ASSERT(math::Equals(newStart.x, 10.0f) && math::Equals(newStart.z, 10.0f),
        "After reverse, the start is the former end");
    TEST_ASSERT(path->IsReversed(), "IsReversed reflects the reversed state");

    return true;
}

bool TestPath3DScripting() {
    TEST_SECTION("Path3D CallMethod Scripting Bridge");

    std::shared_ptr<Path3D> path = std::make_shared<Path3D>();
    path->RegisterProperties();
    path->CallMethod("add_point", nlohmann::json::array({0.0f, 0.0f, 0.0f}));
    path->CallMethod("add_point", nlohmann::json::array({0.0f, 0.0f, 20.0f}));

    nlohmann::json count = path->CallMethod("get_point_count", nlohmann::json::array());
    TEST_ASSERT(count.is_number() && count.get<int>() == 2, "add_point/get_point_count via CallMethod");

    path->CallMethod("set_progress", nlohmann::json::array({0.5f}));
    nlohmann::json pos = path->CallMethod("get_current_position", nlohmann::json::array());
    TEST_ASSERT(pos.is_object() && math::Equals(pos["z"].get<float>(), 10.0f),
        "get_current_position via CallMethod returns the midpoint");

    nlohmann::json len = path->CallMethod("get_curve_length", nlohmann::json::array());
    TEST_ASSERT(len.is_number() && math::Equals(len.get<float>(), 20.0f),
        "get_curve_length via the inherited Curve3D bridge");

    path->CallMethod("start_following", nlohmann::json::array());
    nlohmann::json following = path->CallMethod("is_following", nlohmann::json::array());
    TEST_ASSERT(following.is_boolean() && following.get<bool>(), "start_following/is_following via CallMethod");

    return true;
}

bool TestCurve2DPathScripting() {
    TEST_SECTION("Curve2D / Path2D CallMethod Backport");

    std::shared_ptr<Path2D> path = std::make_shared<Path2D>();
    path->RegisterProperties();
    path->CallMethod("add_point", nlohmann::json::array({0.0f, 0.0f}));
    path->CallMethod("add_point", nlohmann::json::array({100.0f, 0.0f}));

    nlohmann::json count = path->CallMethod("get_point_count", nlohmann::json::array());
    TEST_ASSERT(count.is_number() && count.get<int>() == 2, "Curve2D add_point/get_point_count via CallMethod");

    nlohmann::json len = path->CallMethod("get_curve_length", nlohmann::json::array());
    TEST_ASSERT(len.is_number() && math::Equals(len.get<float>(), 100.0f),
        "Inherited Curve2D get_curve_length via Path2D CallMethod");

    path->CallMethod("set_progress", nlohmann::json::array({0.5f}));
    nlohmann::json pos = path->CallMethod("get_current_position", nlohmann::json::array());
    TEST_ASSERT(pos.is_object() && math::Equals(pos["x"].get<float>(), 50.0f),
        "Path2D get_current_position via CallMethod returns the midpoint");

    return true;
}

bool TestCurve3DTilt() {
    TEST_SECTION("Curve3D Tilt / Up-Vector Tests");

    std::shared_ptr<Curve3D> curve = std::make_shared<Curve3D>();
    curve->RegisterProperties();
    curve->AddPoint(math::Vec3(0.0f, 0.0f, 0.0f));
    curve->AddPoint(math::Vec3(10.0f, 0.0f, 0.0f));
    curve->SetPointTilt(0, 0.0f);
    curve->SetPointTilt(1, 1.5707963f);  // 90 degrees at the end

    TEST_ASSERT(math::Equals(curve->GetPointTilt(1), 1.5707963f), "Per-point tilt is stored");
    TEST_ASSERT(math::Equals(curve->SampleTilt(0.0f), 0.0f), "Tilt at start is 0");
    TEST_ASSERT(curve->SampleTilt(0.5f) > 0.7f && curve->SampleTilt(0.5f) < 0.86f,
        "Tilt interpolates to ~45deg at the midpoint");

    // With no tilt the up vector along a +X tangent should be world up.
    curve->SetPointTilt(1, 0.0f);
    math::Vec3 up0 = curve->SampleUpVector(0.0f);
    TEST_ASSERT(math::Equals(up0.y, 1.0f), "Untilted up vector is world-up");
    TEST_ASSERT(math::Equals(up0.Length(), 1.0f), "Up vector is unit length");

    // A 90-degree tilt about a +X tangent rotates world-up toward -Z (right-hand rule).
    curve->SetPointTilt(0, 1.5707963f);
    curve->SetPointTilt(1, 1.5707963f);
    math::Vec3 upTilted = curve->SampleUpVector(0.5f);
    TEST_ASSERT(math::Equals(upTilted.Length(), 1.0f), "Tilted up vector stays unit length");
    TEST_ASSERT(std::abs(upTilted.y) < 0.05f, "90deg tilt removes the world-up component");

    math::Quat orient = curve->SampleOrientation(0.5f);
    float qlen = std::sqrt(orient.w()*orient.w() + orient.x()*orient.x() + orient.y()*orient.y() + orient.z()*orient.z());
    TEST_ASSERT(math::Equals(qlen, 1.0f), "Sampled orientation is a unit quaternion");

    return true;
}

bool TestPathFollow3D() {
    TEST_SECTION("PathFollow3D Tests");

    auto pathHost = std::make_shared<core::Node3D>("PathHost");
    auto path = std::make_shared<Path3D>();
    path->RegisterProperties();
    pathHost->AddComponent(path);
    path->AddPoint(math::Vec3(0.0f, 0.0f, 0.0f));
    path->AddPoint(math::Vec3(10.0f, 0.0f, 0.0f));
    path->AddPoint(math::Vec3(10.0f, 0.0f, 10.0f));

    auto follower = std::make_shared<core::Node3D>("Follower");
    pathHost->AddChild(follower);
    auto pf = std::make_shared<PathFollow3D>();
    pf->RegisterProperties();
    follower->AddComponent(pf);

    // Resolves the Path3D on the parent node automatically.
    TEST_ASSERT(pf->ResolvePath() != nullptr, "PathFollow3D resolves the parent's path");

    pf->SetRotationMode(PathFollow3D::RotationMode::None);
    pf->SetProgressRatio(0.0f);
    math::Vec3 atStart = follower->GetPosition();
    TEST_ASSERT(math::Equals(atStart.x, 0.0f) && math::Equals(atStart.z, 0.0f),
        "At ratio 0 the follower sits at the path start");

    pf->SetProgressRatio(0.5f);
    math::Vec3 atMid = follower->GetPosition();
    TEST_ASSERT(math::Equals(atMid.x, 10.0f) && math::Equals(atMid.z, 0.0f),
        "At ratio 0.5 the follower sits at the path corner (parent identity => local==path-local)");

    // Auto-advance: speed over the 20-unit path.
    pf->SetProgressRatio(0.0f);
    pf->SetSpeed(10.0f);
    pf->SetLoop(true);
    pf->StartFollowing();
    pf->OnUpdate(1.0f);  // 10 units / 20 total = +0.5 ratio
    TEST_ASSERT(pf->GetProgressRatio() > 0.45f && pf->GetProgressRatio() < 0.55f,
        "Auto-advance moves progress by speed*dt/length");

    // Loop wrap.
    pf->SetProgressRatio(0.9f);
    pf->OnUpdate(1.0f);  // +0.5 -> 1.4 -> wraps to 0.4
    TEST_ASSERT(pf->GetProgressRatio() < 0.5f, "Looping wraps progress past 1.0");

    // Rotation mode produces a valid orientation without crashing.
    pf->SetRotationMode(PathFollow3D::RotationMode::Forward);
    pf->SetProgressRatio(0.25f);
    math::Quat rot = follower->GetRotation();
    float qlen = std::sqrt(rot.w()*rot.w() + rot.x()*rot.x() + rot.y()*rot.y() + rot.z()*rot.z());
    TEST_ASSERT(math::Equals(qlen, 1.0f), "Follower rotation is a unit quaternion");

    // Scripting bridge.
    nlohmann::json r = pf->CallMethod("get_progress_ratio", nlohmann::json::array());
    TEST_ASSERT(r.is_number(), "get_progress_ratio via CallMethod");
    pf->CallMethod("set_progress_ratio", nlohmann::json::array({0.0f}));
    TEST_ASSERT(math::Equals(pf->GetProgressRatio(), 0.0f), "set_progress_ratio via CallMethod");

    return true;
}

} // namespace

void RunCurvePathTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "CURVE / PATH (2D + 3D) TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Curve / Path");
    bool allPassed = true;

    allPassed &= TestPointManagement();
    allPassed &= TestCurveLength();
    allPassed &= TestCurveSampling();
    allPassed &= TestCurveTangent();
    allPassed &= TestEmptyCurve();
    allPassed &= TestBezierCurve();
    allPassed &= TestPath2DQueries();
    allPassed &= TestPath2DProgress();

    allPassed &= TestCurve3DSampling();
    allPassed &= TestCurve3DWorldHelpers();
    allPassed &= TestCurve3DBezier();
    allPassed &= TestCurve3DSerialization();
    allPassed &= TestPath3DQueries();
    allPassed &= TestPath3DReverse();
    allPassed &= TestPath3DScripting();
    allPassed &= TestCurve2DPathScripting();
    allPassed &= TestCurve3DTilt();
    allPassed &= TestPathFollow3D();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL CURVE / PATH (2D + 3D) TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
