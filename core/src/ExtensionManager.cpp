#include "lupine/core/ExtensionManager.hpp"
#include "lupine/core/NativeComponentWrapper.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Vec4.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/logger/Logger.hpp"

#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace lupine {
namespace core {

namespace {

/* ----------------------------------------------------------------------------
 * Handle conversions. LupineComponentHandle is a Component*, LupineNodeHandle a
 * Node*. They are only valid for the duration of a host call, which is exactly
 * how plugins use them.
 * -------------------------------------------------------------------------- */
Component* ToComponent(LupineComponentHandle h) { return reinterpret_cast<Component*>(h); }
Node* ToNode(LupineNodeHandle h) { return reinterpret_cast<Node*>(h); }
LupineNodeHandle FromNode(Node* n) { return reinterpret_cast<LupineNodeHandle>(n); }
LupineComponentHandle FromComponent(Component* c) { return reinterpret_cast<LupineComponentHandle>(c); }
RenderContext* ToRenderContext(LupineRenderContextHandle h) { return reinterpret_cast<RenderContext*>(h); }

/* Host-owned string duplication so allocation + free both live on the host side. */
char* DupCStr(const char* s, size_t len) {
    char* out = static_cast<char*>(std::malloc(len + 1));
    if (!out) return nullptr;
    std::memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
char* DupString(const std::string& s) { return DupCStr(s.c_str(), s.size()); }

bool TryParseJson(const char* text, nlohmann::json& out) {
    if (!text) { out = nlohmann::json(); return true; }
    try {
        out = nlohmann::json::parse(text);
        return true;
    } catch (...) {
        return false;
    }
}

Node* WalkNodePath(Node* start, const char* path) {
    if (!start || !path) return nullptr;
    std::string p(path);
    if (p.empty()) return start;
    Node* current = start;
    std::stringstream ss(p);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".") continue;
        std::shared_ptr<Node> child = current->GetChild(segment);
        if (!child) return nullptr;
        current = child.get();
    }
    return current;
}

/* ----------------------------------------------------------------------------
 * Host interface thunks
 * -------------------------------------------------------------------------- */

void LUPINE_EXT_CALL HostStringFree(char* str) {
    std::free(str);
}

char* LUPINE_EXT_CALL HostStringDup(const char* s) {
    if (!s) return nullptr;
    return DupCStr(s, std::strlen(s));
}

void LUPINE_EXT_CALL HostLog(int32_t level, const char* category, const char* message) {
    std::string text;
    if (category && category[0]) {
        text.append("[").append(category).append("] ");
    }
    if (message) text.append(message);
    switch (level) {
        case LUPINE_LOG_TRACE:
        case LUPINE_LOG_DEBUG: LOG_DEBUG(LogCategory::Core, "{}", text); break;
        case LUPINE_LOG_INFO:  LOG_INFO(LogCategory::Core, "{}", text); break;
        case LUPINE_LOG_WARN:  LOG_WARN(LogCategory::Core, "{}", text); break;
        default:               LOG_ERROR(LogCategory::Core, "{}", text); break;
    }
}

LupineBool LUPINE_EXT_CALL HostRegisterComponentClass(LupineRegistrationContext ctx,
                                                      const LupineComponentClassDesc* desc) {
    bool ok = NativeExtensionRegistry::GetInstance().RegisterComponentClass(desc);
    if (ok && ctx && desc && desc->class_name) {
        auto* record = static_cast<ExtensionManager::LoadedExtension*>(ctx);
        record->registeredClasses.emplace_back(desc->class_name);
    }
    return ok ? 1 : 0;
}

void LUPINE_EXT_CALL HostDefineProperty(LupineComponentHandle self, const char* descriptor_json) {
    Component* comp = ToComponent(self);
    if (!comp || !descriptor_json) return;
    nlohmann::json parsed;
    if (!TryParseJson(descriptor_json, parsed) || !parsed.is_object()) return;
    try {
        PropertyDescriptor desc = PropertyDescriptor::Deserialize(parsed);
        if (!desc.name.empty()) {
            comp->GetPropertyRegistry().DefineProperty(desc);
        }
    } catch (...) {
        LOG_WARN(LogCategory::Core, "Extension define_property: failed to parse descriptor");
    }
}

char* LUPINE_EXT_CALL HostGetPropertyJson(LupineComponentHandle self, const char* name) {
    Component* comp = ToComponent(self);
    if (!comp || !name) return nullptr;
    const ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
    if (!prop) return nullptr;
    return DupString(prop->GetValueAsJson().dump());
}

LupineBool LUPINE_EXT_CALL HostSetPropertyJson(LupineComponentHandle self, const char* name, const char* value_json) {
    Component* comp = ToComponent(self);
    if (!comp || !name) return 0;
    nlohmann::json value;
    if (!TryParseJson(value_json, value)) return 0;
    ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
    if (!prop) return 0;
    prop->SetValueFromJson(value);
    return 1;
}

LupineNodeHandle LUPINE_EXT_CALL HostGetOwnerNode(LupineComponentHandle self) {
    Component* comp = ToComponent(self);
    if (!comp) return nullptr;
    return FromNode(comp->GetOwner());
}

char* LUPINE_EXT_CALL HostNodeGetName(LupineNodeHandle node) {
    Node* n = ToNode(node);
    if (!n) return nullptr;
    return DupString(n->GetName());
}

char* LUPINE_EXT_CALL HostNodeGetPath(LupineNodeHandle node) {
    Node* n = ToNode(node);
    if (!n) return nullptr;
    return DupString(n->GetPath());
}

LupineComponentHandle LUPINE_EXT_CALL HostNodeGetComponentByType(LupineNodeHandle node, const char* type_name) {
    Node* n = ToNode(node);
    if (!n || !type_name) return nullptr;
    std::shared_ptr<Component> comp = n->GetComponent(type_name);
    return FromComponent(comp.get());
}

LupineNodeHandle LUPINE_EXT_CALL HostNodeGetNode(LupineNodeHandle node, const char* path) {
    return FromNode(WalkNodePath(ToNode(node), path));
}

char* LUPINE_EXT_CALL HostNodeGetPropertyJson(LupineNodeHandle node, const char* name) {
    Node* n = ToNode(node);
    if (!n || !name) return nullptr;
    for (const std::shared_ptr<Component>& comp : n->GetComponents()) {
        if (comp && comp->GetPropertyRegistry().HasProperty(name)) {
            const ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
            if (prop) return DupString(prop->GetValueAsJson().dump());
        }
    }
    n->RegisterProperties();
    nlohmann::json serialized = n->Serialize();
    if (serialized.contains("properties") && serialized["properties"].contains(name)) {
        return DupString(serialized["properties"][name].dump());
    }
    return nullptr;
}

LupineBool LUPINE_EXT_CALL HostNodeSetPropertyJson(LupineNodeHandle node, const char* name, const char* value_json) {
    Node* n = ToNode(node);
    if (!n || !name) return 0;
    nlohmann::json value;
    if (!TryParseJson(value_json, value)) return 0;
    for (const std::shared_ptr<Component>& comp : n->GetComponents()) {
        if (comp && comp->GetPropertyRegistry().HasProperty(name)) {
            ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(name);
            if (prop) {
                prop->SetValueFromJson(value);
                comp->OnPropertyChanged(name, value);
                return 1;
            }
        }
    }
    n->RegisterProperties();
    nlohmann::json serialized = n->Serialize();
    if (!(serialized.contains("properties") && serialized["properties"].contains(name))) {
        return 0;
    }
    nlohmann::json patch;
    patch["properties"][name] = value;
    n->Deserialize(patch);
    return 1;
}

char* LUPINE_EXT_CALL HostComponentGetTypeName(LupineComponentHandle comp) {
    Component* c = ToComponent(comp);
    if (!c) return nullptr;
    return DupString(c->GetTypeName());
}

char* LUPINE_EXT_CALL HostComponentGetPropertyJson(LupineComponentHandle comp, const char* name) {
    Component* c = ToComponent(comp);
    if (!c || !name) return nullptr;
    const ComponentProperty* prop = c->GetPropertyRegistry().GetProperty(name);
    if (prop) return DupString(prop->GetValueAsJson().dump());
    nlohmann::json serialized = c->Serialize();
    if (serialized.contains("properties") && serialized["properties"].contains(name)) {
        return DupString(serialized["properties"][name].dump());
    }
    return nullptr;
}

LupineBool LUPINE_EXT_CALL HostComponentSetPropertyJson(LupineComponentHandle comp, const char* name, const char* value_json) {
    Component* c = ToComponent(comp);
    if (!c || !name) return 0;
    nlohmann::json value;
    if (!TryParseJson(value_json, value)) return 0;
    ComponentProperty* prop = c->GetPropertyRegistry().GetProperty(name);
    if (prop) {
        prop->SetValueFromJson(value);
        c->OnPropertyChanged(name, value);
        return 1;
    }
    nlohmann::json serialized = c->Serialize();
    if (!(serialized.contains("properties") && serialized["properties"].contains(name))) {
        return 0;
    }
    nlohmann::json patch;
    patch["properties"][name] = value;
    c->Deserialize(patch);
    c->OnPropertyChanged(name, value);
    return 1;
}

char* LUPINE_EXT_CALL HostComponentCallMethodJson(LupineComponentHandle comp, const char* method, const char* args_json) {
    Component* c = ToComponent(comp);
    if (!c || !method) return nullptr;
    nlohmann::json args;
    if (!TryParseJson(args_json, args)) return nullptr;
    nlohmann::json result = c->CallMethod(method, args);
    if (result.is_null()) return nullptr;
    return DupString(result.dump());
}

/* ----------------------------------------------------------------------------
 * Rendering bridge thunks (ABI v2)
 *
 * The render_draw_* thunks cast the borrowed LupineRenderContextHandle back to
 * the live lupine::RenderContext and forward to its high-level draw methods. The
 * debug_draw_* thunks forward to lupine::DebugDraw (a no-op outside the editor).
 * -------------------------------------------------------------------------- */

void LUPINE_EXT_CALL HostRenderDrawQuad(LupineRenderContextHandle ctx,
        float pos_x, float pos_y, float pos_z,
        float size_x, float size_y,
        float r, float g, float b, float a,
        int32_t blend_mode) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    rc->drawQuad(math::Vec3(pos_x, pos_y, pos_z), math::Vec2(size_x, size_y),
                 math::Color(r, g, b, a), TextureHandle(), blend_mode);
}

void LUPINE_EXT_CALL HostRenderDrawTexturedQuad(LupineRenderContextHandle ctx,
        float pos_x, float pos_y, float pos_z,
        float size_x, float size_y,
        float r, float g, float b, float a,
        uint32_t texture_id, int32_t blend_mode) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    rc->drawQuad(math::Vec3(pos_x, pos_y, pos_z), math::Vec2(size_x, size_y),
                 math::Color(r, g, b, a), TextureHandle(texture_id), blend_mode);
}

void LUPINE_EXT_CALL HostRenderDrawRect(LupineRenderContextHandle ctx,
        float pos_x, float pos_y,
        float size_x, float size_y,
        float r, float g, float b, float a,
        int32_t blend_mode) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    rc->drawRoundedRect(math::Vec2(pos_x, pos_y), math::Vec2(size_x, size_y),
                        0.0f, math::Color(r, g, b, a), blend_mode);
}

void LUPINE_EXT_CALL HostRenderDrawRoundedRect(LupineRenderContextHandle ctx,
        float pos_x, float pos_y,
        float size_x, float size_y,
        float corner_radius,
        float r, float g, float b, float a,
        int32_t blend_mode) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    rc->drawRoundedRect(math::Vec2(pos_x, pos_y), math::Vec2(size_x, size_y),
                        corner_radius, math::Color(r, g, b, a), blend_mode);
}

void LUPINE_EXT_CALL HostRenderDrawSprite(LupineRenderContextHandle ctx,
        uint32_t texture_id,
        float pos_x, float pos_y,
        float size_x, float size_y,
        float pivot_x, float pivot_y,
        float rotation,
        float r, float g, float b, float a,
        float uv_min_x, float uv_min_y,
        float uv_max_x, float uv_max_y,
        int32_t layer, int32_t blend_mode) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    SpriteDrawData sprite;
    sprite.texture = TextureHandle(texture_id);
    sprite.position = math::Vec2(pos_x, pos_y);
    sprite.size = math::Vec2(size_x, size_y);
    sprite.pivot = math::Vec2(pivot_x, pivot_y);
    sprite.rotation = rotation;
    sprite.tint = math::Color(r, g, b, a);
    sprite.uvMin = math::Vec2(uv_min_x, uv_min_y);
    sprite.uvMax = math::Vec2(uv_max_x, uv_max_y);
    sprite.layer = static_cast<int>(layer);
    sprite.blendMode = static_cast<int>(blend_mode);
    rc->drawSprite(sprite);
}

void LUPINE_EXT_CALL HostRenderDrawLine(LupineRenderContextHandle ctx,
        float start_x, float start_y, float start_z,
        float end_x, float end_y, float end_z,
        float r, float g, float b, float a,
        float thickness) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    rc->drawLine(math::Vec3(start_x, start_y, start_z), math::Vec3(end_x, end_y, end_z),
                 math::Color(r, g, b, a), thickness);
}

void LUPINE_EXT_CALL HostRenderDrawCircle(LupineRenderContextHandle ctx,
        float center_x, float center_y, float center_z,
        float radius,
        float r, float g, float b, float a,
        LupineBool filled) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    rc->drawCircle(math::Vec3(center_x, center_y, center_z), radius,
                   math::Color(r, g, b, a), filled != 0);
}

void LUPINE_EXT_CALL HostRenderDrawPolygon(LupineRenderContextHandle ctx,
        float center_x, float center_y,
        float radius,
        int32_t sides,
        float r, float g, float b, float a,
        float rotation, int32_t blend_mode) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    rc->drawPolygon(math::Vec2(center_x, center_y), radius, static_cast<int>(sides),
                    math::Color(r, g, b, a), rotation, blend_mode);
}

void LUPINE_EXT_CALL HostRenderDrawBox(LupineRenderContextHandle ctx,
        float center_x, float center_y, float center_z,
        float size_x, float size_y, float size_z,
        float r, float g, float b, float a,
        LupineBool wireframe) {
    RenderContext* rc = ToRenderContext(ctx);
    if (!rc) return;
    rc->drawBox(math::Vec3(center_x, center_y, center_z), math::Vec3(size_x, size_y, size_z),
                math::Color(r, g, b, a), wireframe != 0);
}

LupineBool LUPINE_EXT_CALL HostDebugDrawAvailable(void) {
    return DebugDraw::IsAvailable() ? 1 : 0;
}

void LUPINE_EXT_CALL HostDebugDrawLine(
        float start_x, float start_y, float start_z,
        float end_x, float end_y, float end_z,
        float r, float g, float b, float a,
        float duration, LupineBool depth_test) {
    DebugDraw::Line(math::Vec3(start_x, start_y, start_z), math::Vec3(end_x, end_y, end_z),
                    math::Color(r, g, b, a), duration, depth_test != 0);
}

void LUPINE_EXT_CALL HostDebugDrawBox(
        float center_x, float center_y, float center_z,
        float size_x, float size_y, float size_z,
        float r, float g, float b, float a,
        LupineBool wireframe, float duration, LupineBool depth_test) {
    DebugDraw::Box(math::Vec3(center_x, center_y, center_z), math::Vec3(size_x, size_y, size_z),
                   math::Color(r, g, b, a), wireframe != 0, duration, depth_test != 0);
}

void LUPINE_EXT_CALL HostDebugDrawSphere(
        float center_x, float center_y, float center_z,
        float radius,
        float r, float g, float b, float a,
        LupineBool wireframe, int32_t segments, float duration, LupineBool depth_test) {
    DebugDraw::Sphere(math::Vec3(center_x, center_y, center_z), radius,
                      math::Color(r, g, b, a), wireframe != 0, static_cast<int>(segments),
                      duration, depth_test != 0);
}

void LUPINE_EXT_CALL HostDebugDrawCircle(
        float center_x, float center_y, float center_z,
        float normal_x, float normal_y, float normal_z,
        float radius,
        float r, float g, float b, float a,
        int32_t segments, float duration, LupineBool depth_test) {
    DebugDraw::Circle(math::Vec3(center_x, center_y, center_z), math::Vec3(normal_x, normal_y, normal_z),
                      radius, math::Color(r, g, b, a), static_cast<int>(segments),
                      duration, depth_test != 0);
}

void LUPINE_EXT_CALL HostDebugDrawText(
        float pos_x, float pos_y, float pos_z,
        const char* text,
        float r, float g, float b, float a,
        float size, float duration) {
    DebugDraw::Text(math::Vec3(pos_x, pos_y, pos_z), text ? std::string(text) : std::string(),
                    math::Color(r, g, b, a), size, duration);
}

void LUPINE_EXT_CALL HostDebugDrawRect2D(
        float center_x, float center_y,
        float size_x, float size_y,
        float r, float g, float b, float a) {
    DebugDraw::OrientedBox2D(math::Vec2(center_x, center_y), math::Vec2(size_x, size_y), 0.0f,
                             math::Color(r, g, b, a));
}

/* The single shared host interface table, filled once. */
const LupineHostInterface& GetHostInterface() {
    static const LupineHostInterface s_interface = []() {
        LupineHostInterface i{};
        i.struct_size = static_cast<uint32_t>(sizeof(LupineHostInterface));
        i.abi_version = LUPINE_EXTENSION_ABI_VERSION;
        i.string_free = &HostStringFree;
        i.string_dup = &HostStringDup;
        i.log = &HostLog;
        i.register_component_class = &HostRegisterComponentClass;
        i.define_property = &HostDefineProperty;
        i.get_property_json = &HostGetPropertyJson;
        i.set_property_json = &HostSetPropertyJson;
        i.get_owner_node = &HostGetOwnerNode;
        i.node_get_name = &HostNodeGetName;
        i.node_get_path = &HostNodeGetPath;
        i.node_get_component_by_type = &HostNodeGetComponentByType;
        i.node_get_node = &HostNodeGetNode;
        i.node_get_property_json = &HostNodeGetPropertyJson;
        i.node_set_property_json = &HostNodeSetPropertyJson;
        i.component_get_type_name = &HostComponentGetTypeName;
        i.component_get_property_json = &HostComponentGetPropertyJson;
        i.component_set_property_json = &HostComponentSetPropertyJson;
        i.component_call_method_json = &HostComponentCallMethodJson;
        i.render_draw_quad = &HostRenderDrawQuad;
        i.render_draw_textured_quad = &HostRenderDrawTexturedQuad;
        i.render_draw_rect = &HostRenderDrawRect;
        i.render_draw_rounded_rect = &HostRenderDrawRoundedRect;
        i.render_draw_sprite = &HostRenderDrawSprite;
        i.render_draw_line = &HostRenderDrawLine;
        i.render_draw_circle = &HostRenderDrawCircle;
        i.render_draw_polygon = &HostRenderDrawPolygon;
        i.render_draw_box = &HostRenderDrawBox;
        i.debug_draw_available = &HostDebugDrawAvailable;
        i.debug_draw_line = &HostDebugDrawLine;
        i.debug_draw_box = &HostDebugDrawBox;
        i.debug_draw_sphere = &HostDebugDrawSphere;
        i.debug_draw_circle = &HostDebugDrawCircle;
        i.debug_draw_text = &HostDebugDrawText;
        i.debug_draw_rect_2d = &HostDebugDrawRect2D;
        return i;
    }();
    return s_interface;
}

/* ----------------------------------------------------------------------------
 * Platform dynamic-library helpers
 * -------------------------------------------------------------------------- */

void* OpenLibrary(const std::string& path, std::string& errorOut) {
#if defined(_WIN32) || defined(_WIN64)
    HMODULE handle = ::LoadLibraryA(path.c_str());
    if (!handle) {
        errorOut = "LoadLibrary failed (error " + std::to_string(::GetLastError()) + ")";
        return nullptr;
    }
    return reinterpret_cast<void*>(handle);
#else
    void* handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* err = ::dlerror();
        errorOut = err ? err : "dlopen failed";
        return nullptr;
    }
    return handle;
#endif
}

void* ResolveSymbol(void* handle, const char* name) {
#if defined(_WIN32) || defined(_WIN64)
    return reinterpret_cast<void*>(::GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
#else
    return ::dlsym(handle, name);
#endif
}

void CloseLibrary(void* handle) {
    if (!handle) return;
#if defined(_WIN32) || defined(_WIN64)
    ::FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    ::dlclose(handle);
#endif
}

/* Candidate "<platform>.<arch>" / "<platform>" manifest keys for this build. */
std::vector<std::string> PlatformBinaryKeys() {
#if defined(_WIN32) || defined(_WIN64)
    const char* plat = "windows";
#elif defined(__APPLE__)
    const char* plat = "macos";
#else
    const char* plat = "linux";
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    const char* arch = "arm64";
#else
    const char* arch = "x86_64";
#endif
    return { std::string(plat) + "." + arch, std::string(plat) };
}

} // namespace

/* ============================================================================
 * NativeExtensionRegistry
 * ============================================================================ */

NativeExtensionRegistry& NativeExtensionRegistry::GetInstance() {
    static NativeExtensionRegistry instance;
    return instance;
}

bool NativeExtensionRegistry::RegisterComponentClass(const LupineComponentClassDesc* desc) {
    if (!desc || !desc->class_name || desc->class_name[0] == '\0') {
        LOG_ERROR(LogCategory::Core, "NativeExtensionRegistry: null/empty class name");
        return false;
    }
    if (!desc->vtable.create || !desc->vtable.destroy || !desc->vtable.define_properties) {
        LOG_ERROR(LogCategory::Core,
            "NativeExtensionRegistry: class '{}' missing create/destroy/define_properties", desc->class_name);
        return false;
    }

    std::string name = desc->class_name;
    if (TypeRegistry::GetInstance().IsTypeRegistered(name)) {
        LOG_ERROR(LogCategory::Core,
            "NativeExtensionRegistry: type '{}' is already registered; skipping", name);
        return false;
    }

    auto cls = std::make_unique<NativeComponentClass>();
    cls->className = name;
    cls->baseClassName = (desc->base_class_name ? desc->base_class_name : "");
    cls->displayCategory = (desc->display_category ? desc->display_category : "");
    cls->hookFlags = desc->hook_flags;
    cls->vtable = desc->vtable;

    m_Classes[name] = std::move(cls);

    TypeRegistry::GetInstance().RegisterType(name,
        [name]() -> std::shared_ptr<ISerializable> {
            return std::make_shared<NativeComponentWrapper>(name);
        });

    LOG_INFO(LogCategory::Core,
        "NativeExtensionRegistry: registered native component class '{}'", name);
    return true;
}

const NativeComponentClass* NativeExtensionRegistry::GetClass(const std::string& className) const {
    auto it = m_Classes.find(className);
    return it != m_Classes.end() ? it->second.get() : nullptr;
}

bool NativeExtensionRegistry::IsNativeClass(const std::string& className) const {
    return m_Classes.find(className) != m_Classes.end();
}

std::vector<const NativeComponentClass*> NativeExtensionRegistry::GetAllClasses() const {
    std::vector<const NativeComponentClass*> out;
    out.reserve(m_Classes.size());
    for (const auto& kv : m_Classes) {
        out.push_back(kv.second.get());
    }
    return out;
}

/* ============================================================================
 * ExtensionManager
 * ============================================================================ */

ExtensionManager& ExtensionManager::GetInstance() {
    static ExtensionManager instance;
    return instance;
}

ExtensionManager::~ExtensionManager() {
    UnloadAll();
}

const LupineHostInterface* ExtensionManager::HostInterface() {
    return &GetHostInterface();
}

bool ExtensionManager::IsBinaryLoaded(const std::string& binaryPath) const {
    for (const LoadedExtension& ext : m_Loaded) {
        if (ext.ok && ext.binaryPath == binaryPath) return true;
    }
    return false;
}

const ExtensionManager::LoadedExtension& ExtensionManager::LoadExtensionBinary(
    const std::string& binaryPath, const std::string& manifestPath, const std::string& name) {

    m_Loaded.emplace_back();
    LoadedExtension& record = m_Loaded.back();
    record.binaryPath = binaryPath;
    record.manifestPath = manifestPath;
    record.name = name.empty() ? binaryPath : name;

    if (IsBinaryLoaded(binaryPath)) {
        // The just-pushed record is not yet ok; remove the duplicate marker by
        // pointing at the first successful load instead.
        m_Loaded.pop_back();
        for (LoadedExtension& ext : m_Loaded) {
            if (ext.ok && ext.binaryPath == binaryPath) return ext;
        }
        m_Loaded.emplace_back();
        LoadedExtension& again = m_Loaded.back();
        again.binaryPath = binaryPath;
        again.error = "duplicate load";
        return again;
    }

    std::string err;
    void* handle = OpenLibrary(binaryPath, err);
    if (!handle) {
        record.error = "failed to load '" + binaryPath + "': " + err;
        LOG_ERROR(LogCategory::Core, "ExtensionManager: {}", record.error);
        return record;
    }
    record.libraryHandle = handle;

    // ABI probe (optional symbol).
    auto abiFn = reinterpret_cast<LupineExtensionAbiVersionFn>(
        ResolveSymbol(handle, LUPINE_EXTENSION_ABI_VERSION_SYMBOL));
    if (abiFn) {
        uint32_t pluginAbi = abiFn();
        if (pluginAbi != LUPINE_EXTENSION_ABI_VERSION) {
            record.error = "ABI mismatch (plugin " + std::to_string(pluginAbi) +
                           " vs host " + std::to_string(LUPINE_EXTENSION_ABI_VERSION) + ")";
            LOG_ERROR(LogCategory::Core, "ExtensionManager: {} for '{}'", record.error, binaryPath);
            CloseLibrary(handle);
            record.libraryHandle = nullptr;
            return record;
        }
    }

    auto initFn = reinterpret_cast<LupineExtensionInitFn>(
        ResolveSymbol(handle, LUPINE_EXTENSION_INIT_SYMBOL));
    if (!initFn) {
        record.error = "missing entry symbol '" LUPINE_EXTENSION_INIT_SYMBOL "'";
        LOG_ERROR(LogCategory::Core, "ExtensionManager: {} in '{}'", record.error, binaryPath);
        CloseLibrary(handle);
        record.libraryHandle = nullptr;
        return record;
    }

    record.deinitFn = reinterpret_cast<LupineExtensionDeinitFn>(
        ResolveSymbol(handle, LUPINE_EXTENSION_DEINIT_SYMBOL));

    LupineExtensionHandle pluginState = nullptr;
    LupineBool ok = 0;
    try {
        ok = initFn(LUPINE_EXTENSION_ABI_VERSION, HostInterface(),
                    static_cast<LupineRegistrationContext>(&record), &pluginState);
    } catch (...) {
        ok = 0;
    }

    if (!ok) {
        record.error = "lupine_extension_init returned failure";
        LOG_ERROR(LogCategory::Core, "ExtensionManager: {} for '{}'", record.error, binaryPath);
        CloseLibrary(handle);
        record.libraryHandle = nullptr;
        return record;
    }

    record.pluginState = pluginState;
    record.ok = true;
    LOG_INFO(LogCategory::Core,
        "ExtensionManager: loaded extension '{}' ({} class(es)) from '{}'",
        record.name, record.registeredClasses.size(), binaryPath);
    return record;
}

int ExtensionManager::LoadExtensionsForProject(const std::string& projectDir) {
    namespace fs = std::filesystem;
    int loaded = 0;

    std::error_code ec;
    if (projectDir.empty() || !fs::exists(projectDir, ec)) {
        return 0;
    }

    std::vector<std::string> keys = PlatformBinaryKeys();

    for (fs::recursive_directory_iterator it(projectDir, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        if (it->path().extension() != ".lupineext") continue;

        std::string manifestPath = it->path().string();
        std::ifstream in(manifestPath, std::ios::binary);
        if (!in) {
            LOG_WARN(LogCategory::Core, "ExtensionManager: cannot read manifest '{}'", manifestPath);
            continue;
        }
        std::stringstream buffer;
        buffer << in.rdbuf();

        nlohmann::json manifest;
        try {
            manifest = nlohmann::json::parse(buffer.str());
        } catch (const std::exception& e) {
            LOG_WARN(LogCategory::Core, "ExtensionManager: invalid manifest '{}': {}", manifestPath, e.what());
            continue;
        }

        std::string name = manifest.value("name", it->path().stem().string());

        if (!manifest.contains("binaries") || !manifest["binaries"].is_object()) {
            LOG_WARN(LogCategory::Core, "ExtensionManager: manifest '{}' has no 'binaries' map", manifestPath);
            continue;
        }

        std::string relBinary;
        for (const std::string& key : keys) {
            if (manifest["binaries"].contains(key) && manifest["binaries"][key].is_string()) {
                relBinary = manifest["binaries"][key].get<std::string>();
                break;
            }
        }
        if (relBinary.empty()) {
            LOG_WARN(LogCategory::Core,
                "ExtensionManager: manifest '{}' has no binary for this platform", manifestPath);
            continue;
        }

        fs::path binaryPath = it->path().parent_path() / relBinary;
        binaryPath = binaryPath.lexically_normal();
        if (!fs::exists(binaryPath, ec)) {
            LOG_ERROR(LogCategory::Core,
                "ExtensionManager: binary '{}' from manifest '{}' does not exist",
                binaryPath.string(), manifestPath);
            continue;
        }

        const LoadedExtension& result = LoadExtensionBinary(binaryPath.string(), manifestPath, name);
        if (result.ok) loaded++;
    }

    return loaded;
}

void ExtensionManager::UnloadAll() {
    // Unload in reverse load order. Note: the engine TypeRegistry has no
    // unregistration, so already-registered class factories remain but their
    // backing library is closed here. Unloading is intended for shutdown only.
    for (auto it = m_Loaded.rbegin(); it != m_Loaded.rend(); ++it) {
        LoadedExtension& ext = *it;
        if (ext.ok && ext.deinitFn) {
            try { ext.deinitFn(ext.pluginState); } catch (...) {}
        }
        if (ext.libraryHandle) {
            CloseLibrary(ext.libraryHandle);
            ext.libraryHandle = nullptr;
        }
    }
    m_Loaded.clear();
}

} // namespace core
} // namespace lupine
