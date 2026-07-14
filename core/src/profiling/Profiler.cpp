#include "lupine/profiling/Profiler.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
#include <thread>

namespace lupine {
namespace profiling {

namespace {
uint64_t CurrentThreadId() {
    return static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

// Per-thread stack of open zones (indices into m_Building.zones) so EndZone can
// stamp the matching sample's duration and depth nesting works across threads.
thread_local std::vector<size_t> t_ZoneStack;
} // namespace

const char* ZoneCategoryName(ZoneCategory category) {
    switch (category) {
        case ZoneCategory::Input:      return "Input";
        case ZoneCategory::Update:     return "Update";
        case ZoneCategory::Physics:    return "Physics";
        case ZoneCategory::Render:     return "Render";
        case ZoneCategory::Scripting:  return "Scripting";
        case ZoneCategory::Networking: return "Networking";
        case ZoneCategory::Audio:      return "Audio";
        case ZoneCategory::Asset:      return "Asset";
        case ZoneCategory::GPU:        return "GPU";
        case ZoneCategory::User:       return "User";
        default:                       return "Unknown";
    }
}

Profiler& Profiler::Get() {
    static Profiler instance;
    return instance;
}

void Profiler::SetEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Enabled == enabled) {
        return;
    }
    m_Enabled = enabled;
    if (!enabled) {
        m_FrameOpen = false;
        m_Building = FrameRecord{};
        t_ZoneStack.clear();
    }
}

void Profiler::SetHistorySize(size_t frames) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_HistorySize = std::max<size_t>(frames, 1);
    while (m_History.size() > m_HistorySize) {
        m_History.pop_front();
    }
}

void Profiler::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_History.clear();
    m_Building = FrameRecord{};
    m_FrameOpen = false;
    t_ZoneStack.clear();
}

void Profiler::BeginFrame(uint64_t frameIndex) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Enabled) {
        return;
    }
    m_Building = FrameRecord{};
    m_Building.frameIndex = frameIndex;
    m_FrameOpen = true;
    m_FrameStart = platform::Timing::Now();
    t_ZoneStack.clear();
}

void Profiler::EndFrame() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Enabled || !m_FrameOpen) {
        return;
    }
    m_Building.frameMs = platform::Timing::GetElapsedMilliseconds(m_FrameStart, platform::Timing::Now());
    m_History.push_back(std::move(m_Building));
    m_Building = FrameRecord{};
    m_FrameOpen = false;
    while (m_History.size() > m_HistorySize) {
        m_History.pop_front();
    }
}

FrameRecord* Profiler::OpenRecord() {
    return m_FrameOpen ? &m_Building : nullptr;
}

void Profiler::BeginZone(const std::string& name, ZoneCategory category) {
    if (!m_Enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Enabled || !m_FrameOpen) {
        return;
    }
    ZoneSample sample;
    sample.name = name;
    sample.category = category;
    sample.startNs = static_cast<uint64_t>(
        platform::Timing::GetElapsedNanoseconds(m_FrameStart, platform::Timing::Now()));
    sample.depth = static_cast<int>(t_ZoneStack.size());
    sample.threadId = CurrentThreadId();
    t_ZoneStack.push_back(m_Building.zones.size());
    m_Building.zones.push_back(std::move(sample));
}

void Profiler::EndZone() {
    if (!m_Enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Enabled || !m_FrameOpen || t_ZoneStack.empty()) {
        return;
    }
    size_t index = t_ZoneStack.back();
    t_ZoneStack.pop_back();
    if (index >= m_Building.zones.size()) {
        return;
    }
    ZoneSample& sample = m_Building.zones[index];
    uint64_t endNs = static_cast<uint64_t>(
        platform::Timing::GetElapsedNanoseconds(m_FrameStart, platform::Timing::Now()));
    sample.durNs = endNs > sample.startNs ? (endNs - sample.startNs) : 0;
}

void Profiler::SetCounter(const std::string& name, double value) {
    if (!m_Enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (FrameRecord* record = OpenRecord()) {
        record->counters[name] = value;
    }
}

void Profiler::AddCounter(const std::string& name, double delta) {
    if (!m_Enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (FrameRecord* record = OpenRecord()) {
        record->counters[name] += delta;
    }
}

void Profiler::SetMemory(const std::string& name, int64_t bytes) {
    if (!m_Enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (FrameRecord* record = OpenRecord()) {
        record->memory[name] = bytes;
    }
}

bool Profiler::HasData() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return !m_History.empty();
}

FrameRecord Profiler::GetLastFrame() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_History.empty()) {
        return FrameRecord{};
    }
    return m_History.back();
}

std::vector<FrameRecord> Profiler::GetHistory(size_t count) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<FrameRecord> out;
    size_t total = m_History.size();
    size_t take = (count == 0 || count > total) ? total : count;
    out.reserve(take);
    for (size_t i = 0; i < take; ++i) {
        out.push_back(m_History[total - 1 - i]);
    }
    return out;
}

std::unordered_map<std::string, double> Profiler::GetAggregatedZones() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::unordered_map<std::string, double> totals;
    if (m_History.empty()) {
        return totals;
    }
    for (const FrameRecord& frame : m_History) {
        for (const ZoneSample& zone : frame.zones) {
            totals[zone.name] += static_cast<double>(zone.durNs) / 1.0e6;
        }
    }
    double frames = static_cast<double>(m_History.size());
    for (auto& entry : totals) {
        entry.second /= frames;
    }
    return totals;
}

double Profiler::GetAverageFrameMs() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_History.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const FrameRecord& frame : m_History) {
        sum += frame.frameMs;
    }
    return sum / static_cast<double>(m_History.size());
}

double Profiler::GetFps() const {
    double avg = GetAverageFrameMs();
    return avg > 0.0 ? 1000.0 / avg : 0.0;
}

std::string Profiler::ToJson() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    nlohmann::json root;
    root["historySize"] = m_HistorySize;
    root["enabled"] = m_Enabled;

    nlohmann::json frames = nlohmann::json::array();
    for (const FrameRecord& frame : m_History) {
        nlohmann::json jf;
        jf["frameIndex"] = frame.frameIndex;
        jf["frameMs"] = frame.frameMs;

        nlohmann::json jzones = nlohmann::json::array();
        for (const ZoneSample& zone : frame.zones) {
            nlohmann::json jz;
            jz["name"] = zone.name;
            jz["category"] = ZoneCategoryName(zone.category);
            jz["startNs"] = zone.startNs;
            jz["durNs"] = zone.durNs;
            jz["depth"] = zone.depth;
            jz["threadId"] = zone.threadId;
            jzones.push_back(std::move(jz));
        }
        jf["zones"] = std::move(jzones);

        nlohmann::json jcounters = nlohmann::json::object();
        for (const auto& counter : frame.counters) {
            jcounters[counter.first] = counter.second;
        }
        jf["counters"] = std::move(jcounters);

        nlohmann::json jmemory = nlohmann::json::object();
        for (const auto& mem : frame.memory) {
            jmemory[mem.first] = mem.second;
        }
        jf["memory"] = std::move(jmemory);

        frames.push_back(std::move(jf));
    }
    root["frames"] = std::move(frames);
    return root.dump();
}

bool Profiler::SaveCapture(const std::string& path) const {
    std::string json = ToJson();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    return out.good();
}

} // namespace profiling
} // namespace lupine
