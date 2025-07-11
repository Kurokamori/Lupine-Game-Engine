#include "WebScriptBridge.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <Python.h>

namespace Lupine {

// Static instance for web script bridge
static std::unique_ptr<WebScriptBridge> g_script_bridge;

WebScriptBridge::WebScriptBridge() {
    InitializeLuaForWeb();
    InitializePythonForWeb();
}

WebScriptBridge::~WebScriptBridge() {
    ShutdownScriptSystems();
}

WebScriptBridge& WebScriptBridge::Instance() {
    if (!g_script_bridge) {
        g_script_bridge = std::make_unique<WebScriptBridge>();
    }
    return *g_script_bridge;
}

void WebScriptBridge::InitializeLuaForWeb() {
    try {
        // Initialize Lua state for web environment
        m_lua_state = luaL_newstate();
        if (!m_lua_state) {
            std::cerr << "Failed to create Lua state for web" << std::endl;
            return;
        }
        
        // Load standard Lua libraries
        luaL_openlibs(m_lua_state);
        
        // Register web-specific Lua functions
        RegisterLuaWebFunctions();
        
        m_lua_initialized = true;
        std::cout << "Lua initialized for web environment" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Lua for web: " << e.what() << std::endl;
        m_lua_initialized = false;
    }
}

void WebScriptBridge::InitializePythonForWeb() {
    try {
        // Initialize Python interpreter for web environment
        // Note: This requires Python to be compiled with Emscripten
        if (!Py_IsInitialized()) {
            Py_Initialize();
        }
        
        if (!Py_IsInitialized()) {
            std::cerr << "Failed to initialize Python for web" << std::endl;
            return;
        }
        
        // Register web-specific Python functions
        RegisterPythonWebFunctions();
        
        m_python_initialized = true;
        std::cout << "Python initialized for web environment" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Python for web: " << e.what() << std::endl;
        m_python_initialized = false;
    }
}

void WebScriptBridge::RegisterLuaWebFunctions() {
    if (!m_lua_state) return;
    
    // Register console logging functions
    lua_register(m_lua_state, "web_log", [](lua_State* L) -> int {
        const char* message = luaL_checkstring(L, 1);
        EM_ASM({
            console.log(UTF8ToString($0));
        }, message);
        return 0;
    });
    
    lua_register(m_lua_state, "web_error", [](lua_State* L) -> int {
        const char* message = luaL_checkstring(L, 1);
        EM_ASM({
            console.error(UTF8ToString($0));
        }, message);
        return 0;
    });
    
    // Register DOM interaction functions
    lua_register(m_lua_state, "web_get_element_by_id", [](lua_State* L) -> int {
        const char* id = luaL_checkstring(L, 1);
        int result = EM_ASM_INT({
            var element = document.getElementById(UTF8ToString($0));
            return element ? 1 : 0;
        }, id);
        lua_pushboolean(L, result);
        return 1;
    });
    
    // Register local storage functions
    lua_register(m_lua_state, "web_set_local_storage", [](lua_State* L) -> int {
        const char* key = luaL_checkstring(L, 1);
        const char* value = luaL_checkstring(L, 2);
        EM_ASM({
            localStorage.setItem(UTF8ToString($0), UTF8ToString($1));
        }, key, value);
        return 0;
    });
    
    lua_register(m_lua_state, "web_get_local_storage", [](lua_State* L) -> int {
        const char* key = luaL_checkstring(L, 1);
        char* result = (char*)EM_ASM_PTR({
            var value = localStorage.getItem(UTF8ToString($0));
            if (value === null) return 0;
            var len = lengthBytesUTF8(value) + 1;
            var ptr = _malloc(len);
            stringToUTF8(value, ptr, len);
            return ptr;
        }, key);
        
        if (result) {
            lua_pushstring(L, result);
            free(result);
        } else {
            lua_pushnil(L);
        }
        return 1;
    });
}

void WebScriptBridge::RegisterPythonWebFunctions() {
    // Python web functions would be registered here
    // This is a placeholder for Python web integration
    std::cout << "Python web functions registered (placeholder)" << std::endl;
}

bool WebScriptBridge::ExecuteLuaScript(const std::string& script) {
    if (!m_lua_initialized || !m_lua_state) {
        std::cerr << "Lua not initialized for web" << std::endl;
        return false;
    }
    
    try {
        int result = luaL_dostring(m_lua_state, script.c_str());
        if (result != LUA_OK) {
            std::string error = lua_tostring(m_lua_state, -1);
            lua_pop(m_lua_state, 1);
            std::cerr << "Lua script error: " << error << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception executing Lua script: " << e.what() << std::endl;
        return false;
    }
}

bool WebScriptBridge::ExecutePythonScript(const std::string& script) {
    if (!m_python_initialized) {
        std::cerr << "Python not initialized for web" << std::endl;
        return false;
    }
    
    try {
        int result = PyRun_SimpleString(script.c_str());
        return result == 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception executing Python script: " << e.what() << std::endl;
        return false;
    }
}

void WebScriptBridge::CallLuaFunction(const std::string& function_name, const std::vector<std::string>& args) {
    if (!m_lua_initialized || !m_lua_state) return;
    
    lua_getglobal(m_lua_state, function_name.c_str());
    if (!lua_isfunction(m_lua_state, -1)) {
        lua_pop(m_lua_state, 1);
        return;
    }
    
    // Push arguments
    for (const auto& arg : args) {
        lua_pushstring(m_lua_state, arg.c_str());
    }
    
    // Call function
    int result = lua_pcall(m_lua_state, args.size(), 0, 0);
    if (result != LUA_OK) {
        std::string error = lua_tostring(m_lua_state, -1);
        lua_pop(m_lua_state, 1);
        std::cerr << "Error calling Lua function " << function_name << ": " << error << std::endl;
    }
}

void WebScriptBridge::ShutdownScriptSystems() {
    if (m_lua_state) {
        lua_close(m_lua_state);
        m_lua_state = nullptr;
        m_lua_initialized = false;
    }
    
    if (m_python_initialized && Py_IsInitialized()) {
        Py_Finalize();
        m_python_initialized = false;
    }
}

bool WebScriptBridge::IsLuaAvailable() const {
    return m_lua_initialized;
}

bool WebScriptBridge::IsPythonAvailable() const {
    return m_python_initialized;
}

} // namespace Lupine

// C-style functions for JavaScript integration
extern "C" {

EMSCRIPTEN_KEEPALIVE
int lupine_execute_lua_script(const char* script) {
    return Lupine::WebScriptBridge::Instance().ExecuteLuaScript(script) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int lupine_execute_python_script(const char* script) {
    return Lupine::WebScriptBridge::Instance().ExecutePythonScript(script) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void lupine_call_lua_function(const char* function_name) {
    std::vector<std::string> args; // No arguments for now
    Lupine::WebScriptBridge::Instance().CallLuaFunction(function_name, args);
}

EMSCRIPTEN_KEEPALIVE
int lupine_is_lua_available() {
    return Lupine::WebScriptBridge::Instance().IsLuaAvailable() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int lupine_is_python_available() {
    return Lupine::WebScriptBridge::Instance().IsPythonAvailable() ? 1 : 0;
}

} // extern "C"

#endif // __EMSCRIPTEN__
