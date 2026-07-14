#pragma once

#include <cstdint>
#include <string>

namespace lupine {
namespace input {

/**
 * Keyboard key codes
 * Based on GLFW key codes for compatibility
 */
enum class KeyCode : uint16_t {
    // Printable keys
    Space = 32,
    Apostrophe = 39,  /* ' */
    Comma = 44,       /* , */
    Minus = 45,       /* - */
    Period = 46,      /* . */
    Slash = 47,       /* / */

    D0 = 48,          /* 0 */
    D1 = 49,          /* 1 */
    D2 = 50,          /* 2 */
    D3 = 51,          /* 3 */
    D4 = 52,          /* 4 */
    D5 = 53,          /* 5 */
    D6 = 54,          /* 6 */
    D7 = 55,          /* 7 */
    D8 = 56,          /* 8 */
    D9 = 57,          /* 9 */

    Semicolon = 59,   /* ; */
    Equal = 61,       /* = */

    A = 65,
    B = 66,
    C = 67,
    D = 68,
    E = 69,
    F = 70,
    G = 71,
    H = 72,
    I = 73,
    J = 74,
    K = 75,
    L = 76,
    M = 77,
    N = 78,
    O = 79,
    P = 80,
    Q = 81,
    R = 82,
    S = 83,
    T = 84,
    U = 85,
    V = 86,
    W = 87,
    X = 88,
    Y = 89,
    Z = 90,

    LeftBracket = 91,  /* [ */
    Backslash = 92,    /* \ */
    RightBracket = 93, /* ] */
    GraveAccent = 96,  /* ` */

    World1 = 161,      /* non-US #1 */
    World2 = 162,      /* non-US #2 */

    // Function keys
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    F1 = 290,
    F2 = 291,
    F3 = 292,
    F4 = 293,
    F5 = 294,
    F6 = 295,
    F7 = 296,
    F8 = 297,
    F9 = 298,
    F10 = 299,
    F11 = 300,
    F12 = 301,
    F13 = 302,
    F14 = 303,
    F15 = 304,
    F16 = 305,
    F17 = 306,
    F18 = 307,
    F19 = 308,
    F20 = 309,
    F21 = 310,
    F22 = 311,
    F23 = 312,
    F24 = 313,
    F25 = 314,

    // Keypad
    KP0 = 320,
    KP1 = 321,
    KP2 = 322,
    KP3 = 323,
    KP4 = 324,
    KP5 = 325,
    KP6 = 326,
    KP7 = 327,
    KP8 = 328,
    KP9 = 329,
    KPDecimal = 330,
    KPDivide = 331,
    KPMultiply = 332,
    KPSubtract = 333,
    KPAdd = 334,
    KPEnter = 335,
    KPEqual = 336,

    // Modifiers
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348,

    // Special
    Unknown = 0
};

/**
 * Mouse button codes
 */
enum class MouseButton : uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,

    Unknown = 255
};

/**
 * Gamepad button codes
 */
enum class GamepadButton : uint8_t {
    // Face buttons (Xbox naming)
    A = 0,              // South (PlayStation Cross)
    B = 1,              // East (PlayStation Circle)
    X = 2,              // West (PlayStation Square)
    Y = 3,              // North (PlayStation Triangle)

    // Shoulder buttons
    LeftBumper = 4,
    RightBumper = 5,

    // Select/Start
    Back = 6,           // Select
    Start = 7,
    Guide = 8,          // Home/PS button

    // Stick clicks
    LeftThumb = 9,
    RightThumb = 10,

    // D-Pad
    DPadUp = 11,
    DPadRight = 12,
    DPadDown = 13,
    DPadLeft = 14,

    Unknown = 255
};

/**
 * Gamepad axis codes
 */
enum class GamepadAxis : uint8_t {
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,
    RightTrigger = 5,

    Unknown = 255
};

/**
 * Input device types
 */
enum class InputDeviceType : uint8_t {
    Keyboard,
    Mouse,
    Gamepad,
    Touch,
    Unknown
};

/**
 * Physical gamepad family.
 *
 * Used to pick the correct on-screen button glyphs/labels for a controller
 * (e.g. Xbox "A" vs PlayStation "Cross" vs the swapped Nintendo layout).
 * Reported by the platform layer (SDL_GameControllerGetType).
 */
enum class GamepadType : uint8_t {
    Unknown = 0,
    Xbox,
    PlayStation,
    Nintendo,
    Steam,
    Generic
};

/**
 * Resolved on-screen prompt for a single binding.
 *
 * glyphId is a stable, device-aware identifier a game can map to its own art
 * (e.g. "gamepad_xbox_a", "key_space", "mouse_left"). label is a human-readable
 * fallback string ("A", "Space", "Left Click"). artPath is an optional texture
 * path supplied by the game (empty unless overridden). Both label and artPath are
 * user-overridable through the InputManager glyph map.
 */
struct InputGlyph {
    std::string glyphId;
    std::string label;
    std::string artPath;
    InputDeviceType device = InputDeviceType::Unknown;
    GamepadType gamepadType = GamepadType::Unknown;
};

// Utility functions for converting codes to strings
std::string KeyCodeToString(KeyCode code);
std::string MouseButtonToString(MouseButton button);
std::string GamepadButtonToString(GamepadButton button);
std::string GamepadAxisToString(GamepadAxis axis);

KeyCode StringToKeyCode(const std::string& str);
MouseButton StringToMouseButton(const std::string& str);
GamepadButton StringToGamepadButton(const std::string& str);
GamepadAxis StringToGamepadAxis(const std::string& str);

// Gamepad family <-> string, and the short identifier prefix used in glyph ids
// ("xbox", "ps", "switch", "steam", "generic").
std::string GamepadTypeToString(GamepadType type);
GamepadType StringToGamepadType(const std::string& str);
std::string GamepadTypePrefix(GamepadType type);

/**
 * Human-readable face/button label for a gamepad button, honoring the controller
 * family. For example GamepadButton::A reads "A" on Xbox, "Cross" on PlayStation,
 * and "B" on Nintendo (whose physical A/B and X/Y positions are swapped). Returns
 * the Xbox-style name for Unknown/Generic.
 */
std::string GamepadFaceLabel(GamepadButton button, GamepadType type);

/**
 * Human-readable label for a gamepad axis, honoring the controller family
 * (e.g. LeftTrigger reads "LT" on Xbox, "L2" on PlayStation, "ZL" on Nintendo).
 */
std::string GamepadAxisLabel(GamepadAxis axis, GamepadType type);

// ============================================================================
// Platform-specific input utilities
// ============================================================================

/**
 * Get the platform-specific "action" modifier key
 * - Windows/Linux: Control
 * - Mac: Command (Super)
 * This allows games to use "action modifier" bindings that work correctly
 * across all platforms (e.g., Ctrl+C on Windows, Cmd+C on Mac)
 */
inline KeyCode GetPlatformActionModifier() {
#ifdef __APPLE__
    return KeyCode::LeftSuper;
#else
    return KeyCode::LeftControl;
#endif
}

/**
 * Get the platform-specific "alternate" modifier key
 * - All platforms: Alt
 */
inline KeyCode GetPlatformAltModifier() {
    return KeyCode::LeftAlt;
}

/**
 * Check if a key code is a modifier key
 */
inline bool IsModifierKey(KeyCode key) {
    switch (key) {
        case KeyCode::LeftShift:
        case KeyCode::RightShift:
        case KeyCode::LeftControl:
        case KeyCode::RightControl:
        case KeyCode::LeftAlt:
        case KeyCode::RightAlt:
        case KeyCode::LeftSuper:
        case KeyCode::RightSuper:
            return true;
        default:
            return false;
    }
}

/**
 * Check if two modifier keys are equivalent across platforms
 * (e.g., LeftControl on Windows is equivalent to LeftSuper on Mac for actions)
 */
inline bool AreModifiersEquivalent(KeyCode key1, KeyCode key2) {
    if (key1 == key2) return true;

    // Left/Right variants of the same modifier are equivalent
    if ((key1 == KeyCode::LeftShift && key2 == KeyCode::RightShift) ||
        (key1 == KeyCode::RightShift && key2 == KeyCode::LeftShift) ||
        (key1 == KeyCode::LeftControl && key2 == KeyCode::RightControl) ||
        (key1 == KeyCode::RightControl && key2 == KeyCode::LeftControl) ||
        (key1 == KeyCode::LeftAlt && key2 == KeyCode::RightAlt) ||
        (key1 == KeyCode::RightAlt && key2 == KeyCode::LeftAlt) ||
        (key1 == KeyCode::LeftSuper && key2 == KeyCode::RightSuper) ||
        (key1 == KeyCode::RightSuper && key2 == KeyCode::LeftSuper)) {
        return true;
    }

#ifdef __APPLE__
    // On Mac, Control and Super (Command) are NOT equivalent for user actions
    // Super/Command is the action modifier, Control has different uses
    return false;
#else
    // On Windows/Linux, Super (Windows key) and Control are NOT equivalent
    return false;
#endif
}

/**
 * Get the current platform identifier for input handling
 */
enum class InputPlatform : uint8_t {
    Windows,
    Mac,
    Linux,
    Web,
    iOS,
    Android,
    Unknown
};

inline InputPlatform GetCurrentPlatform() {
#if defined(__EMSCRIPTEN__)
    return InputPlatform::Web;
#elif defined(_WIN32)
    return InputPlatform::Windows;
#elif defined(__APPLE__)
    #if TARGET_OS_IOS || TARGET_OS_IPHONE
        return InputPlatform::iOS;
    #else
        return InputPlatform::Mac;
    #endif
#elif defined(__ANDROID__)
    return InputPlatform::Android;
#elif defined(__linux__)
    return InputPlatform::Linux;
#else
    return InputPlatform::Unknown;
#endif
}

/**
 * Check if the current platform supports touch input
 */
inline bool PlatformSupportsTouchInput() {
    InputPlatform platform = GetCurrentPlatform();
    return platform == InputPlatform::Web ||
           platform == InputPlatform::iOS ||
           platform == InputPlatform::Android;
}

/**
 * Check if the current platform supports gamepad input
 */
inline bool PlatformSupportsGamepad() {
    // All desktop platforms and web support gamepads
    // iOS and Android have limited gamepad support
    InputPlatform platform = GetCurrentPlatform();
    return platform == InputPlatform::Windows ||
           platform == InputPlatform::Mac ||
           platform == InputPlatform::Linux ||
           platform == InputPlatform::Web;
}

} // namespace input
} // namespace lupine
