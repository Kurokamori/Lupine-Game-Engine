/**
 * @file Component.hpp
 * @brief Lupine native extension SDK — idiomatic C++ component base class.
 *
 * Plugin authors subclass lupine::ext::Component and override the lifecycle
 * hooks they need. The SDK wraps the stable C ABI
 * (lupine/lupine_extension_interface.h): every accessor here forwards to the
 * host interface the engine handed the plugin at load time, and the engine
 * drives your overrides through generated C thunks (see Extension.hpp).
 *
 * Only standard C++17 and nlohmann/json are required to build a plugin.
 */

#ifndef LUPINE_EXT_COMPONENT_HPP
#define LUPINE_EXT_COMPONENT_HPP

#include "lupine/lupine_extension_interface.h"

#include <nlohmann/json.hpp>
#include <string>

namespace lupine {
namespace ext {

namespace detail {
/**
 * Single shared slot holding the host interface pointer for this loaded plugin.
 * Set once by the generated lupine_extension_init (LUPINE_EXTENSION_MAIN).
 * C++17 inline => exactly one instance across all of the plugin's translation
 * units.
 */
inline const LupineHostInterface*& HostRef() {
    static const LupineHostInterface* s_host = nullptr;
    return s_host;
}
} // namespace detail

/** Property value types — integer values MUST match lupine::core::PropertyValueType. */
enum class PropertyType : int {
    Int = 0, Float = 1, String = 2, Bool = 3,
    Vec2 = 4, Vec3 = 5, Vec4 = 6, Color = 7,
    NodePath = 8, ScenePath = 9, Enum = 10, StringArray = 11,
    Double = 12, Quat = 13, Rect = 14, Resource = 15,
    IntArray = 16, FloatArray = 17, Array = 18, Dictionary = 19
};

/** Property editor hints — integer values MUST match lupine::core::PropertyHintType. */
enum class PropertyHintType : int {
    None = 0, Range = 1, Enum = 2, File = 3, MultilineText = 4,
    ExpRange = 5, Length = 6, ColorNoAlpha = 7, Dir = 8,
    Layers2D = 9, Layers3D = 10
};

/* ============================================================================
 * Lightweight handle wrappers for scene-tree access
 * ============================================================================ */

/** A borrowed reference to another component (e.g. a sibling). Valid only for
 *  the duration of the current hook call. */
class ComponentRef {
public:
    ComponentRef() = default;
    explicit ComponentRef(LupineComponentHandle handle) : m_handle(handle) {}

    bool IsValid() const { return m_handle != nullptr; }
    LupineComponentHandle Raw() const { return m_handle; }

    std::string GetTypeName() const {
        return TakeString(Host()->component_get_type_name(m_handle));
    }

    nlohmann::json GetProperty(const std::string& name) const {
        return ParseOwned(Host()->component_get_property_json(m_handle, name.c_str()));
    }
    bool SetProperty(const std::string& name, const nlohmann::json& value) const {
        const std::string s = value.dump();
        return Host()->component_set_property_json(m_handle, name.c_str(), s.c_str()) != 0;
    }
    nlohmann::json Call(const std::string& method, const nlohmann::json& args = nlohmann::json::array()) const {
        const std::string a = args.dump();
        return ParseOwned(Host()->component_call_method_json(m_handle, method.c_str(), a.c_str()));
    }

private:
    static const LupineHostInterface* Host() { return detail::HostRef(); }
    static std::string TakeString(char* s) {
        if (!s) return std::string();
        std::string out(s);
        Host()->string_free(s);
        return out;
    }
    static nlohmann::json ParseOwned(char* s) {
        if (!s) return nlohmann::json();
        nlohmann::json j;
        try { j = nlohmann::json::parse(s); } catch (...) {}
        Host()->string_free(s);
        return j;
    }
    LupineComponentHandle m_handle = nullptr;
};

/** A borrowed reference to a scene-tree node. Valid only for the duration of the
 *  current hook call. */
class NodeRef {
public:
    NodeRef() = default;
    explicit NodeRef(LupineNodeHandle handle) : m_handle(handle) {}

    bool IsValid() const { return m_handle != nullptr; }
    LupineNodeHandle Raw() const { return m_handle; }

    std::string GetName() const { return TakeString(Host()->node_get_name(m_handle)); }
    std::string GetPath() const { return TakeString(Host()->node_get_path(m_handle)); }

    ComponentRef GetComponent(const std::string& typeName) const {
        return ComponentRef(Host()->node_get_component_by_type(m_handle, typeName.c_str()));
    }
    NodeRef GetNode(const std::string& path) const {
        return NodeRef(Host()->node_get_node(m_handle, path.c_str()));
    }
    nlohmann::json GetProperty(const std::string& name) const {
        return ParseOwned(Host()->node_get_property_json(m_handle, name.c_str()));
    }
    bool SetProperty(const std::string& name, const nlohmann::json& value) const {
        const std::string s = value.dump();
        return Host()->node_set_property_json(m_handle, name.c_str(), s.c_str()) != 0;
    }

private:
    static const LupineHostInterface* Host() { return detail::HostRef(); }
    static std::string TakeString(char* s) {
        if (!s) return std::string();
        std::string out(s);
        Host()->string_free(s);
        return out;
    }
    static nlohmann::json ParseOwned(char* s) {
        if (!s) return nlohmann::json();
        nlohmann::json j;
        try { j = nlohmann::json::parse(s); } catch (...) {}
        Host()->string_free(s);
        return j;
    }
    LupineNodeHandle m_handle = nullptr;
};

/* ============================================================================
 * Rendering bridge (ABI v2)
 * ============================================================================ */

/**
 * Idiomatic wrapper over the host render context handed to BuildDrawCommands.
 * Each method records one runtime-visible primitive by forwarding to the host's
 * render_draw_* functions. Vectors are explicit floats and colours are r,g,b,a,
 * mirroring the C ABI exactly. A RenderContext is valid only for the duration of
 * the BuildDrawCommands call that produced it — never store it.
 *
 * Blend modes match the engine: 0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay.
 */
class RenderContext {
public:
    explicit RenderContext(LupineRenderContextHandle handle) : m_handle(handle) {}

    LupineRenderContextHandle Raw() const { return m_handle; }

    /** Filled 2D/3D quad (untextured). */
    void DrawQuad(float posX, float posY, float posZ, float sizeX, float sizeY,
                  float r, float g, float b, float a, int blendMode = 0) const {
        Host()->render_draw_quad(m_handle, posX, posY, posZ, sizeX, sizeY, r, g, b, a, blendMode);
    }

    /** Filled quad sampling a host texture handle. */
    void DrawTexturedQuad(float posX, float posY, float posZ, float sizeX, float sizeY,
                          float r, float g, float b, float a,
                          uint32_t textureId, int blendMode = 0) const {
        Host()->render_draw_textured_quad(m_handle, posX, posY, posZ, sizeX, sizeY,
                                          r, g, b, a, textureId, blendMode);
    }

    /** Axis-aligned 2D rectangle (top-left origin). */
    void DrawRect(float posX, float posY, float sizeX, float sizeY,
                  float r, float g, float b, float a, int blendMode = 0) const {
        Host()->render_draw_rect(m_handle, posX, posY, sizeX, sizeY, r, g, b, a, blendMode);
    }

    /** 2D rounded rectangle (top-left origin) with a uniform corner radius. */
    void DrawRoundedRect(float posX, float posY, float sizeX, float sizeY, float cornerRadius,
                         float r, float g, float b, float a, int blendMode = 0) const {
        Host()->render_draw_rounded_rect(m_handle, posX, posY, sizeX, sizeY, cornerRadius,
                                         r, g, b, a, blendMode);
    }

    /** 2D sprite with pivot, rotation, tint, UV sub-rect, layer and blend mode. */
    void DrawSprite(uint32_t textureId, float posX, float posY, float sizeX, float sizeY,
                    float pivotX, float pivotY, float rotation,
                    float r, float g, float b, float a,
                    float uvMinX, float uvMinY, float uvMaxX, float uvMaxY,
                    int layer = 0, int blendMode = 0) const {
        Host()->render_draw_sprite(m_handle, textureId, posX, posY, sizeX, sizeY,
                                   pivotX, pivotY, rotation, r, g, b, a,
                                   uvMinX, uvMinY, uvMaxX, uvMaxY, layer, blendMode);
    }

    /** Line segment between two points with a pixel thickness. */
    void DrawLine(float startX, float startY, float startZ,
                  float endX, float endY, float endZ,
                  float r, float g, float b, float a, float thickness = 1.0f) const {
        Host()->render_draw_line(m_handle, startX, startY, startZ, endX, endY, endZ,
                                 r, g, b, a, thickness);
    }

    /** Filled or outlined circle centred at a world point. */
    void DrawCircle(float centerX, float centerY, float centerZ, float radius,
                    float r, float g, float b, float a, bool filled = true) const {
        Host()->render_draw_circle(m_handle, centerX, centerY, centerZ, radius,
                                   r, g, b, a, filled ? 1 : 0);
    }

    /** Regular SDF polygon (sides>=3) centred at a 2D point, optionally rotated. */
    void DrawPolygon(float centerX, float centerY, float radius, int sides,
                     float r, float g, float b, float a, float rotation = 0.0f, int blendMode = 0) const {
        Host()->render_draw_polygon(m_handle, centerX, centerY, radius, sides,
                                    r, g, b, a, rotation, blendMode);
    }

    /** Wireframe or filled axis-aligned box centred at a world point. */
    void DrawBox(float centerX, float centerY, float centerZ,
                 float sizeX, float sizeY, float sizeZ,
                 float r, float g, float b, float a, bool wireframe = false) const {
        Host()->render_draw_box(m_handle, centerX, centerY, centerZ, sizeX, sizeY, sizeZ,
                                r, g, b, a, wireframe ? 1 : 0);
    }

private:
    static const LupineHostInterface* Host() { return detail::HostRef(); }
    LupineRenderContextHandle m_handle = nullptr;
};

/**
 * Editor-only debug drawing. Every call is a no-op in a runtime build (gate with
 * Available() to skip the work entirely). Call these from OnRender, not from
 * BuildDrawCommands. Colours are r,g,b,a; duration 0 draws for one frame.
 */
class DebugDraw {
public:
    static bool Available() {
        return Host()->debug_draw_available && Host()->debug_draw_available() != 0;
    }

    static void Line(float startX, float startY, float startZ,
                     float endX, float endY, float endZ,
                     float r, float g, float b, float a,
                     float duration = 0.0f, bool depthTest = true) {
        Host()->debug_draw_line(startX, startY, startZ, endX, endY, endZ,
                                r, g, b, a, duration, depthTest ? 1 : 0);
    }

    static void Box(float centerX, float centerY, float centerZ,
                    float sizeX, float sizeY, float sizeZ,
                    float r, float g, float b, float a,
                    bool wireframe = true, float duration = 0.0f, bool depthTest = true) {
        Host()->debug_draw_box(centerX, centerY, centerZ, sizeX, sizeY, sizeZ,
                               r, g, b, a, wireframe ? 1 : 0, duration, depthTest ? 1 : 0);
    }

    static void Sphere(float centerX, float centerY, float centerZ, float radius,
                       float r, float g, float b, float a,
                       bool wireframe = true, int segments = 16,
                       float duration = 0.0f, bool depthTest = true) {
        Host()->debug_draw_sphere(centerX, centerY, centerZ, radius,
                                  r, g, b, a, wireframe ? 1 : 0, segments, duration, depthTest ? 1 : 0);
    }

    static void Circle(float centerX, float centerY, float centerZ,
                       float normalX, float normalY, float normalZ, float radius,
                       float r, float g, float b, float a,
                       int segments = 32, float duration = 0.0f, bool depthTest = true) {
        Host()->debug_draw_circle(centerX, centerY, centerZ, normalX, normalY, normalZ, radius,
                                  r, g, b, a, segments, duration, depthTest ? 1 : 0);
    }

    static void Text(float posX, float posY, float posZ, const std::string& text,
                     float r, float g, float b, float a, float size = 1.0f, float duration = 0.0f) {
        Host()->debug_draw_text(posX, posY, posZ, text.c_str(), r, g, b, a, size, duration);
    }

private:
    static const LupineHostInterface* Host() { return detail::HostRef(); }
};

/* ============================================================================
 * Component base class
 * ============================================================================ */

class Component {
public:
    virtual ~Component() = default;

    /* ---- Override these in your subclass ---- */

    /** Declare exported, editable, serialized properties here using Export*(). */
    virtual void DefineProperties() {}

    virtual void OnAwake() {}
    virtual void OnReady() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(float deltaTime) { (void)deltaTime; }
    virtual void OnProcess(float deltaTime) { (void)deltaTime; }
    virtual void OnPhysicsProcess(float deltaTime) { (void)deltaTime; }
    virtual void OnInput(float deltaTime) { (void)deltaTime; }
    virtual void OnRender() {}
    virtual void OnLateUpdate(float deltaTime) { (void)deltaTime; }
    virtual void OnEnterTree() {}
    virtual void OnExitTree() {}

    /** A discrete input event arrived; payload is the InputEvent JSON. */
    virtual void OnInputEvent(const nlohmann::json& event) { (void)event; }
    /** An exported property changed (from the editor or a script). */
    virtual void OnPropertyChanged(const std::string& name, const nlohmann::json& value) { (void)name; (void)value; }
    /** Generic control-method dispatch. Return a JSON value, or null for none. */
    virtual nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) {
        (void)method; (void)args; return nlohmann::json();
    }

    /**
     * Record runtime-visible draw geometry through `ctx`. Called during the render
     * gather stage when the class registers with LUPINE_HOOK_DRAW (see
     * LUPINE_REGISTER_COMPONENT_DRAWABLE). Editor-only debug visualization belongs
     * in OnRender via the DebugDraw helpers instead.
     */
    virtual void BuildDrawCommands(RenderContext& ctx) { (void)ctx; }

    /**
     * Report this component's world-space axis-aligned bounding box for culling
     * and editor selection. Write the minimum corner to outMin[3] and the maximum
     * corner to outMax[3]. Return true if bounds were written; return false to let
     * the host derive a bounding box from the owning node's transform.
     */
    virtual bool GetRenderBounds(float* outMin, float* outMax) {
        (void)outMin; (void)outMax; return false;
    }

    /** INTERNAL: bound by the SDK thunks before each call. Do not call yourself. */
    void _BindHandle(LupineComponentHandle self) { m_self = self; }

protected:
    /* ---- Scene-tree access ---- */
    NodeRef GetOwnerNode() const { return NodeRef(Host()->get_owner_node(m_self)); }
    LupineComponentHandle Self() const { return m_self; }

    /* ---- This component's own exported property values ---- */
    nlohmann::json GetProperty(const std::string& name) const {
        char* s = Host()->get_property_json(m_self, name.c_str());
        if (!s) return nlohmann::json();
        nlohmann::json j;
        try { j = nlohmann::json::parse(s); } catch (...) {}
        Host()->string_free(s);
        return j;
    }
    void SetProperty(const std::string& name, const nlohmann::json& value) const {
        const std::string s = value.dump();
        Host()->set_property_json(m_self, name.c_str(), s.c_str());
    }

    /* Typed convenience getters/setters. */
    float GetFloat(const std::string& name, float fallback = 0.0f) const {
        const nlohmann::json j = GetProperty(name);
        return j.is_number() ? j.get<float>() : fallback;
    }
    int GetInt(const std::string& name, int fallback = 0) const {
        const nlohmann::json j = GetProperty(name);
        return j.is_number_integer() ? j.get<int>() : (j.is_number() ? static_cast<int>(j.get<double>()) : fallback);
    }
    bool GetBool(const std::string& name, bool fallback = false) const {
        const nlohmann::json j = GetProperty(name);
        return j.is_boolean() ? j.get<bool>() : fallback;
    }
    std::string GetString(const std::string& name, const std::string& fallback = std::string()) const {
        const nlohmann::json j = GetProperty(name);
        return j.is_string() ? j.get<std::string>() : fallback;
    }
    void SetFloat(const std::string& name, float v) const { SetProperty(name, v); }
    void SetInt(const std::string& name, int v) const { SetProperty(name, v); }
    void SetBool(const std::string& name, bool v) const { SetProperty(name, v); }
    void SetString(const std::string& name, const std::string& v) const { SetProperty(name, v); }

    /* ---- Logging ---- */
    void LogInfo(const std::string& msg) const { Host()->log(LUPINE_LOG_INFO, "Extension", msg.c_str()); }
    void LogWarn(const std::string& msg) const { Host()->log(LUPINE_LOG_WARN, "Extension", msg.c_str()); }
    void LogError(const std::string& msg) const { Host()->log(LUPINE_LOG_ERROR, "Extension", msg.c_str()); }

    /* ---- Property declaration helpers (call from DefineProperties) ---- */
    void ExportFloat(const std::string& name, float def, const std::string& group = "") {
        EmitDescriptor(name, PropertyType::Float, nlohmann::json(def), group);
    }
    void ExportFloatRange(const std::string& name, float def, float min, float max, float step,
                          const std::string& group = "") {
        EmitDescriptor(name, PropertyType::Float, nlohmann::json(def), group,
                       PropertyHintType::Range, RangeHint(min, max, step));
    }
    void ExportInt(const std::string& name, int def, const std::string& group = "") {
        EmitDescriptor(name, PropertyType::Int, nlohmann::json(def), group);
    }
    void ExportIntRange(const std::string& name, int def, int min, int max, int step,
                        const std::string& group = "") {
        EmitDescriptor(name, PropertyType::Int, nlohmann::json(def), group,
                       PropertyHintType::Range, RangeHint(min, max, step));
    }
    void ExportBool(const std::string& name, bool def, const std::string& group = "") {
        EmitDescriptor(name, PropertyType::Bool, nlohmann::json(def), group);
    }
    void ExportString(const std::string& name, const std::string& def, const std::string& group = "") {
        EmitDescriptor(name, PropertyType::String, nlohmann::json(def), group);
    }
    void ExportFile(const std::string& name, const std::string& def, const std::string& filter,
                    const std::string& group = "") {
        EmitDescriptor(name, PropertyType::String, nlohmann::json(def), group,
                       PropertyHintType::File, filter);
    }
    void ExportColor(const std::string& name, float r, float g, float b, float a = 1.0f,
                     const std::string& group = "") {
        nlohmann::json def = { {"r", r}, {"g", g}, {"b", b}, {"a", a} };
        EmitDescriptor(name, PropertyType::Color, def, group);
    }
    void ExportVec2(const std::string& name, float x, float y, const std::string& group = "") {
        nlohmann::json def = { {"x", x}, {"y", y} };
        EmitDescriptor(name, PropertyType::Vec2, def, group);
    }
    void ExportVec3(const std::string& name, float x, float y, float z, const std::string& group = "") {
        nlohmann::json def = { {"x", x}, {"y", y}, {"z", z} };
        EmitDescriptor(name, PropertyType::Vec3, def, group);
    }
    /** Enum property. `def` is the default index; `options` is a comma-separated list. */
    void ExportEnum(const std::string& name, int def, const std::string& options,
                    const std::string& group = "") {
        EmitDescriptor(name, PropertyType::Enum, nlohmann::json(def), group,
                       PropertyHintType::Enum, options);
    }

    static const LupineHostInterface* Host() { return detail::HostRef(); }

private:
    static std::string RangeHint(float min, float max, float step) {
        return std::to_string(min) + "," + std::to_string(max) + "," + std::to_string(step);
    }
    static std::string RangeHint(int min, int max, int step) {
        return std::to_string(min) + "," + std::to_string(max) + "," + std::to_string(step);
    }

    void EmitDescriptor(const std::string& name, PropertyType type, const nlohmann::json& def,
                        const std::string& group,
                        PropertyHintType hintType = PropertyHintType::None,
                        const std::string& hintString = "") {
        nlohmann::json d;
        d["name"] = name;
        d["type"] = static_cast<int>(type);
        d["default"] = def;
        d["hint"] = { {"type", static_cast<int>(hintType)}, {"hint_string", hintString} };
        d["description"] = "";
        d["group"] = group;
        const std::string dumped = d.dump();
        Host()->define_property(m_self, dumped.c_str());
    }

    LupineComponentHandle m_self = nullptr;
};

} // namespace ext
} // namespace lupine

#endif // LUPINE_EXT_COMPONENT_HPP
