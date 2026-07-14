#include "lupine/animation/AnimationInterp.hpp"
#include "lupine/math/Math.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace lupine;
using namespace lupine::animation;

namespace {

bool TestEasingEndpoints() {
    TEST_SECTION("Easing Endpoint Tests");

    const std::vector<std::string> curves = {
        "linear", "sine_in", "sine_out", "sine_in_out",
        "quad_in", "quad_out", "quad_in_out",
        "cubic_in", "cubic_out", "cubic_in_out",
        "expo_in", "expo_out", "expo_in_out",
        "circ_in", "circ_out",
        "back_in", "back_out",
        "elastic_in", "elastic_out",
        "bounce_in", "bounce_out",
        "ease_in", "ease_out", "ease_in_out"
    };

    bool allEndpointsValid = true;
    for (const std::string& curve : curves) {
        float at0 = ApplyEasing(curve, 0.0f);
        float at1 = ApplyEasing(curve, 1.0f);
        if (!math::Equals(at0, 0.0f) || !math::Equals(at1, 1.0f)) {
            allEndpointsValid = false;
            std::cout << "    curve '" << curve << "' endpoints: f(0)=" << at0
                      << " f(1)=" << at1 << std::endl;
        }
    }
    TEST_ASSERT(allEndpointsValid, "All easing curves map 0->0 and 1->1");

    return true;
}

bool TestEasingValues() {
    TEST_SECTION("Easing Value Tests");

    TEST_ASSERT(math::Equals(ApplyEasing("linear", 0.5f), 0.5f), "Linear midpoint is 0.5");
    TEST_ASSERT(math::Equals(ApplyEasing("quad_in", 0.5f), 0.25f), "quad_in(0.5) = 0.25");
    TEST_ASSERT(math::Equals(ApplyEasing("cubic_in", 0.5f), 0.125f), "cubic_in(0.5) = 0.125");
    TEST_ASSERT(math::Equals(ApplyEasing("quad_out", 0.5f), 0.75f), "quad_out(0.5) = 0.75");

    TEST_ASSERT(math::Equals(ApplyEasing("unknown_curve", 0.4f), 0.4f),
        "Unknown easing name falls back to linear");

    TEST_ASSERT(math::Equals(ApplyEasing("linear", -1.0f), 0.0f), "Easing clamps t below 0");
    TEST_ASSERT(math::Equals(ApplyEasing("linear", 2.0f), 1.0f), "Easing clamps t above 1");

    return true;
}

bool TestLerpJson() {
    TEST_SECTION("LerpJson Tests");

    nlohmann::json a = 0.0;
    nlohmann::json b = 10.0;
    nlohmann::json mid = LerpJson(a, b, 0.5f);
    TEST_ASSERT(mid.is_number(), "Lerp of two numbers is a number");
    TEST_ASSERT(math::Equals(static_cast<float>(mid.get<double>()), 5.0f),
        "Lerp(0,10,0.5) = 5");

    nlohmann::json va = nlohmann::json::array({ 0.0, 0.0, 0.0 });
    nlohmann::json vb = nlohmann::json::array({ 10.0, 20.0, 40.0 });
    nlohmann::json vmid = LerpJson(va, vb, 0.25f);
    TEST_ASSERT(vmid.is_array() && vmid.size() == 3, "Lerp of arrays yields an array");
    TEST_ASSERT(math::Equals(static_cast<float>(vmid[0].get<double>()), 2.5f),
        "Array lerp component 0 correct");
    TEST_ASSERT(math::Equals(static_cast<float>(vmid[1].get<double>()), 5.0f),
        "Array lerp component 1 correct");
    TEST_ASSERT(math::Equals(static_cast<float>(vmid[2].get<double>()), 10.0f),
        "Array lerp component 2 correct");

    nlohmann::json endpoint = LerpJson(a, b, 1.0f);
    TEST_ASSERT(math::Equals(static_cast<float>(endpoint.get<double>()), 10.0f),
        "Lerp at t=1 returns the end value");

    return true;
}

bool TestJsonComponent() {
    TEST_SECTION("JsonComponent Tests");

    nlohmann::json arr = nlohmann::json::array({ 1.0, 2.0, 3.0 });
    TEST_ASSERT(math::Equals(JsonComponent(arr, 0), 1.0f), "Component 0 of array");
    TEST_ASSERT(math::Equals(JsonComponent(arr, 2), 3.0f), "Component 2 of array");
    TEST_ASSERT(math::Equals(JsonComponent(arr, 5), 0.0f), "Out-of-range index returns 0");

    nlohmann::json num = 7.5;
    TEST_ASSERT(math::Equals(JsonComponent(num, 0), 7.5f), "Component 0 of a scalar is its value");
    TEST_ASSERT(math::Equals(JsonComponent(num, 1), 0.0f), "Non-zero index of a scalar is 0");

    nlohmann::json str = "text";
    TEST_ASSERT(math::Equals(JsonComponent(str, 0), 0.0f), "Non-numeric JSON yields 0");

    return true;
}

bool TestCubicHermite() {
    TEST_SECTION("Cubic Hermite Tests");

    float start = CubicHermite(0.0f, 10.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    float end = CubicHermite(0.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    TEST_ASSERT(math::Equals(start, 0.0f), "Hermite at localT=0 returns v0");
    TEST_ASSERT(math::Equals(end, 10.0f), "Hermite at localT=1 returns v1");

    float mid = CubicHermite(0.0f, 10.0f, 0.0f, 0.0f, 1.0f, 0.5f);
    TEST_ASSERT(math::Equals(mid, 5.0f), "Hermite with zero tangents matches midpoint of flat segment");

    float clampedLow = CubicHermite(2.0f, 8.0f, 0.0f, 0.0f, 1.0f, -0.5f);
    float clampedHigh = CubicHermite(2.0f, 8.0f, 0.0f, 0.0f, 1.0f, 1.5f);
    TEST_ASSERT(math::Equals(clampedLow, 2.0f), "Hermite clamps localT below 0 to v0");
    TEST_ASSERT(math::Equals(clampedHigh, 8.0f), "Hermite clamps localT above 1 to v1");

    return true;
}

} // namespace

void RunAnimationInterpTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "ANIMATION INTERPOLATION TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Animation Interpolation");
    bool allPassed = true;

    allPassed &= TestEasingEndpoints();
    allPassed &= TestEasingValues();
    allPassed &= TestLerpJson();
    allPassed &= TestJsonComponent();
    allPassed &= TestCubicHermite();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL ANIMATION INTERPOLATION TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
