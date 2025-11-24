#pragma once

/**
 * Lupine Engine - Main Engine Module
 *
 * This module provides the core engine functionality including:
 * - Node system (Node, Node2D, Node3D)
 * - Component system
 * - Scene management
 * - Project management
 */

#include "lupine/core/Core.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"

// Core module includes
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/SceneInstance.hpp"

// Engine module includes
#include "EditorBridge.hpp"

#include <memory>

namespace lupine {
namespace engine {

/**
 * Initialize the engine module
 * This should be called once at application startup
 */
void InitializeEngine();

/**
 * Shutdown the engine module
 * This should be called once at application shutdown
 */
void ShutdownEngine();

/**
 * Register built-in node and component types
 */
void RegisterBuiltInTypes();

} // namespace engine
} // namespace lupine
