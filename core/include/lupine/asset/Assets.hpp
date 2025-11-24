#pragma once

/**
 * Lupine Engine - Asset Module
 *
 * This module provides asset loading and management functionality including:
 * - Base asset class with UUID tracking and reference counting
 * - Image loading (PNG, JPG, BMP, TGA, HDR)
 * - Model loading (GLTF, FBX, OBJ) with meshes, materials, textures, skeletons, and animations
 * - Font loading (TTF, OTF) with glyph atlas generation
 * - Audio loading (WAV, MP3, OGG) with streaming and preload support
 */

#include "Asset.hpp"
#include "ImageAsset.hpp"
#include "ModelAsset.hpp"
#include "FontAsset.hpp"
#include "AudioAsset.hpp"
#include "SpriteAnimationAsset.hpp"

namespace lupine {
namespace asset {

// Initialize the asset module
void InitializeAssets();

// Shutdown the asset module
void ShutdownAssets();

} // namespace asset
} // namespace lupine

