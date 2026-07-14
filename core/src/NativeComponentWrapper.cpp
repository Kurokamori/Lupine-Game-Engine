#include "lupine/core/NativeComponentWrapper.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/logger/Logger.hpp"

#include <cmath>

namespace lupine {
namespace core {

namespace {
/** Borrowed handle the wrapper presents to the plugin for the active render
 *  context; valid only for the duration of the build_draw_commands call. */
LupineRenderContextHandle ToRenderHandle(RenderContext& ctx) {
    return reinterpret_cast<LupineRenderContextHandle>(&ctx);
}
} // namespace

NativeComponentWrapper::NativeComponentWrapper()
    : Component("NativeComponentWrapper") {
}

NativeComponentWrapper::NativeComponentWrapper(const std::string& className)
    : Component(className), m_ClassName(className) {

    m_Class = NativeExtensionRegistry::GetInstance().GetClass(className);
    if (!m_Class) {
        LOG_ERROR(LogCategory::Core,
            "NativeComponentWrapper: no registered native class '{}'", className);
        return;
    }

    if (!m_Class->vtable.create) {
        LOG_ERROR(LogCategory::Core,
            "NativeComponentWrapper: native class '{}' has no create thunk", className);
        return;
    }

    m_Instance = m_Class->vtable.create(SelfHandle());
    if (!m_Instance) {
        LOG_ERROR(LogCategory::Core,
            "NativeComponentWrapper: native class '{}' create returned null", className);
    }
}

NativeComponentWrapper::~NativeComponentWrapper() {
    if (m_Instance && m_Class) {
        // Mirror Component lifecycle: deliver OnDestroy once before tearing the
        // plugin object down, then call the plugin destructor thunk.
        if (!m_Destroyed && m_Class->HasHook(LUPINE_HOOK_DESTROY) && m_Class->vtable.on_destroy) {
            m_Class->vtable.on_destroy(m_Instance, SelfHandle());
            m_Destroyed = true;
        }
        if (m_Class->vtable.destroy) {
            m_Class->vtable.destroy(m_Instance);
        }
    }
    m_Instance = nullptr;
}

void NativeComponentWrapper::DefineProperties() {
    // The plugin enumerates its exported properties by calling
    // host->define_property(self, ...) for each, which feeds DefineProperty here.
    if (m_Instance && m_Class && m_Class->vtable.define_properties) {
        m_Class->vtable.define_properties(m_Instance, SelfHandle());
    }
}

void NativeComponentWrapper::OnAwake() {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_AWAKE) && m_Class->vtable.on_awake) {
        m_Class->vtable.on_awake(m_Instance, SelfHandle());
    }
}

void NativeComponentWrapper::OnReady() {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_READY) && m_Class->vtable.on_ready) {
        m_Class->vtable.on_ready(m_Instance, SelfHandle());
    }
}

void NativeComponentWrapper::OnDestroy() {
    if (m_Instance && m_Class && !m_Destroyed &&
        m_Class->HasHook(LUPINE_HOOK_DESTROY) && m_Class->vtable.on_destroy) {
        m_Class->vtable.on_destroy(m_Instance, SelfHandle());
        m_Destroyed = true;
    }
}

void NativeComponentWrapper::OnUpdate(float deltaTime) {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_UPDATE) && m_Class->vtable.on_update) {
        m_Class->vtable.on_update(m_Instance, SelfHandle(), deltaTime);
    }
}

void NativeComponentWrapper::OnInput(float deltaTime) {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_INPUT) && m_Class->vtable.on_input) {
        m_Class->vtable.on_input(m_Instance, SelfHandle(), deltaTime);
    }
}

void NativeComponentWrapper::OnInputEvent(const nlohmann::json& event) {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_INPUT_EVENT) && m_Class->vtable.on_input_event) {
        std::string payload = event.dump();
        m_Class->vtable.on_input_event(m_Instance, SelfHandle(), payload.c_str());
    }
}

void NativeComponentWrapper::OnProcess(float deltaTime) {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_PROCESS) && m_Class->vtable.on_process) {
        m_Class->vtable.on_process(m_Instance, SelfHandle(), deltaTime);
    }
}

void NativeComponentWrapper::OnPhysicsProcess(float deltaTime) {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_PHYSICS_PROCESS) && m_Class->vtable.on_physics_process) {
        m_Class->vtable.on_physics_process(m_Instance, SelfHandle(), deltaTime);
    }
}

void NativeComponentWrapper::OnRender() {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_RENDER) && m_Class->vtable.on_render) {
        m_Class->vtable.on_render(m_Instance, SelfHandle());
    }
}

void NativeComponentWrapper::OnLateUpdate(float deltaTime) {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_LATE_UPDATE) && m_Class->vtable.on_late_update) {
        m_Class->vtable.on_late_update(m_Instance, SelfHandle(), deltaTime);
    }
}

void NativeComponentWrapper::OnEnterTree() {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_ENTER_TREE) && m_Class->vtable.on_enter_tree) {
        m_Class->vtable.on_enter_tree(m_Instance, SelfHandle());
    }
}

void NativeComponentWrapper::OnExitTree() {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_EXIT_TREE) && m_Class->vtable.on_exit_tree) {
        m_Class->vtable.on_exit_tree(m_Instance, SelfHandle());
    }
}

void NativeComponentWrapper::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_PROPERTY_CHANGED) && m_Class->vtable.on_property_changed) {
        std::string valueStr = newValue.dump();
        m_Class->vtable.on_property_changed(m_Instance, SelfHandle(), propertyName.c_str(), valueStr.c_str());
    }
}

nlohmann::json NativeComponentWrapper::CallMethod(const std::string& method, const nlohmann::json& args) {
    if (!m_Instance || !m_Class || !m_Class->HasHook(LUPINE_HOOK_CALL_METHOD) || !m_Class->vtable.call_method) {
        return nlohmann::json();
    }

    std::string argsStr = args.is_null() ? std::string("null") : args.dump();
    char* result = m_Class->vtable.call_method(m_Instance, SelfHandle(), method.c_str(), argsStr.c_str());
    if (!result) {
        return nlohmann::json();
    }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(result);
    } catch (const std::exception& e) {
        LOG_WARN(LogCategory::Core,
            "NativeComponentWrapper: call_method '{}' on '{}' returned invalid JSON: {}",
            method, m_ClassName, e.what());
        parsed = nlohmann::json();
    }

    // The result was allocated via host->string_dup, so the host frees it.
    const LupineHostInterface* host = ExtensionManager::HostInterface();
    if (host && host->string_free) {
        host->string_free(result);
    }
    return parsed;
}

void NativeComponentWrapper::buildDrawCommands(RenderContext& ctx) {
    if (m_Instance && m_Class && m_Class->HasHook(LUPINE_HOOK_DRAW) && m_Class->vtable.build_draw_commands) {
        m_Class->vtable.build_draw_commands(m_Instance, SelfHandle(), ToRenderHandle(ctx));
    }
}

AABB NativeComponentWrapper::getWorldBounds() const {
    const bool draws = m_Class && m_Class->HasHook(LUPINE_HOOK_DRAW) && m_Class->vtable.build_draw_commands;
    if (!draws) {
        // No draw hook: this component contributes no scene geometry. Report an
        // empty box so culling and the editor selection outline ignore it.
        return AABB(math::Vec3(0.0f, 0.0f, 0.0f), math::Vec3(0.0f, 0.0f, 0.0f));
    }

    if (m_Instance && m_Class->vtable.get_render_bounds) {
        float minXYZ[3] = { 0.0f, 0.0f, 0.0f };
        float maxXYZ[3] = { 0.0f, 0.0f, 0.0f };
        LupineComponentHandle self = const_cast<NativeComponentWrapper*>(this)->SelfHandle();
        if (m_Class->vtable.get_render_bounds(m_Instance, self, minXYZ, maxXYZ) != 0) {
            return AABB(math::Vec3(minXYZ[0], minXYZ[1], minXYZ[2]),
                        math::Vec3(maxXYZ[0], maxXYZ[1], maxXYZ[2]));
        }
    }

    Node* owner = GetOwner();
    if (auto* node3D = dynamic_cast<Node3D*>(owner)) {
        math::Vec3 p = node3D->GetGlobalPosition();
        math::Vec3 s = node3D->GetGlobalScale();
        math::Vec3 half(0.5f * std::fabs(s.x), 0.5f * std::fabs(s.y), 0.5f * std::fabs(s.z));
        return AABB(p - half, p + half);
    }
    if (auto* node2D = dynamic_cast<Node2D*>(owner)) {
        math::Vec2 p = node2D->GetGlobalPosition();
        math::Vec2 s = node2D->GetGlobalScale();
        float hx = 32.0f * std::fabs(s.x);
        float hy = 32.0f * std::fabs(s.y);
        return AABB(math::Vec3(p.x - hx, p.y - hy, 0.0f),
                    math::Vec3(p.x + hx, p.y + hy, 0.0f));
    }
    return AABB(math::Vec3(0.0f, 0.0f, 0.0f), math::Vec3(0.0f, 0.0f, 0.0f));
}

SpatialType NativeComponentWrapper::getSpatialType() const {
    Node* owner = GetOwner();
    if (dynamic_cast<Node3D*>(owner)) {
        return SpatialType::World3D;
    }
    if (dynamic_cast<Node2D*>(owner)) {
        return SpatialType::World2D;
    }
    return SpatialType::World3D;
}

bool NativeComponentWrapper::isRenderContentDynamic() const {
    return m_Class && m_Class->HasHook(LUPINE_HOOK_DRAW) && m_Class->vtable.build_draw_commands;
}

} // namespace core
} // namespace lupine
