#pragma once

#include "IGfxDevice.hpp"
#include <memory>

namespace lupine {

/**
 * Factory for creating graphics devices.
 * Selects the appropriate backend based on platform and availability.
 */
class GfxDeviceFactory {
public:
    /**
     * Create a graphics device for the specified backend.
     */
    static std::unique_ptr<IGfxDevice> create(GraphicsBackend backend);

    /**
     * Get the default backend for the current platform.
     */
    static GraphicsBackend getDefaultBackend();

    /**
     * Check if a backend is available on the current platform.
     */
    static bool isBackendAvailable(GraphicsBackend backend);

    /**
     * Get a human-readable name for a backend.
     */
    static const char* getBackendName(GraphicsBackend backend);
};

} // namespace lupine
