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

namespace lupine {
namespace core {

// Core module initialization
void InitializeCore();
void ShutdownCore();

// Built-in type registration
void RegisterBuiltInTypes();

} // namespace core
} // namespace lupine

