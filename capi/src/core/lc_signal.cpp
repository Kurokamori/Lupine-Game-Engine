#include "core/lc_signal.h"
#include "core/lc_core.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/core/Component.hpp>
#include <lupine/core/SignalObject.hpp>
#include <lupine/core/SignalDispatcher.hpp>
#include <lupine/core/EventBus.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace {

// Parse a JSON-array argument string into the engine's SignalArgs. A null or
// empty string, or any non-array payload, yields an empty argument list.
lupine::core::SignalArgs ParseArgs(const char* args_json) {
    lupine::core::SignalArgs args;
    if (!args_json || args_json[0] == '\0') {
        return args;
    }
    nlohmann::json parsed = nlohmann::json::parse(args_json, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_array()) {
        for (const nlohmann::json& v : parsed) {
            args.push_back(v);
        }
    } else if (!parsed.is_discarded() && !parsed.is_null()) {
        args.push_back(parsed);
    }
    return args;
}

// Build a native slot that serialises the emitted arguments to a JSON array
// string and forwards them to a C callback.
lupine::core::SignalSlot MakeCallbackSlot(LCSignalCallback callback, void* user_data) {
    return [callback, user_data](const lupine::core::SignalArgs& args) {
        if (!callback) {
            return;
        }
        nlohmann::json arr = nlohmann::json::array();
        for (const nlohmann::json& a : args) {
            arr.push_back(a);
        }
        std::string serialized = arr.dump();
        callback(serialized.c_str(), user_data);
    };
}

} // namespace

// ===========================================================================
// Node signals
// ===========================================================================

LC_API LCResult lc_node_add_user_signal(LCNodeHandle node, const char* signal) {
    if (!signal) {
        SetError(LC_ERROR_NULL_POINTER, "signal name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        n->AddUserSignal(signal);
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_emit(LCNodeHandle node, const char* signal, const char* args_json) {
    if (!signal) {
        SetError(LC_ERROR_NULL_POINTER, "signal name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        n->Emit(signal, ParseArgs(args_json));
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_connect(LCNodeHandle node, const char* signal,
                                LCNodeHandle target, const char* method,
                                uint32_t flags, uint64_t* out_connection_id) {
    if (!signal || !method) {
        SetError(LC_ERROR_NULL_POINTER, "signal or method name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    auto t = GetNode(target);
    if (!n || !t) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        uint64_t id = n->Connect(signal, t.get(), method, flags);
        if (out_connection_id) {
            *out_connection_id = id;
        }
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_connect_callback(LCNodeHandle node, const char* signal,
                                         LCSignalCallback callback, void* user_data,
                                         uint32_t flags, uint64_t* out_connection_id) {
    if (!signal || !callback) {
        SetError(LC_ERROR_NULL_POINTER, "signal name or callback is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        uint64_t id = n->Connect(signal, MakeCallbackSlot(callback, user_data), flags);
        if (out_connection_id) {
            *out_connection_id = id;
        }
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_node_disconnect(LCNodeHandle node, const char* signal, uint64_t connection_id) {
    if (!signal) {
        SetError(LC_ERROR_NULL_POINTER, "signal name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    n->Disconnect(signal, connection_id);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_is_connected(LCNodeHandle node, const char* signal, bool* out_connected) {
    if (!signal || !out_connected) {
        SetError(LC_ERROR_NULL_POINTER, "signal name or output is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    *out_connected = n->IsConnected(signal);
    return LC_SUCCESS;
}

LC_API LCResult lc_node_call_deferred(LCNodeHandle node, const char* method, const char* args_json) {
    if (!method) {
        SetError(LC_ERROR_NULL_POINTER, "method name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        lupine::core::SignalDispatcher::Get().CallDeferred(n.get(), method, ParseArgs(args_json));
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

// ===========================================================================
// Component signals
// ===========================================================================

LC_API LCResult lc_component_add_user_signal(LCComponentHandle component, const char* signal) {
    if (!signal) {
        SetError(LC_ERROR_NULL_POINTER, "signal name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto c = GetComponent(component);
    if (!c) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        c->AddUserSignal(signal);
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_emit(LCComponentHandle component, const char* signal, const char* args_json) {
    if (!signal) {
        SetError(LC_ERROR_NULL_POINTER, "signal name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto c = GetComponent(component);
    if (!c) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        c->Emit(signal, ParseArgs(args_json));
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_connect(LCComponentHandle component, const char* signal,
                                     LCNodeHandle target, const char* method,
                                     uint32_t flags, uint64_t* out_connection_id) {
    if (!signal || !method) {
        SetError(LC_ERROR_NULL_POINTER, "signal or method name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto c = GetComponent(component);
    auto t = GetNode(target);
    if (!c || !t) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        uint64_t id = c->Connect(signal, t.get(), method, flags);
        if (out_connection_id) {
            *out_connection_id = id;
        }
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_connect_callback(LCComponentHandle component, const char* signal,
                                              LCSignalCallback callback, void* user_data,
                                              uint32_t flags, uint64_t* out_connection_id) {
    if (!signal || !callback) {
        SetError(LC_ERROR_NULL_POINTER, "signal name or callback is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto c = GetComponent(component);
    if (!c) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        uint64_t id = c->Connect(signal, MakeCallbackSlot(callback, user_data), flags);
        if (out_connection_id) {
            *out_connection_id = id;
        }
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_component_disconnect(LCComponentHandle component, const char* signal, uint64_t connection_id) {
    if (!signal) {
        SetError(LC_ERROR_NULL_POINTER, "signal name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto c = GetComponent(component);
    if (!c) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    c->Disconnect(signal, connection_id);
    return LC_SUCCESS;
}

LC_API LCResult lc_component_is_connected(LCComponentHandle component, const char* signal, bool* out_connected) {
    if (!signal || !out_connected) {
        SetError(LC_ERROR_NULL_POINTER, "signal name or output is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto c = GetComponent(component);
    if (!c) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    *out_connected = c->IsConnected(signal);
    return LC_SUCCESS;
}

// ===========================================================================
// Global event bus
// ===========================================================================

LC_API LCResult lc_event_emit(const char* event, const char* args_json) {
    if (!event) {
        SetError(LC_ERROR_NULL_POINTER, "event name is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::EventBus::Get().Emit(event, ParseArgs(args_json));
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_event_subscribe(const char* event, LCNodeHandle target, const char* method,
                                   uint32_t flags, uint64_t* out_subscription_id) {
    if (!event || !method) {
        SetError(LC_ERROR_NULL_POINTER, "event or method name is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto t = GetNode(target);
    if (!t) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        uint64_t id = lupine::core::EventBus::Get().Subscribe(event, t.get(), method, flags);
        if (out_subscription_id) {
            *out_subscription_id = id;
        }
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_event_subscribe_callback(const char* event, LCSignalCallback callback, void* user_data,
                                            uint32_t flags, uint64_t* out_subscription_id) {
    if (!event || !callback) {
        SetError(LC_ERROR_NULL_POINTER, "event name or callback is null");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        uint64_t id = lupine::core::EventBus::Get().Subscribe(event, MakeCallbackSlot(callback, user_data), flags);
        if (out_subscription_id) {
            *out_subscription_id = id;
        }
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_event_unsubscribe(const char* event, uint64_t subscription_id) {
    if (!event) {
        SetError(LC_ERROR_NULL_POINTER, "event name is null");
        return LC_ERROR_NULL_POINTER;
    }
    lupine::core::EventBus::Get().Unsubscribe(event, subscription_id);
    return LC_SUCCESS;
}
