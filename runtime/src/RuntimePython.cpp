#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "lupine/runtime/Runtime.hpp"
#include "lupine/runtime/RuntimeApp.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/input/InputCodes.hpp"

namespace py = pybind11;

PYBIND11_MODULE(lupine_runtime, m) {
    m.doc() = "Lupine Runtime Python Module - Game engine runtime for playing games and scenes";

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
        .def_readwrite("project_path", &lupine::RuntimeApp::Config::projectPath,
                       "Path to project file (.lupine)")
        .def_readwrite("scene_path", &lupine::RuntimeApp::Config::scenePath,
                       "Optional: Path to specific scene to load");

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
        .def("reload_scene", &lupine::RuntimeApp::reloadScene,
             py::call_guard<py::gil_scoped_release>(),
             "Reload the current scene (releases GIL)")
        .def("load_scene", &lupine::RuntimeApp::loadScene,
             py::call_guard<py::gil_scoped_release>(),
             py::arg("scene_path"),
             "Load a different scene (releases GIL)")

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
        .value("Space", lupine::input::KeyCode::Space)
        .value("Enter", lupine::input::KeyCode::Enter)
        .value("Escape", lupine::input::KeyCode::Escape)
        .value("Tab", lupine::input::KeyCode::Tab)
        .value("Backspace", lupine::input::KeyCode::Backspace)
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
        .value("LeftShift", lupine::input::KeyCode::LeftShift)
        .value("LeftControl", lupine::input::KeyCode::LeftControl)
        .value("LeftAlt", lupine::input::KeyCode::LeftAlt)
        .export_values();

    py::enum_<lupine::input::MouseButton>(m, "MouseButton")
        .value("Left", lupine::input::MouseButton::Left)
        .value("Right", lupine::input::MouseButton::Right)
        .value("Middle", lupine::input::MouseButton::Middle)
        .export_values();

    py::enum_<lupine::input::GamepadButton>(m, "GamepadButton")
        .value("A", lupine::input::GamepadButton::A)
        .value("B", lupine::input::GamepadButton::B)
        .value("X", lupine::input::GamepadButton::X)
        .value("Y", lupine::input::GamepadButton::Y)
        .value("Start", lupine::input::GamepadButton::Start)
        .export_values();

    py::class_<lupine::input::InputManager>(m, "InputManager",
                                             "Manages input from keyboard, mouse, and gamepad")

        .def("is_key_pressed", &lupine::input::InputManager::IsKeyPressed,
             py::arg("key"), "Check if key is currently pressed")
        .def("is_key_just_pressed", &lupine::input::InputManager::IsKeyJustPressed,
             py::arg("key"), "Check if key was just pressed this frame")
        .def("is_key_just_released", &lupine::input::InputManager::IsKeyJustReleased,
             py::arg("key"), "Check if key was just released this frame")

        .def("is_mouse_button_pressed", &lupine::input::InputManager::IsMouseButtonPressed,
             py::arg("button"), "Check if mouse button is currently pressed")
        .def("is_mouse_button_just_pressed", &lupine::input::InputManager::IsMouseButtonJustPressed,
             py::arg("button"), "Check if mouse button was just pressed this frame")
        .def("is_mouse_button_just_released", &lupine::input::InputManager::IsMouseButtonJustReleased,
             py::arg("button"), "Check if mouse button was just released this frame")

        .def("is_action_pressed", &lupine::input::InputManager::IsActionPressed,
             py::arg("action"), "Check if action is currently pressed")
        .def("is_action_just_pressed", &lupine::input::InputManager::IsActionJustPressed,
             py::arg("action"), "Check if action was just pressed this frame")
        .def("is_action_just_released", &lupine::input::InputManager::IsActionJustReleased,
             py::arg("action"), "Check if action was just released this frame")

        .def("get_axis_value", &lupine::input::InputManager::GetAxisValue,
             py::arg("axis"), "Get axis value (smoothed)")
        .def("get_axis_value_raw", &lupine::input::InputManager::GetAxisValueRaw,
             py::arg("axis"), "Get raw axis value (unprocessed)");
}
