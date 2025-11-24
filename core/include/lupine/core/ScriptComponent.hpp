#pragma once

#include "Component.hpp"
#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/scripting/PythonEnvironment.hpp"
#include "lupine/scripting/LuaEnvironment.hpp"
#ifdef LUPINE_HAS_MRUBY
#include "lupine/scripting/MRubyEnvironment.hpp"
#endif
#include <memory>
#include <string>
#include <map>

namespace lupine {
namespace core {

/**
 * Base class for script components
 * Provides common functionality for Python and Lua script components
 */
class ScriptComponent : public Component {
public:
    ScriptComponent();
    explicit ScriptComponent(const std::string& name);
    virtual ~ScriptComponent();

    // ISerializable interface
    std::string GetTypeName() const override { return "ScriptComponent"; }
    void RegisterProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // Script management
    const std::string& GetScriptPath() const { return m_ScriptPath; }
    void SetScriptPath(const std::string& path);
    
    bool LoadScript();
    bool ReloadScript();
    
    virtual scripting::ScriptLanguage GetLanguage() const = 0;
    
    // Get the script's display name (used in editor)
    std::string GetScriptDisplayName() const;
    
    // Export properties (parsed from script)
    struct ExportProperty {
        std::string name;
        PropertyDescriptor descriptor;
    };

    const std::vector<ExportProperty>& GetExportProperties() const { return m_ExportProperties; }

    // Get the ScriptAPI instance
    scripting::ScriptAPI* GetScriptAPI() { return m_ScriptAPI.get(); }
    const scripting::ScriptAPI* GetScriptAPI() const { return m_ScriptAPI.get(); }

    // Lifecycle hooks - override these in derived classes
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnInput(float deltaTime) override;
    void OnProcess(float deltaTime) override;
    void OnPhysicsProcess(float deltaTime) override;
    void OnRender() override;

protected:
    std::string m_ScriptPath;
    std::vector<ExportProperty> m_ExportProperties;
    bool m_ScriptLoaded = false;
    std::unique_ptr<scripting::ScriptAPI> m_ScriptAPI;

    // Parse export properties from script content
    virtual void ParseExportProperties(const std::string& scriptContent) = 0;

    // Get script environment
    virtual scripting::IScriptEnvironment* GetEnvironment() = 0;

    // Helper to call lifecycle functions in script
    bool CallScriptFunction(const std::string& functionName);
    bool CallScriptFunctionWithDelta(const std::string& functionName, float deltaTime);
};

/**
 * Python script component
 */
class PythonScriptComponent : public ScriptComponent {
public:
    PythonScriptComponent();
    explicit PythonScriptComponent(const std::string& name);
    ~PythonScriptComponent() override;

    std::string GetTypeName() const override { return "PythonScriptComponent"; }
    
    scripting::ScriptLanguage GetLanguage() const override { return scripting::ScriptLanguage::Python; }
    
    // Get the Python environment
    scripting::PythonEnvironment* GetPythonEnvironment() { return m_PythonEnv.get(); }

protected:
    void ParseExportProperties(const std::string& scriptContent) override;
    scripting::IScriptEnvironment* GetEnvironment() override { return m_PythonEnv.get(); }

private:
    std::unique_ptr<scripting::PythonEnvironment> m_PythonEnv;
};

/**
 * Lua script component
 */
class LuaScriptComponent : public ScriptComponent {
public:
    LuaScriptComponent();
    explicit LuaScriptComponent(const std::string& name);
    ~LuaScriptComponent() override;

    std::string GetTypeName() const override { return "LuaScriptComponent"; }

    scripting::ScriptLanguage GetLanguage() const override { return scripting::ScriptLanguage::Lua; }

    // Get the Lua environment
    scripting::LuaEnvironment* GetLuaEnvironment() { return m_LuaEnv.get(); }

protected:
    void ParseExportProperties(const std::string& scriptContent) override;
    scripting::IScriptEnvironment* GetEnvironment() override { return m_LuaEnv.get(); }

private:
    std::unique_ptr<scripting::LuaEnvironment> m_LuaEnv;
};

#ifdef LUPINE_HAS_MRUBY
/**
 * mRuby script component
 */
class MRubyScriptComponent : public ScriptComponent {
public:
    MRubyScriptComponent();
    explicit MRubyScriptComponent(const std::string& name);
    ~MRubyScriptComponent() override;

    std::string GetTypeName() const override { return "MRubyScriptComponent"; }

    scripting::ScriptLanguage GetLanguage() const override { return scripting::ScriptLanguage::MRuby; }

    // Get the mRuby environment
    scripting::MRubyEnvironment* GetMRubyEnvironment() { return m_MRubyEnv.get(); }

protected:
    void ParseExportProperties(const std::string& scriptContent) override;
    scripting::IScriptEnvironment* GetEnvironment() override { return m_MRubyEnv.get(); }

private:
    std::unique_ptr<scripting::MRubyEnvironment> m_MRubyEnv;
};
#endif // LUPINE_HAS_MRUBY

} // namespace core
} // namespace lupine
