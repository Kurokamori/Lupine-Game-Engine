// OrbitMover — a complete example Lupine native (C++ DLL) component.
//
// It demonstrates the full author surface of the LupineExtension SDK:
//   - subclassing lupine::ext::Component,
//   - declaring editable + serialized properties in DefineProperties(),
//   - lifecycle hooks (OnReady / OnUpdate / OnDestroy),
//   - reading the component's own property values,
//   - driving the owning scene-tree node through the host interface,
//   - exposing a control method via CallMethod,
//   - registering the class and emitting the extension entry points.
//
// Build it with the lupine_add_extension() CMake helper (see CMakeLists.txt);
// the result is hello_component.dll / .so / .dylib, loaded by the engine when a
// hello_component.lupineext manifest sits in the opened project.

#include <lupine/ext/Extension.hpp>

#include <cmath>
#include <string>

class OrbitMover : public lupine::ext::Component {
public:
    void DefineProperties() override {
        // Each Export* call declares a property the editor inspector can edit and
        // that the scene file serializes. The values live in the engine; this
        // component reads them back with GetFloat/GetBool.
        ExportFloatRange("angular_speed", 1.5f, -10.0f, 10.0f, 0.05f, "Orbit");
        ExportFloatRange("radius", 96.0f, 0.0f, 2048.0f, 1.0f, "Orbit");
        ExportBool("log_each_second", true, "Debug");
        ExportColor("tint", 1.0f, 0.6f, 0.2f, 1.0f, "Debug");
    }

    void OnReady() override {
        m_angle = 0.0f;
        m_logAccumulator = 0.0f;
        lupine::ext::NodeRef owner = GetOwnerNode();
        m_ownerName = owner.IsValid() ? owner.GetName() : std::string("<detached>");
        LogInfo("OrbitMover ready on node '" + m_ownerName + "'");
    }

    void OnUpdate(float deltaTime) override {
        const float speed = GetFloat("angular_speed");
        const float radius = GetFloat("radius");
        m_angle += speed * deltaTime;

        // Drive the owning node's 2D position in a circle. "position" is a
        // node-level Vec2 property on Node2D-derived nodes; SetProperty routes the
        // value through the host's generic property setter.
        lupine::ext::NodeRef owner = GetOwnerNode();
        if (owner.IsValid()) {
            owner.SetProperty("position", {
                {"x", radius * std::cos(m_angle)},
                {"y", radius * std::sin(m_angle)}
            });
        }

        if (GetBool("log_each_second")) {
            m_logAccumulator += deltaTime;
            if (m_logAccumulator >= 1.0f) {
                m_logAccumulator = 0.0f;
                LogInfo("OrbitMover '" + m_ownerName + "' angle = " + std::to_string(m_angle));
            }
        }
    }

    void OnDestroy() override {
        LogInfo("OrbitMover destroyed on node '" + m_ownerName + "'");
    }

    // Reachable from scripts (Lua/mRuby/Python) via ComponentRef:call("get_angle").
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override {
        (void)args;
        if (method == "get_angle") {
            return m_angle;
        }
        if (method == "reset") {
            m_angle = 0.0f;
            return true;
        }
        return nlohmann::json();
    }

private:
    float m_angle = 0.0f;
    float m_logAccumulator = 0.0f;
    std::string m_ownerName;
};

// Register the class (type name "OrbitMover", base Component, editor category
// "Native/Examples"), then emit the required exported entry points exactly once.
LUPINE_REGISTER_COMPONENT_EX(OrbitMover, "OrbitMover", "", "Native/Examples", LUPINE_HOOKS_ALL)
LUPINE_EXTENSION_MAIN()
