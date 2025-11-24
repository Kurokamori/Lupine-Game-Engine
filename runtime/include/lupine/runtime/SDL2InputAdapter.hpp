#pragma once

#include "lupine/input/InputManager.hpp"
#include <SDL2/SDL.h>
#include <memory>
#include <unordered_map>

namespace lupine {
namespace runtime {

/**
 * SDL2 Input Adapter
 * 
 * Bridges SDL2 input events to the core InputManager.
 * Polls SDL2 for keyboard, mouse, and gamepad events and updates
 * the InputManager device states accordingly.
 * 
 * Responsibilities:
 * - Poll SDL2 events for input
 * - Map SDL2 keycodes/scancodes to core KeyCode enum
 * - Map SDL2 mouse buttons to core MouseButton enum
 * - Map SDL2 gamepad buttons/axes to core enums
 * - Update InputManager device states
 * - Handle gamepad connection/disconnection
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
    void UpdateKeyboardState();
    void UpdateMouseState();
    void UpdateGamepadStates();

    // SDL2 to Core mapping functions
    input::KeyCode MapSDLKeyToKeyCode(SDL_Keycode key) const;
    input::MouseButton MapSDLMouseButton(uint8_t button) const;
    input::GamepadButton MapSDLGamepadButton(SDL_GameControllerButton button) const;
    input::GamepadAxis MapSDLGamepadAxis(SDL_GameControllerAxis axis) const;
};

} // namespace runtime
} // namespace lupine
