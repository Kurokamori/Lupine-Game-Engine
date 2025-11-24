#pragma once

/**
 * Lupine Engine - Input Module
 * 
 * Complete input management system for game engines
 * 
 * Features:
 * - Abstract device model (keyboard, mouse, gamepad)
 * - Input action and axis mapping system
 * - State tracking (pressed, just pressed, just released)
 * - Project settings serialization
 * - Runtime and Engine APIs
 * 
 * Usage:
 * 1. Engine layer: Poll platform input and feed to InputManager
 * 2. Game logic: Query actions/axes or raw input
 * 3. Editor: Configure input mappings via InputMap
 */

#include "InputCodes.hpp"
#include "InputDevice.hpp"
#include "InputAction.hpp"
#include "InputManager.hpp"
