#include "profiling/lc_profiler.h"
#include "../core/lc_internal.h"

#include <lupine/profiling/Profiler.hpp>

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

LCResult WriteAllocatedString(const std::string& value, char** out_str) {
    if (!out_str) {
        SetError(LC_ERROR_NULL_POINTER, "output string pointer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    char* buffer = DuplicateString(value);
    if (!buffer) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate string result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    *out_str = buffer;
    return LC_SUCCESS;
}

lupine::profiling::ZoneCategory ParseCategory(const char* category) {
    using lupine::profiling::ZoneCategory;
    if (!category) {
        return ZoneCategory::User;
    }
    std::string lowered(category);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "input")      return ZoneCategory::Input;
    if (lowered == "update")     return ZoneCategory::Update;
    if (lowered == "physics")    return ZoneCategory::Physics;
    if (lowered == "render")     return ZoneCategory::Render;
    if (lowered == "scripting")  return ZoneCategory::Scripting;
    if (lowered == "networking") return ZoneCategory::Networking;
    if (lowered == "audio")      return ZoneCategory::Audio;
    if (lowered == "asset")      return ZoneCategory::Asset;
    if (lowered == "gpu")        return ZoneCategory::GPU;
    return ZoneCategory::User;
}

} // anonymous namespace

LC_API void lc_profiler_set_enabled(bool enabled) {
    try {
        lupine::profiling::Profiler::Get().SetEnabled(enabled);
    } catch (...) {
    }
}

LC_API bool lc_profiler_is_enabled(void) {
    try {
        return lupine::profiling::Profiler::Get().IsEnabled();
    } catch (...) {
        return false;
    }
}

LC_API void lc_profiler_set_history_size(uint32_t frames) {
    try {
        lupine::profiling::Profiler::Get().SetHistorySize(static_cast<size_t>(frames));
    } catch (...) {
    }
}

LC_API void lc_profiler_clear(void) {
    try {
        lupine::profiling::Profiler::Get().Clear();
    } catch (...) {
    }
}

LC_API void lc_profiler_begin_zone(const char* name, const char* category) {
    try {
        lupine::profiling::Profiler::Get().BeginZone(name ? name : "", ParseCategory(category));
    } catch (...) {
    }
}

LC_API void lc_profiler_end_zone(void) {
    try {
        lupine::profiling::Profiler::Get().EndZone();
    } catch (...) {
    }
}

LC_API double lc_profiler_average_frame_ms(void) {
    try {
        return lupine::profiling::Profiler::Get().GetAverageFrameMs();
    } catch (...) {
        return 0.0;
    }
}

LC_API double lc_profiler_fps(void) {
    try {
        return lupine::profiling::Profiler::Get().GetFps();
    } catch (...) {
        return 0.0;
    }
}

LC_API LCResult lc_profiler_get_history_json(char** out_json) {
    try {
        return WriteAllocatedString(lupine::profiling::Profiler::Get().ToJson(), out_json);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception serializing profiler history");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_profiler_get_last_frame_json(char** out_json) {
    try {
        lupine::profiling::Profiler& profiler = lupine::profiling::Profiler::Get();
        lupine::profiling::FrameRecord frame = profiler.GetLastFrame();
        nlohmann::json jf;
        jf["frameIndex"] = frame.frameIndex;
        jf["frameMs"] = frame.frameMs;
        nlohmann::json jzones = nlohmann::json::array();
        for (const lupine::profiling::ZoneSample& zone : frame.zones) {
            nlohmann::json jz;
            jz["name"] = zone.name;
            jz["category"] = lupine::profiling::ZoneCategoryName(zone.category);
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
        return WriteAllocatedString(jf.dump(), out_json);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception serializing profiler frame");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_profiler_save_capture(const char* path) {
    if (!path) {
        SetError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        if (!lupine::profiling::Profiler::Get().SaveCapture(path)) {
            SetError(LC_ERROR_INTERNAL_ERROR, "Failed to write profiler capture file");
            return LC_ERROR_INTERNAL_ERROR;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception saving profiler capture");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
