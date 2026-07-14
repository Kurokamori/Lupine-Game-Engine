#pragma once

#include "lupine/platform/Timing.hpp"
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lupine {
namespace profiling {

/**
 * Profiler - central per-frame instrumentation aggregator.
 *
 * A single engine-wide singleton (mirrors core::DebugDrawQueue::Get) that collects
 * timing zones, scalar counters and memory snapshots each frame into a ring buffer.
 * Designed to be zero-cost when disabled: every capture entry point early-returns on
 * IsEnabled(), and the whole subsystem is gated behind LUPINE_ENABLE_PROFILER so a
 * shipping build can compile it out entirely.
 *
 * Capture model:
 *   BeginFrame(idx)  -> opens the current FrameRecord
 *   ProfileZone / BeginZone+EndZone push nested ZoneSamples onto the open record
 *   counters and memory are written into the open record
 *   EndFrame()       -> stamps total frame time and flushes into the ring buffer
 *
 * Threading: zone depth is tracked thread-local. The ring buffer and the open record
 * are guarded by a mutex so background systems (asset loader, networking) can record
 * zones safely. The vast majority of capture happens on the main thread.
 */

enum class ZoneCategory : uint8_t {
    Input = 0,
    Update,
    Physics,
    Render,
    Scripting,
    Networking,
    Audio,
    Asset,
    GPU,
    User,
    Count
};

const char* ZoneCategoryName(ZoneCategory category);

struct ZoneSample {
    std::string name;
    ZoneCategory category = ZoneCategory::User;
    uint64_t startNs = 0;     // offset from frame start
    uint64_t durNs = 0;
    int depth = 0;            // nesting depth for flame graph
    uint64_t threadId = 0;
};

struct FrameRecord {
    uint64_t frameIndex = 0;
    double frameMs = 0.0;
    std::vector<ZoneSample> zones;
    std::unordered_map<std::string, double> counters;
    std::unordered_map<std::string, int64_t> memory;
};

class Profiler {
public:
    static Profiler& Get();

    // --- lifecycle / config ---
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_Enabled; }

    void SetHistorySize(size_t frames);
    size_t GetHistorySize() const { return m_HistorySize; }

    void Clear();

    // --- frame boundaries (called from RuntimeApp::runFrame) ---
    void BeginFrame(uint64_t frameIndex);
    void EndFrame();

    // --- zone capture ---
    // BeginZone/EndZone are the explicit form (used by scripts and the C-API).
    // Prefer the ProfileZone RAII guard / LUPINE_PROFILE_ZONE macro in C++.
    void BeginZone(const std::string& name, ZoneCategory category = ZoneCategory::User);
    void EndZone();

    // --- counters / memory (written into the open frame record) ---
    void SetCounter(const std::string& name, double value);
    void AddCounter(const std::string& name, double delta);
    void SetMemory(const std::string& name, int64_t bytes);

    // --- queries ---
    bool HasData() const;
    FrameRecord GetLastFrame() const;
    std::vector<FrameRecord> GetHistory(size_t count = 0) const;        // most recent first; 0 = all
    std::unordered_map<std::string, double> GetAggregatedZones() const; // avg ms by zone name over window
    double GetAverageFrameMs() const;
    double GetFps() const;

    // Serialize the current window to a nlohmann json object (returned as string to
    // keep this header free of the json include; see Profiler.cpp).
    std::string ToJson() const;

    // Save the current window to a .lprof file (JSON). Returns false on I/O error.
    bool SaveCapture(const std::string& path) const;

private:
    Profiler() = default;

    FrameRecord* OpenRecord();   // current in-flight record (m_Building) or nullptr

    mutable std::mutex m_Mutex;
    bool m_Enabled = false;
    size_t m_HistorySize = 300;

    FrameRecord m_Building;                 // frame currently being captured
    bool m_FrameOpen = false;
    platform::Timing::TimePoint m_FrameStart;

    std::deque<FrameRecord> m_History;      // completed frames, oldest front
};

#if defined(LUPINE_ENABLE_PROFILER)

/**
 * RAII zone guard. Opens a zone on construction, closes it on destruction.
 * Cheap when the profiler is disabled (BeginZone/EndZone early-return).
 */
class ProfileZone {
public:
    ProfileZone(const std::string& name, ZoneCategory category = ZoneCategory::User) {
        Profiler::Get().BeginZone(name, category);
    }
    ~ProfileZone() { Profiler::Get().EndZone(); }

    ProfileZone(const ProfileZone&) = delete;
    ProfileZone& operator=(const ProfileZone&) = delete;
};

#define LUPINE_PROFILE_ZONE_CAT2(a, b) a##b
#define LUPINE_PROFILE_ZONE_CAT(a, b) LUPINE_PROFILE_ZONE_CAT2(a, b)
#define LUPINE_PROFILE_ZONE(name, category) \
    ::lupine::profiling::ProfileZone LUPINE_PROFILE_ZONE_CAT(_lupineProfZone_, __LINE__)(name, category)

/**
 * Time a single statement and accumulate its milliseconds into a per-key counter
 * on the current frame. Used for per-component-type aggregation, where one zone
 * per node would explode the zone list. The timing work only runs when capture is
 * enabled; otherwise it degrades to the bare statement.
 */
#define LUPINE_PROFILE_ACCUM(counterKey, statement)                                       \
    do {                                                                                  \
        ::lupine::profiling::Profiler& _lpProf = ::lupine::profiling::Profiler::Get();    \
        if (_lpProf.IsEnabled()) {                                                        \
            ::lupine::platform::Timing::TimePoint _lpT0 = ::lupine::platform::Timing::Now(); \
            statement;                                                                    \
            _lpProf.AddCounter(counterKey,                                                \
                ::lupine::platform::Timing::GetElapsedMilliseconds(_lpT0,                 \
                    ::lupine::platform::Timing::Now()));                                   \
        } else {                                                                          \
            statement;                                                                    \
        }                                                                                 \
    } while (0)

#else

#define LUPINE_PROFILE_ZONE(name, category) do {} while (0)
#define LUPINE_PROFILE_ACCUM(counterKey, statement) do { statement; } while (0)

#endif // LUPINE_ENABLE_PROFILER

} // namespace profiling
} // namespace lupine
