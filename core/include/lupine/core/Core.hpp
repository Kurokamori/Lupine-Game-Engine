#pragma once

/**
 * Lupine Engine - Core Module
 *
 * This module provides core functionality for the engine including:
 * - Serialization and reflection system
 * - UUID generation
 * - Type registry
 * - Asset loading and management
 * - Audio management and playback
 */

#include "PropertyDescriptor.hpp"
#include "Serialization.hpp"
#include "UUID.hpp"
#include "lupine/asset/Assets.hpp"
#include "lupine/audio/Audio.hpp"

#include <string>
#include <vector>

namespace lupine {
namespace core {

// Core module initialization
void InitializeCore();
void ShutdownCore();

// Built-in type registration
void RegisterBuiltInTypes();

// Process-global command-line / runtime arguments.
//
// Set once at startup by the runtime entry points (RuntimeApp::initialize and
// the standalone runtime main) and read back by the scripting layer
// (get_cmdline_args() in Lua/MRuby/MicroPython) and the C-API
// (lc_cmdline_arg_count / lc_cmdline_arg_at). These are the extra arguments a
// game is launched with - the editor uses them to give each multi-instance
// play session a distinct role (e.g. --server vs --client). Empty by default.
void SetCommandLineArgs(const std::vector<std::string>& args);
const std::vector<std::string>& GetCommandLineArgs();

} // namespace core
} // namespace lupine

