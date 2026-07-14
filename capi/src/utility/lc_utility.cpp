
#include "utility/lc_utility.h"
#include "../core/lc_internal.h"

#include <lupine/components/Timer.hpp>
#include <lupine/components/Tween.hpp>
#include <lupine/core/Node.hpp>

#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace {

void SetUtilityError(LCResult code, const char* message) {
    ::SetError(code, message);

}
} // anonymous namespace


/* ============================================================================
 * Timer Functions
 * ============================================================================ */

LC_API LCResult lc_timer_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string timerName = name ? name : "";
        auto timer = std::make_shared<lupine::components::Timer>(timerName);
        timer->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(timer);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to create Timer");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_start(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->Start();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to start timer");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_stop(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->Stop();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to stop timer");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_reset(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->Reset();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to reset timer");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_restart(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->Restart();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to restart timer");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_is_running(LCComponentHandle component, bool* out_running) {
    if (!out_running) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_running is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_running = timer->IsRunning();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to check if timer is running");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_is_finished(LCComponentHandle component, bool* out_finished) {
    if (!out_finished) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_finished is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_finished = timer->IsFinished();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to check if timer is finished");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_get_time_remaining(LCComponentHandle component, float* out_time) {
    if (!out_time) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_time is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_time = timer->GetTimeRemaining();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to get time remaining");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_get_duration(LCComponentHandle component, float* out_duration) {
    if (!out_duration) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_duration is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_duration = timer->GetDuration();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to get duration");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_set_duration(LCComponentHandle component, float duration) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->SetDuration(duration);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to set duration");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_get_elapsed(LCComponentHandle component, float* out_elapsed) {
    if (!out_elapsed) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_elapsed is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_elapsed = timer->GetElapsed();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to get elapsed");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_set_elapsed(LCComponentHandle component, float elapsed) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->SetElapsed(elapsed);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to set elapsed");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_get_loop(LCComponentHandle component, bool* out_loop) {
    if (!out_loop) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_loop is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_loop = timer->GetLoop();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to get loop");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_set_loop(LCComponentHandle component, bool loop) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->SetLoop(loop);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to set loop");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_get_auto_start(LCComponentHandle component, bool* out_auto_start) {
    if (!out_auto_start) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_auto_start is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_auto_start = timer->GetAutoStart();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to get auto start");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_set_auto_start(LCComponentHandle component, bool auto_start) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->SetAutoStart(auto_start);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to set auto start");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_get_ignore_time_scale(LCComponentHandle component, bool* out_ignore) {
    if (!out_ignore) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_ignore is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_ignore = timer->GetIgnoreTimeScale();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to get ignore time scale");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_timer_set_ignore_time_scale(LCComponentHandle component, bool ignore) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->SetIgnoreTimeScale(ignore);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to set ignore time scale");
        return LC_ERROR_INTERNAL_ERROR;
    }


}

LC_API LCResult lc_timer_create_on_node(LCNodeHandle owner, float delay, bool repeating,
                                        int repeat_count, const char* name,
                                        LCComponentHandle* out_timer) {
    if (!out_timer) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_timer is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto node = GetNode(owner);
        if (!node) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        std::string timerName = name ? name : "Timer";
        auto timer = std::make_shared<lupine::components::Timer>(timerName.empty() ? "Timer" : timerName);
        timer->RegisterProperties();
        timer->SetDuration(delay);
        timer->SetLoop(repeating);
        timer->SetRepeatCount(repeating ? repeat_count : 1);

        node->AddComponent(timer);
        timer->Start();

        *out_timer = CreateComponentHandle(timer);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to create Timer on node");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_timer_set_repeat_count(LCComponentHandle component, int repeat_count) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        timer->SetRepeatCount(repeat_count);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to set repeat count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_timer_get_repeat_count(LCComponentHandle component, int* out_count) {
    if (!out_count) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto timer = std::dynamic_pointer_cast<lupine::components::Timer>(comp);
        if (!timer) {
            SetUtilityError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a Timer");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_count = timer->GetRepeatCount();
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to get repeat count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_timer_list(LCNodeHandle owner, LCComponentHandle* out_array,
                              size_t cap, size_t* out_count) {
    if (!out_count) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto node = GetNode(owner);
        if (!node) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        std::vector<std::shared_ptr<lupine::core::Component>> timers;
        for (const auto& comp : node->GetComponents()) {
            if (comp && comp->GetTypeName() == "Timer") {
                timers.push_back(comp);
            }
        }

        *out_count = timers.size();

        if (out_array && cap > 0) {
            size_t count = std::min(cap, timers.size());
            for (size_t i = 0; i < count; ++i) {
                out_array[i] = CreateComponentHandle(timers[i]);
            }
        }

        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to list timers");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Tween Functions
 * ============================================================================ */

LC_API LCResult lc_tween_create(LCNodeHandle target, const char* channel,
                                const char* to_value_json, float duration,
                                const char* easing, LCComponentHandle* out_tween) {
    if (!out_tween) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_tween is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (!channel) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "channel is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto node = GetNode(target);
        if (!node) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        nlohmann::json toValue;
        if (to_value_json && to_value_json[0] != '\0') {
            toValue = nlohmann::json::parse(to_value_json);
        }

        std::string easingName = easing ? easing : "linear";
        if (easingName.empty()) {
            easingName = "linear";
        }

        auto tween = std::make_shared<lupine::components::Tween>();
        tween->RegisterProperties();
        tween->Configure(channel, toValue, duration, easingName);
        tween->SetAutoRemove(true);

        node->AddComponent(tween);
        tween->Start();

        *out_tween = CreateComponentHandle(tween);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to create Tween");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_tween_list(LCNodeHandle owner, LCComponentHandle* out_array,
                              size_t cap, size_t* out_count) {
    if (!out_count) {
        SetUtilityError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto node = GetNode(owner);
        if (!node) {
            SetUtilityError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        std::vector<std::shared_ptr<lupine::core::Component>> tweens;
        for (const auto& comp : node->GetComponents()) {
            if (comp && comp->GetTypeName() == "Tween") {
                tweens.push_back(comp);
            }
        }

        *out_count = tweens.size();

        if (out_array && cap > 0) {
            size_t count = std::min(cap, tweens.size());
            for (size_t i = 0; i < count; ++i) {
                out_array[i] = CreateComponentHandle(tweens[i]);
            }
        }

        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to list tweens");
        return LC_ERROR_INTERNAL_ERROR;
    }
}