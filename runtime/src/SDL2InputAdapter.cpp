#include "lupine/runtime/SDL2InputAdapter.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace runtime {

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

    return true;
}

void SDL2InputAdapter::Shutdown() {

    for (auto& [instanceID, controller] : m_GameControllers) {
        if (controller) {
            SDL_GameControllerClose(controller);
        }
    }
    m_GameControllers.clear();
    m_JoystickIDToGamepadID.clear();

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

    m_InputManager = nullptr;

}

void SDL2InputAdapter::PollInput() {
    if (!m_InputManager) return;

    UpdateKeyboardState();

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
                return true;
            }
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            input::MouseButton button = MapSDLMouseButton(event.button.button);
            if (button != input::MouseButton::Unknown) {
                m_InputManager->GetMouse()->SetButtonState(button, event.type == SDL_MOUSEBUTTONDOWN);
                return true;
            }
            break;
        }

        case SDL_MOUSEMOTION: {

            m_InputManager->GetMouse()->SetPosition(glm::vec2(event.motion.x, event.motion.y));
            return true;
        }

        case SDL_WINDOWEVENT: {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {

                m_InputManager->SetWindowSize(event.window.data1, event.window.data2);
                return false;
            }
            break;
        }

        case SDL_MOUSEWHEEL: {
            glm::vec2 scrollDelta(event.wheel.x, event.wheel.y);
            m_InputManager->GetMouse()->SetScrollDelta(scrollDelta);
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
                    auto* gamepad = m_InputManager->GetGamepad(gamepadID);
                    if (gamepad) {

                        float normalizedValue = event.caxis.value / 32767.0f;
                        gamepad->SetAxisValue(axis, normalizedValue);
                    }
                }
                return true;
            }
            break;
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

    const char* name = SDL_GameControllerName(controller);

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

void SDL2InputAdapter::UpdateKeyboardState() {

    const Uint8* keyState = SDL_GetKeyboardState(nullptr);

}

void SDL2InputAdapter::UpdateMouseState() {

    int mouseX, mouseY;
    Uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);

    int globalX, globalY;
    SDL_GetGlobalMouseState(&globalX, &globalY);

    m_InputManager->GetMouse()->SetPosition(glm::vec2(static_cast<float>(mouseX), static_cast<float>(mouseY)));

    glm::vec2 verifyPos = m_InputManager->GetMouse()->GetPosition();

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

}
}
