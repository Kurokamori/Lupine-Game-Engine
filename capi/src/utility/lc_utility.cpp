
#include "utility/lc_utility.h"
#include "../core/lc_internal.h"

#include <lupine\components\Timer.hpp>

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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
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
            SetUtilityError(LC_ERROR_TYPE_MISMATCH, "Component is not a Timer");
            return LC_ERROR_TYPE_MISMATCH;
        }

        timer->SetIgnoreTimeScale(ignore);
        return LC_SUCCESS;
    } catch (...) {
        SetUtilityError(LC_ERROR_INTERNAL_ERROR, "Failed to set ignore time scale");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

