#include "lupine/input/InputCodes.hpp"
#include <unordered_map>

namespace lupine {
namespace input {

static const std::unordered_map<KeyCode, std::string> s_KeyCodeToString = {
    {KeyCode::Space, "Space"},
    {KeyCode::Apostrophe, "Apostrophe"},
    {KeyCode::Comma, "Comma"},
    {KeyCode::Minus, "Minus"},
    {KeyCode::Period, "Period"},
    {KeyCode::Slash, "Slash"},

    {KeyCode::D0, "0"}, {KeyCode::D1, "1"}, {KeyCode::D2, "2"},
    {KeyCode::D3, "3"}, {KeyCode::D4, "4"}, {KeyCode::D5, "5"},
    {KeyCode::D6, "6"}, {KeyCode::D7, "7"}, {KeyCode::D8, "8"}, {KeyCode::D9, "9"},

    {KeyCode::Semicolon, "Semicolon"},
    {KeyCode::Equal, "Equal"},

    {KeyCode::A, "A"}, {KeyCode::B, "B"}, {KeyCode::C, "C"}, {KeyCode::D, "D"},
    {KeyCode::E, "E"}, {KeyCode::F, "F"}, {KeyCode::G, "G"}, {KeyCode::H, "H"},
    {KeyCode::I, "I"}, {KeyCode::J, "J"}, {KeyCode::K, "K"}, {KeyCode::L, "L"},
    {KeyCode::M, "M"}, {KeyCode::N, "N"}, {KeyCode::O, "O"}, {KeyCode::P, "P"},
    {KeyCode::Q, "Q"}, {KeyCode::R, "R"}, {KeyCode::S, "S"}, {KeyCode::T, "T"},
    {KeyCode::U, "U"}, {KeyCode::V, "V"}, {KeyCode::W, "W"}, {KeyCode::X, "X"},
    {KeyCode::Y, "Y"}, {KeyCode::Z, "Z"},

    {KeyCode::LeftBracket, "LeftBracket"},
    {KeyCode::Backslash, "Backslash"},
    {KeyCode::RightBracket, "RightBracket"},
    {KeyCode::GraveAccent, "GraveAccent"},

    {KeyCode::Escape, "Escape"},
    {KeyCode::Enter, "Enter"},
    {KeyCode::Tab, "Tab"},
    {KeyCode::Backspace, "Backspace"},
    {KeyCode::Insert, "Insert"},
    {KeyCode::Delete, "Delete"},
    {KeyCode::Right, "Right"},
    {KeyCode::Left, "Left"},
    {KeyCode::Down, "Down"},
    {KeyCode::Up, "Up"},
    {KeyCode::PageUp, "PageUp"},
    {KeyCode::PageDown, "PageDown"},
    {KeyCode::Home, "Home"},
    {KeyCode::End, "End"},

    {KeyCode::CapsLock, "CapsLock"},
    {KeyCode::ScrollLock, "ScrollLock"},
    {KeyCode::NumLock, "NumLock"},
    {KeyCode::PrintScreen, "PrintScreen"},
    {KeyCode::Pause, "Pause"},

    {KeyCode::F1, "F1"}, {KeyCode::F2, "F2"}, {KeyCode::F3, "F3"}, {KeyCode::F4, "F4"},
    {KeyCode::F5, "F5"}, {KeyCode::F6, "F6"}, {KeyCode::F7, "F7"}, {KeyCode::F8, "F8"},
    {KeyCode::F9, "F9"}, {KeyCode::F10, "F10"}, {KeyCode::F11, "F11"}, {KeyCode::F12, "F12"},

    {KeyCode::LeftShift, "LeftShift"},
    {KeyCode::LeftControl, "LeftControl"},
    {KeyCode::LeftAlt, "LeftAlt"},
    {KeyCode::LeftSuper, "LeftSuper"},
    {KeyCode::RightShift, "RightShift"},
    {KeyCode::RightControl, "RightControl"},
    {KeyCode::RightAlt, "RightAlt"},
    {KeyCode::RightSuper, "RightSuper"},
    {KeyCode::Menu, "Menu"},

    {KeyCode::Unknown, "Unknown"}
};

std::string KeyCodeToString(KeyCode code) {
    auto it = s_KeyCodeToString.find(code);
    return it != s_KeyCodeToString.end() ? it->second : "Unknown";
}

KeyCode StringToKeyCode(const std::string& str) {
    for (const auto& [code, name] : s_KeyCodeToString) {
        if (name == str) return code;
    }
    return KeyCode::Unknown;
}

std::string MouseButtonToString(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "Left";
        case MouseButton::Right: return "Right";
        case MouseButton::Middle: return "Middle";
        case MouseButton::Button4: return "Button4";
        case MouseButton::Button5: return "Button5";
        case MouseButton::Button6: return "Button6";
        case MouseButton::Button7: return "Button7";
        case MouseButton::Button8: return "Button8";
        default: return "Unknown";
    }
}

MouseButton StringToMouseButton(const std::string& str) {
    if (str == "Left") return MouseButton::Left;
    if (str == "Right") return MouseButton::Right;
    if (str == "Middle") return MouseButton::Middle;
    if (str == "Button4") return MouseButton::Button4;
    if (str == "Button5") return MouseButton::Button5;
    if (str == "Button6") return MouseButton::Button6;
    if (str == "Button7") return MouseButton::Button7;
    if (str == "Button8") return MouseButton::Button8;
    return MouseButton::Unknown;
}

std::string GamepadButtonToString(GamepadButton button) {
    switch (button) {
        case GamepadButton::A: return "A";
        case GamepadButton::B: return "B";
        case GamepadButton::X: return "X";
        case GamepadButton::Y: return "Y";
        case GamepadButton::LeftBumper: return "LeftBumper";
        case GamepadButton::RightBumper: return "RightBumper";
        case GamepadButton::Back: return "Back";
        case GamepadButton::Start: return "Start";
        case GamepadButton::Guide: return "Guide";
        case GamepadButton::LeftThumb: return "LeftThumb";
        case GamepadButton::RightThumb: return "RightThumb";
        case GamepadButton::DPadUp: return "DPadUp";
        case GamepadButton::DPadRight: return "DPadRight";
        case GamepadButton::DPadDown: return "DPadDown";
        case GamepadButton::DPadLeft: return "DPadLeft";
        default: return "Unknown";
    }
}

GamepadButton StringToGamepadButton(const std::string& str) {
    if (str == "A") return GamepadButton::A;
    if (str == "B") return GamepadButton::B;
    if (str == "X") return GamepadButton::X;
    if (str == "Y") return GamepadButton::Y;
    if (str == "LeftBumper") return GamepadButton::LeftBumper;
    if (str == "RightBumper") return GamepadButton::RightBumper;
    if (str == "Back") return GamepadButton::Back;
    if (str == "Start") return GamepadButton::Start;
    if (str == "Guide") return GamepadButton::Guide;
    if (str == "LeftThumb") return GamepadButton::LeftThumb;
    if (str == "RightThumb") return GamepadButton::RightThumb;
    if (str == "DPadUp") return GamepadButton::DPadUp;
    if (str == "DPadRight") return GamepadButton::DPadRight;
    if (str == "DPadDown") return GamepadButton::DPadDown;
    if (str == "DPadLeft") return GamepadButton::DPadLeft;
    return GamepadButton::Unknown;
}

std::string GamepadAxisToString(GamepadAxis axis) {
    switch (axis) {
        case GamepadAxis::LeftX: return "LeftX";
        case GamepadAxis::LeftY: return "LeftY";
        case GamepadAxis::RightX: return "RightX";
        case GamepadAxis::RightY: return "RightY";
        case GamepadAxis::LeftTrigger: return "LeftTrigger";
        case GamepadAxis::RightTrigger: return "RightTrigger";
        default: return "Unknown";
    }
}

GamepadAxis StringToGamepadAxis(const std::string& str) {
    if (str == "LeftX") return GamepadAxis::LeftX;
    if (str == "LeftY") return GamepadAxis::LeftY;
    if (str == "RightX") return GamepadAxis::RightX;
    if (str == "RightY") return GamepadAxis::RightY;
    if (str == "LeftTrigger") return GamepadAxis::LeftTrigger;
    if (str == "RightTrigger") return GamepadAxis::RightTrigger;
    return GamepadAxis::Unknown;
}

std::string GamepadTypeToString(GamepadType type) {
    switch (type) {
        case GamepadType::Xbox: return "Xbox";
        case GamepadType::PlayStation: return "PlayStation";
        case GamepadType::Nintendo: return "Nintendo";
        case GamepadType::Steam: return "Steam";
        case GamepadType::Generic: return "Generic";
        default: return "Unknown";
    }
}

GamepadType StringToGamepadType(const std::string& str) {
    if (str == "Xbox") return GamepadType::Xbox;
    if (str == "PlayStation") return GamepadType::PlayStation;
    if (str == "Nintendo") return GamepadType::Nintendo;
    if (str == "Steam") return GamepadType::Steam;
    if (str == "Generic") return GamepadType::Generic;
    return GamepadType::Unknown;
}

std::string GamepadTypePrefix(GamepadType type) {
    switch (type) {
        case GamepadType::Xbox: return "xbox";
        case GamepadType::PlayStation: return "ps";
        case GamepadType::Nintendo: return "switch";
        case GamepadType::Steam: return "steam";
        default: return "generic";
    }
}

std::string GamepadFaceLabel(GamepadButton button, GamepadType type) {
    // PlayStation uses shape names for the face buttons and L1/L2/R1/R2 for the
    // shoulders; Share/Options for select/start. The DualShock/DualSense home
    // button is the PS button.
    if (type == GamepadType::PlayStation) {
        switch (button) {
            case GamepadButton::A: return "Cross";
            case GamepadButton::B: return "Circle";
            case GamepadButton::X: return "Square";
            case GamepadButton::Y: return "Triangle";
            case GamepadButton::LeftBumper: return "L1";
            case GamepadButton::RightBumper: return "R1";
            case GamepadButton::Back: return "Share";
            case GamepadButton::Start: return "Options";
            case GamepadButton::Guide: return "PS";
            case GamepadButton::LeftThumb: return "L3";
            case GamepadButton::RightThumb: return "R3";
            case GamepadButton::DPadUp: return "D-Pad Up";
            case GamepadButton::DPadRight: return "D-Pad Right";
            case GamepadButton::DPadDown: return "D-Pad Down";
            case GamepadButton::DPadLeft: return "D-Pad Left";
            default: return "Unknown";
        }
    }

    // Nintendo controllers swap the physical A/B and X/Y positions relative to
    // Xbox. SDL reports by position (south == A), so the south button is labeled
    // "B", east "A", west "Y", north "X". Shoulders are L/R, select/start are -/+.
    if (type == GamepadType::Nintendo) {
        switch (button) {
            case GamepadButton::A: return "B";
            case GamepadButton::B: return "A";
            case GamepadButton::X: return "Y";
            case GamepadButton::Y: return "X";
            case GamepadButton::LeftBumper: return "L";
            case GamepadButton::RightBumper: return "R";
            case GamepadButton::Back: return "-";
            case GamepadButton::Start: return "+";
            case GamepadButton::Guide: return "Home";
            case GamepadButton::LeftThumb: return "L Stick";
            case GamepadButton::RightThumb: return "R Stick";
            case GamepadButton::DPadUp: return "D-Pad Up";
            case GamepadButton::DPadRight: return "D-Pad Right";
            case GamepadButton::DPadDown: return "D-Pad Down";
            case GamepadButton::DPadLeft: return "D-Pad Left";
            default: return "Unknown";
        }
    }

    // Xbox / Steam / Generic / Unknown all use the Xbox-style ABXY layout.
    switch (button) {
        case GamepadButton::A: return "A";
        case GamepadButton::B: return "B";
        case GamepadButton::X: return "X";
        case GamepadButton::Y: return "Y";
        case GamepadButton::LeftBumper: return "LB";
        case GamepadButton::RightBumper: return "RB";
        case GamepadButton::Back: return "Back";
        case GamepadButton::Start: return "Start";
        case GamepadButton::Guide: return "Guide";
        case GamepadButton::LeftThumb: return "L Stick";
        case GamepadButton::RightThumb: return "R Stick";
        case GamepadButton::DPadUp: return "D-Pad Up";
        case GamepadButton::DPadRight: return "D-Pad Right";
        case GamepadButton::DPadDown: return "D-Pad Down";
        case GamepadButton::DPadLeft: return "D-Pad Left";
        default: return "Unknown";
    }
}

std::string GamepadAxisLabel(GamepadAxis axis, GamepadType type) {
    switch (axis) {
        case GamepadAxis::LeftX: return "Left Stick X";
        case GamepadAxis::LeftY: return "Left Stick Y";
        case GamepadAxis::RightX: return "Right Stick X";
        case GamepadAxis::RightY: return "Right Stick Y";
        case GamepadAxis::LeftTrigger:
            if (type == GamepadType::PlayStation) return "L2";
            if (type == GamepadType::Nintendo) return "ZL";
            return "LT";
        case GamepadAxis::RightTrigger:
            if (type == GamepadType::PlayStation) return "R2";
            if (type == GamepadType::Nintendo) return "ZR";
            return "RT";
        default: return "Unknown";
    }
}

}
}
