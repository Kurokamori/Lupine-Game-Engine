#pragma once

#include "lupine/rendering/ResourceHandles.hpp"

namespace lupine {

class IGfxDevice;

namespace components {

/**
 * Procedural fallback shapes used by particle emitters when no user texture is
 * assigned. Generated once per device and cached.
 */
enum class ParticleShape {
    Square = 0,   ///< Solid filled square (a 2x2 white texture).
    Circle = 1    ///< Soft-edged filled circle (radial alpha falloff).
};

/**
 * Get (creating and caching on first use) a small built-in texture for the
 * requested particle shape. The texture is white so it tints cleanly with the
 * particle color. Returns an invalid handle if the device is null.
 */
TextureHandle GetBuiltinParticleTexture(IGfxDevice* device, ParticleShape shape);

} // namespace components
} // namespace lupine
