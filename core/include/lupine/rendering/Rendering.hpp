#pragma once

/**
 * Lupine Engine Rendering System
 *
 * Main include file for the rendering subsystem.
 * Include this header to access all rendering functionality.
 */

// Core types
#include "ResourceHandles.hpp"

// Graphics device abstraction
#include "gfx/GfxTypes.hpp"
#include "gfx/GfxDescriptors.hpp"
#include "gfx/GfxCommandList.hpp"
#include "gfx/IGfxDevice.hpp"

// Backend implementations
#include "backends/GfxDeviceOpenGL.hpp"
#include "backends/GfxDeviceVulkan.hpp"
#include "backends/GfxDeviceMetal.hpp"
#include "backends/GfxDeviceWebGL.hpp"
#include "backends/GfxDeviceDX11.hpp"
#include "backends/GfxDeviceDX12.hpp"

// High-level rendering
#include "Material.hpp"
#include "RenderCamera.hpp"
#include "RenderView.hpp"
#include "DrawCommand.hpp"
#include "RenderContext.hpp"
#include "RenderWorld.hpp"
