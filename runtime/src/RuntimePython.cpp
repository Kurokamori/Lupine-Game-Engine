#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "lupine/runtime/Runtime.hpp"
#include "lupine/runtime/RuntimeApp.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/input/InputCodes.hpp"
#include "lupine/rendering/gfx/GfxTypes.hpp"
#include "lupine/profiling/Profiler.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include <SDL2/SDL.h>
#include <unordered_map>
#include <cmath>

namespace py = pybind11;

namespace {

// ============================================================================
// Editor gamepad capture poller
//
// Lets the editor's input-binding dialog detect a physical gamepad press while
// rebinding, without standing up a full RuntimeApp. Only SDL's game-controller
// subsystem is initialized (no window / render context), and controller state is
// read by polling (SDL_GameControllerUpdate) rather than an event loop.
// ============================================================================

struct EditorGamepadPoller {
    std::unordered_map<SDL_JoystickID, SDL_GameController*> controllers;

    void EnsureInit() {
        if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
            SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        }
    }

    void OpenAll() {
        int count = SDL_NumJoysticks();
        for (int i = 0; i < count; ++i) {
            if (!SDL_IsGameController(i)) {
                continue;
            }
            SDL_GameController* gc = SDL_GameControllerOpen(i);
            if (!gc) {
                continue;
            }
            SDL_Joystick* js = SDL_GameControllerGetJoystick(gc);
            SDL_JoystickID id = SDL_JoystickInstanceID(js);
            controllers.emplace(id, gc);
        }
    }

    void Shutdown() {
        for (auto& [id, gc] : controllers) {
            (void)id;
            if (gc) {
                SDL_GameControllerClose(gc);
            }
        }
        controllers.clear();
    }
};

EditorGamepadPoller& Poller() {
    static EditorGamepadPoller poller;
    return poller;
}

const char* SdlButtonToCanonical(SDL_GameControllerButton button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A: return "A";
        case SDL_CONTROLLER_BUTTON_B: return "B";
        case SDL_CONTROLLER_BUTTON_X: return "X";
        case SDL_CONTROLLER_BUTTON_Y: return "Y";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LeftBumper";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RightBumper";
        case SDL_CONTROLLER_BUTTON_BACK: return "Back";
        case SDL_CONTROLLER_BUTTON_START: return "Start";
        case SDL_CONTROLLER_BUTTON_GUIDE: return "Guide";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "LeftThumb";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "RightThumb";
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return "DPadUp";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "DPadRight";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "DPadDown";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "DPadLeft";
        default: return nullptr;
    }
}

const char* SdlAxisToCanonical(SDL_GameControllerAxis axis) {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: return "LeftX";
        case SDL_CONTROLLER_AXIS_LEFTY: return "LeftY";
        case SDL_CONTROLLER_AXIS_RIGHTX: return "RightX";
        case SDL_CONTROLLER_AXIS_RIGHTY: return "RightY";
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return "LeftTrigger";
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "RightTrigger";
        default: return nullptr;
    }
}

// Poll connected controllers for the first pressed button or deflected axis.
// Returns a dict describing the input in engine-canonical terms, or None.
// axis_threshold is in [0, 1]; sticks/triggers must exceed it to register so a
// resting stick (drift) does not capture spuriously.
py::object GamepadCapturePoll(float axisThreshold) {
    EditorGamepadPoller& poller = Poller();
    poller.EnsureInit();
    poller.OpenAll();
    SDL_GameControllerUpdate();

    const int threshold = static_cast<int>(axisThreshold * 32767.0f);

    for (auto& [id, gc] : poller.controllers) {
        (void)id;
        if (!gc || !SDL_GameControllerGetAttached(gc)) {
            continue;
        }

        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b) {
            if (SDL_GameControllerGetButton(gc, static_cast<SDL_GameControllerButton>(b))) {
                const char* name = SdlButtonToCanonical(static_cast<SDL_GameControllerButton>(b));
                if (!name) {
                    continue;
                }
                py::dict d;
                d["kind"] = "button";
                d["name"] = name;
                d["gamepad_id"] = 0;  // 0 = any gamepad (binding default)
                return d;
            }
        }

        for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a) {
            Sint16 value = SDL_GameControllerGetAxis(gc, static_cast<SDL_GameControllerAxis>(a));
            if (std::abs(static_cast<int>(value)) >= threshold) {
                const char* name = SdlAxisToCanonical(static_cast<SDL_GameControllerAxis>(a));
                if (!name) {
                    continue;
                }
                py::dict d;
                d["kind"] = "axis";
                d["name"] = name;
                d["scale"] = (value < 0) ? -1.0 : 1.0;
                d["gamepad_id"] = 0;
                return d;
            }
        }
    }

    return py::none();
}

int GamepadCaptureCount() {
    EditorGamepadPoller& poller = Poller();
    poller.EnsureInit();
    poller.OpenAll();
    int count = 0;
    for (auto& [id, gc] : poller.controllers) {
        (void)id;
        if (gc && SDL_GameControllerGetAttached(gc)) {
            ++count;
        }
    }
    return count;
}

} // namespace

PYBIND11_MODULE(lupine_runtime, m) {
    m.doc() = "Lupine Runtime Python Module - Game engine runtime for playing games and scenes";

    // Drain buffered runtime log messages as (level, message) tuples so the editor
    // can mirror them into its Console panel. Safe to call from the main thread.
    m.def("drain_log_messages", &lupine::DrainRuntimeLogMessages,
          "Return and clear buffered runtime log messages as (level, message) tuples");

    // Editor gamepad capture: poll connected controllers for a pressed button or
    // deflected axis without standing up a RuntimeApp. Used by the input-binding
    // dialog so rebinding can capture live gamepad input.
    m.def("gamepad_capture_poll", &GamepadCapturePoll, py::arg("axis_threshold") = 0.6f,
          "Poll game controllers for the first pressed button / deflected axis. "
          "Returns {kind:'button'|'axis', name:str, gamepad_id:int, scale:float?} or None.");
    m.def("gamepad_capture_count", &GamepadCaptureCount,
          "Number of connected game controllers visible to the capture poller.");
    m.def("gamepad_capture_shutdown", []() { Poller().Shutdown(); },
          "Close controllers opened by gamepad_capture_poll.");

    // Profiler access for the editor Profiler panel. The live game runs inside this
    // module, so the populated profiling::Profiler singleton is this module's. Data
    // crosses the binding as JSON strings so the panel can parse with stdlib json.
    m.def("profiler_set_enabled",
          [](bool enabled) { lupine::profiling::Profiler::Get().SetEnabled(enabled); },
          py::arg("enabled"), "Enable or disable profiler capture.");
    m.def("profiler_is_enabled",
          []() { return lupine::profiling::Profiler::Get().IsEnabled(); },
          "Whether profiler capture is enabled.");
    m.def("profiler_set_history_size",
          [](int frames) { lupine::profiling::Profiler::Get().SetHistorySize(static_cast<size_t>(frames < 1 ? 1 : frames)); },
          py::arg("frames"), "Set the number of frames retained in the history ring buffer.");
    m.def("profiler_clear",
          []() { lupine::profiling::Profiler::Get().Clear(); },
          "Discard all captured history.");
    m.def("profiler_average_frame_ms",
          []() { return lupine::profiling::Profiler::Get().GetAverageFrameMs(); },
          "Average frame time over the history window, in milliseconds.");
    m.def("profiler_fps",
          []() { return lupine::profiling::Profiler::Get().GetFps(); },
          "Frames per second derived from the average frame time.");
    m.def("profiler_get_history_json",
          []() { return lupine::profiling::Profiler::Get().ToJson(); },
          "Serialize the entire history window to a JSON string.");
    m.def("profiler_save_capture",
          [](const std::string& path) { return lupine::profiling::Profiler::Get().SaveCapture(path); },
          py::arg("path"), "Save the current history window to a .lprof (JSON) file.");

    m.def("get_default_user_data_path",
          [](const std::string& appName) {
              return lupine::platform::VirtualFileSystem::GetDefaultUserDataPath(appName);
          },
          py::arg("app_name") = "Lupine",
          "Resolve the platform default user:// directory (e.g. %APPDATA%/Lupine). "
          "The editor uses this as the base for per-instance user folders.");

    // Note: GraphicsBackend enum is already registered in lupine_engine module
    // We use it here for the backend field but don't re-register it

    py::class_<lupine::RuntimeApp::Config>(m, "RuntimeConfig")
        .def(py::init<>())
        .def_readwrite("title", &lupine::RuntimeApp::Config::title,
                       "Window title")
        .def_readwrite("window_width", &lupine::RuntimeApp::Config::windowWidth,
                       "Window width in pixels")
        .def_readwrite("window_height", &lupine::RuntimeApp::Config::windowHeight,
                       "Window height in pixels")
        .def_readwrite("debugging", &lupine::RuntimeApp::Config::debugging,
                       "Enable debug logging")
        .def_readwrite("vsync", &lupine::RuntimeApp::Config::vsync,
                       "Enable vertical sync")
        .def_readwrite("resizable", &lupine::RuntimeApp::Config::resizable,
                       "Make window resizable")
        .def_readwrite("backend", &lupine::RuntimeApp::Config::backend,
                       "Graphics backend to use (OpenGL, Vulkan, Metal, etc.)")
        .def_readwrite("project_path", &lupine::RuntimeApp::Config::projectPath,
                       "Path to project file (.lupine)")
        .def_readwrite("scene_path", &lupine::RuntimeApp::Config::scenePath,
                       "Optional: Path to specific scene to load")
        .def_readwrite("user_path", &lupine::RuntimeApp::Config::userPath,
                       "Optional: physical directory to mount as user:// "
                       "(per-instance isolation). Empty = platform default")
        .def_readwrite("args", &lupine::RuntimeApp::Config::args,
                       "Extra runtime arguments forwarded to the game "
                       "(readable via get_cmdline_args())");

    py::class_<lupine::RuntimeApp>(m, "RuntimeApp",
                                   "Main runtime application for playing Lupine games")
        .def(py::init<>(), "Create a new runtime instance")

        .def("initialize", &lupine::RuntimeApp::initialize, py::arg("config"),
             "Initialize the runtime with the given configuration")

        .def("run", &lupine::RuntimeApp::run,
             py::call_guard<py::gil_scoped_release>(),
             "Run the main loop (blocking - releases GIL)")
        .def("run_async", &lupine::RuntimeApp::runAsync,
             "Run the main loop in a separate thread (non-blocking)")
        .def("run_frame", &lupine::RuntimeApp::runFrame,
             "Run a single frame (for manual control). Returns False if app should exit")
        .def("process_events", &lupine::RuntimeApp::processEvents,
             "Process SDL events only (must be called from main thread when running async). Returns False if app should exit")
        .def("stop", &lupine::RuntimeApp::stop,
             py::call_guard<py::gil_scoped_release>(),
             "Stop the runtime (exits main loop gracefully - releases GIL)")
        .def("shutdown", &lupine::RuntimeApp::shutdown,
             py::call_guard<py::gil_scoped_release>(),
             "Shutdown the runtime (cleanup resources - releases GIL)")

        .def("pause", &lupine::RuntimeApp::pause,
             "Pause the runtime (stops updates but continues rendering)")
        .def("resume", &lupine::RuntimeApp::resume,
             "Resume the runtime from paused state")
        .def("step", &lupine::RuntimeApp::step,
             "Advance exactly one frame while paused (no-op when not paused)")
        .def("reload_scene", &lupine::RuntimeApp::reloadScene,
             py::call_guard<py::gil_scoped_release>(),
             "Reload the current scene (releases GIL)")
        .def("reload_scripts", &lupine::RuntimeApp::reloadScripts,
             py::call_guard<py::gil_scoped_release>(),
             "Hot-reload all script components in the current scene (releases GIL)")
        .def("load_scene", &lupine::RuntimeApp::loadScene,
             py::call_guard<py::gil_scoped_release>(),
             py::arg("scene_path"),
             "Load a different scene (releases GIL)")
        .def("apply_live_edits", &lupine::RuntimeApp::applyLiveEdits,
             py::call_guard<py::gil_scoped_release>(),
             py::arg("edits_json"),
             "Apply a JSON batch of editor live edits (node/component properties, "
             "node transforms, asset reloads, script reloads) to the running scene "
             "without restarting (releases GIL)")

        .def("is_running", &lupine::RuntimeApp::isRunning,
             "Check if the runtime is running")
        .def("is_paused", &lupine::RuntimeApp::isPaused,
             "Check if the runtime is paused")
        .def("get_config", &lupine::RuntimeApp::getConfig,
             py::return_value_policy::reference,
             "Get the current runtime configuration")

        .def("get_input_manager", &lupine::RuntimeApp::getInputManager,
             py::return_value_policy::reference,
             "Get the input manager")
        .def("get_scene_manager", &lupine::RuntimeApp::getSceneManager,
             py::return_value_policy::reference,
             "Get the scene manager");

    py::class_<lupine::core::SceneManager>(m, "SceneManager",
                                               "Manages scenes and project loading")
        .def("load_project", &lupine::core::SceneManager::LoadProject,
             py::arg("project_path"),
             "Load a project from file (loads main scene)")
        .def("load_project_with_scene", &lupine::core::SceneManager::LoadProjectWithScene,
             py::arg("project_path"), py::arg("scene_path"),
             "Load a project and specific scene")
        .def("load_scene", &lupine::core::SceneManager::LoadScene,
             py::arg("scene_path"),
             "Load or switch to a different scene")
        .def("unload_current_scene", &lupine::core::SceneManager::UnloadCurrentScene,
             "Unload the current scene")
        .def("unload_project", &lupine::core::SceneManager::UnloadProject,
             "Unload the current project");

    py::enum_<lupine::input::KeyCode>(m, "KeyCode")
        .value("Unknown", lupine::input::KeyCode::Unknown)
        // Printable keys
        .value("Space", lupine::input::KeyCode::Space)
        .value("Apostrophe", lupine::input::KeyCode::Apostrophe)
        .value("Comma", lupine::input::KeyCode::Comma)
        .value("Minus", lupine::input::KeyCode::Minus)
        .value("Period", lupine::input::KeyCode::Period)
        .value("Slash", lupine::input::KeyCode::Slash)
        // Number keys
        .value("D0", lupine::input::KeyCode::D0)
        .value("D1", lupine::input::KeyCode::D1)
        .value("D2", lupine::input::KeyCode::D2)
        .value("D3", lupine::input::KeyCode::D3)
        .value("D4", lupine::input::KeyCode::D4)
        .value("D5", lupine::input::KeyCode::D5)
        .value("D6", lupine::input::KeyCode::D6)
        .value("D7", lupine::input::KeyCode::D7)
        .value("D8", lupine::input::KeyCode::D8)
        .value("D9", lupine::input::KeyCode::D9)
        .value("Semicolon", lupine::input::KeyCode::Semicolon)
        .value("Equal", lupine::input::KeyCode::Equal)
        // Letter keys
        .value("A", lupine::input::KeyCode::A)
        .value("B", lupine::input::KeyCode::B)
        .value("C", lupine::input::KeyCode::C)
        .value("D", lupine::input::KeyCode::D)
        .value("E", lupine::input::KeyCode::E)
        .value("F", lupine::input::KeyCode::F)
        .value("G", lupine::input::KeyCode::G)
        .value("H", lupine::input::KeyCode::H)
        .value("I", lupine::input::KeyCode::I)
        .value("J", lupine::input::KeyCode::J)
        .value("K", lupine::input::KeyCode::K)
        .value("L", lupine::input::KeyCode::L)
        .value("M", lupine::input::KeyCode::M)
        .value("N", lupine::input::KeyCode::N)
        .value("O", lupine::input::KeyCode::O)
        .value("P", lupine::input::KeyCode::P)
        .value("Q", lupine::input::KeyCode::Q)
        .value("R", lupine::input::KeyCode::R)
        .value("S", lupine::input::KeyCode::S)
        .value("T", lupine::input::KeyCode::T)
        .value("U", lupine::input::KeyCode::U)
        .value("V", lupine::input::KeyCode::V)
        .value("W", lupine::input::KeyCode::W)
        .value("X", lupine::input::KeyCode::X)
        .value("Y", lupine::input::KeyCode::Y)
        .value("Z", lupine::input::KeyCode::Z)
        .value("LeftBracket", lupine::input::KeyCode::LeftBracket)
        .value("Backslash", lupine::input::KeyCode::Backslash)
        .value("RightBracket", lupine::input::KeyCode::RightBracket)
        .value("GraveAccent", lupine::input::KeyCode::GraveAccent)
        // Function keys
        .value("Escape", lupine::input::KeyCode::Escape)
        .value("Enter", lupine::input::KeyCode::Enter)
        .value("Tab", lupine::input::KeyCode::Tab)
        .value("Backspace", lupine::input::KeyCode::Backspace)
        .value("Insert", lupine::input::KeyCode::Insert)
        .value("Delete", lupine::input::KeyCode::Delete)
        // Arrow keys
        .value("Right", lupine::input::KeyCode::Right)
        .value("Left", lupine::input::KeyCode::Left)
        .value("Down", lupine::input::KeyCode::Down)
        .value("Up", lupine::input::KeyCode::Up)
        .value("PageUp", lupine::input::KeyCode::PageUp)
        .value("PageDown", lupine::input::KeyCode::PageDown)
        .value("Home", lupine::input::KeyCode::Home)
        .value("End", lupine::input::KeyCode::End)
        .value("CapsLock", lupine::input::KeyCode::CapsLock)
        .value("ScrollLock", lupine::input::KeyCode::ScrollLock)
        .value("NumLock", lupine::input::KeyCode::NumLock)
        .value("PrintScreen", lupine::input::KeyCode::PrintScreen)
        .value("Pause", lupine::input::KeyCode::Pause)
        // F keys
        .value("F1", lupine::input::KeyCode::F1)
        .value("F2", lupine::input::KeyCode::F2)
        .value("F3", lupine::input::KeyCode::F3)
        .value("F4", lupine::input::KeyCode::F4)
        .value("F5", lupine::input::KeyCode::F5)
        .value("F6", lupine::input::KeyCode::F6)
        .value("F7", lupine::input::KeyCode::F7)
        .value("F8", lupine::input::KeyCode::F8)
        .value("F9", lupine::input::KeyCode::F9)
        .value("F10", lupine::input::KeyCode::F10)
        .value("F11", lupine::input::KeyCode::F11)
        .value("F12", lupine::input::KeyCode::F12)
        // Keypad
        .value("KP0", lupine::input::KeyCode::KP0)
        .value("KP1", lupine::input::KeyCode::KP1)
        .value("KP2", lupine::input::KeyCode::KP2)
        .value("KP3", lupine::input::KeyCode::KP3)
        .value("KP4", lupine::input::KeyCode::KP4)
        .value("KP5", lupine::input::KeyCode::KP5)
        .value("KP6", lupine::input::KeyCode::KP6)
        .value("KP7", lupine::input::KeyCode::KP7)
        .value("KP8", lupine::input::KeyCode::KP8)
        .value("KP9", lupine::input::KeyCode::KP9)
        .value("KPDecimal", lupine::input::KeyCode::KPDecimal)
        .value("KPDivide", lupine::input::KeyCode::KPDivide)
        .value("KPMultiply", lupine::input::KeyCode::KPMultiply)
        .value("KPSubtract", lupine::input::KeyCode::KPSubtract)
        .value("KPAdd", lupine::input::KeyCode::KPAdd)
        .value("KPEnter", lupine::input::KeyCode::KPEnter)
        .value("KPEqual", lupine::input::KeyCode::KPEqual)
        // Modifiers
        .value("LeftShift", lupine::input::KeyCode::LeftShift)
        .value("LeftControl", lupine::input::KeyCode::LeftControl)
        .value("LeftAlt", lupine::input::KeyCode::LeftAlt)
        .value("LeftSuper", lupine::input::KeyCode::LeftSuper)
        .value("RightShift", lupine::input::KeyCode::RightShift)
        .value("RightControl", lupine::input::KeyCode::RightControl)
        .value("RightAlt", lupine::input::KeyCode::RightAlt)
        .value("RightSuper", lupine::input::KeyCode::RightSuper)
        .value("Menu", lupine::input::KeyCode::Menu)
        .export_values();

    py::enum_<lupine::input::MouseButton>(m, "MouseButton")
        .value("Left", lupine::input::MouseButton::Left)
        .value("Right", lupine::input::MouseButton::Right)
        .value("Middle", lupine::input::MouseButton::Middle)
        .export_values();

    py::enum_<lupine::input::GamepadButton>(m, "GamepadButton")
        // Face buttons
        .value("A", lupine::input::GamepadButton::A)
        .value("B", lupine::input::GamepadButton::B)
        .value("X", lupine::input::GamepadButton::X)
        .value("Y", lupine::input::GamepadButton::Y)
        // Shoulder buttons
        .value("LeftBumper", lupine::input::GamepadButton::LeftBumper)
        .value("RightBumper", lupine::input::GamepadButton::RightBumper)
        // Menu buttons
        .value("Back", lupine::input::GamepadButton::Back)
        .value("Start", lupine::input::GamepadButton::Start)
        .value("Guide", lupine::input::GamepadButton::Guide)
        // Stick clicks
        .value("LeftThumb", lupine::input::GamepadButton::LeftThumb)
        .value("RightThumb", lupine::input::GamepadButton::RightThumb)
        // D-Pad
        .value("DPadUp", lupine::input::GamepadButton::DPadUp)
        .value("DPadRight", lupine::input::GamepadButton::DPadRight)
        .value("DPadDown", lupine::input::GamepadButton::DPadDown)
        .value("DPadLeft", lupine::input::GamepadButton::DPadLeft)
        .export_values();

    py::enum_<lupine::input::GamepadAxis>(m, "GamepadAxis")
        .value("LeftX", lupine::input::GamepadAxis::LeftX)
        .value("LeftY", lupine::input::GamepadAxis::LeftY)
        .value("RightX", lupine::input::GamepadAxis::RightX)
        .value("RightY", lupine::input::GamepadAxis::RightY)
        .value("LeftTrigger", lupine::input::GamepadAxis::LeftTrigger)
        .value("RightTrigger", lupine::input::GamepadAxis::RightTrigger)
        .export_values();

    py::class_<lupine::input::InputManager>(m, "InputManager",
                                             "Manages input from keyboard, mouse, and gamepad")
        // Keyboard
        .def("is_key_pressed", &lupine::input::InputManager::IsKeyPressed,
             py::arg("key"), "Check if key is currently pressed")
        .def("is_key_just_pressed", &lupine::input::InputManager::IsKeyJustPressed,
             py::arg("key"), "Check if key was just pressed this frame")
        .def("is_key_just_released", &lupine::input::InputManager::IsKeyJustReleased,
             py::arg("key"), "Check if key was just released this frame")

        // Mouse buttons
        .def("is_mouse_button_pressed", &lupine::input::InputManager::IsMouseButtonPressed,
             py::arg("button"), "Check if mouse button is currently pressed")
        .def("is_mouse_button_just_pressed", &lupine::input::InputManager::IsMouseButtonJustPressed,
             py::arg("button"), "Check if mouse button was just pressed this frame")
        .def("is_mouse_button_just_released", &lupine::input::InputManager::IsMouseButtonJustReleased,
             py::arg("button"), "Check if mouse button was just released this frame")

        // Mouse position and movement
        .def("get_mouse_position", [](const lupine::input::InputManager& im) {
            auto pos = im.GetMousePosition();
            return py::make_tuple(pos.x, pos.y);
        }, "Get mouse position as (x, y) tuple")
        .def("get_mouse_delta", [](const lupine::input::InputManager& im) {
            auto delta = im.GetMouseDelta();
            return py::make_tuple(delta.x, delta.y);
        }, "Get mouse delta movement as (x, y) tuple")
        .def("get_mouse_scroll_delta", [](const lupine::input::InputManager& im) {
            auto delta = im.GetMouseScrollDelta();
            return py::make_tuple(delta.x, delta.y);
        }, "Get mouse scroll delta as (x, y) tuple")

        // Gamepad buttons
        .def("is_gamepad_button_pressed", &lupine::input::InputManager::IsGamepadButtonPressed,
             py::arg("button"), py::arg("gamepad_id") = 0,
             "Check if gamepad button is currently pressed")
        .def("is_gamepad_button_just_pressed", &lupine::input::InputManager::IsGamepadButtonJustPressed,
             py::arg("button"), py::arg("gamepad_id") = 0,
             "Check if gamepad button was just pressed this frame")
        .def("is_gamepad_button_just_released", &lupine::input::InputManager::IsGamepadButtonJustReleased,
             py::arg("button"), py::arg("gamepad_id") = 0,
             "Check if gamepad button was just released this frame")
        .def("get_gamepad_axis", &lupine::input::InputManager::GetGamepadAxis,
             py::arg("axis"), py::arg("gamepad_id") = 0,
             "Get gamepad axis value (-1.0 to 1.0)")

        // Actions (mapped inputs)
        .def("is_action_pressed",
             static_cast<bool (lupine::input::InputManager::*)(const std::string&) const>(&lupine::input::InputManager::IsActionPressed),
             py::arg("action"), "Check if action is currently pressed")
        .def("is_action_just_pressed",
             static_cast<bool (lupine::input::InputManager::*)(const std::string&) const>(&lupine::input::InputManager::IsActionJustPressed),
             py::arg("action"), "Check if action was just pressed this frame")
        .def("is_action_just_released",
             static_cast<bool (lupine::input::InputManager::*)(const std::string&) const>(&lupine::input::InputManager::IsActionJustReleased),
             py::arg("action"), "Check if action was just released this frame")

        // Axes (mapped inputs)
        .def("get_axis_value",
             static_cast<float (lupine::input::InputManager::*)(const std::string&) const>(&lupine::input::InputManager::GetAxisValue),
             py::arg("axis"), "Get axis value (smoothed)")
        .def("get_axis_value_raw", &lupine::input::InputManager::GetAxisValueRaw,
             py::arg("axis"), "Get raw axis value (unprocessed)")

        // Settings
        .def("set_mouse_cursor_visible", &lupine::input::InputManager::SetMouseCursorVisible,
             py::arg("visible"), "Show or hide the mouse cursor")
        .def("is_mouse_cursor_visible", &lupine::input::InputManager::IsMouseCursorVisible,
             "Check if the mouse cursor is visible");
}
