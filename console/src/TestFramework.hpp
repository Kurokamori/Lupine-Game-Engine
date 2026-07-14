#pragma once

#include "lupine/logger/Logger.hpp"
#include <string>
#include <vector>
#include <iostream>

namespace lupine_test {

struct TestFailure {
    std::string suite;
    std::string message;
};

struct TestStats {
    int total = 0;
    int passed = 0;
    int failed = 0;
    std::vector<TestFailure> failures;
    std::string currentSuite = "Unknown";
};

inline TestStats& Results() {
    static TestStats stats;
    return stats;
}

inline void ResetResults() {
    Results() = TestStats();
}

inline void SetCurrentSuite(const std::string& name) {
    Results().currentSuite = name;
}

inline void RecordPass() {
    TestStats& stats = Results();
    ++stats.total;
    ++stats.passed;
}

inline void RecordFail(const std::string& message) {
    TestStats& stats = Results();
    ++stats.total;
    ++stats.failed;
    stats.failures.push_back(TestFailure{ stats.currentSuite, message });
}

inline void PrintAggregateReport() {
    const TestStats& stats = Results();
    std::cout << "\n========================================" << std::endl;
    std::cout << "          AGGREGATE TEST REPORT" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Total assertions : " << stats.total << std::endl;
    std::cout << "  Passed           : " << stats.passed << std::endl;
    std::cout << "  Failed           : " << stats.failed << std::endl;

    if (stats.failures.empty()) {
        std::cout << "\n  ALL TESTS PASSED!" << std::endl;
    } else {
        std::cout << "\n  Failed assertions (" << stats.failures.size() << "):" << std::endl;
        for (const TestFailure& failure : stats.failures) {
            std::cout << "    [" << failure.suite << "] " << failure.message << std::endl;
        }
    }
    std::cout << "========================================\n" << std::endl;
}

} // namespace lupine_test

#define TEST_SECTION(name) \
    std::cout << "\n--- " << name << " ---" << std::endl;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            ::lupine_test::RecordFail(message); \
            LOG_ERROR(lupine::LogCategory::Core, "TEST FAILED: {} - {}", #condition, message); \
            std::cout << "  [FAIL] " << message << std::endl; \
            return false; \
        } else { \
            ::lupine_test::RecordPass(); \
            std::cout << "  [PASS] " << message << std::endl; \
        } \
    } while (0)

#define TEST_RESULT(condition, message) \
    do { \
        if (condition) { \
            ::lupine_test::RecordPass(); \
            std::cout << "  [PASS] " << message << std::endl; \
        } else { \
            ::lupine_test::RecordFail(message); \
            std::cout << "  [FAIL] " << message << std::endl; \
        } \
    } while (0)
