#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace scripting {

nlohmann::json IScriptEnvironment::CallMethod(const std::string& functionName,
                                              const nlohmann::json& selfData,
                                              const nlohmann::json& args) {
    (void)functionName;
    (void)selfData;
    (void)args;
    return nlohmann::json(nullptr);
}

bool IScriptEnvironment::CallFunctionArgs(const std::string& functionName,
                                          const nlohmann::json& args) {
    (void)functionName;
    (void)args;
    return false;
}

nlohmann::json IScriptEnvironment::CallFunctionResult(const std::string& functionName,
                                                      const nlohmann::json& args) {
    (void)functionName;
    (void)args;
    return nlohmann::json(nullptr);
}

void IScriptEnvironment::SetGlobalJson(const std::string& name, const nlohmann::json& value) {
    (void)name;
    (void)value;
}

nlohmann::json IScriptEnvironment::GetGlobalJson(const std::string& name,
                                                 const nlohmann::json& defaultValue) {
    (void)name;
    return defaultValue;
}

void InitializeScripting() {

}

void ShutdownScripting() {

}

}
}
