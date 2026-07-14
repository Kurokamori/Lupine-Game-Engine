#pragma once

#include "lupine/input/InputManager.hpp"
#include <SDL2/SDL.h>
#include <memory>
#include <unordered_map>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace lupine {
namespace runtime {

/**
 * SDL2 Input Adapter
 *
 * Bridges SDL2 input events to the core InputManager.
 * Polls SDL2 for keyboard, mouse, and gamepad events and updates
 * the InputManager device states accordingly.
 *
 * Platform support:
 * - Windows: Full keyboard, mouse, gamepad support
 * - Mac: Full support with Command key mapped to Super
 * - Web: SDL2 via Emscripten, plus touch input support
 *
 * Responsibilities:
 * - Poll SDL2 events for input
 * - Map SDL2 keycodes/scancodes to core KeyCode enum
 * - Map SDL2 mouse buttons to core MouseButton enum
 * - Map SDL2 gamepad buttons/axes to core enums
 * - Update InputManager device states
 * - Handle gamepad connection/disconnection
 * - Handle touch input (Web/mobile platforms)
 */
class SDL2InputAdapter {
public:
    SDL2InputAdapter();
    ~SDL2InputAdapter();

    /**
     * Initialize the adapter with an InputManager
     */
    bool Initialize(input::InputManager* inputManager);

    /**
     * Shutdown and cleanup
     */
    void Shutdown();

    /**
     * Poll SDL2 input and update InputManager
     * Call this once per frame before InputManager::Update()
     */
    void PollInput();

    /**
     * Process a single SDL event
     * Returns true if the event was handled
     */
    bool ProcessEvent(const SDL_Event& event);

#ifdef __EMSCRIPTEN__
    /**
     * Initialize Emscripten-specific input callbacks
     * Called automatically during Initialize on web builds
     */
    void InitializeEmscriptenCallbacks();

    /**
     * Cleanup Emscripten-specific input callbacks
     */
    void ShutdownEmscriptenCallbacks();
#endif

private:
    input::InputManager* m_InputManager = nullptr;

    // SDL2 Gamepad management
    std::unordered_map<SDL_JoystickID, SDL_GameController*> m_GameControllers;
    std::unordered_map<SDL_JoystickID, uint32_t> m_JoystickIDToGamepadID;
    uint32_t m_NextGamepadID = 0;

    // Helper functions
    void InitializeGameControllers();
    void OpenGameController(int deviceIndex);
    void CloseGameController(SDL_JoystickID instanceID);

    // Resolve the open SDL_GameController for a core gamepad id (nullptr if none).
    SDL_GameController* FindControllerByGamepadID(uint32_t gamepadID) const;
    void UpdateMouseState();
    void UpdateGamepadStates();

    // SDL2 to Core mapping functions
    input::KeyCode MapSDLKeyToKeyCode(SDL_Keycode key) const;
    input::MouseButton MapSDLMouseButton(uint8_t button) const;
    input::GamepadButton MapSDLGamepadButton(SDL_GameControllerButton button) const;
    input::GamepadAxis MapSDLGamepadAxis(SDL_GameControllerAxis axis) const;
    input::GamepadType MapSDLGamepadType(SDL_GameControllerType type) const;

#ifdef __EMSCRIPTEN__
    // Touch input handling for web
    bool ProcessTouchEvent(const SDL_Event& event);

    // Get the scale factor between internal canvas size and CSS display size
    // Mouse coords come in CSS space, but we need internal canvas space
    glm::vec2 GetCanvasScaleFactor() const;

    // Static instance pointer for Emscripten callbacks
    static SDL2InputAdapter* s_Instance;

    // Emscripten callback wrappers
    static EM_BOOL EmscriptenKeyCallback(int eventType, const EmscriptenKeyboardEvent* event, void* userData);
    static EM_BOOL EmscriptenMouseCallback(int eventType, const EmscriptenMouseEvent* event, void* userData);
    static EM_BOOL EmscriptenTouchCallback(int eventType, const EmscriptenTouchEvent* event, void* userData);
    static EM_BOOL EmscriptenWheelCallback(int eventType, const EmscriptenWheelEvent* event, void* userData);
#endif
};

} // namespace runtime
} // namespace lupine
