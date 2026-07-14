/**
 * @file Extension.hpp
 * @brief Lupine native extension SDK — registration + entry-point machinery.
 *
 * Include this header in your plugin sources. Subclass lupine::ext::Component
 * (see Component.hpp), then register each class and emit the binary entry points:
 *
 * @code
 * #include <lupine/ext/Extension.hpp>
 *
 * class OrbitMover : public lupine::ext::Component {
 *     float m_angle = 0.0f;
 * public:
 *     void DefineProperties() override {
 *         ExportFloatRange("speed", 1.0f, 0.0f, 10.0f, 0.1f, "Movement");
 *         ExportFloatRange("radius", 100.0f, 0.0f, 1000.0f, 1.0f, "Movement");
 *     }
 *     void OnUpdate(float dt) override {
 *         m_angle += GetFloat("speed") * dt;
 *         NodeRef owner = GetOwnerNode();
 *         owner.SetProperty("position", { {"x", GetFloat("radius") * std::cos(m_angle)},
 *                                         {"y", GetFloat("radius") * std::sin(m_angle)} });
 *     }
 * };
 *
 * LUPINE_REGISTER_COMPONENT(OrbitMover)   // type name "OrbitMover", base Component
 * LUPINE_EXTENSION_MAIN()                 // emit the exported entry points (once)
 * @endcode
 */

#ifndef LUPINE_EXT_EXTENSION_HPP
#define LUPINE_EXT_EXTENSION_HPP

#include "lupine/ext/Component.hpp"

#include <vector>
#include <cstdint>

namespace lupine {
namespace ext {
namespace detail {

inline Component* AsComponent(LupineInstance inst) {
    return static_cast<Component*>(inst);
}

/* ---- Shared (non-templated) lifecycle thunks: one entity each across TUs ---- */

inline void LUPINE_EXT_CALL ThunkDefineProperties(LupineInstance inst, LupineComponentHandle self) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->DefineProperties();
}
inline void LUPINE_EXT_CALL ThunkOnAwake(LupineInstance inst, LupineComponentHandle self) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnAwake();
}
inline void LUPINE_EXT_CALL ThunkOnReady(LupineInstance inst, LupineComponentHandle self) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnReady();
}
inline void LUPINE_EXT_CALL ThunkOnDestroy(LupineInstance inst, LupineComponentHandle self) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnDestroy();
}
inline void LUPINE_EXT_CALL ThunkOnUpdate(LupineInstance inst, LupineComponentHandle self, float dt) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnUpdate(dt);
}
inline void LUPINE_EXT_CALL ThunkOnProcess(LupineInstance inst, LupineComponentHandle self, float dt) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnProcess(dt);
}
inline void LUPINE_EXT_CALL ThunkOnPhysicsProcess(LupineInstance inst, LupineComponentHandle self, float dt) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnPhysicsProcess(dt);
}
inline void LUPINE_EXT_CALL ThunkOnInput(LupineInstance inst, LupineComponentHandle self, float dt) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnInput(dt);
}
inline void LUPINE_EXT_CALL ThunkOnRender(LupineInstance inst, LupineComponentHandle self) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnRender();
}
inline void LUPINE_EXT_CALL ThunkOnLateUpdate(LupineInstance inst, LupineComponentHandle self, float dt) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnLateUpdate(dt);
}
inline void LUPINE_EXT_CALL ThunkOnEnterTree(LupineInstance inst, LupineComponentHandle self) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnEnterTree();
}
inline void LUPINE_EXT_CALL ThunkOnExitTree(LupineInstance inst, LupineComponentHandle self) {
    Component* c = AsComponent(inst); c->_BindHandle(self); c->OnExitTree();
}
inline void LUPINE_EXT_CALL ThunkOnInputEvent(LupineInstance inst, LupineComponentHandle self, const char* event_json) {
    Component* c = AsComponent(inst); c->_BindHandle(self);
    nlohmann::json j;
    if (event_json) { try { j = nlohmann::json::parse(event_json); } catch (...) {} }
    c->OnInputEvent(j);
}
inline void LUPINE_EXT_CALL ThunkOnPropertyChanged(LupineInstance inst, LupineComponentHandle self,
                                                   const char* name, const char* value_json) {
    Component* c = AsComponent(inst); c->_BindHandle(self);
    nlohmann::json v;
    if (value_json) { try { v = nlohmann::json::parse(value_json); } catch (...) {} }
    c->OnPropertyChanged(name ? name : "", v);
}
inline char* LUPINE_EXT_CALL ThunkCallMethod(LupineInstance inst, LupineComponentHandle self,
                                             const char* method, const char* args_json) {
    Component* c = AsComponent(inst); c->_BindHandle(self);
    nlohmann::json a;
    if (args_json) { try { a = nlohmann::json::parse(args_json); } catch (...) {} }
    nlohmann::json r = c->CallMethod(method ? method : "", a);
    if (r.is_null()) return nullptr;
    const std::string s = r.dump();
    return HostRef()->string_dup(s.c_str());
}
inline void LUPINE_EXT_CALL ThunkBuildDrawCommands(LupineInstance inst, LupineComponentHandle self,
                                                   LupineRenderContextHandle ctx) {
    Component* c = AsComponent(inst); c->_BindHandle(self);
    RenderContext wrapper(ctx);
    c->BuildDrawCommands(wrapper);
}
inline LupineBool LUPINE_EXT_CALL ThunkGetRenderBounds(LupineInstance inst, LupineComponentHandle self,
                                                       float* out_min_xyz, float* out_max_xyz) {
    Component* c = AsComponent(inst); c->_BindHandle(self);
    if (!out_min_xyz || !out_max_xyz) return 0;
    return c->GetRenderBounds(out_min_xyz, out_max_xyz) ? 1 : 0;
}

/* ---- Per-class create/destroy thunks ---- */

template <typename T>
LupineInstance LUPINE_EXT_CALL ThunkCreate(LupineComponentHandle self) {
    T* obj = new T();
    Component* base = static_cast<Component*>(obj);
    base->_BindHandle(self);
    return static_cast<LupineInstance>(base);
}
template <typename T>
void LUPINE_EXT_CALL ThunkDestroy(LupineInstance inst) {
    Component* base = static_cast<Component*>(inst);
    delete static_cast<T*>(base);
}

template <typename T>
LupineComponentVTable MakeVTable() {
    LupineComponentVTable v{};
    v.create = &ThunkCreate<T>;
    v.destroy = &ThunkDestroy<T>;
    v.define_properties = &ThunkDefineProperties;
    v.on_awake = &ThunkOnAwake;
    v.on_ready = &ThunkOnReady;
    v.on_update = &ThunkOnUpdate;
    v.on_process = &ThunkOnProcess;
    v.on_physics_process = &ThunkOnPhysicsProcess;
    v.on_input = &ThunkOnInput;
    v.on_render = &ThunkOnRender;
    v.on_late_update = &ThunkOnLateUpdate;
    v.on_enter_tree = &ThunkOnEnterTree;
    v.on_exit_tree = &ThunkOnExitTree;
    v.on_destroy = &ThunkOnDestroy;
    v.on_input_event = &ThunkOnInputEvent;
    v.on_property_changed = &ThunkOnPropertyChanged;
    v.call_method = &ThunkCallMethod;
    v.build_draw_commands = &ThunkBuildDrawCommands;
    v.get_render_bounds = &ThunkGetRenderBounds;
    return v;
}

/** A registrar runs at init to register one class with the host. */
using RegisterFn = void (*)(LupineRegistrationContext);

inline std::vector<RegisterFn>& Registrars() {
    static std::vector<RegisterFn> s_registrars;
    return s_registrars;
}

template <typename T>
void RegisterClassImpl(LupineRegistrationContext ctx, const char* name, const char* base,
                       const char* category, uint32_t hooks) {
    LupineComponentClassDesc desc{};
    desc.struct_size = static_cast<uint32_t>(sizeof(LupineComponentClassDesc));
    desc.class_name = name;
    desc.base_class_name = base;
    desc.display_category = category;
    desc.hook_flags = hooks;
    desc.vtable = MakeVTable<T>();
    if (HostRef() && HostRef()->register_component_class) {
        HostRef()->register_component_class(ctx, &desc);
    }
}

} // namespace detail
} // namespace ext
} // namespace lupine

/** All lifecycle hooks enabled. Safe default: every override you write is called. */
#define LUPINE_HOOKS_ALL                                                     \
    ( LUPINE_HOOK_AWAKE | LUPINE_HOOK_READY | LUPINE_HOOK_UPDATE |           \
      LUPINE_HOOK_PROCESS | LUPINE_HOOK_PHYSICS_PROCESS | LUPINE_HOOK_INPUT |\
      LUPINE_HOOK_RENDER | LUPINE_HOOK_LATE_UPDATE | LUPINE_HOOK_ENTER_TREE |\
      LUPINE_HOOK_EXIT_TREE | LUPINE_HOOK_DESTROY | LUPINE_HOOK_INPUT_EVENT |\
      LUPINE_HOOK_PROPERTY_CHANGED | LUPINE_HOOK_CALL_METHOD )

/**
 * Register a component class with full control over the registered type name,
 * base component, editor category and the set of hooks the host should call.
 * Place at file scope (not inside a function).
 */
#define LUPINE_REGISTER_COMPONENT_EX(T, NAME, BASE, CATEGORY, HOOKS)                              \
    namespace {                                                                                   \
        struct T##_LupineRegistrar {                                                              \
            T##_LupineRegistrar() {                                                               \
                ::lupine::ext::detail::Registrars().push_back(                                    \
                    +[](LupineRegistrationContext ctx) {                                          \
                        ::lupine::ext::detail::RegisterClassImpl<T>(ctx, NAME, BASE, CATEGORY, HOOKS); \
                    });                                                                           \
            }                                                                                     \
        };                                                                                        \
        static T##_LupineRegistrar g_##T##_LupineRegistrar;                                       \
    }

/** Register a component class with default settings (type name == #T, base
 *  Component, category "Native", all hooks enabled). */
#define LUPINE_REGISTER_COMPONENT(T) \
    LUPINE_REGISTER_COMPONENT_EX(T, #T, "", "Native", LUPINE_HOOKS_ALL)

/** Register a component class that extends a built-in component (e.g. "Sprite2D"). */
#define LUPINE_REGISTER_COMPONENT_AS(T, NAME, BASE, CATEGORY) \
    LUPINE_REGISTER_COMPONENT_EX(T, NAME, BASE, CATEGORY, LUPINE_HOOKS_ALL)

/**
 * All lifecycle hooks PLUS the rendering hook. LUPINE_HOOK_DRAW is deliberately
 * NOT part of LUPINE_HOOKS_ALL: enabling it makes the component a renderable
 * (gathered every frame, never statically cached), so it must be opted into.
 */
#define LUPINE_HOOKS_ALL_DRAWABLE  (LUPINE_HOOKS_ALL | LUPINE_HOOK_DRAW)

/**
 * Register a component that renders at runtime. Identical to
 * LUPINE_REGISTER_COMPONENT but advertises LUPINE_HOOK_DRAW so the host calls
 * BuildDrawCommands during the render gather stage. Override BuildDrawCommands
 * (and optionally GetRenderBounds) in the class.
 */
#define LUPINE_REGISTER_COMPONENT_DRAWABLE(T) \
    LUPINE_REGISTER_COMPONENT_EX(T, #T, "", "Native", LUPINE_HOOKS_ALL_DRAWABLE)

/**
 * Emit the required exported entry points. Place EXACTLY ONCE in your plugin,
 * at file scope in any one source file.
 */
#define LUPINE_EXTENSION_MAIN()                                                                   \
    extern "C" LUPINE_EXT_EXPORT uint32_t LUPINE_EXT_CALL lupine_extension_abi_version(void) {    \
        return LUPINE_EXTENSION_ABI_VERSION;                                                      \
    }                                                                                             \
    extern "C" LUPINE_EXT_EXPORT LupineBool LUPINE_EXT_CALL lupine_extension_init(                \
        uint32_t host_abi, const LupineHostInterface* host,                                       \
        LupineRegistrationContext ctx, LupineExtensionHandle* out_extension) {                    \
        if (host_abi != LUPINE_EXTENSION_ABI_VERSION || host == nullptr) return 0;                \
        ::lupine::ext::detail::HostRef() = host;                                                  \
        for (::lupine::ext::detail::RegisterFn fn : ::lupine::ext::detail::Registrars()) {        \
            fn(ctx);                                                                              \
        }                                                                                         \
        if (out_extension) *out_extension = nullptr;                                              \
        return 1;                                                                                 \
    }                                                                                             \
    extern "C" LUPINE_EXT_EXPORT void LUPINE_EXT_CALL lupine_extension_deinit(                    \
        LupineExtensionHandle extension) {                                                        \
        (void)extension;                                                                          \
    }

#endif // LUPINE_EXT_EXTENSION_HPP
