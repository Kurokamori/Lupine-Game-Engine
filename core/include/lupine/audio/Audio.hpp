#pragma once

/**
 * Lupine Engine - Audio Module
 * 
 * This module provides comprehensive audio management functionality including:
 * - Audio asset loading (WAV, MP3, OGG)
 * - Preloaded and streaming audio support
 * - Audio bus system with hierarchical mixing
 * - Playback controls (play, pause, stop, resume)
 * - Looping, one-shot, and scheduled playback modes
 * - 3D spatial audio with distance attenuation
 * - Stereo panning (L/R)
 * - Volume control per source and per bus
 * - Mute and solo functionality
 * - Serialization support for project settings
 * 
 * Note: This is a backend-agnostic audio management system.
 * It handles audio data and playback logic but does not interface
 * with actual audio hardware. For hardware playback, integrate with
 * an audio backend like OpenAL, FMOD, Wwise, or platform-specific APIs.
 */

#include "AudioManager.hpp"
#include "lupine/asset/AudioAsset.hpp"

namespace lupine {
namespace audio {

// Initialize the audio module
void InitializeAudio();

// Shutdown the audio module
void ShutdownAudio();

} // namespace audio
} // namespace lupine

