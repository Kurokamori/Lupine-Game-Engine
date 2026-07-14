#include "lupine/runtime/SDL2InputAdapter.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/input/InputCodes.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace lupine {
namespace runtime {

#ifdef __EMSCRIPTEN__
// Static instance for Emscripten callbacks
SDL2InputAdapter* SDL2InputAdapter::s_Instance = nullptr;
#endif

SDL2InputAdapter::SDL2InputAdapter() = default;

SDL2InputAdapter::~SDL2InputAdapter() {
    Shutdown();
}

bool SDL2InputAdapter::Initialize(input::InputManager* inputManager) {
    if (!inputManager) {
        return false;
    }

    m_InputManager = inputManager;

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        return false;
    }

    InitializeGameControllers();

    // Install the gamepad vibration backend so core's SetGamepadVibration drives
    // SDL rumble. Motor strengths arrive clamped to [0, 1]; a durationMs of 0
    // means "until changed/stopped", which SDL expresses as the maximum duration.
    m_InputManager->SetVibrationProvider(
        [this](uint32_t gamepadID, float leftMotor, float rightMotor, uint32_t durationMs) {
            SDL_GameController* controller = FindControllerByGamepadID(gamepadID);
            if (!controller) {
                return;
            }
            Uint16 low = static_cast<Uint16>(leftMotor * 65535.0f);
            Uint16 high = static_cast<Uint16>(rightMotor * 65535.0f);
            Uint32 duration = (durationMs == 0) ? 0xFFFFFFFFu : static_cast<Uint32>(durationMs);
            SDL_GameControllerRumble(controller, low, high, duration);
        });

#ifdef __EMSCRIPTEN__
    InitializeEmscriptenCallbacks();
#endif

    return true;
}

SDL_GameController* SDL2InputAdapter::FindControllerByGamepadID(uint32_t gamepadID) const {
    for (const auto& [instanceID, mappedGamepadID] : m_JoystickIDToGamepadID) {
        if (mappedGamepadID == gamepadID) {
            auto it = m_GameControllers.find(instanceID);
            if (it != m_GameControllers.end()) {
                return it->second;
            }
        }
    }
    return nullptr;
}

void SDL2InputAdapter::Shutdown() {
#ifdef __EMSCRIPTEN__
    ShutdownEmscriptenCallbacks();
#endif

    for (auto& [instanceID, controller] : m_GameControllers) {
        if (controller) {
            SDL_GameControllerClose(controller);
        }
    }
    m_GameControllers.clear();
    m_JoystickIDToGamepadID.clear();

    // Clear the vibration provider so its captured `this` cannot dangle if the
    // InputManager (a singleton) outlives this adapter.
    if (m_InputManager) {
        m_InputManager->SetVibrationProvider(nullptr);
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

    m_InputManager = nullptr;

}

void SDL2InputAdapter::PollInput() {
    if (!m_InputManager) return;

    UpdateMouseState();

    UpdateGamepadStates();
}

bool SDL2InputAdapter::ProcessEvent(const SDL_Event& event) {
    if (!m_InputManager) return false;

    switch (event.type) {

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            input::KeyCode keyCode = MapSDLKeyToKeyCode(event.key.keysym.sym);
            if (keyCode != input::KeyCode::Unknown) {
                m_InputManager->GetKeyboard()->SetKeyState(keyCode, event.type == SDL_KEYDOWN);

                input::InputEvent ie;
                ie.type = (event.type == SDL_KEYDOWN) ? input::InputEventType::KeyDown
                                                      : input::InputEventType::KeyUp;
                ie.key = keyCode;
                ie.repeat = event.key.repeat != 0;
                m_InputManager->PushInputEvent(ie);
                return true;
            }
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            input::MouseButton button = MapSDLMouseButton(event.button.button);
            if (button != input::MouseButton::Unknown) {
                m_InputManager->GetMouse()->SetButtonState(button, event.type == SDL_MOUSEBUTTONDOWN);

                input::InputEvent ie;
                ie.type = (event.type == SDL_MOUSEBUTTONDOWN) ? input::InputEventType::MouseButtonDown
                                                              : input::InputEventType::MouseButtonUp;
                ie.mouseButton = button;
                ie.position = m_InputManager->GetMouse()->GetPosition();
                m_InputManager->PushInputEvent(ie);
                return true;
            }
            break;
        }

        case SDL_MOUSEMOTION: {
#ifdef __EMSCRIPTEN__
            // On web, SDL reports mouse coordinates in CSS display space, but we need
            // coordinates in internal canvas space (which matches our rendering resolution).
            // Scale the coordinates by the ratio of internal canvas size to CSS display size.
            glm::vec2 scale = GetCanvasScaleFactor();
            float scaledX = static_cast<float>(event.motion.x) * scale.x;
            float scaledY = static_cast<float>(event.motion.y) * scale.y;
            m_InputManager->GetMouse()->SetPosition(glm::vec2(scaledX, scaledY));
#else
            // On desktop, SDL reports coordinates in window pixel space which matches our viewport
            m_InputManager->GetMouse()->SetPosition(
                glm::vec2(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y)));
#endif
            input::InputEvent ie;
            ie.type = input::InputEventType::MouseMotion;
            ie.position = m_InputManager->GetMouse()->GetPosition();
            ie.relative = glm::vec2(static_cast<float>(event.motion.xrel),
                                    static_cast<float>(event.motion.yrel));
            m_InputManager->PushInputEvent(ie);
            return true;
        }

        case SDL_WINDOWEVENT: {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
#ifdef __EMSCRIPTEN__
                // On Emscripten, always use internal canvas size for consistency
                // The resize event might report CSS display size, but we need
                // the internal canvas resolution to match our mouse coordinate scaling
                int canvasWidth = 0, canvasHeight = 0;
                emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);
                m_InputManager->SetWindowSize(canvasWidth, canvasHeight);
#else
                m_InputManager->SetWindowSize(event.window.data1, event.window.data2);
#endif
                return false;
            }
            break;
        }

        case SDL_MOUSEWHEEL: {
            glm::vec2 scrollDelta(event.wheel.x, event.wheel.y);
            m_InputManager->GetMouse()->SetScrollDelta(scrollDelta);

            input::InputEvent ie;
            ie.type = input::InputEventType::MouseWheel;
            ie.scroll = scrollDelta;
            m_InputManager->PushInputEvent(ie);
            return true;
        }

        case SDL_CONTROLLERDEVICEADDED: {
            OpenGameController(event.cdevice.which);
            return true;
        }

        case SDL_CONTROLLERDEVICEREMOVED: {
            CloseGameController(event.cdevice.which);
            return true;
        }

        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
            auto it = m_JoystickIDToGamepadID.find(event.cbutton.which);
            if (it != m_JoystickIDToGamepadID.end()) {
                uint32_t gamepadID = it->second;
                input::GamepadButton button = MapSDLGamepadButton(
                    static_cast<SDL_GameControllerButton>(event.cbutton.button));

                if (button != input::GamepadButton::Unknown) {
                    auto* gamepad = m_InputManager->GetGamepad(gamepadID);
                    if (gamepad) {
                        gamepad->SetButtonState(button, event.type == SDL_CONTROLLERBUTTONDOWN);
                    }

                    input::InputEvent ie;
                    ie.type = (event.type == SDL_CONTROLLERBUTTONDOWN) ? input::InputEventType::GamepadButtonDown
                                                                       : input::InputEventType::GamepadButtonUp;
                    ie.gamepadButton = button;
                    ie.gamepadID = gamepadID;
                    m_InputManager->PushInputEvent(ie);
                }
                return true;
            }
            break;
        }

        case SDL_CONTROLLERAXISMOTION: {
            auto it = m_JoystickIDToGamepadID.find(event.caxis.which);
            if (it != m_JoystickIDToGamepadID.end()) {
                uint32_t gamepadID = it->second;
                input::GamepadAxis axis = MapSDLGamepadAxis(
                    static_cast<SDL_GameControllerAxis>(event.caxis.axis));

                if (axis != input::GamepadAxis::Unknown) {
                    float normalizedValue = event.caxis.value / 32767.0f;
                    auto* gamepad = m_InputManager->GetGamepad(gamepadID);
                    if (gamepad) {
                        gamepad->SetAxisValue(axis, normalizedValue);
                    }

                    input::InputEvent ie;
                    ie.type = input::InputEventType::GamepadAxis;
                    ie.gamepadAxis = axis;
                    ie.axisValue = normalizedValue;
                    ie.gamepadID = gamepadID;
                    m_InputManager->PushInputEvent(ie);
                }
                return true;
            }
            break;
        }

        // Touch events (SDL handles these on mobile/web)
        case SDL_FINGERDOWN: {
            // SDL normalized coordinates (0-1)
            glm::ivec2 windowSize = m_InputManager->GetWindowSize();
            glm::vec2 pos(
                event.tfinger.x * windowSize.x,
                event.tfinger.y * windowSize.y
            );
            m_InputManager->GetTouch()->BeginTouch(
                static_cast<uint32_t>(event.tfinger.fingerId),
                pos,
                event.tfinger.pressure
            );
            return true;
        }

        case SDL_FINGERMOTION: {
            glm::ivec2 windowSize = m_InputManager->GetWindowSize();
            glm::vec2 pos(
                event.tfinger.x * windowSize.x,
                event.tfinger.y * windowSize.y
            );
            m_InputManager->GetTouch()->UpdateTouch(
                static_cast<uint32_t>(event.tfinger.fingerId),
                pos,
                event.tfinger.pressure
            );
            return true;
        }

        case SDL_FINGERUP: {
            m_InputManager->GetTouch()->EndTouch(
                static_cast<uint32_t>(event.tfinger.fingerId)
            );
            return true;
        }

        case SDL_TEXTINPUT: {
            // SDL delivers committed text as UTF-8 (layout/dead-keys/IME resolved).
            // Decode to UTF-32 codepoints so text fields receive proper characters.
            const unsigned char* bytes = reinterpret_cast<const unsigned char*>(event.text.text);
            size_t i = 0;
            while (bytes[i] != '\0') {
                unsigned char lead = bytes[i];
                uint32_t codepoint = 0;
                size_t extra = 0;

                if (lead < 0x80) {
                    codepoint = lead;
                    extra = 0;
                } else if ((lead & 0xE0) == 0xC0) {
                    codepoint = lead & 0x1F;
                    extra = 1;
                } else if ((lead & 0xF0) == 0xE0) {
                    codepoint = lead & 0x0F;
                    extra = 2;
                } else if ((lead & 0xF8) == 0xF0) {
                    codepoint = lead & 0x07;
                    extra = 3;
                } else {
                    // Invalid lead byte; skip it and resync.
                    ++i;
                    continue;
                }

                ++i;
                bool valid = true;
                for (size_t k = 0; k < extra; ++k) {
                    unsigned char cont = bytes[i];
                    if ((cont & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                    codepoint = (codepoint << 6) | (cont & 0x3F);
                    ++i;
                }

                if (valid) {
                    m_InputManager->GetKeyboard()->AddTextInput(codepoint);
                }
            }
            return true;
        }
    }

    return false;
}

void SDL2InputAdapter::InitializeGameControllers() {
    int numJoysticks = SDL_NumJoysticks();

    for (int i = 0; i < numJoysticks; ++i) {
        if (SDL_IsGameController(i)) {
            OpenGameController(i);
        }
    }
}

void SDL2InputAdapter::OpenGameController(int deviceIndex) {
    SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) {

        return;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    SDL_JoystickID instanceID = SDL_JoystickInstanceID(joystick);

    uint32_t gamepadID = m_NextGamepadID++;
    m_GameControllers[instanceID] = controller;
    m_JoystickIDToGamepadID[instanceID] = gamepadID;

    m_InputManager->RegisterGamepad(gamepadID);

    // Record the human-readable name and controller family so glyph/prompt
    // resolution can pick the right button labels (Xbox / PlayStation / Nintendo).
    if (auto* gamepad = m_InputManager->GetGamepad(gamepadID)) {
        const char* name = SDL_GameControllerName(controller);
        if (name) {
            gamepad->SetDeviceName(name);
        }
        gamepad->SetGamepadType(MapSDLGamepadType(SDL_GameControllerGetType(controller)));
    }
}

void SDL2InputAdapter::CloseGameController(SDL_JoystickID instanceID) {
    auto it = m_GameControllers.find(instanceID);
    if (it != m_GameControllers.end()) {

        auto idIt = m_JoystickIDToGamepadID.find(instanceID);
        if (idIt != m_JoystickIDToGamepadID.end()) {
            uint32_t gamepadID = idIt->second;
            m_InputManager->UnregisterGamepad(gamepadID);
            m_JoystickIDToGamepadID.erase(idIt);

        }

        SDL_GameControllerClose(it->second);
        m_GameControllers.erase(it);
    }
}

void SDL2InputAdapter::UpdateMouseState() {

    int mouseX, mouseY;
    Uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);

    (void)buttons;  // Buttons are handled via events

#ifdef __EMSCRIPTEN__
    // On web, SDL reports mouse coordinates in CSS display space, but we need
    // coordinates in internal canvas space (which matches our rendering resolution).
    glm::vec2 scale = GetCanvasScaleFactor();
    float scaledX = static_cast<float>(mouseX) * scale.x;
    float scaledY = static_cast<float>(mouseY) * scale.y;
    m_InputManager->GetMouse()->SetPosition(glm::vec2(scaledX, scaledY));
#else
    // On desktop, SDL reports coordinates in window pixel space which matches our viewport
    m_InputManager->GetMouse()->SetPosition(
        glm::vec2(static_cast<float>(mouseX), static_cast<float>(mouseY)));
#endif
}

void SDL2InputAdapter::UpdateGamepadStates() {

}

input::KeyCode SDL2InputAdapter::MapSDLKeyToKeyCode(SDL_Keycode key) const {

    switch (key) {

        case SDLK_SPACE: return input::KeyCode::Space;
        case SDLK_QUOTE: return input::KeyCode::Apostrophe;
        case SDLK_COMMA: return input::KeyCode::Comma;
        case SDLK_MINUS: return input::KeyCode::Minus;
        case SDLK_PERIOD: return input::KeyCode::Period;
        case SDLK_SLASH: return input::KeyCode::Slash;

        case SDLK_0: return input::KeyCode::D0;
        case SDLK_1: return input::KeyCode::D1;
        case SDLK_2: return input::KeyCode::D2;
        case SDLK_3: return input::KeyCode::D3;
        case SDLK_4: return input::KeyCode::D4;
        case SDLK_5: return input::KeyCode::D5;
        case SDLK_6: return input::KeyCode::D6;
        case SDLK_7: return input::KeyCode::D7;
        case SDLK_8: return input::KeyCode::D8;
        case SDLK_9: return input::KeyCode::D9;

        case SDLK_SEMICOLON: return input::KeyCode::Semicolon;
        case SDLK_EQUALS: return input::KeyCode::Equal;

        case SDLK_a: return input::KeyCode::A;
        case SDLK_b: return input::KeyCode::B;
        case SDLK_c: return input::KeyCode::C;
        case SDLK_d: return input::KeyCode::D;
        case SDLK_e: return input::KeyCode::E;
        case SDLK_f: return input::KeyCode::F;
        case SDLK_g: return input::KeyCode::G;
        case SDLK_h: return input::KeyCode::H;
        case SDLK_i: return input::KeyCode::I;
        case SDLK_j: return input::KeyCode::J;
        case SDLK_k: return input::KeyCode::K;
        case SDLK_l: return input::KeyCode::L;
        case SDLK_m: return input::KeyCode::M;
        case SDLK_n: return input::KeyCode::N;
        case SDLK_o: return input::KeyCode::O;
        case SDLK_p: return input::KeyCode::P;
        case SDLK_q: return input::KeyCode::Q;
        case SDLK_r: return input::KeyCode::R;
        case SDLK_s: return input::KeyCode::S;
        case SDLK_t: return input::KeyCode::T;
        case SDLK_u: return input::KeyCode::U;
        case SDLK_v: return input::KeyCode::V;
        case SDLK_w: return input::KeyCode::W;
        case SDLK_x: return input::KeyCode::X;
        case SDLK_y: return input::KeyCode::Y;
        case SDLK_z: return input::KeyCode::Z;

        case SDLK_LEFTBRACKET: return input::KeyCode::LeftBracket;
        case SDLK_BACKSLASH: return input::KeyCode::Backslash;
        case SDLK_RIGHTBRACKET: return input::KeyCode::RightBracket;
        case SDLK_BACKQUOTE: return input::KeyCode::GraveAccent;

        case SDLK_ESCAPE: return input::KeyCode::Escape;
        case SDLK_RETURN: return input::KeyCode::Enter;
        case SDLK_TAB: return input::KeyCode::Tab;
        case SDLK_BACKSPACE: return input::KeyCode::Backspace;
        case SDLK_INSERT: return input::KeyCode::Insert;
        case SDLK_DELETE: return input::KeyCode::Delete;
        case SDLK_RIGHT: return input::KeyCode::Right;
        case SDLK_LEFT: return input::KeyCode::Left;
        case SDLK_DOWN: return input::KeyCode::Down;
        case SDLK_UP: return input::KeyCode::Up;
        case SDLK_PAGEUP: return input::KeyCode::PageUp;
        case SDLK_PAGEDOWN: return input::KeyCode::PageDown;
        case SDLK_HOME: return input::KeyCode::Home;
        case SDLK_END: return input::KeyCode::End;
        case SDLK_CAPSLOCK: return input::KeyCode::CapsLock;
        case SDLK_SCROLLLOCK: return input::KeyCode::ScrollLock;
        case SDLK_NUMLOCKCLEAR: return input::KeyCode::NumLock;
        case SDLK_PRINTSCREEN: return input::KeyCode::PrintScreen;
        case SDLK_PAUSE: return input::KeyCode::Pause;

        case SDLK_F1: return input::KeyCode::F1;
        case SDLK_F2: return input::KeyCode::F2;
        case SDLK_F3: return input::KeyCode::F3;
        case SDLK_F4: return input::KeyCode::F4;
        case SDLK_F5: return input::KeyCode::F5;
        case SDLK_F6: return input::KeyCode::F6;
        case SDLK_F7: return input::KeyCode::F7;
        case SDLK_F8: return input::KeyCode::F8;
        case SDLK_F9: return input::KeyCode::F9;
        case SDLK_F10: return input::KeyCode::F10;
        case SDLK_F11: return input::KeyCode::F11;
        case SDLK_F12: return input::KeyCode::F12;
        case SDLK_F13: return input::KeyCode::F13;
        case SDLK_F14: return input::KeyCode::F14;
        case SDLK_F15: return input::KeyCode::F15;
        case SDLK_F16: return input::KeyCode::F16;
        case SDLK_F17: return input::KeyCode::F17;
        case SDLK_F18: return input::KeyCode::F18;
        case SDLK_F19: return input::KeyCode::F19;
        case SDLK_F20: return input::KeyCode::F20;
        case SDLK_F21: return input::KeyCode::F21;
        case SDLK_F22: return input::KeyCode::F22;
        case SDLK_F23: return input::KeyCode::F23;
        case SDLK_F24: return input::KeyCode::F24;

        case SDLK_KP_0: return input::KeyCode::KP0;
        case SDLK_KP_1: return input::KeyCode::KP1;
        case SDLK_KP_2: return input::KeyCode::KP2;
        case SDLK_KP_3: return input::KeyCode::KP3;
        case SDLK_KP_4: return input::KeyCode::KP4;
        case SDLK_KP_5: return input::KeyCode::KP5;
        case SDLK_KP_6: return input::KeyCode::KP6;
        case SDLK_KP_7: return input::KeyCode::KP7;
        case SDLK_KP_8: return input::KeyCode::KP8;
        case SDLK_KP_9: return input::KeyCode::KP9;
        case SDLK_KP_PERIOD: return input::KeyCode::KPDecimal;
        case SDLK_KP_DIVIDE: return input::KeyCode::KPDivide;
        case SDLK_KP_MULTIPLY: return input::KeyCode::KPMultiply;
        case SDLK_KP_MINUS: return input::KeyCode::KPSubtract;
        case SDLK_KP_PLUS: return input::KeyCode::KPAdd;
        case SDLK_KP_ENTER: return input::KeyCode::KPEnter;
        case SDLK_KP_EQUALS: return input::KeyCode::KPEqual;

        case SDLK_LSHIFT: return input::KeyCode::LeftShift;
        case SDLK_LCTRL: return input::KeyCode::LeftControl;
        case SDLK_LALT: return input::KeyCode::LeftAlt;
        case SDLK_LGUI: return input::KeyCode::LeftSuper;
        case SDLK_RSHIFT: return input::KeyCode::RightShift;
        case SDLK_RCTRL: return input::KeyCode::RightControl;
        case SDLK_RALT: return input::KeyCode::RightAlt;
        case SDLK_RGUI: return input::KeyCode::RightSuper;
        case SDLK_MENU: return input::KeyCode::Menu;

        default:
            return input::KeyCode::Unknown;
    }
}

input::MouseButton SDL2InputAdapter::MapSDLMouseButton(uint8_t button) const {
    switch (button) {
        case SDL_BUTTON_LEFT: return input::MouseButton::Left;
        case SDL_BUTTON_RIGHT: return input::MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return input::MouseButton::Middle;
        case SDL_BUTTON_X1: return input::MouseButton::Button4;
        case SDL_BUTTON_X2: return input::MouseButton::Button5;
        default: return input::MouseButton::Unknown;
    }
}

input::GamepadButton SDL2InputAdapter::MapSDLGamepadButton(SDL_GameControllerButton button) const {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A: return input::GamepadButton::A;
        case SDL_CONTROLLER_BUTTON_B: return input::GamepadButton::B;
        case SDL_CONTROLLER_BUTTON_X: return input::GamepadButton::X;
        case SDL_CONTROLLER_BUTTON_Y: return input::GamepadButton::Y;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return input::GamepadButton::LeftBumper;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return input::GamepadButton::RightBumper;
        case SDL_CONTROLLER_BUTTON_BACK: return input::GamepadButton::Back;
        case SDL_CONTROLLER_BUTTON_START: return input::GamepadButton::Start;
        case SDL_CONTROLLER_BUTTON_GUIDE: return input::GamepadButton::Guide;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return input::GamepadButton::LeftThumb;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return input::GamepadButton::RightThumb;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return input::GamepadButton::DPadUp;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return input::GamepadButton::DPadRight;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return input::GamepadButton::DPadDown;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return input::GamepadButton::DPadLeft;
        default: return input::GamepadButton::Unknown;
    }
}

input::GamepadAxis SDL2InputAdapter::MapSDLGamepadAxis(SDL_GameControllerAxis axis) const {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: return input::GamepadAxis::LeftX;
        case SDL_CONTROLLER_AXIS_LEFTY: return input::GamepadAxis::LeftY;
        case SDL_CONTROLLER_AXIS_RIGHTX: return input::GamepadAxis::RightX;
        case SDL_CONTROLLER_AXIS_RIGHTY: return input::GamepadAxis::RightY;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return input::GamepadAxis::LeftTrigger;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return input::GamepadAxis::RightTrigger;
        default: return input::GamepadAxis::Unknown;
    }
}

input::GamepadType SDL2InputAdapter::MapSDLGamepadType(SDL_GameControllerType type) const {
    switch (type) {
        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE:
            return input::GamepadType::Xbox;

        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
#if SDL_VERSION_ATLEAST(2, 0, 14)
        case SDL_CONTROLLER_TYPE_PS5:
#endif
            return input::GamepadType::PlayStation;

#if SDL_VERSION_ATLEAST(2, 0, 12)
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#endif
#if SDL_VERSION_ATLEAST(2, 24, 0)
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#endif
            return input::GamepadType::Nintendo;

        case SDL_CONTROLLER_TYPE_UNKNOWN:
            return input::GamepadType::Unknown;

        default:
            // Virtual / Amazon Luna / Google Stadia / NVIDIA SHIELD and any other
            // recognized-but-unmapped controllers use the generic glyph set.
            return input::GamepadType::Generic;
    }
}

// ============================================================================
// Emscripten/Web-specific implementations
// ============================================================================

#ifdef __EMSCRIPTEN__

void SDL2InputAdapter::InitializeEmscriptenCallbacks() {
    s_Instance = this;

    // Set up Emscripten-specific input callbacks for better browser integration
    // These provide lower-latency input than SDL events in some browsers

    // Keyboard callbacks - capture on canvas for game input
    emscripten_set_keydown_callback("#canvas", this, EM_TRUE, EmscriptenKeyCallback);
    emscripten_set_keyup_callback("#canvas", this, EM_TRUE, EmscriptenKeyCallback);

    // Also capture on window for when canvas doesn't have focus
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_FALSE, EmscriptenKeyCallback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_FALSE, EmscriptenKeyCallback);

    // Touch callbacks for mobile web
    emscripten_set_touchstart_callback("#canvas", this, EM_TRUE, EmscriptenTouchCallback);
    emscripten_set_touchend_callback("#canvas", this, EM_TRUE, EmscriptenTouchCallback);
    emscripten_set_touchmove_callback("#canvas", this, EM_TRUE, EmscriptenTouchCallback);
    emscripten_set_touchcancel_callback("#canvas", this, EM_TRUE, EmscriptenTouchCallback);

    // Mouse wheel for better scroll handling
    emscripten_set_wheel_callback("#canvas", this, EM_TRUE, EmscriptenWheelCallback);

}

void SDL2InputAdapter::ShutdownEmscriptenCallbacks() {
    // Remove callbacks
    emscripten_set_keydown_callback("#canvas", nullptr, EM_TRUE, nullptr);
    emscripten_set_keyup_callback("#canvas", nullptr, EM_TRUE, nullptr);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, nullptr);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, nullptr);
    emscripten_set_touchstart_callback("#canvas", nullptr, EM_TRUE, nullptr);
    emscripten_set_touchend_callback("#canvas", nullptr, EM_TRUE, nullptr);
    emscripten_set_touchmove_callback("#canvas", nullptr, EM_TRUE, nullptr);
    emscripten_set_touchcancel_callback("#canvas", nullptr, EM_TRUE, nullptr);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, nullptr);

    s_Instance = nullptr;
}

glm::vec2 SDL2InputAdapter::GetCanvasScaleFactor() const {
    // Get the internal canvas rendering resolution (canvas.width/height in JS)
    int canvasWidth = 0, canvasHeight = 0;
    emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);

    // Get the CSS display size (the actual size shown on screen)
    double cssWidth = 0.0, cssHeight = 0.0;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);

    // Calculate scale factor: mouse coords are in CSS space, we need canvas space
    // If CSS size is smaller than canvas size, we need to scale up mouse coords
    glm::vec2 scale(1.0f, 1.0f);
    if (cssWidth > 0.0 && cssHeight > 0.0) {
        scale.x = static_cast<float>(canvasWidth) / static_cast<float>(cssWidth);
        scale.y = static_cast<float>(canvasHeight) / static_cast<float>(cssHeight);
    }

    return scale;
}

EM_BOOL SDL2InputAdapter::EmscriptenKeyCallback(int eventType, const EmscriptenKeyboardEvent* event, void* userData) {
    SDL2InputAdapter* adapter = static_cast<SDL2InputAdapter*>(userData);
    if (!adapter || !adapter->m_InputManager) return EM_FALSE;

    // Map Emscripten key code to SDL keycode, then to our KeyCode
    // The event->key contains the key name (e.g., "KeyA", "ArrowUp", "Space")
    // event->code contains the physical key code

    SDL_Keycode sdlKey = SDLK_UNKNOWN;

    // Map common keys from event->code (physical key location)
    const char* code = event->code;

    // Letter keys
    if (code[0] == 'K' && code[1] == 'e' && code[2] == 'y' && code[3] >= 'A' && code[3] <= 'Z') {
        sdlKey = SDLK_a + (code[3] - 'A');
    }
    // Digit keys
    else if (code[0] == 'D' && code[1] == 'i' && code[2] == 'g' && code[3] == 'i' && code[4] == 't') {
        sdlKey = SDLK_0 + (code[5] - '0');
    }
    // Function keys
    else if (code[0] == 'F' && code[1] >= '1' && code[1] <= '9') {
        if (code[2] == '\0') {
            sdlKey = SDLK_F1 + (code[1] - '1');
        } else if (code[1] == '1' && code[2] >= '0' && code[2] <= '2') {
            sdlKey = SDLK_F10 + (code[2] - '0');
        }
    }
    // Arrow keys
    else if (strcmp(code, "ArrowUp") == 0) sdlKey = SDLK_UP;
    else if (strcmp(code, "ArrowDown") == 0) sdlKey = SDLK_DOWN;
    else if (strcmp(code, "ArrowLeft") == 0) sdlKey = SDLK_LEFT;
    else if (strcmp(code, "ArrowRight") == 0) sdlKey = SDLK_RIGHT;
    // Special keys
    else if (strcmp(code, "Space") == 0) sdlKey = SDLK_SPACE;
    else if (strcmp(code, "Enter") == 0) sdlKey = SDLK_RETURN;
    else if (strcmp(code, "Escape") == 0) sdlKey = SDLK_ESCAPE;
    else if (strcmp(code, "Tab") == 0) sdlKey = SDLK_TAB;
    else if (strcmp(code, "Backspace") == 0) sdlKey = SDLK_BACKSPACE;
    else if (strcmp(code, "Delete") == 0) sdlKey = SDLK_DELETE;
    else if (strcmp(code, "Insert") == 0) sdlKey = SDLK_INSERT;
    else if (strcmp(code, "Home") == 0) sdlKey = SDLK_HOME;
    else if (strcmp(code, "End") == 0) sdlKey = SDLK_END;
    else if (strcmp(code, "PageUp") == 0) sdlKey = SDLK_PAGEUP;
    else if (strcmp(code, "PageDown") == 0) sdlKey = SDLK_PAGEDOWN;
    // Modifier keys
    else if (strcmp(code, "ShiftLeft") == 0) sdlKey = SDLK_LSHIFT;
    else if (strcmp(code, "ShiftRight") == 0) sdlKey = SDLK_RSHIFT;
    else if (strcmp(code, "ControlLeft") == 0) sdlKey = SDLK_LCTRL;
    else if (strcmp(code, "ControlRight") == 0) sdlKey = SDLK_RCTRL;
    else if (strcmp(code, "AltLeft") == 0) sdlKey = SDLK_LALT;
    else if (strcmp(code, "AltRight") == 0) sdlKey = SDLK_RALT;
    else if (strcmp(code, "MetaLeft") == 0) sdlKey = SDLK_LGUI;  // Command/Windows key
    else if (strcmp(code, "MetaRight") == 0) sdlKey = SDLK_RGUI;
    // Punctuation
    else if (strcmp(code, "Comma") == 0) sdlKey = SDLK_COMMA;
    else if (strcmp(code, "Period") == 0) sdlKey = SDLK_PERIOD;
    else if (strcmp(code, "Slash") == 0) sdlKey = SDLK_SLASH;
    else if (strcmp(code, "Semicolon") == 0) sdlKey = SDLK_SEMICOLON;
    else if (strcmp(code, "Quote") == 0) sdlKey = SDLK_QUOTE;
    else if (strcmp(code, "BracketLeft") == 0) sdlKey = SDLK_LEFTBRACKET;
    else if (strcmp(code, "BracketRight") == 0) sdlKey = SDLK_RIGHTBRACKET;
    else if (strcmp(code, "Backslash") == 0) sdlKey = SDLK_BACKSLASH;
    else if (strcmp(code, "Backquote") == 0) sdlKey = SDLK_BACKQUOTE;
    else if (strcmp(code, "Minus") == 0) sdlKey = SDLK_MINUS;
    else if (strcmp(code, "Equal") == 0) sdlKey = SDLK_EQUALS;

    if (sdlKey != SDLK_UNKNOWN) {
        input::KeyCode keyCode = adapter->MapSDLKeyToKeyCode(sdlKey);
        if (keyCode != input::KeyCode::Unknown) {
            bool pressed = (eventType == EMSCRIPTEN_EVENT_KEYDOWN);
            adapter->m_InputManager->GetKeyboard()->SetKeyState(keyCode, pressed);
            return EM_TRUE;  // Event consumed
        }
    }

    return EM_FALSE;  // Let browser handle unrecognized keys
}

EM_BOOL SDL2InputAdapter::EmscriptenMouseCallback(int eventType, const EmscriptenMouseEvent* event, void* userData) {
    // Mouse events are typically handled by SDL, but this can be used for additional processing
    (void)eventType;
    (void)event;
    (void)userData;
    return EM_FALSE;
}

EM_BOOL SDL2InputAdapter::EmscriptenTouchCallback(int eventType, const EmscriptenTouchEvent* event, void* userData) {
    SDL2InputAdapter* adapter = static_cast<SDL2InputAdapter*>(userData);
    if (!adapter || !adapter->m_InputManager) return EM_FALSE;

    // For simple games, map first touch to mouse position and left click
    if (event->numTouches > 0) {
        const EmscriptenTouchPoint& touch = event->touches[0];

        // Touch targetX/Y are in CSS pixels - scale to internal canvas resolution
        // using the same approach as mouse coordinates
        glm::vec2 scale = adapter->GetCanvasScaleFactor();
        float scaledX = static_cast<float>(touch.targetX) * scale.x;
        float scaledY = static_cast<float>(touch.targetY) * scale.y;

        // Update mouse position from touch
        adapter->m_InputManager->GetMouse()->SetPosition(glm::vec2(scaledX, scaledY));

        // Map touch to left mouse button
        bool pressed = (eventType == EMSCRIPTEN_EVENT_TOUCHSTART || eventType == EMSCRIPTEN_EVENT_TOUCHMOVE);
        if (eventType == EMSCRIPTEN_EVENT_TOUCHEND || eventType == EMSCRIPTEN_EVENT_TOUCHCANCEL) {
            pressed = false;
        }
        adapter->m_InputManager->GetMouse()->SetButtonState(input::MouseButton::Left, pressed);
    }

    return EM_TRUE;  // Consume touch events to prevent mouse emulation issues
}

EM_BOOL SDL2InputAdapter::EmscriptenWheelCallback(int eventType, const EmscriptenWheelEvent* event, void* userData) {
    SDL2InputAdapter* adapter = static_cast<SDL2InputAdapter*>(userData);
    if (!adapter || !adapter->m_InputManager) return EM_FALSE;

    (void)eventType;

    // Normalize wheel delta (browsers report different units)
    float deltaX = static_cast<float>(event->deltaX);
    float deltaY = static_cast<float>(event->deltaY);

    // Normalize based on deltaMode
    switch (event->deltaMode) {
        case DOM_DELTA_PIXEL:
            deltaX /= 100.0f;
            deltaY /= 100.0f;
            break;
        case DOM_DELTA_LINE:
            // Line-based scrolling, values are typically reasonable
            break;
        case DOM_DELTA_PAGE:
            deltaX *= 10.0f;
            deltaY *= 10.0f;
            break;
    }

    adapter->m_InputManager->GetMouse()->SetScrollDelta(glm::vec2(deltaX, -deltaY));  // Invert Y for natural scrolling

    return EM_TRUE;
}

bool SDL2InputAdapter::ProcessTouchEvent(const SDL_Event& event) {
    // SDL touch events - convert to mouse for simple games
    if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP || event.type == SDL_FINGERMOTION) {
        // SDL normalized coordinates (0-1) - these are already normalized to [0,1] range
        // relative to the window, so we just convert to game coordinates using window size
        float x = event.tfinger.x;
        float y = event.tfinger.y;

        // Convert to game coordinates (window size is already in game space)
        glm::ivec2 windowSize = m_InputManager->GetWindowSize();
        m_InputManager->GetMouse()->SetPosition(
            glm::vec2(x * static_cast<float>(windowSize.x), y * static_cast<float>(windowSize.y))
        );

        // Map to left mouse button
        bool pressed = (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERMOTION);
        m_InputManager->GetMouse()->SetButtonState(input::MouseButton::Left, pressed);

        return true;
    }
    return false;
}

#endif // __EMSCRIPTEN__

}
}
