// Standalone ABI conformance harness for the Lupine native extension system.
//
// It implements a minimal host (the engine's role) entirely in this file, loads
// a built extension binary, and drives one component instance through the full
// lifecycle across the C ABI — without linking the engine. This validates the
// ABI contract and the SDK end to end. Build/run instructions are at the bottom.
//
// It is NOT part of the engine build; it is a developer test for the extension
// system itself.

#include <lupine/lupine_extension_interface.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    static void* OpenLib(const char* p) { return (void*)LoadLibraryA(p); }
    static void* Sym(void* h, const char* n) { return (void*)GetProcAddress((HMODULE)h, n); }
    static void CloseLib(void* h) { FreeLibrary((HMODULE)h); }
#else
    #include <dlfcn.h>
    static void* OpenLib(const char* p) { return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
    static void* Sym(void* h, const char* n) { return dlsym(h, n); }
    static void CloseLib(void* h) { dlclose(h); }
#endif

// ---- Minimal host-side objects ----
struct MockComponent {
    std::map<std::string, nlohmann::json> properties; // name -> current value
    std::vector<std::string> declaredOrder;
};
struct MockNode {
    std::string name = "Player";
    std::map<std::string, nlohmann::json> properties;
};

static MockNode g_node;
static MockComponent g_component;

// Captured registration.
static std::string g_registeredClass;
static std::string g_registeredBase;
static std::string g_registeredCategory;
static const LupineComponentVTable* g_vtable = nullptr;
static LupineComponentVTable g_vtableStorage;

static int g_failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++g_failures; } \
                              else { std::printf("  ok:   %s\n", msg); } } while (0)

// ---- Host interface thunks ----
static char* HDup(const std::string& s) {
    char* o = (char*)std::malloc(s.size() + 1);
    std::memcpy(o, s.c_str(), s.size() + 1);
    return o;
}
static void  LUPINE_EXT_CALL h_string_free(char* s) { std::free(s); }
static char* LUPINE_EXT_CALL h_string_dup(const char* s) { return s ? HDup(s) : nullptr; }
static void  LUPINE_EXT_CALL h_log(int32_t lvl, const char* cat, const char* msg) {
    std::printf("    [plugin log lvl=%d %s] %s\n", lvl, cat ? cat : "", msg ? msg : "");
}
static LupineBool LUPINE_EXT_CALL h_register(LupineRegistrationContext, const LupineComponentClassDesc* d) {
    g_registeredClass = d->class_name ? d->class_name : "";
    g_registeredBase = d->base_class_name ? d->base_class_name : "";
    g_registeredCategory = d->display_category ? d->display_category : "";
    g_vtableStorage = d->vtable;
    g_vtable = &g_vtableStorage;
    return 1;
}
static void LUPINE_EXT_CALL h_define_property(LupineComponentHandle self, const char* json) {
    auto* c = reinterpret_cast<MockComponent*>(self);
    nlohmann::json d = nlohmann::json::parse(json);
    const std::string name = d.value("name", "");
    c->properties[name] = d.value("default", nlohmann::json());
    c->declaredOrder.push_back(name);
}
static char* LUPINE_EXT_CALL h_get_prop(LupineComponentHandle self, const char* name) {
    auto* c = reinterpret_cast<MockComponent*>(self);
    auto it = c->properties.find(name);
    if (it == c->properties.end()) return nullptr;
    return HDup(it->second.dump());
}
static LupineBool LUPINE_EXT_CALL h_set_prop(LupineComponentHandle self, const char* name, const char* val) {
    auto* c = reinterpret_cast<MockComponent*>(self);
    c->properties[name] = nlohmann::json::parse(val);
    return 1;
}
static LupineNodeHandle LUPINE_EXT_CALL h_owner(LupineComponentHandle) {
    return reinterpret_cast<LupineNodeHandle>(&g_node);
}
static char* LUPINE_EXT_CALL h_node_name(LupineNodeHandle n) {
    return HDup(reinterpret_cast<MockNode*>(n)->name);
}
static char* LUPINE_EXT_CALL h_node_path(LupineNodeHandle n) {
    return HDup("/root/" + reinterpret_cast<MockNode*>(n)->name);
}
static LupineComponentHandle LUPINE_EXT_CALL h_node_comp(LupineNodeHandle, const char*) { return nullptr; }
static LupineNodeHandle LUPINE_EXT_CALL h_node_node(LupineNodeHandle n, const char*) { return n; }
static char* LUPINE_EXT_CALL h_node_get_prop(LupineNodeHandle n, const char* name) {
    auto* nd = reinterpret_cast<MockNode*>(n);
    auto it = nd->properties.find(name);
    return it == nd->properties.end() ? nullptr : HDup(it->second.dump());
}
static LupineBool LUPINE_EXT_CALL h_node_set_prop(LupineNodeHandle n, const char* name, const char* val) {
    reinterpret_cast<MockNode*>(n)->properties[name] = nlohmann::json::parse(val);
    return 1;
}
static char* LUPINE_EXT_CALL h_comp_type(LupineComponentHandle) { return HDup("MockComponent"); }
static char* LUPINE_EXT_CALL h_comp_get(LupineComponentHandle s, const char* n) { return h_get_prop(s, n); }
static LupineBool LUPINE_EXT_CALL h_comp_set(LupineComponentHandle s, const char* n, const char* v) { return h_set_prop(s, n, v); }
static char* LUPINE_EXT_CALL h_comp_call(LupineComponentHandle, const char*, const char*) { return nullptr; }

static LupineHostInterface MakeHost() {
    LupineHostInterface h{};
    h.struct_size = sizeof(LupineHostInterface);
    h.abi_version = LUPINE_EXTENSION_ABI_VERSION;
    h.string_free = h_string_free;
    h.string_dup = h_string_dup;
    h.log = h_log;
    h.register_component_class = h_register;
    h.define_property = h_define_property;
    h.get_property_json = h_get_prop;
    h.set_property_json = h_set_prop;
    h.get_owner_node = h_owner;
    h.node_get_name = h_node_name;
    h.node_get_path = h_node_path;
    h.node_get_component_by_type = h_node_comp;
    h.node_get_node = h_node_node;
    h.node_get_property_json = h_node_get_prop;
    h.node_set_property_json = h_node_set_prop;
    h.component_get_type_name = h_comp_type;
    h.component_get_property_json = h_comp_get;
    h.component_set_property_json = h_comp_set;
    h.component_call_method_json = h_comp_call;
    return h;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: mock_host <extension-binary>\n"); return 2; }

    void* lib = OpenLib(argv[1]);
    if (!lib) { std::printf("FAIL: cannot load '%s'\n", argv[1]); return 1; }

    auto abiFn = reinterpret_cast<LupineExtensionAbiVersionFn>(Sym(lib, LUPINE_EXTENSION_ABI_VERSION_SYMBOL));
    auto initFn = reinterpret_cast<LupineExtensionInitFn>(Sym(lib, LUPINE_EXTENSION_INIT_SYMBOL));
    auto deinitFn = reinterpret_cast<LupineExtensionDeinitFn>(Sym(lib, LUPINE_EXTENSION_DEINIT_SYMBOL));

    std::printf("Loading extension: %s\n", argv[1]);
    CHECK(abiFn != nullptr, "exports lupine_extension_abi_version");
    CHECK(initFn != nullptr, "exports lupine_extension_init");
    CHECK(deinitFn != nullptr, "exports lupine_extension_deinit");
    if (!initFn) { CloseLib(lib); return 1; }
    CHECK(abiFn && abiFn() == LUPINE_EXTENSION_ABI_VERSION, "ABI version matches host");

    LupineHostInterface host = MakeHost();
    LupineExtensionHandle state = nullptr;
    LupineBool ok = initFn(LUPINE_EXTENSION_ABI_VERSION, &host,
                           reinterpret_cast<LupineRegistrationContext>(0x1), &state);
    CHECK(ok != 0, "lupine_extension_init returned success");
    CHECK(g_registeredClass == "OrbitMover", "registered class name 'OrbitMover'");
    CHECK(g_registeredCategory == "Native/Examples", "registered category 'Native/Examples'");
    CHECK(g_vtable && g_vtable->create && g_vtable->define_properties, "vtable has create + define_properties");
    if (!g_vtable) { CloseLib(lib); return 1; }

    // Create one instance; the host handle is our MockComponent.
    LupineComponentHandle self = reinterpret_cast<LupineComponentHandle>(&g_component);
    LupineInstance inst = g_vtable->create(self);
    CHECK(inst != nullptr, "create returned a plugin instance");

    g_vtable->define_properties(inst, self);
    CHECK(g_component.properties.count("angular_speed") == 1, "declared property 'angular_speed'");
    CHECK(g_component.properties.count("radius") == 1, "declared property 'radius'");
    CHECK(g_component.properties.count("log_each_second") == 1, "declared property 'log_each_second'");
    CHECK(std::fabs(g_component.properties["angular_speed"].get<float>() - 1.5f) < 1e-6, "angular_speed default 1.5");
    CHECK(std::fabs(g_component.properties["radius"].get<float>() - 96.0f) < 1e-6, "radius default 96");

    // Disable per-second logging noise for a clean run, force a known radius.
    g_component.properties["log_each_second"] = false;
    g_component.properties["radius"] = 100.0f;
    g_component.properties["angular_speed"] = 2.0f;

    if (g_vtable->on_ready) g_vtable->on_ready(inst, self);

    // Step a quarter turn: with speed 2.0, angle reaches ~pi/2 after dt summing to ~0.785.
    const float dt = 0.05f;
    for (int i = 0; i < 16; ++i) g_vtable->on_update(inst, self, dt);
    CHECK(g_node.properties.count("position") == 1, "owner node 'position' was driven");
    if (g_node.properties.count("position")) {
        float x = g_node.properties["position"]["x"].get<float>();
        float y = g_node.properties["position"]["y"].get<float>();
        float r = std::sqrt(x * x + y * y);
        std::printf("    position = (%.2f, %.2f), |r| = %.2f\n", x, y, r);
        CHECK(std::fabs(r - 100.0f) < 0.5f, "position lies on the radius-100 circle");
    }

    // call_method("get_angle") should return a number ~1.6 (2.0 * 0.05 * 16).
    if (g_vtable->call_method) {
        char* res = g_vtable->call_method(inst, self, "get_angle", "[]");
        CHECK(res != nullptr, "call_method('get_angle') returned a value");
        if (res) {
            float angle = nlohmann::json::parse(res).get<float>();
            std::printf("    get_angle -> %.3f\n", angle);
            CHECK(std::fabs(angle - 1.6f) < 1e-3, "get_angle ~= 1.6");
            host.string_free(res);
        }
    }

    if (g_vtable->on_destroy) g_vtable->on_destroy(inst, self);
    g_vtable->destroy(inst);
    if (deinitFn) deinitFn(state);
    CloseLib(lib);

    std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}

// Build & run (from repo root), after building hello_component.dll:
//   clang++ -std=c++17 -I extension/include -I external/json/include \
//       extension/examples/hello_component/test/mock_host.cpp -o mock_host
//   ./mock_host <path-to>/hello_component.dll
