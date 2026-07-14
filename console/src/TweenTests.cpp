#include "lupine/components/Tween.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/math/Math.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

std::shared_ptr<Tween> MakeTweenOn(const std::shared_ptr<Node2D>& node) {
    std::shared_ptr<Tween> tween = std::make_shared<Tween>();
    tween->RegisterProperties();
    node->AddComponent(tween);
    return tween;
}

bool TestConfigureAndDefaults() {
    TEST_SECTION("Tween Configuration Tests");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<Tween> tween = MakeTweenOn(node);

    tween->Configure("position_2d", nlohmann::json::array({ 100.0, 0.0 }), 2.0f, "linear");
    TEST_ASSERT(tween->GetChannel() == "position_2d", "Channel is set by Configure");
    TEST_ASSERT(math::Equals(tween->GetDuration(), 2.0f), "Duration is set by Configure");
    TEST_ASSERT(tween->GetEasing() == "linear", "Easing is set by Configure");
    TEST_ASSERT(!tween->IsRunning(), "Tween is not running before Start");

    return true;
}

bool TestLinearInterpolation() {
    TEST_SECTION("Linear Interpolation Tests");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<Tween> tween = MakeTweenOn(node);

    tween->Configure("position_2d", nlohmann::json::array({ 100.0, 0.0 }), 1.0f, "linear");
    tween->SetFrom(nlohmann::json::array({ 0.0, 0.0 }));
    tween->Start();
    TEST_ASSERT(tween->IsRunning(), "Tween is running after Start");

    tween->OnUpdate(0.5f);
    TEST_ASSERT(math::Equals(tween->GetProgress(), 0.5f), "Progress is 0.5 halfway through");
    TEST_ASSERT(math::Equals(node->GetPosition().x, 50.0f),
        "Position X is interpolated to 50 at the midpoint");
    TEST_ASSERT(tween->IsRunning(), "Tween still running at the midpoint");

    tween->OnUpdate(0.5f);
    TEST_ASSERT(math::Equals(node->GetPosition().x, 100.0f), "Position X reaches 100 at the end");
    TEST_ASSERT(!tween->IsRunning(), "Tween stops running once complete");
    TEST_ASSERT(tween->IsFinished(), "Tween reports finished at the end");

    return true;
}

bool TestEasedInterpolation() {
    TEST_SECTION("Eased Interpolation Tests");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<Tween> tween = MakeTweenOn(node);

    tween->Configure("position_2d", nlohmann::json::array({ 100.0, 0.0 }), 1.0f, "quad_in");
    tween->SetFrom(nlohmann::json::array({ 0.0, 0.0 }));
    tween->Start();

    tween->OnUpdate(0.5f);
    TEST_ASSERT(math::Equals(node->GetPosition().x, 25.0f),
        "quad_in at t=0.5 yields 25 (0.25 * 100)");

    return true;
}

bool TestFinishedSignal() {
    TEST_SECTION("Finished Signal Tests");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<Tween> tween = MakeTweenOn(node);

    int finishedCount = 0;
    tween->Connect("finished", [&finishedCount](const SignalArgs&) { ++finishedCount; });

    tween->Configure("position_2d", nlohmann::json::array({ 10.0, 0.0 }), 1.0f, "linear");
    tween->SetFrom(nlohmann::json::array({ 0.0, 0.0 }));
    tween->Start();

    tween->OnUpdate(0.5f);
    TEST_ASSERT(finishedCount == 0, "Finished not emitted before completion");

    tween->OnUpdate(0.6f);
    TEST_ASSERT(finishedCount == 1, "Finished emitted once on completion");

    tween->OnUpdate(0.5f);
    TEST_ASSERT(finishedCount == 1, "Finished not emitted again after completion (no longer running)");

    return true;
}

bool TestLooping() {
    TEST_SECTION("Looping Tests");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<Tween> tween = MakeTweenOn(node);

    tween->Configure("position_2d", nlohmann::json::array({ 100.0, 0.0 }), 1.0f, "linear");
    tween->SetFrom(nlohmann::json::array({ 0.0, 0.0 }));
    tween->SetLoop(true);
    tween->Start();

    tween->OnUpdate(1.0f);
    TEST_ASSERT(tween->IsRunning(), "Looping tween keeps running past the end");
    TEST_ASSERT(math::Equals(tween->GetElapsed(), 0.0f),
        "Looping tween resets elapsed to 0 after a cycle");

    tween->OnUpdate(0.25f);
    TEST_ASSERT(math::Equals(node->GetPosition().x, 25.0f),
        "Looping tween restarts interpolation from the beginning");

    return true;
}

bool TestStopAndReset() {
    TEST_SECTION("Stop / Reset Tests");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<Tween> tween = MakeTweenOn(node);

    tween->Configure("position_2d", nlohmann::json::array({ 100.0, 0.0 }), 1.0f, "linear");
    tween->SetFrom(nlohmann::json::array({ 0.0, 0.0 }));
    tween->Start();
    tween->OnUpdate(0.4f);

    tween->Stop();
    TEST_ASSERT(!tween->IsRunning(), "Stop halts the tween");

    tween->OnUpdate(0.5f);
    TEST_ASSERT(math::Equals(node->GetPosition().x, 40.0f),
        "A stopped tween does not advance the value");

    tween->Reset();
    TEST_ASSERT(math::Equals(tween->GetElapsed(), 0.0f), "Reset returns elapsed to 0");
    TEST_ASSERT(math::Equals(tween->GetProgress(), 0.0f), "Reset returns progress to 0");

    return true;
}

} // namespace

void RunTweenTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "TWEEN RUNTIME TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Tween Runtime");
    bool allPassed = true;

    allPassed &= TestConfigureAndDefaults();
    allPassed &= TestLinearInterpolation();
    allPassed &= TestEasedInterpolation();
    allPassed &= TestFinishedSignal();
    allPassed &= TestLooping();
    allPassed &= TestStopAndReset();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL TWEEN TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
